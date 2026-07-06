#pragma once

// Prepare the DIALOG_BG_MAIN screen: the background drawn behind the 3D world view.
// Extracted from gui/backgrounds.cpp so the cut-out math is unit-testable without
// linking the gui library (this header depends only on Graphics::Texture and the
// constexpr Gui::Color values).

#include "graphics/texture.hpp"
#include "gui/colors.hpp"

#include <glm/glm.hpp>

namespace Gui {

// Black out transparent pixels, then punch a centered hole (for the 3D world view)
// with a maroon border into a DIALOG screen texture.
//
// The cut-out coordinates are authored in the ORIGINAL BAK screen space (320x200).
// A 4K substitute has larger actual pixel dims but keeps the original dims as its
// target (Task 1.3), so the coords are scaled from target space to actual pixel
// space. For the proprietary 320x200 asset the scale is 1.0 -> byte-identical to the
// pre-4K behaviour; for a 2048x1280 substitute (target 320x200) the scale is 6.4 and
// the hole lands at the proportionally-correct, frame-aligned location.
inline void PrepareMainViewBackground(Graphics::Texture& tex)
{
    for (auto& pixel : tex.GetTexture())
    {
        if (pixel.a == 0)
        {
            pixel = Color::black;
        }
    }

    const float sx = static_cast<float>(tex.GetWidth())
        / static_cast<float>(tex.GetTargetWidth());
    const float sy = static_cast<float>(tex.GetHeight())
        / static_cast<float>(tex.GetTargetHeight());
    const auto SX = [sx](int v){ return static_cast<unsigned>(v * sx); };
    const auto SY = [sy](int v){ return static_cast<unsigned>(v * sy); };

    // Cut out the center for the 3D world view.
    for (unsigned x = SX(13); x < SX(294 + 13); x++)
    {
        for (unsigned y = SY(200 - 13); y > SY(200 - (100 + 13)); y--)
        {
            tex.SetPixel(x, y, glm::vec4{0});
        }
    }
    // Maroon border around the cut-out.
    for (unsigned x = SX(13); x < SX(294 + 13); x++)
    {
        tex.SetPixel(x, SY(200 - (100 + 13)), Color::frameMaroon);
        tex.SetPixel(x, SY(200 - 13), Color::frameMaroon);
    }
    for (unsigned y = SY(200 - 13); y > SY(200 - (100 + 14)); y--)
    {
        tex.SetPixel(SX(12), y, Color::frameMaroon);
        tex.SetPixel(SX(294 + 13), y, Color::frameMaroon);
    }
}

} // namespace Gui