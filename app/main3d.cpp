#include "app/config.hpp"
// ===== BAK_AGENT (removable automation harness) =====
#include "app/agentHarness.hpp"
#include <optional> // std::optional<AgentHarness>
// ===== END BAK_AGENT =====

#include "bak/backgroundSounds.hpp"
#include "bak/camera.hpp"
#include "bak/constants.hpp"
#include "bak/dialogJson.hpp"
#include "bak/lua/core.hpp"

#include "bak/state/encounter.hpp"
#include "bak/encounter//encounter.hpp"
#include "bak/zone.hpp"

extern "C" {
#include "com/getopt.h"
}

#include "com/logger.hpp"
#include "com/path.hpp"
#include "com/visit.hpp"

#include "game/console.hpp"
#include "game/gameRunner.hpp"
#include "game/screens.hpp"
#include "game/systems.hpp"

#include "graphics/inputHandler.hpp"
#include "graphics/guiRenderer.hpp"
#include "graphics/glfw.hpp"
#include "graphics/graphicsConfig.hpp"
#include "graphics/renderer.hpp"
#include "graphics/sprites.hpp"
#include "graphics/vramTracker.hpp"

#include "gui/guiManager.hpp"
#include "gui/window.hpp"

#include "imgui/imguiWrapper.hpp"

#include <GL/glew.h>

#include <GLFW/glfw3.h>

#include <cmath>
#include <filesystem>
#include <functional>
#include <memory>
#include <numbers>
#include <sstream>

#undef main
struct Options
{
    bool showImgui{true};
    std::string logLevel{""};
    std::string configFile{""};
};

Options Parse(int argc, char** argv)
{
    Options values{};

    struct option options[] = {
        {"help", no_argument,       0, 'h'},
        {"config", required_argument, 0, 'c'},
        {"log_level", required_argument, 0, 'l'},
        {"imgui", no_argument, 0, 'i'},
    };
    int optionIndex = 0;
    int opt;
    while ((opt = getopt_long(argc, argv, "hil:c:", options, &optionIndex)) != -1)
    {
        if (opt == 'h')
        {
            exit(0);
        }
        else if (opt == 'c')
        {
            if (optarg == nullptr)
            {
                std::cerr << "No argument provide to '-c/--config'" << std::endl;
                exit(1);
            }
            values.configFile = std::string{optarg};
        }
        else if (opt == 'i')
        {
            values.showImgui = false;
        }
        else if (opt == 'l')
        {
            if (optarg == nullptr)
            {
                std::cerr << "No argument provide to '-c/--config'" << std::endl;
                exit(1);
            }
            values.logLevel = std::string{optarg};
        }
    }

    return values;
}

Config::Config LoadConfigFile(std::string configPath)
{
    auto config = Config::Config{};
    std::string loadedPath;
    auto TryLoad = [&config,&loadedPath](std::string path)
    {
        try
        {
            std::cout << "Loading config file: " << path << std::endl;
            config = Config::LoadConfig(path);
            loadedPath = path;
            return "";
        }
        catch (const std::exception& error)
        {
            std::cerr << "Failed to load config file due to: " << error.what() << std::endl;
            exit(1);
        }
    };

    const auto defaultConfig = (Paths::Get().GetBakDirectoryPath() / "config.json").string();
    const auto currentDirectoryConfig = "config.json";

    if (!configPath.empty())
    {
        TryLoad(configPath);
    }
    else if (std::filesystem::exists(currentDirectoryConfig))
    {
        TryLoad(currentDirectoryConfig);
    }
    else if (std::filesystem::exists(defaultConfig))
    {
        TryLoad(defaultConfig);
    }
    else
    {
        std::cout << "Not loading a config file.\n";
    }

    // Task 0.6: resolve relative Paths entries against the config file's
    // directory (absolute + symlink-resolved), not cwd, so engine/config.json
    // is portable and main3d no longer has to be run from the project root.
    // Empty/absolute entries pass through; no-op if no config was loaded.
    if (!loadedPath.empty())
        Config::ResolvePaths(config.mPaths, loadedPath);

    // Task 4.4: record the actually-loaded config path so the in-game settings
    // screen can persist graphics values back to it without a ctor param. Empty
    // if no config was loaded (the screen logs + no-ops on an empty path).
    if (!loadedPath.empty())
        Graphics::GraphicsConfig::Get().SetConfigPath(loadedPath);

    return config;
}

