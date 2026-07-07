// Task 3.3-C: unit tests for the RGBA8 staging path. Pure math + Texture
// tiling; no GL context required. Verifies the "identical pixels" claim: the
// GL-spec float->RGBA8 conversion round-trips byte-derived floats (k/255)
// losslessly, Quantize packs R,G,B,A in the right order, and BuildRgba8Staging
// tiles a smaller texture into maxDim*maxDim via GetPixel's modulo wrap.

#include "graphics/texture.hpp"
#include "graphics/textureUpload.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

using namespace Graphics;

// Every decoder source produces k/255.0f floats. Quantizing them back to a
// byte must recover k exactly -- this is the core "identical pixels" proof.
TEST(TextureUploadTest, QuantizeChannelRoundTripsByteDerivedFloats)
{
    for (unsigned k = 0; k <= 255; ++k)
    {
        const float f = static_cast<float>(k) / 255.0f;
        EXPECT_EQ(QuantizeChannel(f), static_cast<std::uint8_t>(k))
            << "k=" << k << " f=" << f;
    }
}

TEST(TextureUploadTest, QuantizeChannelClampsOutOfRange)
{
    EXPECT_EQ(QuantizeChannel(-1.0f), 0);
    EXPECT_EQ(QuantizeChannel(-0.001f), 0);
    EXPECT_EQ(QuantizeChannel(0.0f), 0);
    EXPECT_EQ(QuantizeChannel(1.0f), 255);
    EXPECT_EQ(QuantizeChannel(1.001f), 255);
    EXPECT_EQ(QuantizeChannel(2.0f), 255);
}

TEST(TextureUploadTest, QuantizePacksRgbaByteOrder)
{
    const auto F = [](unsigned k){ return static_cast<float>(k) / 255.0f; };
    const glm::vec4 pixel{F(10), F(20), F(30), F(40)};
    std::uint8_t out[4]{};
    Quantize(pixel, out);
    EXPECT_EQ(out[0], 10); // R
    EXPECT_EQ(out[1], 20); // G
    EXPECT_EQ(out[2], 30); // B
    EXPECT_EQ(out[3], 40); // A
}

namespace
{
glm::vec4 Color(unsigned r, unsigned g, unsigned b, unsigned a)
{
    const auto F = [](unsigned k){ return static_cast<float>(k) / 255.0f; };
    return glm::vec4{F(r), F(g), F(b), F(a)};
}
}

TEST(TextureUploadTest, BuildRgba8StagingSizeAndTiling)
{
    // 2x2 source tiled into a 4x4 staging buffer via GetPixel's modulo wrap.
    Texture tex{2, 2, 2, 2};
    tex.SetPixel(0, 0, Color(255, 0,   0,   255)); // red
    tex.SetPixel(1, 0, Color(0,   255, 0,   255)); // green
    tex.SetPixel(0, 1, Color(0,   0,   255, 255)); // blue
    tex.SetPixel(1, 1, Color(255, 255, 255, 255)); // white

    const unsigned maxDim = 4;
    const auto staging = BuildRgba8Staging(tex, maxDim);

    EXPECT_EQ(staging.size(), maxDim * maxDim * 4u);

    // Pixel (x,y) bytes at offset (y*maxDim + x)*4.
    const auto expectPx = [&](unsigned x, unsigned y,
                              std::uint8_t r, std::uint8_t g,
                              std::uint8_t b, std::uint8_t a)
    {
        const std::size_t o = (static_cast<std::size_t>(y) * maxDim + x) * 4;
        EXPECT_EQ(staging[o + 0], r) << "x=" << x << " y=" << y << " R";
        EXPECT_EQ(staging[o + 1], g) << "x=" << x << " y=" << y << " G";
        EXPECT_EQ(staging[o + 2], b) << "x=" << x << " y=" << y << " B";
        EXPECT_EQ(staging[o + 3], a) << "x=" << x << " y=" << y << " A";
    };

    // Identity region: staging(x,y) == source(x,y) for x,y in [0,2).
    expectPx(0, 0, 255, 0,   0,   255);
    expectPx(1, 0, 0,   255, 0,   255);
    expectPx(0, 1, 0,   0,   255, 255);
    expectPx(1, 1, 255, 255, 255, 255);

    // Modulo-wrap region: staging(2,0) wraps to source(0,0)=red, staging(3,3)
    // wraps to source(1,1)=white. Proves the tiling matches GetPixel's wrap.
    expectPx(2, 0, 255, 0,   0,   255);
    expectPx(3, 3, 255, 255, 255, 255);
}