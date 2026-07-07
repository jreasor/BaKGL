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

    // Task 3.3-B: ceiling on the number of layers a single TextureStore may
    // upload into one GL_TEXTURE_2D_ARRAY (was the compile-time
    // TextureBuffer::sMaxTextures = 256). 0 => 256 (built-in default). The
    // LoadTexturesGL guard reads this, so the cap is data-driven via
    // config.json -> Graphics.MaxTextures.
    unsigned GetMaxTextures() const;
    void SetMaxTextures(unsigned max);

    // Task 3.3-C: opt-in RGBA8 (GL_UNSIGNED_BYTE) staging for glTexSubImage3D.
    // Default false = legacy GL_FLOAT staging (driver converts floats to RGBA8
    // during upload). The RGBA8 path builds the bytes on the CPU
    // (BuildRgba8Staging); in the unoptimized Debug build its per-channel
    // scalar quantize is ~2.5x slower than the driver's conversion on the fill
    // (the dominant zone-hitch cost), so it is gated off until a vectorized
    // build or Task 3.3-D's async PBO path makes it worthwhile.
    bool GetRGBA8Upload() const;
    void SetRGBA8Upload(bool enabled);

    // Task 3.3-D: opt-in async PBO texture upload. Default false = synchronous
    // upload (the C path: RGBA8 staging -> glTexSubImage3D with a client
    // pointer, which blocks the render thread on the GPU transfer). When true,
    // LoadTexturesGL stages the RGBA8 bytes in a PixelUnpackBuffer and issues
    // glTexSubImage3D(data=nullptr) so the driver enqueues a GPU-side copy and
    // returns immediately. Only the `upload` segment of A's timer split is
    // PBO-acceleratable -- fill (CPU BuildRgba8Staging) and mipmap
    // (glGenerateMipmap) are not -- and a single PBO with no double-buffering
    // still syncs at glGenerateMipmap, so this is an async *foundation* +
    // decoupled staging for a future off-thread fill / double-buffered path,
    // not a zone-hitch fix. Falls back to the C path (RGBA8Upload) when off.
    bool GetAsyncTextureUpload() const;
    void SetAsyncTextureUpload(bool enabled);

    // Task 3.2 — hero fullscreen backgrounds: base names (no extension) that bypass
    // the MaxTextureDim cap and get a dedicated one-layer sheet at the substitute's
    // full uncapped resolution. Consulted by the SCX substitute path so the bypass
    // is data-driven (no param threading). Empty set = today's behavior (all capped).
    bool IsHero(const std::string& baseName) const;
    void SetHeroTextures(const std::vector<std::string>& heroes);

    // Task 3.5: anisotropic filtering level applied to LinearMipmap textures
    // (GL_TEXTURE_MAX_ANISOTROPY_EXT). 0 = off (plain trilinear); >0 clamped to
    // the driver's GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT at apply time. Engages
    // where the sample footprint is anisotropic -- oblique terrain/ground
    // (head-on GUI screens and billboarded combat sprites are an isotropic-
    // footprint no-op).
    float GetAnisotropicFilter() const;
    void SetAnisotropicFilter(float level);

    // Task 4.4: ON = use original 1993 art (no assets_4k/ substitution). Gates
    // FindSubstitute (textureFactory.cpp). Set at startup from config + updated at
    // runtime by the settings screen (best-effort: affects subsequently-loaded
    // textures only -- already-loaded ones refresh on zone change / restart).
    bool GetOriginalMode() const;
    void SetOriginalMode(bool enabled);

    // Task 4.4: UI-scaling knob (multiplies 320x200 -> window). Consumed once at
    // window creation in main3d; the settings screen reads/writes this editable copy
    // and persists it for next boot. Held here so the screen needs no ctor threading.
    float GetResolutionScale() const;
    void SetResolutionScale(float scale);

    // Task 4.4: path of the actually-loaded config.json, recorded by LoadConfigFile
    // so the settings screen can call SaveGraphicsValues without a ctor param.
    const std::string& GetConfigPath() const;
    void SetConfigPath(const std::string& path);

private:
    GraphicsConfig();

    unsigned mMaxTextureDim;
    unsigned mMaxTextures;
    bool mRGBA8Upload;
    bool mAsyncTextureUpload;
    std::unordered_set<std::string> mHeroTextures;
    float mAnisotropicFilter;
    bool mOriginalMode;
    float mResolutionScale;
    std::string mConfigPath;
};

}