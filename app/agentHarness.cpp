#include "app/agentHarness.hpp"

// ===== BAK_AGENT (entire file is the removable automation harness) =====

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "app/config.hpp"
#include "bak/camera.hpp"
#include "bak/coordinates.hpp"
#include "bak/entityType.hpp"
#include "bak/types.hpp"
#include "com/json.hpp"
#include "com/png.hpp"
#include "game/combat/combatManager.hpp"
#include "game/combat/grid.hpp"
#include "game/combat/types.hpp"
#include "game/gameRunner.hpp"
#include "graphics/inputHandler.hpp"
#include "gui/dialogRunner.hpp"
#include "gui/guiManager.hpp"

#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{

// Resolve a command token to a GLFW key code. Accepts three spellings:
//   - a single ASCII letter/digit -> its GLFW_KEY_* (GLFW maps A-Z/0-9 to their
//     ASCII codes: GLFW_KEY_W == 'W' == 87, GLFW_KEY_5 == '5' == 53)
//   - an all-digits token -> a raw GLFW key code (e.g. 265 == GLFW_KEY_UP)
//   - a named key (UP/DOWN/LEFT/RIGHT/SPACE/ENTER/EQUAL/MINUS/...) -> its code
int ParseKey(const std::string& tok)
{
    if (tok.empty()) return -1;

    if (tok.size() == 1)
    {
        const auto c = static_cast<unsigned char>(
            std::toupper(static_cast<unsigned char>(tok[0])));
        if (std::isalnum(c)) return static_cast<int>(c);
    }

    bool allDigit = true;
    for (char ch : tok)
        if (!std::isdigit(static_cast<unsigned char>(ch))) { allDigit = false; break; }
    if (allDigit)
    {
        try { return std::stoi(tok); } catch (...) { return -1; }
    }

    std::string up;
    up.reserve(tok.size());
    for (char ch : tok)
        up.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));

    static const std::unordered_map<std::string, int> names = {
        {"SPACE", GLFW_KEY_SPACE},
        {"ESC", GLFW_KEY_ESCAPE}, {"ESCAPE", GLFW_KEY_ESCAPE},
        {"ENTER", GLFW_KEY_ENTER}, {"RETURN", GLFW_KEY_ENTER},
        {"TAB", GLFW_KEY_TAB},
        {"BACKSPACE", GLFW_KEY_BACKSPACE}, {"BS", GLFW_KEY_BACKSPACE},
        {"UP", GLFW_KEY_UP}, {"DOWN", GLFW_KEY_DOWN},
        {"LEFT", GLFW_KEY_LEFT}, {"RIGHT", GLFW_KEY_RIGHT},
        {"EQUAL", GLFW_KEY_EQUAL}, {"PLUS", GLFW_KEY_EQUAL},
        {"MINUS", GLFW_KEY_MINUS},
        {"F1", GLFW_KEY_F1}, {"F2", GLFW_KEY_F2}, {"F3", GLFW_KEY_F3},
        {"F4", GLFW_KEY_F4}, {"F5", GLFW_KEY_F5},
        {"LEFT_SHIFT", GLFW_KEY_LEFT_SHIFT}, {"RIGHT_SHIFT", GLFW_KEY_RIGHT_SHIFT},
    };
    const auto it = names.find(up);
    return it == names.end() ? -1 : it->second;
}

} // namespace

AgentHarness::AgentHarness(
    GLFWwindow* window,
    Graphics::InputHandler& inputHandler,
    Game::GameRunner& gameRunner,
    Gui::GuiManager& guiManager,
    const Config::Agent& agent)
:
    mWindow{window},
    mInputHandler{inputHandler},
    mGameRunner{gameRunner},
    mGuiManager{guiManager},
    mAgent{agent}
{
}

void AgentHarness::PollCommands()
{
    // Apply a held key every frame (continuous movement) -- the held bindings
    // (WASD/arrows) step the camera by mDeltaTime on each invocation, mirroring a
    // physically-held key that HandleInput would poll.
    if (mHeldKey >= 0)
        mInputHandler.InvokeHeldKey(mHeldKey);

    // Detect command-file writes by mtime. The agent rewrites the file fresh each
    // command; we read + dispatch + truncate, then re-baseline to the post-
    // truncate mtime so the empty file isn't re-read as a new change next frame.
    std::error_code ec;
    const auto mtime = std::filesystem::last_write_time(mAgent.mCommandFile, ec);
    if (ec) return;
    if (mHaveCmdMtime && mtime == mLastCmdMtime) return;

    {
        std::ifstream in{mAgent.mCommandFile};
        if (in.is_open())
        {
            std::string line;
            while (std::getline(in, line))
            {
                if (line.empty()) continue;
                DispatchCommand(line);
            }
        }
    }

    // Truncate so the agent can issue the next command by rewriting the file.
    {
        std::ofstream out{mAgent.mCommandFile, std::ios::out | std::ios::trunc};
    }

    const auto postMtime = std::filesystem::last_write_time(mAgent.mCommandFile, ec);
    if (!ec)
    {
        mLastCmdMtime = postMtime;
        mHaveCmdMtime = true;
    }
}

