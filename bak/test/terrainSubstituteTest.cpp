#include "gtest/gtest.h"

#include "bak/file/fileBuffer.hpp"
#include "bak/image.hpp"
#include "bak/palette.hpp"
#include "bak/textureFactory.hpp"
#include "com/path.hpp"
#include "com/png.hpp"
#include "graphics/graphicsConfig.hpp"
#include "graphics/texture.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

namespace {

// Terrain band offsets (Ground/Road/Waterfall/Path/Dirt/River/Sand/Bank) and their sum,
// mirroring AddTerrainToTextureStore's hardcoded slice of the 320x200 ZxxL.SCX.
constexpr std::array<unsigned, 8> kBandOffsets{70, 20, 20, 32, 20, 27, 6, 5};
constexpr unsigned kBandTotal = 200;

// Synthesize a minimal valid PAL FileBuffer: a "VGA:" tag (0x3a414756, little-endian) +
// LE size + 256 RGB triples. Palette(FileBuffer&) does fb.Find(DataTag::VGA) then reads
// size/3 colors. The terrain substitute branch never reads the palette, but the signature
// requires one.
BAK::Palette MakeDummyPalette()
{
    constexpr unsigned kColors = 256;
    auto fb = BAK::FileBuffer{8 + kColors * 3};
    fb.PutUint8(0x56); // 'V'
    fb.PutUint8(0x47); // 'G'
    fb.PutUint8(0x41); // 'A'
    fb.PutUint8(0x3a); // ':'
    fb.PutUint32LE(kColors * 3);
    for (unsigned i = 0; i < kColors * 3; ++i) fb.PutUint8(0);
    return BAK::Palette{fb};
}

PNGImage MakeSolidImage(std::uint32_t w, std::uint32_t h, PNGColor c)
{
    PNGImage img{};
    img.mWidth = w;
    img.mHeight = h;
    img.mPixels.resize(static_cast<std::size_t>(w) * h, c);
    return img;
}

// Per-type suffixes in BAK::Terrain enum order, mirroring textureFactory.cpp's kTypeNames.
constexpr std::array<const char*, 8> kTypeSuffix{
    "GROUND", "ROAD", "WATERFALL", "PATH", "DIRT", "RIVER", "SAND", "BANK"};

// Assert every texel of a (solid-fill) texture equals the given 8-bit RGBA, with float
// tolerance. Substitute texels carry the PNG's colors; proprietary slices carry
// palette.GetColor(0) = {0,0,0,0} for the all-zero dummy palette + all-zero terrain.
void ExpectSolidColor(
    const Graphics::Texture& t,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a,
    const std::string& label)
{
    const auto& texels = t.GetTexture();
    ASSERT_FALSE(texels.empty()) << label;
    const auto& p = texels.front();
    EXPECT_NEAR(p.r, static_cast<float>(r) / 255.f, 1e-5f) << label;
    EXPECT_NEAR(p.g, static_cast<float>(g) / 255.f, 1e-5f) << label;
    EXPECT_NEAR(p.b, static_cast<float>(b) / 255.f, 1e-5f) << label;
    EXPECT_NEAR(p.a, static_cast<float>(a) / 255.f, 1e-5f) << label;
}

} // namespace

struct TerrainSubstituteTest : public ::testing::Test
{
    std::filesystem::path mTmpDir;

    void SetUp() override
    {
        mTmpDir = std::filesystem::temp_directory_path() / "bak_terrain_substitute_test";
        std::filesystem::remove_all(mTmpDir);
        std::filesystem::create_directories(mTmpDir);
        Paths::Get().SetAssets4kDirectory(mTmpDir.string());
        // Task 4.4: reset the process-wide GraphicsConfig OriginalMode flag.
        // ctest runs each TEST_F as its own process (gtest_discover_tests), so this
        // is redundant there -- but running the bakTest binary directly puts every
        // case in one shared process, where OriginalModeSkipsSubstituteAndUses-
        // Proprietary (which flips it true) would leak into the other substitution
        // tests. Resetting here + in TearDown defends that single-process mode.
        Graphics::GraphicsConfig::Get().SetOriginalMode(false);
    }

