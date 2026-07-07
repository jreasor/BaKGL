#pragma once

#include "graphics/texture.hpp"

#include <cstdint>
#include <vector>

namespace Graphics {

// Task 3.3-C: RGBA8 staging for glTexSubImage3D.
//
// The legacy upload path built a std::vector<glm::vec4> (16 B/px) and handed it
// to glTexSubImage3D with GL_FLOAT, leaving the driver to convert each float
// channel to an 8-bit value for the GL_RGBA8 internal format. Building the
// RGBA8 bytes ourselves (4 B/px) cuts the staging buffer and the bus transfer
// 4x and removes the driver float-conversion work, with identical pixels:
//
//   * The GL spec float -> 8-bit unsigned conversion is round(clamp(c,0,1)*255)
//     (OpenGL 4.1 spec, "Conversion from floating-point to normalized unsigned
//     integer"). QuantizeChannel applies exactly that, so the bytes we produce
//     match the ones the driver would have produced from the floats.
//   * Every decoder source is byte-derived (k/255.0f -- see textureFactory.cpp
//     PNGToTexture / terrain slicer / ImageToTexture), so the float -> byte
//     round trip is lossless (the float error in k/255*255 is < 1e-4, far under
//     the 0.5 round threshold).

// GL spec float -> 8-bit unsigned channel: multiply by 255, round to nearest,
// clamp to [0,255]. Matches the driver's GL_FLOAT -> GL_RGBA8 conversion.
std::uint8_t QuantizeChannel(float c);

// Pack an RGBA pixel (channels in [0,1]) into 4 RGBA8 bytes in R,G,B,A order,
// the layout GL_RGBA / GL_UNSIGNED_BYTE expects. out must point to >= 4 bytes.
void Quantize(const glm::vec4& pixel, std::uint8_t out[4]);

// Build the maxDim x maxDim RGBA8 staging buffer for one texture, tiling it via
// Texture::GetPixel (which wraps with modulo, matching the legacy
// std::vector<glm::vec4> fill). Returns a contiguous std::vector<std::uint8_t>
// of size maxDim*maxDim*4, ready for glTexSubImage3D(GL_RGBA, GL_UNSIGNED_BYTE).
std::vector<std::uint8_t> BuildRgba8Staging(const Texture& tex, unsigned maxDim);

}