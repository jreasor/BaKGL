#pragma once

#include <string_view>
#include <vector>

namespace Graphics {
class TextureStore;
}

namespace BAK {

class Image;
class Palette;

class TextureFactory
{
public:
    static Graphics::TextureStore MakeTextureStore(
        std::string_view bmx,
        std::string_view pal);

    static void AddToTextureStore(
        Graphics::TextureStore&,
        std::string_view bmx,
        std::string_view pal);

    // Task 3.2: allowUncapped=true bypasses the MaxTextureDim downscale so a hero SCX
    // substitute loads at its full resolution. Only Backgrounds passes true (it routes
    // heroes to a dedicated one-layer sheet, so the shared sheet's maxDim never inflates).
    // Every other caller (gdsScene/contents/staticTTM) takes the default false and stays
    // capped regardless of HeroTextures — closing the cross-cutting blast radius of the
    // earlier data-driven internal IsHero() call.
    static void AddScreenToTextureStore(
        Graphics::TextureStore&,
        std::string_view scx,
        std::string_view pal,
        bool allowUncapped = false);

    static void AddTerrainToTextureStore(
        Graphics::TextureStore&,
        const Image& terrain,
        const Palette&,
        std::string_view scx);

    static void AddToTextureStore(
        Graphics::TextureStore& store,
        const Image& image,
        const Palette& palette);

    static void AddToTextureStore(
        Graphics::TextureStore& store,
        const std::vector<Image>& images,
        const Palette& palette);

    // Combat/monster-sprite substitution (Task 1.8). Variant-aware: cs<=9 looks
    // up <base>_CS<N>.BMX/<i>.PNG, cs>9 looks up <base>.BMX/<i>.PNG. Strict
    // fallback to the (already-swapped) proprietary palette — never to a
    // different variant's 4K art. See the .cpp for the full rationale.
    static void AddToTextureStore(
        Graphics::TextureStore& store,
        std::string_view bmx,
        const std::vector<Image>& images,
        const Palette& palette,
        unsigned colorSwap);
};

}