    void TearDown() override
    {
        Graphics::GraphicsConfig::Get().SetOriginalMode(false);
        Paths::Get().SetAssets4kDirectory("");
        std::filesystem::remove_all(mTmpDir);
    }
};

// Substitute at the native 320x200 aspect: each strip's pixel height matches the original
// offset, target dims equal the original BAK strip dims (Task 1.3), and the solid fill
// round-trips through WritePNG/LoadPNG into the slice texels.
TEST_F(TerrainSubstituteTest, SlicesEightStripsFromWholeImagePng)
{
    const auto png = MakeSolidImage(320, 200, PNGColor{1, 2, 3, 255});
    WritePNG((mTmpDir / "Z01L.PNG").string().c_str(), png);

    auto store = Graphics::TextureStore{};
    auto terrain = BAK::Image{320, 200, 0, false};
    auto pal = MakeDummyPalette();

    BAK::TextureFactory::AddTerrainToTextureStore(store, terrain, pal, "Z01L.SCX");

    ASSERT_EQ(store.size(), 8u);
    for (unsigned i = 0; i < 8; ++i)
    {
        const auto& t = store.GetTexture(i);
        EXPECT_EQ(t.GetWidth(), 320u) << "strip " << i;
        EXPECT_EQ(t.GetHeight(), kBandOffsets[i]) << "strip " << i;
        EXPECT_EQ(t.GetTargetWidth(), 320u) << "strip " << i;
        EXPECT_EQ(t.GetTargetHeight(), kBandOffsets[i]) << "strip " << i;

        const auto& texels = t.GetTexture();
        ASSERT_EQ(texels.size(), static_cast<std::size_t>(320) * kBandOffsets[i]);
        const auto& p0 = texels.front();
        EXPECT_NEAR(p0.r, 1.f / 255.f, 1e-5f);
        EXPECT_NEAR(p0.g, 2.f / 255.f, 1e-5f);
        EXPECT_NEAR(p0.b, 3.f / 255.f, 1e-5f);
        EXPECT_NEAR(p0.a, 1.f, 1e-5f);
    }
}

// Substitute at a non-native aspect (320x400): band fractions scale strip heights
// proportionally (2x here) while target dims stay at the original BAK offsets (Task 1.3),
// and the 8 bands tile [0, H] with no gaps or overlaps.
TEST_F(TerrainSubstituteTest, ScalesBandFractionsForTallerPng)
{
    const auto png = MakeSolidImage(320, 400, PNGColor{9, 8, 7, 255});
    WritePNG((mTmpDir / "Z02L.PNG").string().c_str(), png);

    auto store = Graphics::TextureStore{};
    auto terrain = BAK::Image{320, 200, 0, false};
    auto pal = MakeDummyPalette();

    BAK::TextureFactory::AddTerrainToTextureStore(store, terrain, pal, "Z02L.SCX");

    ASSERT_EQ(store.size(), 8u);
    std::uint32_t totalStripH = 0;
    for (unsigned i = 0; i < 8; ++i)
    {
        const auto& t = store.GetTexture(i);
        EXPECT_EQ(t.GetWidth(), 320u) << "strip " << i;
        EXPECT_EQ(t.GetHeight(), 2u * kBandOffsets[i]) << "strip " << i;
        EXPECT_EQ(t.GetTargetWidth(), 320u) << "strip " << i;
        EXPECT_EQ(t.GetTargetHeight(), kBandOffsets[i]) << "strip " << i;
        totalStripH += t.GetHeight();
    }
    EXPECT_EQ(totalStripH, 400u);
}

