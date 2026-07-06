#pragma once

#include "graphics/opengl.hpp"

namespace Graphics {
class MeshObjectStorage;
class Texture;

class RenderData
{
public:
    RenderData();

    // Task 3.1: 3D upload path (zone world + combat). Defaults preserve pre-3.1
    // behavior (Nearest filter, Repeat wrap). The zone/terrain caller passes
    // LinearMipmap; wrap defaults to Repeat so tiled terrain doesn't seam (the
    // TextureBuffer LinearMipmap default is ClampToEdge for non-tiling GUI screens).
    void LoadData(
        const MeshObjectStorage& objectStore,
        const std::vector<Texture>& textures,
        unsigned maxDimension,
        FilterMode filter = FilterMode::Nearest,
        WrapMode wrap = WrapMode::Repeat);

    void Bind(GLuint textureTarget) const;
private:
    VertexArrayObject mVertexArrayObject;
    GLBuffers mGLBuffers;
    TextureBuffer mTextureBuffer;
};

}
