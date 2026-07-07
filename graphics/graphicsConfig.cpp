#include "graphics/graphicsConfig.hpp"

#include "com/logger.hpp"

namespace Graphics {

GraphicsConfig& GraphicsConfig::Get()
{
    static GraphicsConfig config{};
    return config;
}

unsigned GraphicsConfig::GetMaxTextureDim() const
{
    return mMaxTextureDim;
}

void GraphicsConfig::SetMaxTextureDim(unsigned dim)
{
    // 0 => "use the built-in default"; clamp tiny values to a sane floor so the
    // downscale path can't produce degenerate textures.
    constexpr unsigned kDefault = 2048;
    constexpr unsigned kFloor = 64;
    auto effective = dim;
    if (effective == 0)
        effective = kDefault;
    if (effective < kFloor)
        effective = kFloor;
    Logging::LogInfo(__FUNCTION__) << "MaxTextureDim = " << effective << "\n";
    mMaxTextureDim = effective;
}

unsigned GraphicsConfig::GetMaxTextures() const
{
    return mMaxTextures;
}

void GraphicsConfig::SetMaxTextures(unsigned max)
{
    // 0 => "use the built-in default"; clamp to a sane floor so a misconfigured
    // cap can't make every non-empty store throw at boot (a 1-layer store is the
    // minimum the GL path supports).
    constexpr unsigned kDefault = 256;
    constexpr unsigned kFloor = 1;
    auto effective = max;
    if (effective == 0)
        effective = kDefault;
    if (effective < kFloor)
        effective = kFloor;
    Logging::LogInfo(__FUNCTION__) << "MaxTextures = " << effective << "\n";
    mMaxTextures = effective;
}

bool GraphicsConfig::GetRGBA8Upload() const
{
    return mRGBA8Upload;
}

void GraphicsConfig::SetRGBA8Upload(bool enabled)
{
    Logging::LogInfo(__FUNCTION__) << "RGBA8Upload = "
        << (enabled ? "true" : "false") << "\n";
    mRGBA8Upload = enabled;
}

bool GraphicsConfig::GetAsyncTextureUpload() const
{
    return mAsyncTextureUpload;
}

void GraphicsConfig::SetAsyncTextureUpload(bool enabled)
{
    Logging::LogInfo(__FUNCTION__) << "AsyncTextureUpload = "
        << (enabled ? "true" : "false") << "\n";
    mAsyncTextureUpload = enabled;
}

bool GraphicsConfig::IsHero(const std::string& baseName) const
{
    return mHeroTextures.contains(baseName);
}

void GraphicsConfig::SetHeroTextures(const std::vector<std::string>& heroes)
{
    mHeroTextures.clear();
    for (const auto& h : heroes) mHeroTextures.insert(h);
    Logging::LogInfo(__FUNCTION__) << "HeroTextures = " << mHeroTextures.size()
        << " entries\n";
}

GraphicsConfig::GraphicsConfig()
:   mMaxTextureDim{2048},
    mMaxTextures{256},
    mRGBA8Upload{false},
    mAsyncTextureUpload{false}
{}

}