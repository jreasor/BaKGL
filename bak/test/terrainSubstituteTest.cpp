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