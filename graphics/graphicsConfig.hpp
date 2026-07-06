#pragma once

#include <string>
#include <unordered_set>
#include <vector>

namespace Graphics {

// Process-wide graphics settings derived from config.json. Mirrors the Paths
// singleton pattern: set once at startup from the loaded config, read from
// deep in the call stack (e.g. BAK::PNGToTexture) without threading params
// through every TextureFactory consumer. Room to grow for the Phase 5/7
// graphics toggles (EnableSSAO, EnableBloom, ...).
class GraphicsConfig
{
public:
    static GraphicsConfig& Get();

    // Cap (in pixels) applied to the largest dimension of substitute PNGs.
    // Textures exceeding it are downscaled before entering a TextureStore so
    // the shared GL_TEXTURE_2D_ARRAY stays bounded (see ROADMAP §1 blocker #1).
    unsigned GetMaxTextureDim() const;
    void SetMaxTextureDim(unsigned dim);

    // Task 3.2 — hero fullscreen backgrounds: base names (no extension) that bypass
    // the MaxTextureDim cap and get a dedicated one-layer sheet at the substitute's
    // full uncapped resolution. Consulted by the SCX substitute path so the bypass
    // is data-driven (no param threading). Empty set = today's behavior (all capped).
    bool IsHero(const std::string& baseName) const;
    void SetHeroTextures(const std::vector<std::string>& heroes);

private:
    GraphicsConfig();

    unsigned mMaxTextureDim;
    std::unordered_set<std::string> mHeroTextures;
};

}