void AgentHarness::DispatchCommand(const std::string& line)
{
    std::istringstream iss{line};
    std::string cmd;
    if (!(iss >> cmd)) return;

    if (cmd == "dump")
    {
        mDumpPending = true;
    }
    else if (cmd == "click")
    {
        double x = 0, y = 0;
        if (!(iss >> x >> y)) return;
        std::string which{"left"};
        iss >> which;
        const int button = (which == "right")
            ? GLFW_MOUSE_BUTTON_RIGHT
            : GLFW_MOUSE_BUTTON_LEFT;
        // Position the cursor first; HandleMouseCallback reads glfwGetCursorPos
        // internally, so the bound lambdas (guiScaleInv / pickScale) see this pos
        // exactly as a physical click would.
        glfwSetCursorPos(mWindow, x, y);
        mInputHandler.HandleMouseCallback(mWindow, button, GLFW_PRESS, 0);
        mInputHandler.HandleMouseCallback(mWindow, button, GLFW_RELEASE, 0);
    }
    else if (cmd == "press")
    {
        std::string tok;
        if (iss >> tok)
        {
            const int key = ParseKey(tok);
            if (key >= 0)
                mInputHandler.HandleKeyboardCallback(mWindow, key, 0, GLFW_PRESS, 0);
        }
    }
    else if (cmd == "hold")
    {
        std::string tok;
        if (iss >> tok)
        {
            const int key = ParseKey(tok);
            if (key >= 0) mHeldKey = key;
        }
    }
    else if (cmd == "release")
    {
        std::string tok;
        if (iss >> tok)
        {
            const int key = ParseKey(tok);
            if (mHeldKey == key) mHeldKey = -1;
        }
        else
        {
            mHeldKey = -1; // no arg -> release whatever is held
        }
    }
    else if (cmd == "char")
    {
        // Take the first non-space char of the remainder (text entry, e.g. save-
        // game names). GLFW's char callback takes a codepoint; we pass one byte.
        std::string rest;
        std::getline(iss, rest);
        const auto first = rest.find_first_not_of(" \t");
        if (first != std::string::npos)
        {
            const auto codepoint = static_cast<unsigned>(
                static_cast<unsigned char>(rest[first]));
            mInputHandler.HandleCharacterCallback(mWindow, codepoint);
        }
    }
    else if (cmd == "quit")
    {
        glfwSetWindowShouldClose(mWindow, GLFW_TRUE);
    }
}

void AgentHarness::MaybeCapture(int fbW, int fbH)
{
    if (!mDumpPending) return;
    mDumpPending = false;
    CaptureFrame(fbW, fbH);
    WriteStateJson();
    ++mFrame;
}

void AgentHarness::EnsureCaptureTarget(int w, int h)
{
    if (mCaptureFbo != 0 && mCaptureW == w && mCaptureH == h) return;

    if (mCaptureFbo != 0)
    {
        glDeleteFramebuffers(1, &mCaptureFbo);
        glDeleteRenderbuffers(1, &mCaptureColor);
        mCaptureFbo = 0;
        mCaptureColor = 0;
    }
    if (w <= 0 || h <= 0) return;

    glGenFramebuffers(1, &mCaptureFbo);
    glGenRenderbuffers(1, &mCaptureColor);
    glBindRenderbuffer(GL_RENDERBUFFER, mCaptureColor);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, w, h);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, mCaptureFbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, mCaptureColor);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    mCaptureW = w;
    mCaptureH = h;
}