// No substitute present -> falls through to the proprietary-SCX slice (unchanged behavior),
// still adding exactly 8 textures with the original dims. Guards that the substitute
// branch is not spuriously triggered and the fallback path remains intact.
TEST_F(TerrainSubstituteTest, FallsBackWhenNoSubstitutePng)
{
    auto store = Graphics::TextureStore{};
    auto terrain = BAK::Image{320, 200, 0, false};
    auto pal = MakeDummyPalette();

    BAK::TextureFactory::AddTerrainToTextureStore(store, terrain, pal, "Z03L.SCX");

    ASSERT_EQ(store.size(), 8u);
    for (unsigned i = 0; i < 8; ++i)
    {
        const auto& t = store.GetTexture(i);
        EXPECT_EQ(t.GetWidth(), 320u) << "strip " << i;
        EXPECT_EQ(t.GetHeight(), kBandOffsets[i]) << "strip " << i;
        EXPECT_EQ(t.GetTargetWidth(), 320u) << "strip " << i;
        EXPECT_EQ(t.GetTargetHeight(), kBandOffsets[i]) << "strip " << i;
    }
}

// Task 4.4: OriginalMode=true gates FindSubstitute -- even though a 4K ZxxL.PNG
// is present on disk, the gate returns nullopt and the proprietary SCX slice path
// runs instead. Proves the gate is active at runtime: strips come out at the
// original BAK dims (320 x band offset) and, because the dummy palette is
// all-black, the texels are RGB 0 -- NOT the substitute's solid {1,2,3}/255.
// (SlicesEightStripsFromWholeImagePng above covers the off-path with the flag
// false; this test covers the on-path.)
TEST_F(TerrainSubstituteTest, OriginalModeSkipsSubstituteAndUsesProprietary)
{
    const auto png = MakeSolidImage(320, 200, PNGColor{1, 2, 3, 255});
    WritePNG((mTmpDir / "Z04L.PNG").string().c_str(), png);

    Graphics::GraphicsConfig::Get().SetOriginalMode(true);

    auto store = Graphics::TextureStore{};
    auto terrain = BAK::Image{320, 200, 0, false};
    auto pal = MakeDummyPalette();

    BAK::TextureFactory::AddTerrainToTextureStore(store, terrain, pal, "Z04L.SCX");

    ASSERT_EQ(store.size(), 8u);
    for (unsigned i = 0; i < 8; ++i)
    {
        const auto& t = store.GetTexture(i);
        // proprietary dims (320 x original band offset), not the substitute's
        EXPECT_EQ(t.GetWidth(), 320u) << "strip " << i;
        EXPECT_EQ(t.GetHeight(), kBandOffsets[i]) << "strip " << i;
        EXPECT_EQ(t.GetTargetWidth(), 320u) << "strip " << i;
        EXPECT_EQ(t.GetTargetHeight(), kBandOffsets[i]) << "strip " << i;

        // dummy palette is all-black, so proprietary texels are black -- NOT the
        // substitute's {1,2,3}/255, proving the gate skipped the 4K PNG.
        const auto& texels = t.GetTexture();
        ASSERT_EQ(texels.size(), static_cast<std::size_t>(320) * kBandOffsets[i]);
        const auto& p0 = texels.front();
        EXPECT_NEAR(p0.r, 0.f, 1e-5f) << "strip " << i;
        EXPECT_NEAR(p0.g, 0.f, 1e-5f) << "strip " << i;
        EXPECT_NEAR(p0.b, 0.f, 1e-5f) << "strip " << i;
    }
}

