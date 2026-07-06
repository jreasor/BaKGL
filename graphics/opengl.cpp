#include "graphics/opengl.hpp"

#include "graphics/texture.hpp"

#include "com/assert.hpp"
#include "com/logger.hpp"

#include <GL/glew.h>

#include <cmath>

namespace Graphics {

GLenum ToGlEnum(GLBindPoint p)
{
    switch (p)
    {
    case GLBindPoint::ArrayBuffer: return GL_ARRAY_BUFFER;
    case GLBindPoint::ElementArrayBuffer: return GL_ELEMENT_ARRAY_BUFFER;
    case GLBindPoint::TextureBuffer: return GL_TEXTURE_BUFFER;
    default: ASSERT(false); return GL_ARRAY_BUFFER;
    }
}

GLenum ToGlEnum(GLUpdateType p)
{
    switch (p)
    {
    case GLUpdateType::StaticDraw: return GL_STATIC_DRAW;
    case GLUpdateType::DynamicDraw: return GL_DYNAMIC_DRAW;
    default: ASSERT(false); return GL_STATIC_DRAW;
    }
}

VertexArrayObject::VertexArrayObject()
:
    mVertexArrayId{GenVertexArrayGL()},
    mActive{true}
{}

VertexArrayObject::VertexArrayObject(VertexArrayObject&& other) noexcept
{
    (*this) = std::move(other);
}

VertexArrayObject& VertexArrayObject::operator=(VertexArrayObject&& other) noexcept
{
    if (this == &other) return *this;

    mVertexArrayId = other.mVertexArrayId;
    other.mActive = false;
    return *this;
}

VertexArrayObject::~VertexArrayObject()
{
    if (mActive)
    {
        Logging::LogDebug("GLBuffers") << "Deleting GL vertex array id: " << mVertexArrayId << " @" << this << "\n";
        glDeleteVertexArrays(1, &mVertexArrayId);
    }
}

void VertexArrayObject::BindGL() const
{
    glBindVertexArray(mVertexArrayId);
}

void VertexArrayObject::UnbindGL() const
{
    glBindVertexArray(0);
}

GLuint VertexArrayObject::GenVertexArrayGL()
{
    GLuint vertexArrayId;
    glGenVertexArrays(1, &vertexArrayId);
    return vertexArrayId;
}

GLBuffers::GLBuffers()
:
    mBuffers{},
    mElementBuffer{GenBufferGL().mValue},
    mActive{true}
{
}

GLBuffers::GLBuffers(GLBuffers&& other) noexcept
{
    (*this) = std::move(other);
}

GLBuffers& GLBuffers::operator=(GLBuffers&& other) noexcept
{
    if (this == &other) return *this;

    for (const auto& [name, buffer] : other.mBuffers)
        mBuffers.emplace(name, buffer);

    mElementBuffer = other.mElementBuffer;

    other.mActive = false;

    return *this;
}

GLBuffers::~GLBuffers()
{
    if (mActive)
    {
        Logging::LogDebug("GLBuffers") << "Deleting GL buffers @" << this << std::endl;
        for (const auto& [name, buffer] : mBuffers)
        {
            Logging::LogDebug("GLBuffers") << "    Deleting buffer: " 
                << name << " GLBufferId: " << buffer.mBuffer.mValue << std::endl;
            glDeleteBuffers(1, &buffer.mBuffer.mValue);
        }
    }
}

void GLBuffers::AddBuffer(
    const std::string& name,
    GLLocation location,
    GLElems elems,
    GLDataType dataType,
    GLBindPoint bindPoint,
    GLUpdateType updateType)
{
    mBuffers.emplace(
        name,
        GLBuffer{
            location,
            elems,
            bindPoint,
            dataType,
            updateType,
            GenBufferGL()});

    BindAttribArrayGL(GetGLBuffer(name));
}

void GLBuffers::AddElementBuffer(
    const std::string& name)
{
    AddBuffer(name, GLNullLocation, GLElems{1}, GLDataType{GL_UNSIGNED_INT}, GLBindPoint::ElementArrayBuffer, GLUpdateType::StaticDraw);
}

void GLBuffers::AddTextureBuffer(
    const std::string& name)
{
    AddBuffer(name, GLNullLocation, GLElems{4}, GLDataType{GL_FLOAT}, GLBindPoint::TextureBuffer, GLUpdateType::DynamicDraw);
}


GLBufferId GLBuffers::GenBufferGL()
{
    GLuint buffer;
    glGenBuffers(1, &buffer);
    return GLBufferId{buffer};
}

void GLBuffers::BindAttribArrayGL(const GLBuffer& buffer)
{
    if (buffer.mLocation != GLNullLocation)
    {
        glEnableVertexAttribArray(buffer.mLocation.mValue);
        glBindBuffer(ToGlEnum(buffer.mGLBindPoint), buffer.mBuffer.mValue);
        glVertexAttribPointer(
            buffer.mLocation.mValue,
            buffer.mElems.mValue,
            buffer.mDataType.mValue,
            GL_FALSE, // normalized?
            0,        // stride
            (void*) 0 // array buffer offset
        );
    }
    else
    {
        glBindBuffer(ToGlEnum(buffer.mGLBindPoint), buffer.mBuffer.mValue);
    }
}

void GLBuffers::BindArraysGL()
{
    for (const auto& [name, buffer] : mBuffers)
    {
        BindAttribArrayGL(buffer);
    }
}


TextureBuffer::TextureBuffer(
    GLuint textureType)
:
    mTextureBuffer{
        std::invoke([](){
            unsigned texture;
            glGenTextures(1, &texture);
            return texture;
        })
    },
    mTextureType{textureType},
    mActive{true}
{}

TextureBuffer::TextureBuffer(TextureBuffer&& other) noexcept
{
    (*this) = std::move(other);
}

TextureBuffer& TextureBuffer::operator=(TextureBuffer&& other) noexcept
{
    if (this == &other) return *this;
    mTextureBuffer = other.mTextureBuffer;
    mTextureType = other.mTextureType;
    other.mActive = false;
    return *this;
}

TextureBuffer::~TextureBuffer()
{
    if (mActive)
    {
        Logging::LogDebug("GLBuffers") << "Deleting GL texture buffer id: " << mTextureBuffer << " @" << this << "\n";
        glDeleteTextures(1, &mTextureBuffer);
    }
}

GLuint TextureBuffer::GetId() const
{
    return mTextureBuffer;
}

void TextureBuffer::BindGL() const
{
    glBindTexture(mTextureType, mTextureBuffer);
}

void TextureBuffer::UnbindGL() const
{
    glBindTexture(mTextureType, 0);
}

void TextureBuffer::MakeDepthBuffer(unsigned width, unsigned height)
{
    ASSERT(mTextureType == GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    BindGL();
    glTexImage2D(mTextureType, 0, GL_DEPTH_COMPONENT,
        width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(mTextureType, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(mTextureType, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(mTextureType, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(mTextureType, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void TextureBuffer::MakePickBuffer(unsigned width, unsigned height)
{
    ASSERT(mTextureType == GL_TEXTURE_2D);
    BindGL();
    glTexImage2D(
        mTextureType, 0,
        GL_RGBA32F,
        width, height, 0,
        GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(mTextureType, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(mTextureType, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

//void TextureBuffer::MakeTexture2DArray()
//{
//    BindGL();
//
//    glTexStorage3D(
//        mTextureType,
//        1,              // levels
//        GL_RGBA8,       // Internal format
//        maxDim, maxDim, // width,height
//        sMaxTextures     // Number of layers
//    );
//}

void TextureBuffer::LoadTexturesGL(
    const std::vector<Texture>& textures,
    unsigned maxDim,
    FilterMode filter,
    WrapMode wrap)
{
    if (textures.size() > sMaxTextures)
    {
        // 4K doesn't raise the layer *count* (only per-texel size), but a shared
        // TextureStore that legitimately exceeds the ceiling would overflow the
        // GL_TEXTURE_2D_ARRAY. Fail loudly with diagnostics (ROADMAP Task 1.5) instead
        // of the bare "Too many textures" throw. Fix is to split the store or raise
        // sMaxTextures; allocation below is exact, so raising the ceiling is VRAM-free
        // for stores that don't approach it.
        Logging::LogError(__FUNCTION__) << "Too many textures: store has "
            << textures.size() << " layers, cap sMaxTextures=" << sMaxTextures
            << " (would overflow GL_TEXTURE_2D_ARRAY)."
            << " Split the TextureStore or raise sMaxTextures.\n";
        throw std::runtime_error("Too many textures (see log for store size / cap)");
    }

    BindGL();

    // Mip storage must be allocated up front for immutable-format textures. The old
    // levels=1 allocation is why the historical glGenerateMipmap call below produced
    // nothing useful (the "Doesn't actually look very good with mipmaps..." note).
    unsigned levels = 1;
    if (filter == FilterMode::LinearMipmap)
    {
        for (auto d = maxDim; d > 1; d >>= 1) ++levels;
    }

    // Allocate exactly as many layers as the store needs (>=1), not the full sMaxTextures
    // ceiling. sMaxTextures is now a pure runaway-guard (checked above); small stores
    // (cursor/font/icons, ~1-20 textures) no longer pre-allocate 256 layers of
    // maxDim*maxDim*RGBA8 each. The fill loop below writes layers 0..n-1, all in range.
    // (ROADMAP Task 1.5)
    const auto layerCount = textures.size() > 0
        ? static_cast<GLsizei>(textures.size())
        : GLsizei{1};

    glTexStorage3D(
        mTextureType,
        levels,         // mip levels: 1 for Nearest, full chain for LinearMipmap
        GL_RGBA8,       // Internal format
        maxDim, maxDim, // width,height
        layerCount      // Number of layers (exact, not the sMaxTextures ceiling)
    );


    unsigned index = 0;
    for (const auto& tex : textures)
    {
        std::vector<glm::vec4> paddedTex(
            maxDim * maxDim,
            glm::vec4{0});

        // Chuck the image in the padded sized texture
        // GetPixel() will wrap and fill the texture
        for (unsigned x = 0; x < maxDim; x++)
            for (unsigned y = 0; y < maxDim; y++)
                paddedTex[x + y * maxDim] = tex.GetPixel(x, y);

        glTexSubImage3D(
            mTextureType,
            0,                 // Mipmap number
            0, 0, index,       // xoffset, yoffset, zoffset
            maxDim, maxDim, 1, // width, height, depth
            GL_RGBA,           // format
            GL_FLOAT,          // type
            paddedTex.data()); // pointer to data

        index++;
    }
    
    if (filter == FilterMode::LinearMipmap)
    {
        glGenerateMipmap(mTextureType);
        glTexParameteri(mTextureType, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(mTextureType, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // Task 3.1: wrap is caller-controlled. GUI screens don't tile (ClampToEdge,
        // the default) so the wrapped-fill padding doesn't bleed into the visible
        // sub-rect at coarse mip levels; tiled terrain passes Repeat. The Nearest
        // branch below keeps its hardcoded GL_REPEAT, so this only affects mipped
        // textures.
        const auto wrapMode = (wrap == WrapMode::Repeat) ? GL_REPEAT : GL_CLAMP_TO_EDGE;
        glTexParameteri(mTextureType, GL_TEXTURE_WRAP_S, wrapMode);
        glTexParameteri(mTextureType, GL_TEXTURE_WRAP_T, wrapMode);
    }
    else
    {
        glTexParameteri(mTextureType, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(mTextureType, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(mTextureType, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(mTextureType, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    
    UnbindGL();
}

PixelPackBuffer::PixelPackBuffer()
:
    mBuffer{GenBufferGL()},
    mActive{true}
{}

PixelPackBuffer::PixelPackBuffer(PixelPackBuffer&& other) noexcept
{
    (*this) = std::move(other);
}

PixelPackBuffer& PixelPackBuffer::operator=(PixelPackBuffer&& other) noexcept
{
    if (this == &other) return *this;
    mBuffer = other.mBuffer;
    other.mActive = false;
    return *this;
}

PixelPackBuffer::~PixelPackBuffer()
{
    if (mActive)
    {
        Logging::LogDebug("GLBuffers") << "Deleting pixel pack buffer id: " << mBuffer << " @" << this << "\n";
        glDeleteBuffers(1, &mBuffer);
    }
}

void PixelPackBuffer::BindGL() const
{
    glBindBuffer(GL_PIXEL_PACK_BUFFER, mBuffer);
}

void PixelPackBuffer::UnbindGL() const
{
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

GLuint PixelPackBuffer::GetId() const
{
    return mBuffer;
}

void* PixelPackBuffer::Map(GLenum access) const
{
    return glMapBuffer(GL_PIXEL_PACK_BUFFER, access);
}

void PixelPackBuffer::Unmap() const
{
    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
}

GLuint PixelPackBuffer::GenBufferGL()
{
    GLuint buffer;
    glGenBuffers(1, &buffer);
    return buffer;
}

}
