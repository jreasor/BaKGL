#pragma once

#include "graphics/sprites.hpp"

#include <glm/glm.hpp>

namespace Gui {

class Backgrounds
{
public:

    // A screen's render identity is (spriteSheet, textureIndex). Non-hero screens live in
    // the shared sheet (mSpriteSheet); hero screens (GraphicsConfig::IsHero, Task 3.2) live
    // in their own dedicated one-layer sheet so the shared sheet's maxDim never inflates.
    // Consumers must use both fields together — binding the shared sheet with a hero's
    // texture index (or vice versa) renders the wrong layer.
    struct ScreenHandle
    {
        Graphics::SpriteSheetIndex mSpriteSheet;
        Graphics::TextureIndex mTexture;
    };

    Backgrounds(
        Graphics::SpriteManager& spriteManager);

    ScreenHandle GetScreen(const std::string& scx) const;

private:
    Graphics::SpriteSheetIndex mSpriteSheet;
    std::unordered_map<std::string, ScreenHandle> mScxToScreen;

    const Logging::Logger& mLogger;
};

}