// Per-type substitution (tier 1): only a Z01L_GROUND.PNG exists (no whole-image
// Z01L.PNG). Band 0 must come from the per-type PNG; bands 1-7 have no substitute of
// any kind and fall to the proprietary SCX slice. Proves tier 1 > tier 3 and that a
// single per-type override leaves the other types on the proprietary fallback. The
// per-type PNG's ACTUAL dims (64x64) differ from the whole-image slicer's (320 x band),
// so the actual-dim asserts discriminate tier 1 from tier 2.
TEST_F(TerrainSubstituteTest, PerTypePngOverridesProprietaryForOneType)
{
    const auto png = MakeSolidImage(64, 64, PNGColor{11, 22, 33, 255});
    WritePNG((mTmpDir / "Z01L_GROUND.PNG").string().c_str(), png);

    auto store = Graphics::TextureStore{};
    auto terrain = BAK::Image{320, 200, 0, false};
    auto pal = MakeDummyPalette();

    BAK::TextureFactory::AddTerrainToTextureStore(store, terrain, pal, "Z01L.SCX");

    ASSERT_EQ(store.size(), 8u);
    // band 0: per-type PNG (actual 64x64, target 320x70), solid {11,22,33}.
    const auto& g = store.GetTexture(0);
    EXPECT_EQ(g.GetWidth(), 64u);
    EXPECT_EQ(g.GetHeight(), 64u);
    EXPECT_EQ(g.GetTargetWidth(), 320u);
    EXPECT_EQ(g.GetTargetHeight(), kBandOffsets[0]);
    ExpectSolidColor(g, 11, 22, 33, 255, "band 0 (per-type Ground)");
    // bands 1-7: proprietary slice (320 x band offset), palette index 0 = {0,0,0,0}.
    for (unsigned i = 1; i < 8; ++i)
    {
        const auto& t = store.GetTexture(i);
        EXPECT_EQ(t.GetWidth(), 320u) << "band " << i;
        EXPECT_EQ(t.GetHeight(), kBandOffsets[i]) << "band " << i;
        EXPECT_EQ(t.GetTargetWidth(), 320u) << "band " << i;
        EXPECT_EQ(t.GetTargetHeight(), kBandOffsets[i]) << "band " << i;
        ExpectSolidColor(t, 0, 0, 0, 0, "band " + std::to_string(i) + " (proprietary)");
    }
}

// Per-type (tier 1) beats whole-image (tier 2): both Z01L.PNG (color A) and
// Z01L_GROUND.PNG (color B) exist. Band 0 must use the per-type PNG; bands 1-7 must
// still slice the whole image. Proves tier 1 > tier 2 and that overriding one type does
// not disturb the others (the whole image is still sliced for non-overridden bands).
TEST_F(TerrainSubstituteTest, PerTypePngOverridesWholeImageForOneType)
{
    const auto whole = MakeSolidImage(320, 200, PNGColor{1, 2, 3, 255});       // A
    WritePNG((mTmpDir / "Z01L.PNG").string().c_str(), whole);
    const auto ground = MakeSolidImage(64, 64, PNGColor{200, 100, 50, 255});   // B
    WritePNG((mTmpDir / "Z01L_GROUND.PNG").string().c_str(), ground);

    auto store = Graphics::TextureStore{};
    auto terrain = BAK::Image{320, 200, 0, false};
    auto pal = MakeDummyPalette();

    BAK::TextureFactory::AddTerrainToTextureStore(store, terrain, pal, "Z01L.SCX");

    ASSERT_EQ(store.size(), 8u);
    // band 0: per-type B wins (actual 64x64, target 320x70).
    const auto& g = store.GetTexture(0);
    EXPECT_EQ(g.GetWidth(), 64u);
    EXPECT_EQ(g.GetHeight(), 64u);
    EXPECT_EQ(g.GetTargetWidth(), 320u);
    EXPECT_EQ(g.GetTargetHeight(), kBandOffsets[0]);
    ExpectSolidColor(g, 200, 100, 50, 255, "band 0 (per-type Ground overrides whole)");
    // bands 1-7: whole-image slice (320 x band offset, since the PNG is 320x200), solid A.
    for (unsigned i = 1; i < 8; ++i)
    {
        const auto& t = store.GetTexture(i);
        EXPECT_EQ(t.GetWidth(), 320u) << "band " << i;
        EXPECT_EQ(t.GetHeight(), kBandOffsets[i]) << "band " << i;
        EXPECT_EQ(t.GetTargetWidth(), 320u) << "band " << i;
        EXPECT_EQ(t.GetTargetHeight(), kBandOffsets[i]) << "band " << i;
        ExpectSolidColor(t, 1, 2, 3, 255, "band " + std::to_string(i) + " (whole-image slice)");
    }
}

