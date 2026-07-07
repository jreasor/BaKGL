#include "graphics/vramTracker.hpp"

#include "com/logger.hpp"

#include <algorithm>
#include <cmath>

namespace Graphics {

VramTracker& VramTracker::Get()
{
    static VramTracker tracker{};
    return tracker;
}

VramTracker::VramTracker()
:
    mTotalBytes{0},
    mTotalCalls{0},
    mTotalLayers{0}
{}

unsigned long long ResidentBytes(unsigned maxDim, unsigned layerCount, bool mipmapped)
{
    if (maxDim == 0 || layerCount == 0) return 0;

    // glTexStorage3D allocates `levels` mip levels up front (immutable storage).
    // Level L is max(maxDim >> L, 1) on a side. Sum the per-level texel count for
    // one layer, then scale by layerCount and 4 bytes/texel (RGBA8). This matches
    // the engine's own level-count computation in LoadTexturesGL
    // (for (auto d = maxDim; d > 1; d >>= 1) ++levels).
    unsigned levels = 1;
    if (mipmapped)
        for (auto d = maxDim; d > 1; d >>= 1) ++levels;

    unsigned long long texelsPerLayer = 0;
    for (unsigned L = 0; L < levels; ++L)
    {
        const auto d = std::max(1u, maxDim >> L);
        texelsPerLayer += static_cast<unsigned long long>(d) * d;
    }

    constexpr unsigned long long kBytesPerTexel = 4; // GL_RGBA8
    return texelsPerLayer * layerCount * kBytesPerTexel;
}

void VramTracker::Account(
    unsigned maxDim,
    unsigned layerCount,
    bool mipmapped,
    const std::string& label)
{
    const auto bytes = ResidentBytes(maxDim, layerCount, mipmapped);
    mTotalBytes += bytes;
    mTotalCalls += 1;
    mTotalLayers += layerCount;
    Logging::LogDebug("VRAM") << label << ": array maxDim=" << maxDim
        << " layers=" << layerCount << " mip=" << (mipmapped ? 1 : 0)
        << " -> " << bytes << " B"
        << " (running " << mTotalBytes << " B, " << mTotalCalls << " calls)\n";
}

void VramTracker::AccountViewport(
    unsigned width,
    unsigned height,
    unsigned bytesPerPixel,
    const std::string& label)
{
    const auto bytes = static_cast<unsigned long long>(width) * height * bytesPerPixel;
    mTotalBytes += bytes;
    mTotalCalls += 1;
    Logging::LogDebug("VRAM") << label << ": viewport " << width << "x" << height
        << " @ " << bytesPerPixel << " B/texel -> " << bytes
        << " B (running " << mTotalBytes << " B)\n";
}

void VramTracker::LogTotal(const std::string& label) const
{
    constexpr unsigned long long kMiB = 1024 * 1024;
    Logging::LogInfo("VRAM") << "Resident texture VRAM [" << label << "]: "
        << mTotalBytes << " B (" << (mTotalBytes / kMiB) << " MiB) over "
        << mTotalCalls << " uploads, " << mTotalLayers << " layers\n";
}

}