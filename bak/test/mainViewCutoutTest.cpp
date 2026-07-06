#include "gtest/gtest.h"

#include "graphics/texture.hpp"
#include "gui/colors.hpp"
#include "gui/screenCutout.hpp"

#include <glm/glm.hpp>

namespace {

// glm::vec4 operator== returns a bvec4 (not bool), so gtest can't compare vec4s
// directly — compare each component with EXPECT_NEAR, mirroring the idiom in
// terrainSubstituteTest.cpp / combatSubstituteTest.cpp.
void ExpectVec(const glm::vec4& got, const glm::vec4& want)
{
    EXPECT_NEAR(got.r, want.r, 1e-5f);
    EXPECT_NEAR(got.g, want.g, 1e-5f);
    EXPECT_NEAR(got.b, want.b, 1e-5f);
    EXPECT_NEAR(got.a, want.a, 1e-5f);
}

Graphics::Texture MakeFilledTexture(
    unsigned w, unsigned h, unsigned tw, unsigned th, glm::vec4 fill)
{
    auto tex = Graphics::Texture{w, h, tw, th};
    for (auto& p : tex.GetTexture()) p = fill;
    return tex;
}

constexpr glm::vec4 kOpaqueWhite{1, 1, 1, 1};
constexpr glm::vec4 kTransparentBlack{0, 0, 0, 0};

} // namespace

// Proprietary 320x200 asset: actual dims == target dims -> scale is 1.0, so the
// cut-out is byte-identical to the pre-4K backgrounds.cpp logic.
TEST(MainViewCutoutTest, Proprietary320x200IsUnscaled)
{
    auto tex = MakeFilledTexture(320, 200, 320, 200, kOpaqueWhite);
    Gui::PrepareMainViewBackground(tex);

    // Hole interior (original coords 13,88) punched to transparent black.
    ExpectVec(tex.GetPixel(13, 88), kTransparentBlack);
    // Maroon border just outside the hole (left/right at x=12 and x=307, y=87).
    ExpectVec(tex.GetPixel(12, 87), Gui::Color::frameMaroon);
    ExpectVec(tex.GetPixel(307, 87), Gui::Color::frameMaroon);
    // Pixel outside the hole + border is untouched (still opaque white).
    ExpectVec(tex.GetPixel(0, 0), kOpaqueWhite);
}

// 4K substitute (2048x1280, target 320x200): scale is 6.4 in both axes, so the
// cut-out + border move to the proportionally-correct location.
TEST(MainViewCutoutTest, Substitute2048x1280ScalesCutout)
{
    auto tex = MakeFilledTexture(2048, 1280, 320, 200, kOpaqueWhite);
    Gui::PrepareMainViewBackground(tex);

    // floor(13 * 6.4) = 83, floor(88 * 6.4) = 563 -> scaled hole interior.
    ExpectVec(tex.GetPixel(83, 563), kTransparentBlack);
    // floor(12 * 6.4) = 76, floor(87 * 6.4) = 556 -> scaled border.
    ExpectVec(tex.GetPixel(76, 556), Gui::Color::frameMaroon);
    // floor(307 * 6.4) = 1964 (< 2048, no modulo wrap) -> scaled right border.
    ExpectVec(tex.GetPixel(1964, 556), Gui::Color::frameMaroon);
    // The OLD unscaled coord (13,88) is now outside the scaled hole -> untouched.
    ExpectVec(tex.GetPixel(13, 88), kOpaqueWhite);
    // Pixel outside everything is untouched.
    ExpectVec(tex.GetPixel(0, 0), kOpaqueWhite);
}

// Non-square substitute (1024x1024, target 320x200): sx=3.2, sy=5.12 — the x and y
// axes scale independently, so the cut-out stays correctly shaped for any aspect.
TEST(MainViewCutoutTest, IndependentAxisScaling)
{
    auto tex = MakeFilledTexture(1024, 1024, 320, 200, kOpaqueWhite);
    Gui::PrepareMainViewBackground(tex);

    // floor(13 * 3.2) = 41, floor(88 * 5.12) = 450 -> scaled hole interior.
    ExpectVec(tex.GetPixel(41, 450), kTransparentBlack);
    // floor(12 * 3.2) = 38, floor(87 * 5.12) = 445 -> scaled border.
    ExpectVec(tex.GetPixel(38, 445), Gui::Color::frameMaroon);
}