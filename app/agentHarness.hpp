#pragma once

// ===== BAK_AGENT (removable automation harness) =====
// Opt-in, off-by-default, fully-removable harness (gated by Config::Agent) that
// lets a blind CLI agent SEE the game (dump -> <DumpDir>/frame.png + state.json)
// and DRIVE it (control file -> the real InputHandler handlers). Entirely
// BAK_AGENT; removal = delete this pair + the app/CMakeLists.txt entry + the
// fenced hooks in main3d.cpp + the one-line getters (grep BAK_AGENT). See the
// approved plan. NOT a ROADMAP task. Inert when Config::Agent::mEnabled is false.
// ===== END BAK_AGENT =====

#include "app/config.hpp"

#include <filesystem>

struct GLFWwindow;

namespace Graphics { class InputHandler; }
namespace Game { class GameRunner; }
namespace Gui { class GuiManager; }

// ===== BAK_AGENT (removable automation harness) =====
class AgentHarness
{
public:
    AgentHarness(
        GLFWwindow* window,
        Graphics::InputHandler& inputHandler,
        Game::GameRunner& gameRunner,
        Gui::GuiManager& guiManager,
        const Config::Agent& agent);

    // Per-frame: apply any held key, then poll the command file + dispatch.
    void PollCommands();
    // Per-frame, before swap: if a `dump` is pending, capture frame.png + state.json.
    void MaybeCapture(int fbW, int fbH);

private:
    void DispatchCommand(const std::string& line);
    void CaptureFrame(int fbW, int fbH);
    void EnsureCaptureTarget(int w, int h);
    void WriteStateJson();

    GLFWwindow* mWindow;
    Graphics::InputHandler& mInputHandler;
    Game::GameRunner& mGameRunner;
    Gui::GuiManager& mGuiManager;
    const Config::Agent& mAgent;

    // Held-key simulation (hold/release): invoked every frame while set, so WASD
    // movement steps each frame exactly like a physically-held key.
    int mHeldKey{-1};

    // Command-file change detection by mtime. Baseline is re-stamped AFTER a
    // read+truncate so the post-truncate (empty) mtime isn't seen as a new write.
    std::filesystem::file_time_type mLastCmdMtime{};
    bool mHaveCmdMtime{false};

    // Set by a `dump` command; consumed by the next MaybeCapture.
    bool mDumpPending{false};

    // Monotonic counter written to state.json so the agent can detect a fresh dump.
    unsigned long mFrame{0};

    // Lazily-created non-MSAA capture FBO. The default framebuffer is multisampled
    // (GLFW_SAMPLES=4) and a direct glReadPixels of it is macOS-fragile, so the
    // capture blit-resolves the back buffer into this, then reads it. `unsigned
    // int` == GLuint; kept out of the GL headers so this header stays GL-free.
    unsigned int mCaptureFbo{0};
    unsigned int mCaptureColor{0};
    int mCaptureW{0};
    int mCaptureH{0};
};
// ===== END BAK_AGENT =====