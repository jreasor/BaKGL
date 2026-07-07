#include "graphics/opengl.hpp"

#include "graphics/texture.hpp"
#include "graphics/textureUpload.hpp"
#include "graphics/vramTracker.hpp"
#include "graphics/graphicsConfig.hpp"

#include "com/assert.hpp"
#include "com/logger.hpp"

#include <GL/glew.h>

#include <chrono>
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
    VramTracker::Get().AccountViewport(width, height, 4, "depth");
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
    VramTracker::Get().AccountViewport(width, height, 16, "pick");
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
    const auto maxTexturesCap = GraphicsConfig::Get().GetMaxTextures();
    if (textures.size() > maxTexturesCap)
    {
        // 4K doesn't raise the layer *count* (only per-texel size), but a shared
        // TextureStore that legitimately exceeds the configured ceiling would
        // overflow the GL_TEXTURE_2D_ARRAY. Fail loudly with diagnostics
        // (ROADMAP Task 1.5) instead of the bare "Too many textures" throw. Fix
        // is to split the store or raise Graphics.MaxTextures; allocation below
        // is exact, so raising the ceiling is VRAM-free for stores that don't
        // approach it.
        Logging::LogError(__FUNCTION__) << "Too many textures: store has "
            << textures.size() << " layers, cap MaxTextures=" << maxTexturesCap
            << " (default " << sMaxTextures << ")"
            << " (would overflow GL_TEXTURE_2D_ARRAY)."
            << " Split the TextureStore or raise Graphics.MaxTextures in config.json.\n";
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

    // Allocate exactly as many layers as the store needs (>=1), not the full
    // configured MaxTextures ceiling. The cap is a pure runaway-guard (checked
    // above); small stores (cursor/font/icons, ~1-20 textures) no longer
    // pre-allocate 256 layers of maxDim*maxDim*RGBA8 each. The fill loop below
    // writes layers 0..n-1, all in range. (ROADMAP Task 1.5)
    const auto layerCount = textures.size() > 0
        ? static_cast<GLsizei>(textures.size())
        : GLsizei{1};

    glTexStorage3D(
        mTextureType,
        levels,         // mip levels: 1 for Nearest, full chain for LinearMipmap
        GL_RGBA8,       // Internal format
        maxDim, maxDim, // width,height
        layerCount      // Number of layers (exact, not the configured cap)
    );


    const bool mipmapped = (filter == FilterMode::LinearMipmap);
    VramTracker::Get().Account(
        maxDim,
        static_cast<unsigned>(layerCount),
        mipmapped,
        "upload");

    // Task 3.3 increment A: split the upload cost into three segments so the
    // zone-load hitch can be attributed. PBOs (increment D) only address the
    // glTexSubImage3D segment; glGenerateMipmap and the GetPixel fill are not
    // PBO-acceleratable, so this split is the go/no-go gate for D's value.
    using Clock = std::chrono::steady_clock;
    auto fillTime = Clock::duration{0};
    auto uploadTime = Clock::duration{0};

    const bool rgba8 = GraphicsConfig::Get().GetRGBA8Upload();
    unsigned index = 0;
    for (const auto& tex : textures)
    {
        if (rgba8)
        {
            // Task 3.3-C (opt-in): RGBA8 staging. BuildRgba8Staging tiles the
            // texture into a maxDim*maxDim RGBA8 buffer (4 B/px) via GetPixel's
            // modulo wrap; QuantizeChannel applies the GL spec float->RGBA8
            // conversion, so the uploaded bytes match the GL_FLOAT path
            // exactly (unit-tested). 4x smaller staging buffer + bus, no
            // driver float conversion. Default OFF: in the Debug build the
            // per-channel scalar quantize is ~2.5x slower than the driver's
            // optimized conversion on the fill (the dominant zone-hitch
            // cost), so this path regresses the hitch until a vectorized build
            // or Task 3.3-D's async PBO path hides the fill off the render
            // thread.
            std::vector<std::uint8_t> staging;
            {
                const auto t0 = Clock::now();
                staging = BuildRgba8Staging(tex, maxDim);
                fillTime += Clock::now() - t0;
            }

            {
                const auto t0 = Clock::now();
                glTexSubImage3D(
                    mTextureType,
                    0,                 // Mipmap number
                    0, 0, index,       // xoffset, yoffset, zoffset
                    maxDim, maxDim, 1, // width, height, depth
                    GL_RGBA,           // format
                    GL_UNSIGNED_BYTE,  // type — RGBA8 bytes, no driver float conversion
                    staging.data());   // pointer to data
                uploadTime += Clock::now() - t0;
            }
        }
        else
        {
            // Legacy GL_FLOAT staging (default). Build a vector<glm::vec4>
            // (16 B/px) tiled via GetPixel and let the driver convert floats to
            // RGBA8 during upload. Faster fill than the opt-in RGBA8 path in
            // the unoptimized Debug build (no per-channel scalar quantize);
            // the driver's conversion is optimized/streaming, so this stays
            // the default until 3.3-D's async PBO path moves the RGBA8 fill
            // off the render thread.
            std::vector<glm::vec4> paddedTex(
                maxDim * maxDim,
                glm::vec4{0});
            {
                const auto t0 = Clock::now();
                for (unsigned x = 0; x < maxDim; x++)
                    for (unsigned y = 0; y < maxDim; y++)
                        paddedTex[x + y * maxDim] = tex.GetPixel(x, y);
                fillTime += Clock::now() - t0;
            }

            {
                const auto t0 = Clock::now();
                glTexSubImage3D(
                    mTextureType,
                    0,                 // Mipmap number
                    0, 0, index,       // xoffset, yoffset, zoffset
                    maxDim, maxDim, 1, // width, height, depth
                    GL_RGBA,           // format
                    GL_FLOAT,          // type
                    paddedTex.data()); // pointer to data
                uploadTime += Clock::now() - t0;
            }
        }

        index++;
    }
    
    auto mipmapTime = Clock::duration{0};
    if (filter == FilterMode::LinearMipmap)
    {
        {
            const auto t0 = Clock::now();
            glGenerateMipmap(mTextureType);
            mipmapTime += Clock::now() - t0;
        }
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

    using ms = std::chrono::duration<double, std::milli>;
    Logging::LogDebug("VRAM") << "LoadTexturesGL: maxDim=" << maxDim
        << " layers=" << layerCount
        << " fill=" << ms(fillTime).count() << "ms"
        << " upload=" << ms(uploadTime).count() << "ms"
        << " mipmap=" << ms(mipmapTime).count() << "ms\n";
    
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