void AgentHarness::CaptureFrame(int fbW, int fbH)
{
    EnsureCaptureTarget(fbW, fbH);
    if (mCaptureFbo == 0) return;

    // Save the GL state we touch so the rest of the frame is unaffected.
    GLint prevReadFbo = 0, prevDrawFbo = 0, prevReadBuffer = 0, prevPackAlign = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFbo);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDrawFbo);
    glGetIntegerv(GL_READ_BUFFER, &prevReadBuffer);
    glGetIntegerv(GL_PACK_ALIGNMENT, &prevPackAlign);

    glPixelStorei(GL_PACK_ALIGNMENT, 4);

    // The default framebuffer is multisampled (GLFW_SAMPLES=4); a direct
    // glReadPixels of an MSAA default FB is macOS-fragile, so blit-resolve the
    // back buffer into the non-MSAA capture FBO first.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mCaptureFbo);
    glBlitFramebuffer(
        0, 0, fbW, fbH,
        0, 0, fbW, fbH,
        GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // Read the resolved pixels (GL origin is bottom-left).
    glBindFramebuffer(GL_READ_FRAMEBUFFER, mCaptureFbo);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    const std::size_t bytes = static_cast<std::size_t>(fbW) * fbH * 4;
    std::vector<std::uint8_t> buf(bytes);
    glReadPixels(0, 0, fbW, fbH, GL_RGBA, GL_UNSIGNED_BYTE, buf.data());

    // Restore GL state.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, prevReadFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDrawFbo);
    glReadBuffer(prevReadBuffer);
    glPixelStorei(GL_PACK_ALIGNMENT, prevPackAlign);

    // Flip rows bottom-up -> top-down for PNG.
    PNGImage image{};
    image.mWidth = static_cast<std::uint32_t>(fbW);
    image.mHeight = static_cast<std::uint32_t>(fbH);
    image.mPixels.resize(static_cast<std::size_t>(fbW) * fbH);
    for (int y = 0; y < fbH; ++y)
    {
        const int srcRow = fbH - 1 - y;
        for (int x = 0; x < fbW; ++x)
        {
            const auto* p = &buf[(static_cast<std::size_t>(srcRow) * fbW + x) * 4];
            image.mPixels[static_cast<std::size_t>(y) * fbW + x] =
                PNGColor{p[0], p[1], p[2], p[3]};
        }
    }

    const std::string path = mAgent.mDumpDir + "/frame.png";
    try
    {
        WritePNG(path.c_str(), image);
    }
    catch (const std::exception&)
    {
        // A failed capture must never take the game down -- swallow it.
    }
}

void AgentHarness::WriteStateJson()
{
    const auto& gameRunner = mGameRunner;
    const auto& gameState = gameRunner.mGameState;
    const auto& guiManager = mGuiManager;
    const auto& combat = gameRunner.mCombatManager;

    nlohmann::json j;
    j["frame"] = mFrame;

    // View mode (combat takes precedence over overhead over main).
    std::string mode = "other";
    if (guiManager.InCombatView()) mode = "combat";
    else if (guiManager.InOverheadMap()) mode = "overhead";
    else if (guiManager.InMainView()) mode = "main";
    j["mode"] = mode;

    const bool combatActive = combat.IsCombatActive();
    j["combatActive"] = combatActive;

    j["zone"] = gameState.GetZone().mValue;

    // Party position + heading -- the source of truth regardless of which camera
    // is active (main3d sets it from the active camera each frame).
    const auto loc = gameState.GetLocation();
    {
        nlohmann::json pos;
        pos["x"] = loc.mPosition.x;
        pos["y"] = loc.mPosition.y;
        j["position"] = pos;
    }
    j["heading"] = loc.mHeading;

    // Main camera position (overhead uses mapCamera, but the party position above
    // is the meaningful one for the agent; this is for orientation/debugging).
    const auto camPos = gameRunner.mCamera.GetPosition();
    {
        nlohmann::json cp;
        cp["x"] = camPos.x;
        cp["y"] = camPos.y;
        cp["z"] = camPos.z;
        j["cameraPos"] = cp;
    }

    // System cursor (GLFW content/point coords).
    {
        double cx = 0, cy = 0;
        glfwGetCursorPos(mWindow, &cx, &cy);
        nlohmann::json cur;
        cur["x"] = cx;
        cur["y"] = cy;
        j["cursor"] = cur;
    }

    // Current dialog text, or null when no dialog is active.
    const auto dialogText = guiManager.GetDialogRunner().GetCurrentDialogText();
    if (dialogText.empty())
        j["dialog"] = nullptr;
    else
        j["dialog"] = std::string{dialogText.data(), dialogText.size()};

    // Screen-specific entity state.
    if (combatActive)
    {
        const auto& grid = combat.GetGrid();
        {
            nlohmann::json gj;
            gj["cols"] = grid.GetCols();
            gj["rows"] = grid.GetRows();
            j["grid"] = gj;
        }

        nlohmann::json arr = nlohmann::json::array();
        for (const auto& c : combat.GetCombatantsRef())
        {
            nlohmann::json cj;
            cj["entity"] = c.mEntityIndex.mValue;
            {
                nlohmann::json gp;
                gp["x"] = c.mGridPos.x;
                gp["y"] = c.mGridPos.y;
                cj["grid"] = gp;
            }
            cj["dead"] = c.IsDead();
            cj["player"] = (c.mCharacter != nullptr);
            arr.push_back(cj);
        }
        j["combatants"] = arr;
    }
    else
    {
        // Clickable world entities (main + overhead views).
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& [idx, ent] : gameRunner.mClickables)
        {
            nlohmann::json ej;
            ej["id"] = idx.mValue;
            ej["type"] = static_cast<unsigned>(ent.mEntityType);
            arr.push_back(ej);
        }
        j["clickables"] = arr;
    }

    const std::string path = mAgent.mDumpDir + "/state.json";
    std::ofstream out{path, std::ios::out | std::ios::trunc};
    if (out.is_open())
        out << j.dump(2);
}

// ===== END BAK_AGENT =====