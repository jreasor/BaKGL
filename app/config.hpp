#pragma once

#include <string>
#include <vector>

namespace Config {

struct Paths
{
    std::string mShaders{};
    std::string mSaves{};
    std::string mGameData{};
    std::string mGraphicsOverrides{};
    std::string mAssets4k{};
    std::string mDialogMods{};
    std::string mLuaMods{};
};

struct Graphics
{
    float mResolutionScale{4.0};
    bool mShadows{true};
    bool mEnableImGui{true};
    bool mDebugDisableFades{false};
    bool mDebugRenderEncounters{false};
    int mDrawDistance{128000};
    unsigned mMaxTextureDim{2048};
    // Task 3.3-B: configurable ceiling on the number of layers a single
    // TextureStore may upload into one GL_TEXTURE_2D_ARRAY (was the hardcoded
    // TextureBuffer::sMaxTextures = 256). 0/unset => 256 (built-in default).
    // Raising it is VRAM-free for stores that don't approach it (allocation is
    // exact-per-store); raise only if a logical sheet legitimately needs >256
    // frames.
    unsigned mMaxTextures{256};
    // Task 3.3-C: opt-in RGBA8 (GL_UNSIGNED_BYTE) staging for glTexSubImage3D.
    // Default false = legacy GL_FLOAT staging (the driver converts floats to
    // RGBA8 during upload). The RGBA8 path builds the bytes on the CPU
    // (BuildRgba8Staging); in the unoptimized Debug build its per-channel
    // scalar quantize is ~2.5x slower than the driver's conversion on the fill
    // (the dominant zone-hitch cost), so it is gated off until a vectorized
    // build or Task 3.3-D's async PBO path makes it worthwhile.
    bool mRGBA8Upload{false};
    // Task 3.3-D: opt-in async PBO texture upload. Default false = the C path
    // (synchronous RGBA8 staging -> glTexSubImage3D with a client pointer).
    // When true, LoadTexturesGL stages the RGBA8 bytes in a PixelUnpackBuffer
    // and issues glTexSubImage3D(data=nullptr) so the driver enqueues a GPU-side
    // copy instead of blocking the render thread. Only the `upload` segment of
    // A's timer split is PBO-acceleratable (fill is CPU, mipmap is glGenerate-
    // Mipmap), and a single PBO still syncs at glGenerateMipmap -- so this is
    // an async foundation + decoupled staging for a future off-thread fill /
    // double-buffered path, not a zone-hitch fix. Falls back to C when off.
    bool mAsyncTextureUpload{false};
    // Task 3.2: base names (no extension) of fullscreen SCX backgrounds that bypass
    // the MaxTextureDim cap and get a dedicated one-layer sheet at the substitute's
    // full uncapped resolution. Empty = today's behavior (all substitutes capped).
    std::vector<std::string> mHeroTextures{};
};

struct Logging
{
    bool mLogToFile{true};
    bool mLogTime{true};
    bool mLogColours{false};
    std::string mLogFilePath{};
    std::string mLogLevel{"DEBUG"};
    std::vector<std::string> mDisabledLoggers{};
    std::vector<std::string> mEnabledLoggers{};
};

struct Audio
{
    bool mEnableAudio{true};
    bool mEnableBackgroundSounds{true};
    std::string mMidiPlayer{"ADLMIDI"};
};

struct Game
{
    bool mAdvanceTime{true};
    double mCombatSpeed{1.0};
};

struct Config
{
    Paths mPaths{};
    Graphics mGraphics{};
    Logging mLogging{};
    Audio mAudio{};
    Game mGame{};
};

Config LoadConfig(std::string path);

}
