#include "gui/backgrounds.hpp"

#include "bak/textureFactory.hpp"

#include "com/logger.hpp"

#include "graphics/graphicsConfig.hpp"
#include "graphics/texture.hpp"
#include "graphics/sprites.hpp"

#include "gui/colors.hpp"
#include "gui/screenCutout.hpp"

#include <glm/glm.hpp>

namespace Gui {

Backgrounds::Backgrounds(
    Graphics::SpriteManager& spriteManager)
:
    mSpriteSheet{spriteManager.AddSpriteSheet()},
    mScxToScreen{},
    mLogger{Logging::LogState::GetLogger("Gui::Backgrounds")}
{
    // Shared-store layer index. CRITICAL: hero screens never touch this — they go to
    // their own dedicated one-layer sheets. If a hero advanced sharedIdx without adding
    // to `textures`, every subsequent shared ScreenHandle.mTexture would be off-by-one
    // and point at the wrong layer.
    unsigned sharedIdx = 0;
    // Shared sheet. maxDim stays <= MaxTextureDim because no uncapped (hero) substitute
    // ever enters it — heroes are routed to dedicated sheets before AddScreenToTextureStore.
    auto textures = Graphics::TextureStore{};

    // Non-hero screen: appends one layer to the shared store, records (sharedSheet, layer).
    const auto AddSharedScreen = [this, &sharedIdx, &textures](
        const std::string& scx,
        const std::string& pal)
    {
        mScxToScreen.emplace(
            std::make_pair(scx, ScreenHandle{mSpriteSheet, Graphics::TextureIndex{sharedIdx++}}));
        BAK::TextureFactory::AddScreenToTextureStore(
            textures, scx, pal);
    };

    // Hero screen: its own dedicated one-layer sheet, substitute loaded UNCAPPED so a
    // true 4K background survives at full res without inflating the shared sheet. The
    // dedicated sheet is lifetime-of-SpriteManager (Backgrounds is never rebuilt today).
    const auto AddHeroScreen = [this, &spriteManager](
        const std::string& scx,
        const std::string& pal)
    {
        auto heroSheet = spriteManager.AddSpriteSheet();
        auto fresh = Graphics::TextureStore{};
        BAK::TextureFactory::AddScreenToTextureStore(
            fresh, scx, pal, /*allowUncapped=*/true);
        spriteManager.GetSpriteSheet(heroSheet).LoadTexturesGL(
            fresh, Graphics::FilterMode::LinearMipmap);
        mScxToScreen.emplace(
            std::make_pair(scx, ScreenHandle{heroSheet, Graphics::TextureIndex{0}}));
        mLogger.Debug() << "Screen " << scx << " -> dedicated sheet "
            << heroSheet << " (hero)\n";
    };

    for (const auto& [scx, pal] : {
        std::make_pair("ENCAMP.SCX", "OPTIONS.PAL"),
        std::make_pair("FRAME.SCX", "OPTIONS.PAL"),
        std::make_pair("CONTENTS.SCX", "CONTENTS.PAL"),
        std::make_pair("CONT2.SCX", "CONTENTS.PAL"),
        std::make_pair("FULLMAP.SCX", "FULLMAP.PAL"),
        std::make_pair("RIFTMAP.SCX", "OPTIONS.PAL"),
        std::make_pair("INT_BORD.SCX", "FULLMAP.PAL"),
        std::make_pair("DIALOG.SCX", "INVENTOR.PAL"),
        std::make_pair("INVENTOR.SCX", "INVENTOR.PAL"),
        std::make_pair("OPTIONS0.SCX", "OPTIONS.PAL"),
        std::make_pair("OPTIONS1.SCX", "OPTIONS.PAL"),
        std::make_pair("OPTIONS2.SCX", "OPTIONS.PAL"),
        std::make_pair("PUZZLE.SCX", "PUZZLE.PAL"),
        std::make_pair("C42.SCX", "TELEPORT.PAL"),
        std::make_pair("CAST.SCX", "OPTIONS.PAL"),
        std::make_pair("CFRAME.SCX", "INVENTOR.PAL"),
        std::make_pair("BOOK.SCX", "BOOK.PAL")
    })
    {
        const auto scxStr = std::string{scx};
        const auto base = scxStr.substr(0, scxStr.rfind('.'));
        if (Graphics::GraphicsConfig::Get().IsHero(base))
            AddHeroScreen(scxStr, pal);
        else
            AddSharedScreen(scxStr, pal);
    }

    // Add this screen and cut out the center so we can use
    // it in the main view without any scissoring. NOTE: the loaded file is DIALOG.SCX
    // but the map key is the synthetic DIALOG_BG_MAIN.SCX. The hero check is therefore
    // IsHero("DIALOG") — IsHero("DIALOG_BG_MAIN") would always be false and silently
    // leave the cut-out in the shared sheet (the exact OOM this isolation prevents).
    {
        if (Graphics::GraphicsConfig::Get().IsHero("DIALOG"))
        {
            auto heroSheet = spriteManager.AddSpriteSheet();
            auto fresh = Graphics::TextureStore{};
            BAK::TextureFactory::AddScreenToTextureStore(
                fresh, "DIALOG.SCX", "OPTIONS.PAL", /*allowUncapped=*/true);
            Gui::PrepareMainViewBackground(fresh.GetTexture(0));
            spriteManager.GetSpriteSheet(heroSheet).LoadTexturesGL(
                fresh, Graphics::FilterMode::LinearMipmap);
            mScxToScreen.emplace(
                std::make_pair("DIALOG_BG_MAIN.SCX",
                    ScreenHandle{heroSheet, Graphics::TextureIndex{0}}));
            mLogger.Debug() << "Screen DIALOG_BG_MAIN.SCX -> dedicated sheet "
                << heroSheet << " (hero, cut-out)\n";
        }
        else
        {
            mScxToScreen.emplace(
                std::make_pair("DIALOG_BG_MAIN.SCX",
                    ScreenHandle{mSpriteSheet, Graphics::TextureIndex{sharedIdx++}}));
            BAK::TextureFactory::AddScreenToTextureStore(
                textures, "DIALOG.SCX", "OPTIONS.PAL");
            auto& tex = textures.GetTexture(sharedIdx - 1);
            Gui::PrepareMainViewBackground(tex);
        }
    }

    // TELEPORT.BMX is a BMX (multi-image) background, not an SCX screen — IsHero is
    // SCX-only by design (Increment A), so it always stays in the shared (capped) sheet.
    const auto AddBackground = [this, &sharedIdx, &textures](
        const std::string& bmx,
        const std::string& pal)
    {
        mScxToScreen.emplace(
            std::make_pair(bmx, ScreenHandle{mSpriteSheet, Graphics::TextureIndex{sharedIdx++}}));
        BAK::TextureFactory::AddToTextureStore(
            textures, bmx, pal);
    };

    for (const auto& [bmx, pal] : {
        std::make_pair("TELEPORT.BMX", "TELEPORT.PAL"),
    })
    {
        AddBackground(bmx, pal);
    }

    spriteManager
        .GetSpriteSheet(mSpriteSheet)
        .LoadTexturesGL(textures, Graphics::FilterMode::LinearMipmap);
}


Backgrounds::ScreenHandle Backgrounds::GetScreen(const std::string& scx) const
{
    ASSERT(mScxToScreen.contains(scx));
    return mScxToScreen.find(scx)->second;
}

}