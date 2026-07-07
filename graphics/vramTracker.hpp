#pragma once

#include <string>

namespace Graphics {

// Process-wide texture VRAM accounting (ROADMAP Task 3.3 increment A). GL-free
// counters + the resident-bytes math, so it is unit-testable without a GL
// context. Mirrors the GraphicsConfig singleton pattern but holds runtime
// telemetry (mutated on every TextureBuffer::LoadTexturesGL + Make*Buffer),
// not set-once-at-boot settings. All game-texture VRAM flows through that one
// upload chokepoint, so the running totals are complete for the session.
class VramTracker
{
public:
    static VramTracker& Get();

    // Account a GL_TEXTURE_2D_ARRAY upload (RGBA8). Resident bytes = the exact
    // mipchain sum (sum of max(maxDim>>L,1)^2 for L in [0,levels)) * layerCount
    // * 4 bytes/texel. For Nearest (mipmapped=false) levels=1 so this collapses
    // to maxDim^2 * layerCount * 4. The exact sum equals base*4/3 (floored) for
    // power-of-two maxDim and is a slight upper bound otherwise — which matches
    // the real glTexStorage3D immutable allocation. label tags the per-call
    // LogDebug line (e.g. "upload", "depth", "pick").
    void Account(
        unsigned maxDim,
        unsigned layerCount,
        bool mipmapped,
        const std::string& label);

    // Account a viewport-sized 2D texture (depth/pick buffers), which are not
    // array textures. bytesPerPixel is a documented estimate (depth ~4 B/texel;
    // pick GL_RGBA32F = 16 B/texel). Lightweight completeness for the budget.
    void AccountViewport(
        unsigned width,
        unsigned height,
        unsigned bytesPerPixel,
        const std::string& label);

    // Log the running totals at LogInfo under the "VRAM" logger, prefixed with
    // label (e.g. "boot", "zone 4"). Per-call detail is at LogDebug under "VRAM".
    void LogTotal(const std::string& label) const;

    // --- test accessors (deltas; the singleton accumulates across the process) ---
    unsigned long long GetTotalBytes() const { return mTotalBytes; }
    unsigned GetTotalCalls() const { return mTotalCalls; }
    unsigned long long GetTotalLayers() const { return mTotalLayers; }

private:
    VramTracker();

    unsigned long long mTotalBytes;
    unsigned mTotalCalls;
    unsigned long long mTotalLayers;
};

// Exact resident bytes for one RGBA8 GL_TEXTURE_2D_ARRAY allocation. Exposed as
// a free function so the unit test can assert the mipchain math without touching
// the process-wide singleton. layerCount>=1; maxDim==0 => 0.
unsigned long long ResidentBytes(unsigned maxDim, unsigned layerCount, bool mipmapped);

}