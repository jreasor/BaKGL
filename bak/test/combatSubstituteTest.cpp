#include "gtest/gtest.h"

#include "bak/file/fileBuffer.hpp"
#include "bak/image.hpp"
#include "bak/palette.hpp"
#include "bak/textureFactory.hpp"
#include "com/path.hpp"
#include "com/png.hpp"
#include "graphics/texture.hpp"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace {

// Minimal all-black PAL: "VGA:" tag (0x3a414756 LE) + LE size + 256 zeroed RGB triples.
// Every palette index maps to black, so the proprietary fallback yields black texels
// regardless of the image's pixel values — that lets the strict-fallback test tell
// "proprietary (black)" apart from "non-variant 4K PNG (red)" by texel color.
BAK::Palette MakeDummyPalette()
{
    constexpr unsigned kColors = 256;
    auto fb = BAK::FileBuffer{8 + kColors * 3};
    fb.PutUint8(0x56); fb.PutUint8(0x47); fb.PutUint8(0x41); fb.PutUint8(0x3a);
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

// Write <variantDir>/0.PNG and 1.PNG as the two given solid colors. The combat
// BMX seam reads a multi-image substitute as a directory of N.PNG files.
void WriteTwoImageSubstitute(
    const std::filesystem::path& tmpDir,
    const std::string& variantDirName,
    PNGColor c0,
    PNGColor c1)
{
    const auto dir = tmpDir / variantDirName;
    std::filesystem::create_directories(dir);
    WritePNG((dir / "0.PNG").string().c_str(), MakeSolidImage(64, 64, c0));
    WritePNG((dir / "1.PNG").string().c_str(), MakeSolidImage(48, 48, c1));
}

// Two BAK::Images: only the proprietary fallback reads their (zeroed) pixels;
// the substitute path only uses their dims as target dims. All-black palette
// -> any pixel index yields black, so the proprietary path is color-deterministic.
std::vector<BAK::Image> MakeTwoImages()
{
    return std::vector<BAK::Image>{
        BAK::Image{64, 64, 0, false},
        BAK::Image{64, 64, 0, false}};
}

const PNGColor kRed{255, 0, 0, 255};
const PNGColor kGreen{0, 255, 0, 255};
const PNGColor kBlue{0, 0, 255, 255};

} // namespace

struct CombatSubstituteTest : public ::testing::Test
{
    std::filesystem::path mTmpDir;

    void SetUp() override
    {
        mTmpDir = std::filesystem::temp_directory_path() / "bak_combat_substitute_test";
        std::filesystem::remove_all(mTmpDir);
        std::filesystem::create_directories(mTmpDir);
        Paths::Get().SetAssets4kDirectory(mTmpDir.string());
    }

    void TearDown() override
    {
        Paths::Get().SetAssets4kDirectory("");
        std::filesystem::remove_all(mTmpDir);
    }
};

// cs > 9 (no swap; cs=255): a non-variant <base>.BMX/0.PNG,1.PNG substitute loads
// directly — final RGBA colors, palette bypassed. Proves the seam and that the
// substitute texels come from the PNG, not the all-black palette.
TEST_F(CombatSubstituteTest, LoadsNonVariantSubstituteWhenColorSwapOutOfRange)
{
    WriteTwoImageSubstitute(mTmpDir, "GNT1.BMX", kRed, kGreen);
    auto store = Graphics::TextureStore{};
    auto images = MakeTwoImages();
    auto pal = MakeDummyPalette();

    BAK::TextureFactory::AddToTextureStore(store, "GNT1.BMX", images, pal, 255u);

    ASSERT_EQ(store.size(), 2u);
    const auto& t0 = store.GetTexture(0).GetTexture();
    ASSERT_EQ(t0.size(), 64u * 64u);
    EXPECT_NEAR(t0.front().r, 1.f, 1e-5f);
    EXPECT_NEAR(t0.front().g, 0.f, 1e-5f);
    const auto& t1 = store.GetTexture(1).GetTexture();
    ASSERT_EQ(t1.size(), 48u * 48u);
    EXPECT_NEAR(t1.front().g, 1.f, 1e-5f);
}

// cs <= 9: the seam looks up the per-variant <base>_CS<N>.BMX/ dir, not the
// non-variant <base>.BMX/. Proves the variant naming selects the right art.
TEST_F(CombatSubstituteTest, LoadsVariantSubstituteWhenColorSwapInRange)
{
    WriteTwoImageSubstitute(mTmpDir, "GNT1_CS4.BMX", kBlue, kRed);
    auto store = Graphics::TextureStore{};
    auto images = MakeTwoImages();
    auto pal = MakeDummyPalette();

    BAK::TextureFactory::AddToTextureStore(store, "GNT1.BMX", images, pal, 4u);

    ASSERT_EQ(store.size(), 2u);
    const auto& t0 = store.GetTexture(0).GetTexture();
    ASSERT_EQ(t0.size(), 64u * 64u);
    EXPECT_NEAR(t0.front().b, 1.f, 1e-5f);
    EXPECT_NEAR(t0.front().r, 0.f, 1e-5f);
    const auto& t1 = store.GetTexture(1).GetTexture();
    ASSERT_EQ(t1.size(), 48u * 48u);
    EXPECT_NEAR(t1.front().r, 1.f, 1e-5f);
}

// STRICT-FALLBACK GUARD (Task 1.8 design decision): cs<=9 with NO variant
// substitute present, but a NON-variant <base>.BMX/ IS present. The seam must
// fall back to the proprietary swapped sprite (black, via the all-black palette)
// and NOT collapse the variant by loading the non-variant 4K art (red).
TEST_F(CombatSubstituteTest, FallsBackToProprietaryNotNonVariantWhenVariantMissing)
{
    // Only the non-variant substitute exists; the _CS4 variant dir is absent.
    WriteTwoImageSubstitute(mTmpDir, "GNT1.BMX", kRed, kRed);
    auto store = Graphics::TextureStore{};
    auto images = MakeTwoImages();
    auto pal = MakeDummyPalette();

    BAK::TextureFactory::AddToTextureStore(store, "GNT1.BMX", images, pal, 4u);

    ASSERT_EQ(store.size(), 2u);
    for (unsigned i = 0; i < 2; ++i)
    {
        const auto& texels = store.GetTexture(i).GetTexture();
        ASSERT_EQ(texels.size(), 64u * 64u);
        const auto& p = texels.front();
        EXPECT_LT(p.r, 0.5f) << "texture " << i << " must not be the red non-variant PNG";
        EXPECT_LT(p.g, 0.5f) << "texture " << i;
        EXPECT_LT(p.b, 0.5f) << "texture " << i;
    }
}