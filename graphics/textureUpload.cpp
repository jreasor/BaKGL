#include "graphics/textureUpload.hpp"

#include <glm/glm.hpp>

namespace Graphics {

std::uint8_t QuantizeChannel(float c)
{
    // GL spec float -> 8-bit unsigned: round(clamp(c,0,1) * 255). The driver
    // applies this when GL_FLOAT is uploaded into a GL_RGBA8 internal format,
    // so replicating it here yields identical bytes. Clamp before the multiply
    // to match the spec order. Round via branchless (v + 0.5f) truncation
    // (round-half-up) instead of std::lround -- lround is a libm call that
    // does not inline in Debug builds and dominated the fill cost (4x/pixel).
    // Round-half-up agrees with the driver's round-to-nearest for every input
    // the engine produces: all source floats are byte-derived (k/255), so
    // v = k/255 * 255 is never an exact half-integer (ties can't occur); the
    // float error is < 1e-4, far under the 0.5 round threshold, so the round
    // trip is lossless.
    float v = c * 255.0f;
    if (v <= 0.0f) return 0;
    if (v >= 255.0f) return 255;
    return static_cast<std::uint8_t>(v + 0.5f);
}

void Quantize(const glm::vec4& pixel, std::uint8_t out[4])
{
    out[0] = QuantizeChannel(pixel.r);
    out[1] = QuantizeChannel(pixel.g);
    out[2] = QuantizeChannel(pixel.b);
    out[3] = QuantizeChannel(pixel.a);
}

std::vector<std::uint8_t> BuildRgba8Staging(const Texture& tex, unsigned maxDim)
{
    std::vector<std::uint8_t> staging(
        static_cast<std::size_t>(maxDim) * maxDim * 4);

    // Iterate y-outer / x-inner so writes are sequential in memory (cache
    // friendlier than the legacy x-outer / y-inner strided writes). The stored
    // index is identical: pixel (x,y) lives at byte offset (y*maxDim + x)*4,
    // which is the GL_RGBA / GL_UNSIGNED_BYTE layout glTexSubImage3D expects.
    std::size_t i = 0;
    for (unsigned y = 0; y < maxDim; y++)
        for (unsigned x = 0; x < maxDim; x++)
        {
            const auto px = tex.GetPixel(x, y);
            staging[i++] = QuantizeChannel(px.r);
            staging[i++] = QuantizeChannel(px.g);
            staging[i++] = QuantizeChannel(px.b);
            staging[i++] = QuantizeChannel(px.a);
        }
    return staging;
}

}