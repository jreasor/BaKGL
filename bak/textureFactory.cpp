#include "bak/textureFactory.hpp"

#include "bak/constants.hpp"
#include "bak/image.hpp"
#include "bak/imageStore.hpp"
#include "bak/palette.hpp"
#include "bak/screen.hpp"
#include "bak/fileBufferFactory.hpp"

#include "com/logger.hpp"
#include "com/png.hpp"
#include "com/path.hpp"
#include "com/string.hpp"
#include "com/stb_image_resize2.h"

#include "graphics/texture.hpp"
#include "graphics/graphicsConfig.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <numeric>
#include <optional>
#include <string>
#include <random>

namespace BAK {

Graphics::Texture ImageToTexture(const Image& image, const Palette& palette)
{
    auto texture = Graphics::Texture::TextureType{};
    const auto imageSize = image.GetWidth() * image.GetHeight();
    texture.reserve(imageSize);

    auto* pixels = image.GetPixels();

    for (unsigned i = 0; i < imageSize; i++)
    {
        texture.push_back(palette.GetColor(pixels[i]));
    }

    auto tex = Graphics::Texture{
        texture,
        static_cast<unsigned>(image.GetWidth()),
        static_cast<unsigned>(image.GetHeight()),
        static_cast<unsigned>(image.GetWidth()),
        static_cast<unsigned>(image.GetHeight()) };

    // For OpenGL
    tex.Invert();

    return tex;
}

// Memory isolation (ROADMAP §1 blocker #1): a single oversized substitute PNG in a
// shared TextureStore drives maxDim — and thus the GL_TEXTURE_2D_ARRAY allocation —
// to that dimension (a 4096px sheet is ~16 GiB). Downscale to the configured cap before
// the texture enters any store. STBIR_RGBA is non-premultiplied, so stbir alpha-weights
// internally; RGB is treated as sRGB, alpha as linear — correct for straight-alpha RGBA8.
// Shared by PNGToTexture and the terrain slicer so every substitute path is capped.
static PNGImage LoadAndCapPNG(const std::string& path, bool allowUncapped = false)
{
    auto image = LoadPNG(path.c_str());
    auto width = image.mWidth;
    auto height = image.mHeight;

    const auto cap = Graphics::GraphicsConfig::Get().GetMaxTextureDim();
    if (!allowUncapped && cap > 0 && (width > cap || height > cap))
    {
        const auto scale = static_cast<float>(cap) / std::max(width, height);
        const auto newW = std::max(1u, static_cast<unsigned>(std::lround(width  * scale)));
        const auto newH = std::max(1u, static_cast<unsigned>(std::lround(height * scale)));
        std::vector<PNGColor> resized(static_cast<std::size_t>(newW) * newH);
        if (stbir_resize_uint8_srgb(
                reinterpret_cast<const unsigned char*>(image.mPixels.data()),
                static_cast<int>(width),  static_cast<int>(height), 0,
                reinterpret_cast<unsigned char*>(resized.data()),
                static_cast<int>(newW),   static_cast<int>(newH),   0,
                STBIR_RGBA))
        {
            Logging::LogInfo(__FUNCTION__) << "Downscaled substitute " << path
                << " (" << width << "x" << height << ") -> ("
                << newW << "x" << newH << ") cap=" << cap << "\n";
            image.mPixels = std::move(resized);
            image.mWidth = newW;
            image.mHeight = newH;
        }
        else
        {
            Logging::LogError(__FUNCTION__) << "stbir_resize_uint8_srgb failed for " << path
                << " (" << width << "x" << height << ") -> (" << newW << "x" << newH
                << "); using uncapped texture (shared-sheet OOM risk)\n";
        }
    }
    else if (allowUncapped && cap > 0 && (width > cap || height > cap))
    {
        Logging::LogDebug(__FUNCTION__) << "Hero substitute " << path
            << " (" << width << "x" << height << ") loaded UNCAPPED"
            << " (cap=" << cap << ")\n";
    }
    return image;
}

Graphics::Texture PNGToTexture(std::string path, unsigned targetWidth, unsigned targetHeight, bool allowUncapped = false)
{
    auto image = LoadAndCapPNG(path, allowUncapped);
    auto width = image.mWidth;
    auto height = image.mHeight;

    const auto Get = [&](int x, int y){
        return image.mPixels[y * width + x];
    };

    auto texture = Graphics::Texture::TextureType{};

    for (int y = height - 1; y >= 0; y--)
    {
        for (int x = 0; x < (int) width; x++)
        {
            auto c = Get(x, y);
            const auto F = [](auto x){
                return static_cast<float>(x) / 255.; };
            texture.push_back(glm::vec4{F(c.r), F(c.g), F(c.b), F(c.a)});
        }
    }
    return Graphics::Texture{texture, width, height, targetWidth, targetHeight};
}

static std::optional<std::filesystem::path> FindSubstitute(
    const std::string& baseName,
    const std::string& ext)
{
    // Task 4.4 "Original" toggle: when on, skip every assets_4k/mod substitute so
    // the engine renders the original 1993 proprietary art. This is the single
    // chokepoint for all 4 substitution seams (BMX, combat, SCX, terrain), so one
    // check here makes every seam fall back to the proprietary decoder. Best-effort
    // at runtime: affects textures loaded AFTER the flip (next zone / restart);
    // already-loaded substitutes stay until their store is rebuilt.
    if (Graphics::GraphicsConfig::Get().GetOriginalMode())
        return std::nullopt;

    const auto fileName = baseName + ext;
    const auto& assets4k = Paths::Get().GetAssets4kDirectoryPath();
    if (!assets4k.empty())
    {
        const auto path = assets4k / fileName;
        if (std::filesystem::exists(path))
            return path;
    }
    const auto modDir = Paths::Get().GetModDirectoryPath() / fileName;
    if (std::filesystem::exists(modDir))
        return modDir;
    return std::nullopt;
}

Graphics::TextureStore TextureFactory::MakeTextureStore(
    std::string_view bmx,
    std::string_view pal)
{
    auto store = Graphics::TextureStore{};
    AddToTextureStore(store, bmx, pal);
    return store;
}

void TextureFactory::AddToTextureStore(
    Graphics::TextureStore& store,
    std::string_view bmx,
    std::string_view pal)
{
    const auto palette = Palette{std::string{pal}};

    auto fb = FileBufferFactory::Get()
        .CreateDataBuffer(std::string{bmx});
    const auto images = LoadImages(fb);

    auto baseName = SplitString(".", std::string(bmx))[0];
    const auto ext = images.size() > 1 ? ".BMX" : ".PNG";
    auto substitute = FindSubstitute(baseName, ext);
    if (substitute && images.size() == 1)
    {
        auto tex = PNGToTexture(substitute->string(), images.back().GetWidth(), images.back().GetHeight());
        Logging::LogDebug(__FUNCTION__) << "Found substitute BMX: " << *substitute
          << " Dims: (" << tex.GetWidth() << ", " << tex.GetHeight() << ") TargetDims: ("
          << tex.GetTargetWidth() << ", " << tex.GetTargetHeight() << ")\n";
        store.AddTexture(tex);
    }
    else if (substitute)
    {
        for (unsigned i = 0; i < images.size(); i++)
        {
            std::stringstream name{};
            name << i << ".PNG";
            auto path = *substitute / name.str();
            if (std::filesystem::exists(path))
            {
                auto tex = PNGToTexture(path.string(), images[i].GetWidth(), images[i].GetHeight());
                Logging::LogDebug(__FUNCTION__) << "Found substitute BMX: " << path
                  << " Dims: (" << tex.GetWidth() << ", " << tex.GetHeight() << ") TargetDims: ("
                  << tex.GetTargetWidth() << ", " << tex.GetTargetHeight() << ")\n";
                store.AddTexture(tex);
            }
            else
            {
                Logging::LogSpam(__FUNCTION__) << "No substitute image " << i
                    << " for BMX: " << bmx << " (looked for " << path << "), using proprietary\n";
                AddToTextureStore(store, images[i], palette);
            }
        }
    }
    else
    {
        Logging::LogSpam(__FUNCTION__) << "No substitute for BMX: " << bmx
            << " (looked for " << baseName << ext << "), using proprietary\n";
        AddToTextureStore(store, images, palette);
    }
}

// Combat/monster-sprite substitution (Task 1.8). Mirrors the multi-image BMX
// substitution but with per-color-swap variant naming, and a palette-OBJECT
// fallback (the caller's already-swapped palette) so the proprietary fallback
// keeps each monster's correct per-monster colors. The per-monster color-swap
// (CS<N>.DAT, a palette-index remap) cannot be applied to a pre-baked RGBA
// substitute (no palette indices), so 4K art must be generated per variant:
//   cs <= 9  -> assets4k/<base>_CS<N>.BMX/<i>.PNG  (e.g. GNT1_CS4.BMX/0.PNG..)
//   cs  > 9  -> assets4k/<base>.BMX/<i>.PNG        (cs=255 = no swap)
// STRICT fallback (design decision 2026-07-05): a cs<=9 variant missing its 4K
// art falls back to the proprietary swapped sprite, NEVER to a different
// variant's 4K art — the swap is the SOLE visual differentiator for ~5 monster
// families (gnt/wyv/ogr/mor/rog), so falling back to the non-variant 4K art
// would collapse them. No OOM risk: substitutes flow through PNGToTexture ->
// LoadAndCapPNG (Task 1.2 cap). See ROADMAP §1.8.
void TextureFactory::AddToTextureStore(
    Graphics::TextureStore& store,
    std::string_view bmx,
    const std::vector<Image>& images,
    const Palette& palette,
    unsigned colorSwap)
{
    auto baseName = SplitString(".", std::string(bmx))[0];
    const auto variantBase = (colorSwap <= 9)
        ? (baseName + "_CS" + std::to_string(colorSwap))
        : baseName;
    const auto ext = images.size() > 1 ? ".BMX" : ".PNG";
    auto substitute = FindSubstitute(variantBase, ext);

    if (substitute && images.size() == 1)
    {
        auto tex = PNGToTexture(substitute->string(), images.back().GetWidth(), images.back().GetHeight());
        Logging::LogDebug(__FUNCTION__) << "Found substitute combat BMX: " << *substitute
          << " Dims: (" << tex.GetWidth() << ", " << tex.GetHeight() << ") TargetDims: ("
          << tex.GetTargetWidth() << ", " << tex.GetTargetHeight() << ")\n";
        store.AddTexture(tex);
    }
    else if (substitute)
    {
        for (unsigned i = 0; i < images.size(); i++)
        {
            std::stringstream name{};
            name << i << ".PNG";
            auto path = *substitute / name.str();
            if (std::filesystem::exists(path))
            {
                auto tex = PNGToTexture(path.string(), images[i].GetWidth(), images[i].GetHeight());
                Logging::LogDebug(__FUNCTION__) << "Found substitute combat BMX: " << path
                  << " Dims: (" << tex.GetWidth() << ", " << tex.GetHeight() << ") TargetDims: ("
                  << tex.GetTargetWidth() << ", " << tex.GetTargetHeight() << ")\n";
                store.AddTexture(tex);
            }
            else
            {
                Logging::LogSpam(__FUNCTION__) << "No substitute image " << i
                    << " for combat BMX: " << bmx << " (looked for " << path << "), using proprietary\n";
                AddToTextureStore(store, images[i], palette);
            }
        }
    }
    else
    {
        Logging::LogSpam(__FUNCTION__) << "No substitute for combat BMX: " << bmx
            << " (looked for " << variantBase << ext << "), using proprietary\n";
        AddToTextureStore(store, images, palette);
    }
}

void TextureFactory::AddScreenToTextureStore(
    Graphics::TextureStore& store,
    std::string_view scx,
    std::string_view pal,
    bool allowUncapped)
{
    auto baseName = SplitString(".", std::string(scx))[0];
    auto substitute = FindSubstitute(baseName, ".PNG");

    if (substitute)
    {
        auto fb = FileBufferFactory::Get()
            .CreateDataBuffer(std::string{scx});
        auto target = LoadScreenResource(fb);
        Logging::LogDebug(__FUNCTION__) << "Found substitute SCX: " << *substitute << "\n";
        store.AddTexture(PNGToTexture(substitute->string(), target.GetWidth(), target.GetHeight(),
            allowUncapped));
    }
    else
    {
        Logging::LogSpam(__FUNCTION__) << "No substitute for SCX: " << scx
            << " (looked for " << baseName << ".PNG), using proprietary\n";
        const auto palette = Palette{std::string{pal}};
        auto fb = FileBufferFactory::Get()
            .CreateDataBuffer(std::string{scx});
        AddToTextureStore(store, LoadScreenResource(fb), palette);
    }
}

// Terrain band layout of the 320x200 ZxxL.SCX: 8 horizontal strips in BAK::Terrain enum
// order (Ground/Road/Waterfall/Path/Dirt/River/Sand/Bank). Shared by the whole-image
// slicer (tier 2) and the proprietary slicer (tier 3) so both honor one definition.
constexpr std::array<unsigned, 8> kBandOffsets{70, 20, 20, 32, 20, 27, 6, 5};
constexpr unsigned kBandTotal = 200;
static_assert(
    std::accumulate(kBandOffsets.begin(), kBandOffsets.end(), 0u) == kBandTotal,
    "terrain band offsets must sum to the SCX height (200)");

// Per-type suffixes in BAK::Terrain enum order (Ground=0..Bank=7). A per-type substitute
// is named <ZONE>_<TYPE>.PNG, e.g. Z01L_GROUND.PNG. The static_assert ties this table to
// the enumerator count so the two cannot drift apart.
constexpr std::array<const char*, 8> kTypeNames{
    "GROUND", "ROAD", "WATERFALL", "PATH", "DIRT", "RIVER", "SAND", "BANK"};
static_assert(
    kTypeNames.size() == static_cast<unsigned>(BAK::Terrain::Bank) + 1,
    "kTypeNames must match the BAK::Terrain enumerator count");

// Tier 2: slice band `bandIndex` out of a whole-image zone substitute (ZxxL.PNG). Band
// boundaries are scaled to the substitute's height (fractions of 200) so any sized PNG
// maps to the same 8 strips. Texels are copied bottom-up to match PNGToTexture's row
// order (GL-ready). targetWidth/Height stay at the original BAK strip dims (Task 1.3) so
// worldFactory's UV math is unchanged. The substitute is NOT shuffled -- the strip-0
// shuffle was a hack to hide repetition in the 320x200 original; a real 4K tile is expected
// to be seamless+flat at the asset source instead (per-type PNGs, tier 1).
static Graphics::Texture SliceWholeImageBand(
    const PNGImage& img,
    unsigned bandIndex,
    unsigned origWidth,
    unsigned origAccumBefore)
{
    const auto W = img.mWidth;
    const auto H = img.mHeight;
    const auto origOffset = kBandOffsets[bandIndex];
    const auto topRow = static_cast<unsigned>(
        std::lround(static_cast<double>(origAccumBefore) * H / kBandTotal));
    const auto botRow = static_cast<unsigned>(
        std::lround(static_cast<double>(origAccumBefore + origOffset) * H / kBandTotal));
    const auto stripH = (botRow > topRow) ? (botRow - topRow) : 1u;

    const auto F = [](auto v){ return static_cast<float>(v) / 255.f; };
    auto texels = Graphics::Texture::TextureType{};
    texels.reserve(static_cast<std::size_t>(W) * stripH);
    // bottom-up within the strip, matching PNGToTexture's row order (GL-ready)
    for (int y = static_cast<int>(botRow) - 1; y >= static_cast<int>(topRow); --y)
    {
        for (unsigned x = 0; x < W; ++x)
        {
            const auto& c = img.mPixels[static_cast<std::size_t>(y) * W + x];
            texels.emplace_back(glm::vec4{F(c.r), F(c.g), F(c.b), F(c.a)});
        }
    }
    return Graphics::Texture{texels, W, stripH, origWidth, origOffset};
}

// Tier 3: slice band `bandIndex` out of the proprietary ZxxL.SCX (the `terrain` Image +
// `palette`). Absolute-row math (startOff * width), palette-mapped. Band 0 (Ground) keeps
// the original engine's std::shuffle -- the 320x200 ground strip is low-entropy and bands
// under GL_REPEAT, so the shuffle hides repetition. Preserved verbatim so OriginalMode
// retains the original look.
static Graphics::Texture SliceProprietaryBand(
    const Image& terrain,
    const Palette& palette,
    unsigned bandIndex,
    unsigned startOff)
{
    const auto offset = kBandOffsets[bandIndex];
    auto* pixels = terrain.GetPixels();
    const auto width = terrain.GetWidth();

    const auto imageStart = startOff * width;
    const auto imageEnd = (startOff + offset) * width;
    auto image = Graphics::Texture::TextureType{};
    image.reserve(imageEnd - imageStart);
    for (unsigned i = imageStart; i < imageEnd; i++)
    {
        const auto& color = palette.GetColor(pixels[i]);
        image.push_back(color);
    }
    if (offset == 70) // Ground band
    {
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(image.begin(), image.end(), g);
    }
    return Graphics::Texture{image, width, offset, width, offset};
}

// Per-type terrain substitution. Each of the 8 terrain types (Ground/Road/Waterfall/Path/
// Dirt/River/Sand/Bank, in BAK::Terrain enum order) can be replaced by its own
// seamless+flat PNG named <ZONE>_<TYPE>.PNG (e.g. Z01L_GROUND.PNG). This kills the
// GL_REPEAT banding a single whole-image ZxxL.PNG caused: its Ground band ran a strong
// vertical brightness gradient that tiled into horizontal stripes. Per-type lookup
// precedence (first hit wins; all routed through FindSubstitute so Graphics.OriginalMode
// gates every tier back to the proprietary 1993 art):
//   1. <ZONE>_<TYPE>.PNG -- per-type 4K art (PNGToTexture: actual=PNG dims, target=BAK strip)
//   2. <ZONE>.PNG        -- whole-image substitute, sliced into band i (legacy fallback)
//   3. proprietary ZxxL.SCX band i (terrain Image + palette)
// The whole-image PNG is decoded lazily (cached) so it is never opened when all 8 types
// have per-type art. Exactly 8 textures are added in enum order regardless of source,
// preserving GetTerrainOffset(t) = mTerrainOffset + (unsigned)t and mHorizonOffset.
void TextureFactory::AddTerrainToTextureStore(
    Graphics::TextureStore& store,
    const Image& terrain,
    const Palette& palette,
    std::string_view scx)
{
    auto baseName = SplitString(".", std::string{scx})[0];
    const auto origWidth = terrain.GetWidth();

    // Tier-2 path, looked up once (cheap exists check); the PNG decode is deferred to
    // GetWholeImg so it never runs when every type has a per-type substitute.
    const auto wholeImgPath = FindSubstitute(baseName, ".PNG");
    std::optional<PNGImage> wholeImg;
    const auto GetWholeImg = [&]() -> const PNGImage* {
        if (!wholeImgPath) return nullptr;
        if (!wholeImg)
        {
            wholeImg = LoadAndCapPNG(wholeImgPath->string());
            Logging::LogDebug(__FUNCTION__) << "Found substitute terrain (whole image): "
                << *wholeImgPath << " Dims: (" << wholeImg->mWidth << "x"
                << wholeImg->mHeight << ")\n";
        }
        return &*wholeImg;
    };

    unsigned origAccum = 0;
    for (unsigned i = 0; i < 8; ++i)
    {
        const auto origOffset = kBandOffsets[i];
        if (auto perType = FindSubstitute(baseName + "_" + kTypeNames[i], ".PNG"))
        {
            Logging::LogDebug(__FUNCTION__) << "Found per-type substitute terrain: "
                << *perType << " (type " << kTypeNames[i] << ")\n";
            auto perTypeTex = PNGToTexture(perType->string(), origWidth, origOffset);
            // Structural-seam guard. The terrain GL layer is square maxDim x maxDim
            // (maxDim = the store's largest texture dim; a per-type PNG that is the
            // largest texture drives it to max(w,h)). GetPixel tiles the PNG into that
            // layer with modulo wrap and the layer wraps with GL_REPEAT, so a wrap
            // boundary falls mid-image whenever maxDim is not a whole multiple of the
            // PNG's height (vertical) or width (horizontal). "Seamless" only matches the
            // PNG's own edges -- not an interior wrap row -- so a non-dividing PNG shows
            // a repeating seam in-game even when perfectly seamless. A square PNG (or a
            // smaller-divides-larger size like 2048x512 / 2048x1024) avoids it; square is
            // also isotropic on the square ground quad, so no pre-stretch is needed.
            const auto pw = perTypeTex.GetWidth();
            const auto ph = perTypeTex.GetHeight();
            const auto layerDim = std::max(pw, ph);
            if (layerDim % pw != 0 || layerDim % ph != 0)
            {
                Logging::LogInfo(__FUNCTION__) << "Per-type terrain substitute "
                    << *perType << " (" << pw << "x" << ph << ") is not a whole divisor"
                    << " of its GL layer maxDim (~" << layerDim << "); GL_REPEAT will wrap"
                    << " mid-image and show a structural repeating seam. Use a SQUARE PNG"
                    << " (e.g. 1024x1024 / 1536x1536) or a smaller-divides-larger size"
                    << " (e.g. 2048x512 / 2048x1024), made seamless on BOTH axes + flat.\n";
            }
            store.AddTexture(perTypeTex);
        }
        else if (const auto* img = GetWholeImg())
        {
            store.AddTexture(SliceWholeImageBand(*img, i, origWidth, origAccum));
        }
        else
        {
            Logging::LogSpam(__FUNCTION__) << "No substitute terrain for type "
                << kTypeNames[i] << " of " << scx << ", using proprietary slice\n";
            store.AddTexture(SliceProprietaryBand(terrain, palette, i, origAccum));
        }
        origAccum += origOffset;
    }
}

void TextureFactory::AddToTextureStore(
    Graphics::TextureStore& store,
    const Image& image,
    const Palette& palette)
{
    store.AddTexture(ImageToTexture(image, palette));
}

void TextureFactory::AddToTextureStore(
    Graphics::TextureStore& store,
    const std::vector<Image>& images,
    const Palette& palette)
{
    for (const auto& image : images)
        AddToTextureStore(store, image, palette);
}

} // namespace BAK {
