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

GraphicsConfig::GraphicsConfig()
:   mMaxTextureDim{2048}
{}

}