int main(int argc, char** argv)
{
    const auto options = Parse(argc, argv);
    const auto config = LoadConfigFile(options.configFile);
    Logging::LogState::SetLogTime(config.mLogging.mLogTime);
    Logging::LogState::SetLogColor(config.mLogging.mLogColours);
    if (options.logLevel != "")
    {
        Logging::LogState::SetLevel(options.logLevel);
    }
    else
    {
        Logging::LogState::SetLevel(config.mLogging.mLogLevel);
    }

    std::unique_ptr<std::ofstream> logFileStream{};

    if (config.mLogging.mLogToFile)
    {
        auto logFilePath = config.mLogging.mLogFilePath.empty()
            ? Paths::Get().GetBakDirectoryPath() / "main3d.log"
            : std::filesystem::path{config.mLogging.mLogFilePath};
        std::cout << "Will log to file: " << logFilePath << "\n";
        auto logDirectory = logFilePath;
        logDirectory.remove_filename();
        if (!std::filesystem::exists(logDirectory))
        {
            std::cerr << "Log file directory: " << logDirectory << " does not exist, will not log to file!\n";
        }
        else
        {
            logFileStream = std::make_unique<std::ofstream>(logFilePath.string(), std::ios::out);
            if (!logFileStream->is_open())
            {
                std::cerr << "Could not open log file: " << logFilePath << ", will not log to file!\n";
            }
            else 
            {
                Logging::LogState::AddStream(logFileStream.get());
            }
        }
    }

    const auto& logger = Logging::LogState::GetLogger("main");
    for (const auto& disabled : config.mLogging.mDisabledLoggers)
    {
        Logging::LogState::Disable(disabled);
    }
    for (const auto& enabled : config.mLogging.mEnabledLoggers)
    {
        Logging::LogState::Enable(enabled);
    }
    
    if (!config.mPaths.mGameData.empty())
    {
        Paths::Get().SetBakDirectory(config.mPaths.mGameData);
    }

    if (!config.mPaths.mGraphicsOverrides.empty())
    {
        Paths::Get().SetModDirectory(config.mPaths.mGraphicsOverrides);
    }

    if (!config.mPaths.mAssets4k.empty())
    {
        Paths::Get().SetAssets4kDirectory(config.mPaths.mAssets4k);
    }

    Graphics::GraphicsConfig::Get().SetMaxTextureDim(config.mGraphics.mMaxTextureDim);
    Graphics::GraphicsConfig::Get().SetMaxTextures(config.mGraphics.mMaxTextures);
    Graphics::GraphicsConfig::Get().SetRGBA8Upload(config.mGraphics.mRGBA8Upload);
    Graphics::GraphicsConfig::Get().SetAsyncTextureUpload(config.mGraphics.mAsyncTextureUpload);
    Graphics::GraphicsConfig::Get().SetHeroTextures(config.mGraphics.mHeroTextures);
    Graphics::GraphicsConfig::Get().SetAnisotropicFilter(config.mGraphics.mAnisotropicFilter);
    // Task 4.4: Original-mode gate (FindSubstitute) + the editable ResolutionScale
    // copy the settings screen reads/writes. Window size still comes from config
    // directly below (line ~253); the singleton holds the persisted-for-next-boot copy.
    Graphics::GraphicsConfig::Get().SetOriginalMode(config.mGraphics.mOriginalMode);
    Graphics::GraphicsConfig::Get().SetResolutionScale(config.mGraphics.mResolutionScale);

    {
        const auto defaultDialog = (Paths::Get().GetBakDirectoryPath() / "dialogMods").string();
        auto dialogsDir = config.mPaths.mDialogMods.empty()
            ? defaultDialog
            : config.mPaths.mDialogMods;
        logger.Info() << "Try to load dialog overrides from directory: " << dialogsDir << "\n";
        BAK::DialogJson::LoadAllFromDirectory(dialogsDir);
    }

    {
        const auto defaultLuaMods = (Paths::Get().GetBakDirectoryPath() / "luaMods").string();
        auto luaModsPath = config.mPaths.mLuaMods.empty()
            ? defaultLuaMods
            : config.mPaths.mLuaMods;
        logger.Info() << "Try to load lua mods from directory: " << luaModsPath << "\n";
        BAK::Lua::Initialize(luaModsPath);
    }

    if (config.mAudio.mEnableAudio)
    {
        auto& provider = AudioA::AudioManagerProvider::Get();
        auto audioManager = std::make_unique<AudioA::AudioManager>();
        audioManager->Set(audioManager.get());
        audioManager->SwitchMidiPlayer(AudioA::StringToMidiPlayer(config.mAudio.mMidiPlayer));
        provider.SetAudioManager(std::move(audioManager));
    }
    else
    {
        auto& provider = AudioA::AudioManagerProvider::Get();
        auto audioManager = std::make_unique<AudioA::NullAudioManager>();
        provider.SetAudioManager(std::move(audioManager));
    }

    bool showImgui = config.mGraphics.mEnableImGui;
    auto guiScalar = config.mGraphics.mResolutionScale;
    // Task 4.1: ResolutionScale is the dynamic UI-scaling knob -- it multiplies
    // the 320x200 logical GUI to the window (default 4.0 -> 1280x800). A
    // non-positive value would make a 0x0 window and crash; clamp to >= 1.0
    // (native 320x200) so a misconfigured config.json degrades gracefully
    // instead of hard-crashing in MakeGlfwWindow.
    if (guiScalar < 1.0f)
    {
        logger.Warn() << " ResolutionScale " << guiScalar
            << " is < 1.0 (would produce a sub-native window); clamping to 1.0\n";
        guiScalar = 1.0f;
    }

    auto nativeWidth = 320.0f;
    auto nativeHeight = 200.0f;

    auto width = nativeWidth * guiScalar;
    auto height = nativeHeight * guiScalar;
    auto guiScaleInv = glm::vec2{1 / guiScalar, 1 / guiScalar};

    // 3D-pick scale: maps the system cursor (GLFW content/point coords, the
    // space clickPos / pointerPos arrive in) into the pick buffer's own pixel
    // space (mScreenDims). The pick buffer is built once at the REQUESTED
    // window size (width x height), but the OS may clamp the window to the
    // monitor work area (e.g. macOS menu bar) so the actual content size is
    // smaller — e.g. requested 3456x2160 clamped to 3456x2104. Without this
    // scale the click is mapped against the pre-clamp height (2160) while the
    // screen renders into the clamped framebuffer (2104), so grid cells sit at
    // different y-pixels in each buffer and clicks land ~28px above the cell
    // (worst at the bottom) — combat grid/enemy clicks read 0. Recomputed each
    // frame from the actual content size, like guiScaleInv. No-op when content
    // size == requested (windowed, no clamp) or on Retina (pick buffer = logical
    // = content). See the "Task 4.4 fix" comment below for the same class of
    // bug on the GUI cursor path.
    glm::vec2 pickScale{1.0f, 1.0f};

    /* OPEN GL / GLFW SETUP  */

    auto window = Graphics::MakeGlfwWindow(
        height,
        width,
        "BaK",
        // ===== BAK_AGENT (removable automation harness) =====
        config.mGraphics.mBorderlessWindow);
        // ===== END BAK_AGENT =====

    auto spriteManager = Graphics::SpriteManager{};
    auto guiRenderer = Graphics::GuiRenderer{
        width,
        height,
        guiScalar,
        spriteManager};

    auto root = Gui::Window{
        spriteManager,
        width / guiScalar,
        height / guiScalar};
        
    auto gameState = BAK::GameState{};

    auto guiManager = Gui::GuiManager{
        root.GetCursor(),
        spriteManager,
        gameState
    };

    guiManager.SetDebugDisableFades(config.mGraphics.mDebugDisableFades);
    root.AddChildFront(&guiManager);
    guiManager.EnterMainMenu(false);

    Camera lightCamera{
        static_cast<unsigned>(width),
        static_cast<unsigned>(height),
        400 * 30.0f,
        2.0f};
    lightCamera.UseOrthoMatrix(400, 400);

    Camera camera{
        static_cast<unsigned>(width),
        static_cast<unsigned>(height),
        400 * 30.0f,
        1.0f};
    Camera* cameraPtr = &camera;

    // ROADMAP 4.7 Overhead Map — separate top-down ortho camera for the map's
    // 3D pass. Configured per-frame in the render loop when InOverheadMap().
    Camera mapCamera{
        static_cast<unsigned>(width),
        static_cast<unsigned>(height),
        400 * 30.0f,
        1.0f};
    // Top-down pass tuning knobs (calibrate via the gemma3 vision loop).
    constexpr float kMapCamHeightBAK  = 100000.0f; // camera Y (BAK units); above the cluster
    constexpr float kMapNorthUpYaw    = std::numbers::pi_v<float>; // north-up; rotate if gemma3 says so
    constexpr float kMapFar           = 2000.0f;   // ortho far plane (normalised GL units)
    constexpr int   kMapDrawDistance  = 2000000;   // BAK units (~31 tiles) — beats the per-item cull
    constexpr bool  kMapRenderSprites = false;     // billboards render edge-on from straight overhead

    // VAO/VBO for the red party-triangle marker, drawn in clip space over the
    // map viewport (tip = facing direction). 3 verts × (vec3 pos + vec3 uv);
    // uses the GUI shader with colorMode = SolidColor (no texture needed).
    GLuint mapMarkerVao = 0, mapMarkerVbo = 0;
    glGenVertexArrays(1, &mapMarkerVao);
    glGenBuffers(1, &mapMarkerVbo);
    glBindVertexArray(mapMarkerVao);
    glBindBuffer(GL_ARRAY_BUFFER, mapMarkerVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 18, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, static_cast<void*>(nullptr));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, reinterpret_cast<void*>(sizeof(float) * 3));
    glBindVertexArray(0);

    guiManager.mMainView.SetHeading(camera.GetHeading());

    // OpenGL 3D Renderer
    constexpr auto sShadowDim = 4096;
    bool runningGame = false;
    auto renderer = Graphics::Renderer{
        width,
        height,
        sShadowDim,
        sShadowDim,
        config.mGraphics.mDrawDistance};

    Game::GameRunner gameRunner{
        camera,
        gameState,
        guiManager,
        config.mGraphics.mDebugRenderEncounters,
        config.mGame.mCombatSpeed};

    // Wire up the zone loader to the GUI manager
    guiManager.SetZoneLoader(&gameRunner);

    bool imGuiInitialised = false;

    auto currentTile = camera.GetGameTile();
    logger.Info() << " Starting on tile: " << currentTile << "\n";

    Graphics::Light light{
        .mDirection =     glm::vec3{.0, -.25,  .00},
        .mAmbientColor =  glm::vec3{.5,  .5,   .5 },
        .mDiffuseColor =  glm::vec3{ 1,  .85,  .87},
        .mSpecularColor = glm::vec3{.2,  .2,   .2 },
        .mFogStrength = 0.0005f,
        .mFogColor =      glm::vec3{.15, .31,  .36}
    };

    const auto UpdateLightCamera = [&]{
        const auto lightPos = camera.GetNormalisedPosition() - 100.0f * glm::normalize(light.mDirection);
        const auto diff = lightCamera.GetNormalisedPosition() - camera.GetNormalisedPosition();
        const auto horizDistance = glm::sqrt((diff.x * diff.x) + (diff.z * diff.z));
        const auto yAngle = -glm::atan(diff.y / horizDistance);
        const auto xAngle = glm::atan(diff.x, diff.z) - ((180.0f / 360.0f) * (2 * 3.141592)) ;

        lightCamera.SetAngle(glm::vec2{xAngle, yAngle});
        lightCamera.SetPosition(lightPos * BAK::gWorldScale);
    };

    auto UpdateGameTile = [&]()
    {
        if (camera.GetGameTile() != currentTile)
        {
            currentTile = camera.GetGameTile();
            logger.Debug() << "New tile: " << currentTile << "\n";
            gameRunner.mGameState.Apply(BAK::State::ClearTileRecentEncounters);
        }
    };

    auto InputAllowed = [&]{
        return guiManager.InMainView()
            || (guiManager.InCombatView() && !gameRunner.IsAnimationActive());
    };

    Graphics::InputHandler inputHandler{};
    inputHandler.BindPressed(GLFW_KEY_G, [&]{
        if (glfwGetKey(window.get(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
            || glfwGetKey(window.get(), GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
        {
            if (guiManager.InMainView()) cameraPtr = &camera;
        }
        else
        {
            gameRunner.ToggleDisplayAllCells();
        }
    });
    inputHandler.Bind(GLFW_KEY_H,     [&]{ if (guiManager.InMainView()) cameraPtr = &lightCamera; });
    inputHandler.Bind(GLFW_KEY_R,     [&]{
        if (guiManager.InMainView())
            UpdateLightCamera();
    });
    inputHandler.Bind(GLFW_KEY_UP,   [&]{
        if (guiManager.InOverheadMap())
        {
            cameraPtr->MoveForward();
            UpdateGameTile();
        }
        else if (InputAllowed())
        {
            cameraPtr->StrafeForward();
            UpdateGameTile();
        }});
    inputHandler.Bind(GLFW_KEY_DOWN, [&]{
        if (guiManager.InOverheadMap())
        {
            cameraPtr->MoveBackward();
            UpdateGameTile();
        }
        else if (InputAllowed())
        {
            cameraPtr->StrafeBackward();
            UpdateGameTile();
        }});
    inputHandler.Bind(GLFW_KEY_LEFT, [&]{
        if (guiManager.InOverheadMap())
        {
            cameraPtr->RotateLeft();
            guiManager.mMainView.SetHeading(cameraPtr->GetHeading());
            UpdateGameTile();
        }
        else if (InputAllowed())
        {
            cameraPtr->StrafeLeft();
            UpdateGameTile();
        }});
    inputHandler.Bind(GLFW_KEY_RIGHT,[&]{
        if (guiManager.InOverheadMap())
        {
            cameraPtr->RotateRight();
            guiManager.mMainView.SetHeading(cameraPtr->GetHeading());
            UpdateGameTile();
        }
        else if (InputAllowed())
        {
            cameraPtr->StrafeRight();
            UpdateGameTile();
        }});

    inputHandler.Bind(GLFW_KEY_W, [&]{ if (InputAllowed() || guiManager.InOverheadMap()){cameraPtr->MoveForward(); UpdateGameTile();}});
    inputHandler.Bind(GLFW_KEY_A, [&]{
        if (guiManager.InOverheadMap())
        {
            cameraPtr->RotateLeft();
            guiManager.mMainView.SetHeading(cameraPtr->GetHeading());
            UpdateGameTile();
        }
        else if (InputAllowed())
        {
            cameraPtr->StrafeLeft();
            UpdateGameTile();
        }});
    inputHandler.Bind(GLFW_KEY_D, [&]{
        if (guiManager.InOverheadMap())
        {
            cameraPtr->RotateRight();
            guiManager.mMainView.SetHeading(cameraPtr->GetHeading());
            UpdateGameTile();
        }
        else if (InputAllowed())
        {
            cameraPtr->StrafeRight();
            UpdateGameTile();
        }});
    inputHandler.Bind(GLFW_KEY_S, [&]{ if (InputAllowed() || guiManager.InOverheadMap()){cameraPtr->MoveBackward(); UpdateGameTile();}});
    inputHandler.Bind(GLFW_KEY_Q, [&]{
        if (InputAllowed())
        {
            cameraPtr->RotateLeft();
            guiManager.mMainView.SetHeading(cameraPtr->GetHeading());
        }});
    inputHandler.Bind(GLFW_KEY_E, [&]{ 
        if (InputAllowed())
        {
            cameraPtr->RotateRight();
            guiManager.mMainView.SetHeading(cameraPtr->GetHeading());
        }});

    // MainView movement/compass buttons -> camera (ROADMAP 4.5). Mirror the
    // keyboard binds above; MainView triggers these via the IGuiManager methods.
    guiManager.mOnMoveForward  = [&]{ if (InputAllowed()){ cameraPtr->MoveForward();  UpdateGameTile(); } };
    guiManager.mOnMoveBackward = [&]{ if (InputAllowed()){ cameraPtr->MoveBackward(); UpdateGameTile(); } };
    guiManager.mOnRotateLeft   = [&]{ if (InputAllowed()){ cameraPtr->RotateLeft();   guiManager.mMainView.SetHeading(cameraPtr->GetHeading()); } };
    guiManager.mOnRotateRight  = [&]{ if (InputAllowed()){ cameraPtr->RotateRight();  guiManager.mMainView.SetHeading(cameraPtr->GetHeading()); } };

    inputHandler.Bind(GLFW_KEY_Z, [&]{ if (InputAllowed()){cameraPtr->StrafeUp();     UpdateGameTile();}});
    inputHandler.Bind(GLFW_KEY_V, [&]{ if (InputAllowed()){cameraPtr->StrafeDown();   UpdateGameTile();}});
    inputHandler.Bind(GLFW_KEY_X, [&]{ if (InputAllowed()) cameraPtr->RotateVerticalUp(); });
    inputHandler.Bind(GLFW_KEY_Y, [&]{ if (InputAllowed()) cameraPtr->RotateVerticalDown(); });
    inputHandler.BindPressed(GLFW_KEY_C, [&]{ if (guiManager.InMainView()) gameRunner.mGameState.Apply(BAK::State::ClearTileRecentEncounters); });
    inputHandler.BindPressed(GLFW_KEY_I, [&]{
        if (!imGuiInitialised)
        {
            ImguiWrapper::Initialise(window.get());
            imGuiInitialised = true;
        }
        showImgui = !showImgui;
    });

    inputHandler.Bind(GLFW_KEY_BACKSPACE,   [&]{ if (root.OnKeyEvent(Gui::KeyPress{GLFW_KEY_BACKSPACE})){ ;} });
    inputHandler.BindCharacter([&](char character){ if(root.OnKeyEvent(Gui::Character{character})){ ;} });
    // ROADMAP 4.7 Overhead Map — route zoom (+/-) and the F5 framebuffer-dump
    // diagnostic to the GUI. These aren't movement keys (not in the WASD set
    // above), so they'd otherwise never reach the screen stack. Forward them as
    // KeyPress events to root.OnKeyEvent only while the overhead map is open.
    inputHandler.BindPressed(GLFW_KEY_EQUAL, [&]{ if (guiManager.InOverheadMap()) (void) root.OnKeyEvent(Gui::KeyPress{GLFW_KEY_EQUAL}); });
    inputHandler.BindPressed(GLFW_KEY_MINUS, [&]{ if (guiManager.InOverheadMap()) (void) root.OnKeyEvent(Gui::KeyPress{GLFW_KEY_MINUS}); });

    Graphics::InputHandler::BindKeyboardToWindow(window.get(), inputHandler);
    Graphics::InputHandler::BindMouseToWindow(window.get(), inputHandler);

    inputHandler.BindMouse(
        GLFW_MOUSE_BUTTON_LEFT,
        [&](auto clickPos)
        {
            bool guiHandled = root.OnMouseEvent(
                Gui::LeftMousePress{guiScaleInv * clickPos});
            if (!guiHandled && InputAllowed())
            {
                const auto clickedId = renderer.GetClickedEntity(clickPos * pickScale);
                if (gameRunner.IsGridVisible())
                {
                    gameRunner.HandleGridCellClick(clickedId, false);
                }
                else
                {
                    gameRunner.CheckClickable(clickedId);
                }
            }
        },
        [&](auto clickPos)
        {
            root.OnMouseEvent(
                Gui::LeftMouseRelease{guiScaleInv * clickPos});
        }
    );

    inputHandler.BindMouse(
        GLFW_MOUSE_BUTTON_RIGHT,
        [&](auto click)
        {
            auto guiHandled = root.OnMouseEvent(
                Gui::RightMousePress{guiScaleInv * click});
            if (!guiHandled && InputAllowed())
            {
                const auto clickedId = renderer.GetClickedEntity(click * pickScale);
                if (gameRunner.IsGridVisible())
                {
                    gameRunner.HandleGridCellClick(clickedId, true);
                }
            }
        },
        [&](auto click)
        {
            root.OnMouseEvent(
                Gui::RightMouseRelease{guiScaleInv * click});
        }
    );

    inputHandler.BindMouseMotion(
        [&](auto pos)
        {
            root.OnMouseEvent(
                Gui::MouseMove{guiScaleInv * pos});
        }
    );

    inputHandler.BindMouseScroll(
        [&](auto pos)
        {
            root.OnMouseEvent(
                Gui::MouseScroll{guiScaleInv * pos});
        }
    );

    double currentTime = 0;
    double lastTime = 0;
    float deltaTime = 0;

    glfwSetCursorPos(window.get(), width/2, height/2);
    //glfwSetInputMode(window.get(), GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    //glfwSetInputMode(window.get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glEnable(GL_MULTISAMPLE);  

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    double pointerPosX, pointerPosY;

    bool consoleOpen = true;
    auto console = Console{};
    console.mCamera = &camera;
    console.mGameRunner = &gameRunner;
    console.mGuiManager = &guiManager;
    console.mGameState = &gameState;
    console.ToggleLog();

    // Do this last so we don't blast Imgui's callback hooks
    if (showImgui)
    {
        ImguiWrapper::Initialise(window.get());
        imGuiInitialised = true;
    }

    // Task 3.3 increment A: all boot TextureStores (Cursor, FontManager, Actors,
    // Backgrounds, Icons, Renderer depth/pick, CombatModelLoader sprites) are
    // now resident. Snapshot the boot VRAM budget before the render loop.
    Graphics::VramTracker::Get().LogTotal("boot");

    // ===== BAK_AGENT (removable automation harness) =====
    // Construct the harness only when enabled; otherwise it stays nullopt and
    // the loop's PollCommands/MaybeCapture no-op (zero behavior change). All
    // refs (window, inputHandler, gameRunner, guiManager) are live by here.
    std::optional<AgentHarness> agentHarness;
    if (config.mAgent.mEnabled)
    {
        agentHarness.emplace(
            window.get(),
            inputHandler,
            gameRunner,
            guiManager,
            config.mAgent);
    }
    // ===== END BAK_AGENT =====

    do
    {
        // macOS Retina: the framebuffer is HiDPI-scaled (e.g. 2x) relative to the
        // window size GLFW reports. The default-FB viewport must use the framebuffer
        // size, not the window size, or in-game content renders into the bottom-left
        // quarter. The camera + pick buffer keep window-size 1280x800 (aspect 1.6 ==
        // FB aspect 1.6), so only the viewport changes -- no projection math touched.
        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(window.get(), &fbW, &fbH);
        if (fbW <= 0 || fbH <= 0)
        {
            fbW = static_cast<int>(width);
            fbH = static_cast<int>(height);
        }

        // Task 4.4 fix: guiScaleInv maps the system cursor (GLFW content/point
        // coords) into the 320x200 logical GUI space. Deriving it from the
        // configured ResolutionScale (1/scale) is only correct when the window
        // content size == 320x*scale -- i.e. windowed mode. In fullscreen the
        // content size is the display's native point size (e.g. 1728x1117 on a
        // 16" MBP, not 3456x2160), so 1/scale makes the in-game cursor desync
        // from the system cursor -- linearly, worst at the bottom-right, and at
        // 4K bad enough that the cursor can't reach the right/bottom of the GUI
        // (can't get back to Settings). Recompute from the ACTUAL window content
        // size each frame so the mapping tracks the on-screen content scale in
        // windowed, fullscreen, any ResolutionScale, any Retina factor. (The mouse
        // callbacks capture guiScaleInv by reference, so this per-frame update
        // propagates; 3D picking uses raw clickPos and is untouched. In windowed
        // mode contentW == width == 320*scale, so this is a no-op there.)
        int contentW = 0, contentH = 0;
        glfwGetWindowSize(window.get(), &contentW, &contentH);
        if (contentW <= 0 || contentH <= 0)
        {
            contentW = static_cast<int>(width);
            contentH = static_cast<int>(height);
        }
        guiScaleInv = glm::vec2{
            nativeWidth  / static_cast<float>(contentW),
            nativeHeight / static_cast<float>(contentH)};

        // pickScale (3D picking) = pick-buffer dims / actual content size.
        // Mirrors guiScaleInv above; see the pickScale declaration for the full
        // rationale. No-op when the window wasn't clamped (contentW == width).
        const auto pickDims = renderer.GetScreenDims();
        pickScale = glm::vec2{
            static_cast<float>(pickDims.x) / static_cast<float>(contentW),
            static_cast<float>(pickDims.y) / static_cast<float>(contentH)};

        currentTime = glfwGetTime();

        deltaTime = float(currentTime - lastTime);
        guiManager.OnTimeDelta(currentTime - lastTime);
        gameRunner.OnTimeDelta(currentTime - lastTime);
        lastTime = currentTime;

        gameRunner.SetHoveredEntity(
            renderer.GetHoveredEntity().transform(
                [](auto v){ return BAK::EntityIndex{v}; }));

        cameraPtr->SetDeltaTime(deltaTime);
        if (guiManager.InMainView() || guiManager.InOverheadMap())
        {
            gameState.SetLocation(cameraPtr->GetGameLocation());
        }

        glfwPollEvents();
        glfwGetCursorPos(window.get(), &pointerPosX, &pointerPosY);
        inputHandler.HandleInput(window.get());

        // ===== BAK_AGENT (removable automation harness) =====
        // Inject held keys + poll the control file after real input, so injected
        // clicks/keys land in this frame's processing (the per-frame guiScaleInv
        // and pickScale above are already current for the bound lambdas).
        if (agentHarness) agentHarness->PollCommands();
        // ===== END BAK_AGENT =====

        if (gameState.GetGameData().IsLoaded())
        {
            // { *** Draw 3D World ***
            UpdateLightCamera();

            glEnable(GL_BLEND);
            glEnable(GL_MULTISAMPLE);  

            double bakTimeOfDay = (gameState.GetWorldTime().GetTime().mTime % 43200);
            auto twoPi = std::numbers::pi_v<double> * 2.0;
            // light starts at 6 after midnight
            auto sixHours = 7200.0;
            auto beginDay = bakTimeOfDay - sixHours;
            bool isNight = bakTimeOfDay < 7200 || bakTimeOfDay > 36000;
            light.mDirection = glm::vec3{
                std::cos(beginDay * (twoPi / (28800 * 2))),
                isNight ? .1 : -.25,
                0};
            float ambient = isNight
                ? .05
                : std::sin(beginDay * (twoPi / 57600));
            light.mAmbientColor = glm::vec3{ambient};
            light.mDiffuseColor = ambient * glm::vec3{
                1.,
                std::sin(beginDay * (twoPi / (57600 * 2))),
                std::sin(beginDay * (twoPi / (57600 * 2)))
            };

            light.mSpecularColor = isNight ? glm::vec3{0} : ambient * glm::vec3{
                1.,
                std::sin(beginDay * (twoPi / (57600 * 2))),
                std::sin(beginDay * (twoPi / (57600 * 2)))
            };
            light.mFogColor = ambient * glm::vec3{.15, .31, .36};

            if (guiManager.InOverheadMap())
            {
                // ROADMAP 4.7 Overhead Map: no shadows on the map (cleared
                // shadow map → depth 1 → no shadow contribution). Bind+clear
                // the shadow FB then unbind. The top-down 3D pass itself runs
                // AFTER the GUI pass so it can overwrite the black map area.
                renderer.BeginDepthMapDraw();
                renderer.EndDepthMapDraw();

                glViewport(0, 0, fbW, fbH);
                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            }
            else
            {
                renderer.BeginDepthMapDraw();
                renderer.DrawDepthMap(
                    gameRunner.GetZoneRenderData(),
                    gameRunner.mSystems->GetRenderables(),
                    lightCamera);
                renderer.DrawDepthMap(
                    gameRunner.GetZoneRenderData(),
                    gameRunner.mSystems->GetSprites(),
                    lightCamera);
                renderer.EndDepthMapDraw();

                glViewport(0, 0, fbW, fbH);
                // Dark blue background
                glClearColor(ambient * 0.15f, ambient * 0.31f, ambient * 0.36f, 0.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                renderer.DrawWithShadow(
                    gameRunner.GetZoneRenderData(),
                    gameRunner.mSystems->GetRenderables(),
                    light,
                    lightCamera,
                    *cameraPtr,
                    false);

                renderer.DrawWithShadow(
                    gameRunner.GetZoneRenderData(),
                    gameRunner.mSystems->GetSprites(),
                    light,
                    lightCamera,
                    *cameraPtr,
                    true);

                const auto& dynamicRenderables = gameRunner.mSystems->GetDynamicRenderables();
                for (const auto& obj : dynamicRenderables)
                {
                    std::vector<DynamicRenderable> data{};
                    data.emplace_back(obj);
                    renderer.DrawWithShadow(
                        *obj.GetRenderData(),
                        data,
                        light,
                        lightCamera,
                        *cameraPtr,
                        true);
                }

                renderer.DrawText3D(
                    gameRunner.mGlyphStore.GetRenderData(),
                    gameRunner.mSystems->GetTextRenderables(),
                    *cameraPtr);
            }
        }

        if (gameState.GetGameData().IsLoaded() && !guiManager.InOverheadMap())
        {
            glDisable(GL_BLEND);
            glDisable(GL_MULTISAMPLE);

            if (gameRunner.mCombatManager.IsCombatActive())
            {
                auto emptyRenderables = std::vector<Renderable>{};
                auto emptyDynamicRenderables = std::vector<DynamicRenderable>{};
                renderer.DrawForPicking(
                    gameRunner.GetZoneRenderData(),
                    gameRunner.mGridCellRenderables,
                    emptyRenderables,
                    emptyDynamicRenderables,
                    *cameraPtr);
            }
            else
            {
                renderer.DrawForPicking(
                    gameRunner.GetZoneRenderData(),
                    gameRunner.mSystems->GetRenderables(),
                    gameRunner.mSystems->GetSprites(),
                    gameRunner.mSystems->GetDynamicRenderables(),
                    *cameraPtr);
            }
            renderer.StartPickReadback(glm::vec2{pointerPosX, pointerPosY} * pickScale);

            glEnable(GL_BLEND);
            glEnable(GL_MULTISAMPLE);
        }

        //// { *** Draw 2D GUI ***
        // DrawForPicking set the viewport to the 1280x800 pick buffer; restore the
        // default-FB viewport before the GUI draw so it fills the window. Sync the
        // GUI scissor to the HiDPI framebuffer so ClipRegion widgets (compass, scroll
        // lists) clip at the right pixels.
        glViewport(0, 0, fbW, fbH);
        guiRenderer.SetFramebufferSize(fbW, fbH);
        guiRenderer.RenderGui(&root);

        // ROADMAP 4.7 Overhead Map — top-down 3D pass. Drawn AFTER the GUI pass
        // (FRAME.SCX chrome + portraits + icon buttons) so the 3D map image
        // overwrites the view-area cut-out, then the party marker is re-rendered
        // on top (scissored to the view-area rect).
        if (gameState.GetGameData().IsLoaded() && guiManager.InOverheadMap())
        {
            // Player's EXACT game position → top-down camera target (player-
            // centred, north-up). Inc 1 used the tile centre because the map was
            // display-only and the party was static; with movement the tile-centre
            // quantization swallowed sub-tile steps (the map wouldn't scroll until
            // a whole tile boundary was crossed — "Up/Down do nothing"). Using the
            // exact position makes every frame's movement scroll the map smoothly.
            const auto playerPos = gameState.GetLocation().mPosition;

            // Ortho box in normalised GL units (gTileSize/gWorldScale = 640/tile).
            // Vertical span = zoom tiles; horizontal keeps the view-area aspect
            // (294:100, the main screen's 3D-view cut-out) so the projected scene
            // isn't distorted within the map rectangle.
            constexpr float kUnitsPerTile = BAK::gTileSize / BAK::gWorldScale;
            const float tilesVert = guiManager.mOverheadMap.GetZoom();
            const float halfH = (tilesVert * 0.5f) * kUnitsPerTile;
            const float halfW = halfH * (Gui::OverheadMap::kViewW / Gui::OverheadMap::kViewH);
            mapCamera.UseOrthoMatrix(-halfW, halfW, -halfH, halfH, 1.0f, kMapFar);

            // Camera above the player's exact position, looking straight down
            // (pitch -pi/2 per Camera::GetDirection). Position in BAK units; the
            // view matrix divides by gWorldScale. BAK +y north → GL -z, so negate.
            mapCamera.SetPosition(glm::vec3{
                static_cast<float>(playerPos.x),
                kMapCamHeightBAK,
                -static_cast<float>(playerPos.y)});
            mapCamera.SetAngle(glm::vec2{
                kMapNorthUpYaw,
                -std::numbers::pi_v<float> / 2.0f});

            // No fog on the map (fog is radial world-distance → would wash the
            // whole map out from this far above).
            Graphics::Light mapLight = light;
            mapLight.mFogStrength = 0.0f;

            // Map sub-viewport = the main screen's 3D-view cut-out (x13 y12
            // w294 h100 in 320×200 logical), scaled to framebuffer pixels and
            // y-flipped to GL bottom-origin (top edge GUI y=12 → GL y=fbH-12·sy).
            const float sx = static_cast<float>(fbW) / 320.0f;
            const float sy = static_cast<float>(fbH) / 200.0f;
            const int viewX = static_cast<int>(Gui::OverheadMap::kViewX * sx);
            const int viewW = static_cast<int>(Gui::OverheadMap::kViewW * sx);
            const int viewYBot = fbH - static_cast<int>(
                (Gui::OverheadMap::kViewY + Gui::OverheadMap::kViewH) * sy);
            const int viewH = static_cast<int>(Gui::OverheadMap::kViewH * sy);
            glViewport(viewX, viewYBot, viewW, viewH);
            glEnable(GL_SCISSOR_TEST);
            glScissor(viewX, viewYBot, viewW, viewH);
            // The GUI pass disables depth test; re-enable for the 3D draw.
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glClear(GL_DEPTH_BUFFER_BIT);

            // Widen the draw distance so the high camera doesn't cull the map,
            // then restore the configured value for the next first-person frame.
            renderer.SetDrawDistance(kMapDrawDistance);
            renderer.DrawWithShadow(
                gameRunner.GetZoneRenderData(),
                gameRunner.mSystems->GetRenderables(),
                mapLight,
                lightCamera,
                mapCamera,
                false);
            if (kMapRenderSprites)
            {
                renderer.DrawWithShadow(
                    gameRunner.GetZoneRenderData(),
                    gameRunner.mSystems->GetSprites(),
                    mapLight,
                    lightCamera,
                    mapCamera,
                    true);
            }
            renderer.SetDrawDistance(config.mGraphics.mDrawDistance);

            // Party marker: a red isosceles triangle (7px base / 4px legs at
            // 320-wide, the original BaK marker) scaled to the current map
            // resolution, tip pointing in the player's facing direction.
            //
            // Built in map-rect FB-pixel space so x/y scale equally (the
            // triangle stays rigid when rotated — no aspect skew), then each
            // axis is converted to clip separately. The facing direction is
            // derived from the first-person camera's forward vector
            // transformed by the top-down mapCamera view matrix — correct by
            // construction and auto-matches the north-up framing (fixes the
            // E/W swap a hand-rolled heading formula produced).
            const float mapScale = static_cast<float>(fbW) / 320.0f;
            const float basePx = 7.0f * mapScale;
            const float legPx  = 4.0f * mapScale;
            const float hPx    = std::sqrt(legPx * legPx - (basePx * 0.5f) * (basePx * 0.5f));

            const glm::vec3 fpDir = camera.GetDirection();
            const float horizLen = std::sqrt(fpDir.x * fpDir.x + fpDir.z * fpDir.z);
            const glm::vec3 fwd = (horizLen > 1e-6f)
                ? glm::vec3{fpDir.x / horizLen, 0.0f, fpDir.z / horizLen}
                : glm::vec3{0.0f, 0.0f, -1.0f};
            const glm::vec4 tipEye = mapCamera.GetViewMatrix() * glm::vec4{fwd, 0.0f};
            const glm::vec2 tipRaw{tipEye.x, tipEye.y};
            const float tlen = std::sqrt(tipRaw.x * tipRaw.x + tipRaw.y * tipRaw.y);
            const glm::vec2 tipDir = (tlen > 1e-6f) ? tipRaw / tlen : glm::vec2{0.0f, 1.0f};
            const glm::vec2 perp{tipDir.y, -tipDir.x}; // 90° clockwise (screen-right)

            // Vertices in FB-pixel space (centroid at origin, y-up), rotated so
            // the tip points along tipDir; then each axis → clip (isotropic).
            const glm::vec2 vTip = tipDir * (hPx * 2.0f / 3.0f);
            const glm::vec2 vBL  = -tipDir * (hPx / 3.0f) + perp * (basePx * 0.5f);
            const glm::vec2 vBR  = -tipDir * (hPx / 3.0f) - perp * (basePx * 0.5f);
            const float cx = 2.0f / static_cast<float>(viewW);
            const float cy = 2.0f / static_cast<float>(viewH);
            const float verts[18] = {
                vTip.x * cx, vTip.y * cy, 0.0f, 0.0f, 0.0f, 0.0f,
                vBL.x  * cx, vBL.y  * cy, 0.0f, 0.0f, 0.0f, 0.0f,
                vBR.x  * cx, vBR.y  * cy, 0.0f, 0.0f, 0.0f, 0.0f,
            };
            glBindVertexArray(mapMarkerVao);
            glBindBuffer(GL_ARRAY_BUFFER, mapMarkerVbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

            guiRenderer.mShader.UseProgramGL();
            const glm::mat4 ident{1.0f};
            guiRenderer.mShader.SetUniform(guiRenderer.mCamera.mMvpMatrixId, ident);
            guiRenderer.mShader.SetUniform(guiRenderer.mCamera.mModelMatrixId, ident);
            guiRenderer.mShader.SetUniform(guiRenderer.mCamera.mViewMatrixId, ident);
            guiRenderer.mShader.SetUniform(guiRenderer.mBlockColorId, glm::vec4{0.85f, 0.10f, 0.10f, 1.0f});
            guiRenderer.mShader.SetUniform(guiRenderer.mColorModeId, 1); // SolidColor

            glDisable(GL_DEPTH_TEST);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glEnable(GL_DEPTH_TEST);
            glBindVertexArray(0);

            // Restore the default-FB viewport + drop scissor for ImGui.
            glViewport(0, 0, fbW, fbH);
            glDisable(GL_SCISSOR_TEST);
        }

        // { *** IMGUI START ***
        if (showImgui)
        {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            ShowLightGui(light);

            ShowCameraGui(camera);
            console.Draw("Console", &consoleOpen);
        }

        if ((guiManager.InMainView() || guiManager.InOverheadMap()) && !guiManager.GetCombatSequenceActive())
        {
            gameRunner.RunGameUpdate(config.mGame.mAdvanceTime);
            if (config.mAudio.mEnableBackgroundSounds)
            {
                BAK::PlayBackgroundSounds(gameRunner.mGameState);
            }
        }

        if (showImgui && gameRunner.mActiveEncounter)
        {
            ImGui::Begin("Encounter");
            std::stringstream ss{};
            ss << "Encounter: " << *gameRunner.mActiveEncounter << std::endl;
            ImGui::TextWrapped(ss.str().c_str());
            ImGui::End();
            
            const auto& encounter = gameRunner.mActiveEncounter->GetEncounter();
            std::visit(
                overloaded{
                    [&](const BAK::Encounter::GDSEntry& gds){
                        ShowDialogGui(
                            gds.mEntryDialog,
                            BAK::DialogStore::Get());
                    },
                    [&](const BAK::Encounter::Block& e){
                        ShowDialogGui(
                            e.mDialog,
                            BAK::DialogStore::Get());
                    },
                    [&](const BAK::Encounter::Combat& e){
                        ShowDialogGui(
                            e.mEntryDialog,
                            BAK::DialogStore::Get());
                    },
                    [&](const BAK::Encounter::Dialog& e){
                        ShowDialogGui(
                            e.mDialog,
                            BAK::DialogStore::Get());
                    },
                    [](const BAK::Encounter::EventFlag&){
                    },
                    [&](const BAK::Encounter::Zone& e){
                        ShowDialogGui(
                            e.mDialog,
                            BAK::DialogStore::Get());
                    },
                },
                encounter);
        }

        if (showImgui)
        {
            ImguiWrapper::Draw(window.get());
        }

        if (showImgui)
        {
            auto& io = ImGui::GetIO();
            if (io.WantCaptureKeyboard || io.WantCaptureMouse)
            {
                inputHandler.SetHandleInput(false);
            }
            else
            {
                inputHandler.SetHandleInput(true);
            }
        }
        else
        {
            inputHandler.SetHandleInput(true);
        }

        // *** IMGUI END *** }
     
        // ===== BAK_AGENT (removable automation harness) =====
        // Capture a clean game-only frame.png + state.json if a `dump` was
        // requested, before the swap (so the dump reflects this frame's render).
        if (agentHarness) agentHarness->MaybeCapture(fbW, fbH);
        // ===== END BAK_AGENT =====

        glfwSwapBuffers(window.get());
    }
    while (glfwGetKey(window.get(), GLFW_KEY_ESCAPE) != GLFW_PRESS
        && glfwWindowShouldClose(window.get()) == 0);

    if (showImgui)
    {
        ImguiWrapper::Shutdown();
    }

    if (logFileStream && logFileStream->is_open())
    {
        logFileStream->close();
    }

    return 0;
}
