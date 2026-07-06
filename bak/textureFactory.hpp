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

    static void AddScreenToTextureStore(
        Graphics::TextureStore&,
        std::string_view scx,
        std::string_view pal);

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