// All 8 per-type PNGs present, no whole-image Z01L.PNG: every band comes from its own
// per-type PNG (tier 1 for all 8), each with its distinct color and the per-type actual
// dims (64x64). Proves the full per-type set is used end-to-end and the 8 textures are
// added in enum order with the right per-type target heights.
TEST_F(TerrainSubstituteTest, AllEightPerTypePngsAreUsed)
{
    const std::array<PNGColor, 8> colors{{
        PNGColor{10, 0, 0, 255}, PNGColor{0, 20, 0, 255}, PNGColor{0, 0, 30, 255},
        PNGColor{40, 40, 0, 255}, PNGColor{0, 50, 50, 255}, PNGColor{60, 0, 60, 255},
        PNGColor{70, 70, 70, 255}, PNGColor{80, 90, 100, 255}}};
    for (unsigned i = 0; i < 8; ++i)
    {
        const auto png = MakeSolidImage(64, 64, colors[i]);
        WritePNG((mTmpDir / (std::string{"Z01L_"} + kTypeSuffix[i] + ".PNG")).string().c_str(), png);
    }

    auto store = Graphics::TextureStore{};
    auto terrain = BAK::Image{320, 200, 0, false};
    auto pal = MakeDummyPalette();

    BAK::TextureFactory::AddTerrainToTextureStore(store, terrain, pal, "Z01L.SCX");

    ASSERT_EQ(store.size(), 8u);
    for (unsigned i = 0; i < 8; ++i)
    {
        const auto& t = store.GetTexture(i);
        // tier 1: actual dims = the per-type PNG's own 64x64; target = original BAK strip.
        EXPECT_EQ(t.GetWidth(), 64u) << "band " << i;
        EXPECT_EQ(t.GetHeight(), 64u) << "band " << i;
        EXPECT_EQ(t.GetTargetWidth(), 320u) << "band " << i;
        EXPECT_EQ(t.GetTargetHeight(), kBandOffsets[i]) << "band " << i;
        ExpectSolidColor(t, colors[i].r, colors[i].g, colors[i].b, colors[i].a,
            "band " + std::to_string(i) + " (" + kTypeSuffix[i] + ")");
    }
}

// Task 4.4 OriginalMode gate for the new per-type tier: with OriginalMode=true, even
// though a Z01L_GROUND.PNG is on disk, FindSubstitute returns nullopt for tiers 1 AND 2,
// so all 8 bands fall to the proprietary SCX slice (tier 3). Bands come out at the
// original BAK dims (320 x band offset), NOT the per-type 64x64, and texels are
// palette index 0 = {0,0,0,0}, NOT the substitute's {11,22,33}/255. Locks the gate.
TEST_F(TerrainSubstituteTest, PerTypePngRespectsOriginalMode)
{
    const auto png = MakeSolidImage(64, 64, PNGColor{11, 22, 33, 255});
    WritePNG((mTmpDir / "Z01L_GROUND.PNG").string().c_str(), png);

    Graphics::GraphicsConfig::Get().SetOriginalMode(true);

    auto store = Graphics::TextureStore{};
    auto terrain = BAK::Image{320, 200, 0, false};
    auto pal = MakeDummyPalette();

    BAK::TextureFactory::AddTerrainToTextureStore(store, terrain, pal, "Z01L.SCX");

    ASSERT_EQ(store.size(), 8u);
    for (unsigned i = 0; i < 8; ++i)
    {
        const auto& t = store.GetTexture(i);
        // proprietary dims (320 x original band offset), NOT the per-type 64x64.
        EXPECT_EQ(t.GetWidth(), 320u) << "band " << i;
        EXPECT_EQ(t.GetHeight(), kBandOffsets[i]) << "band " << i;
        EXPECT_EQ(t.GetTargetWidth(), 320u) << "band " << i;
        EXPECT_EQ(t.GetTargetHeight(), kBandOffsets[i]) << "band " << i;
        // palette index 0 = {0,0,0,0}, NOT the per-type {11,22,33}/255.
        ExpectSolidColor(t, 0, 0, 0, 0,
            "band " + std::to_string(i) + " (OriginalMode -> proprietary)");
    }
}