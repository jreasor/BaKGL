#pragma once

#include "graphics/texture.hpp"

#include "com/logger.hpp"
#include "com/strongType.hpp"

#include <GL/glew.h>

namespace Graphics {

using GLLocation = StrongType<unsigned, struct GLLocationTag>;
using GLElems    = StrongType<unsigned, struct GLElemsTag>;
using GLDataType = StrongType<unsigned, struct GLDataTypeTag>;
using GLBufferId = StrongType<unsigned, struct GLBufferIdTag>;

static constexpr auto GLNullLocation = GLLocation{static_cast<unsigned>(-1)};

enum class GLBindPoint
{
    ArrayBuffer,
    ElementArrayBuffer,
    TextureBuffer
};

GLenum ToGlEnum(GLBindPoint);

enum class GLUpdateType
{
    StaticDraw,
    DynamicDraw 
};

GLenum ToGlEnum(GLUpdateType);

struct GLBuffer
{
    // Location in the shader
    GLLocation mLocation;
    // Elements per object (e.g. color = 4 floats)
    GLElems mElems;
    // GLBindPoint (ARRAY, ELEMENT_ARRAY, etc)
    GLBindPoint mGLBindPoint;
    // e.g. GL_FLOAT, or GL_UNSIGNED
    GLDataType mDataType;
    // e.g. StaticDraw or DynamicDraw
    GLUpdateType mUpdateType;
    // GL assigned buffer id
    GLBufferId mBuffer;
};

class VertexArrayObject
{
public:
    VertexArrayObject();

    VertexArrayObject(const VertexArrayObject&) = delete;
    VertexArrayObject& operator=(const VertexArrayObject&) = delete;

    VertexArrayObject(VertexArrayObject&& other) noexcept;
    VertexArrayObject& operator=(VertexArrayObject&& other) noexcept;

    ~VertexArrayObject();

    void BindGL() const;
    void UnbindGL() const;

private:
    static GLuint GenVertexArrayGL();

    GLuint mVertexArrayId;
    bool mActive;
};

class GLBuffers
{
public:
    GLBuffers();

    GLBuffers(GLBuffers&& other) noexcept;
    GLBuffers& operator=(GLBuffers&& other) noexcept;

    GLBuffers(const GLBuffers&) = delete;
    GLBuffers& operator=(const GLBuffers&) = delete;

    ~GLBuffers();

    const auto& GetGLBuffer(const std::string& name) const
    {
        if (!mBuffers.contains(name))
        {
            Logging::LogDebug("GLBuffers") << "No buffer named: " << name << std::endl;
            throw std::runtime_error("Request for nonexistent GL Buffer");
        }
        return mBuffers.find(name)->second;
    }

    void AddBuffer(
        const std::string& name,
        GLLocation,
        GLElems,
        GLDataType,
        GLBindPoint,
        GLUpdateType);

    template <typename T>
    void AddStaticArrayBuffer(
        const std::string& name,
        GLLocation location)
    {
        static_assert(std::is_same_v<typename T::value_type, float> || std::is_same_v<typename T::value_type, unsigned>);
        constexpr auto dataType = std::invoke([](){
            if constexpr (std::is_same_v<typename T::value_type, float>) return GLDataType{GL_FLOAT};
            else return GLDataType{GL_UNSIGNED_INT};
        });
        AddBuffer(name, location, GLElems{T::length()}, dataType, GLBindPoint::ArrayBuffer, GLUpdateType::StaticDraw);
    }

    void AddElementBuffer(const std::string& name);
    void AddTextureBuffer(const std::string& name);
    
    static GLBufferId GenBufferGL();

    template <typename T>
    void LoadBufferDataGL(
        const std::string& name,
        const std::vector<T>& data)
    {
        LoadBufferDataGL(GetGLBuffer(name), data);
    }

    template <typename T>
    void LoadBufferDataGL(
        const GLBuffer& buffer,
        const std::vector<T>& data)
    {
        glBindBuffer(
            ToGlEnum(buffer.mGLBindPoint),
            buffer.mBuffer.mValue);
        glBufferData(
            ToGlEnum(buffer.mGLBindPoint),
            data.size() * sizeof(T),
            &data.front(),
            ToGlEnum(buffer.mUpdateType));
    }

    template <typename T>
    void ModifyBufferDataGL(
        const std::string& name,
        GLenum target,
        unsigned offset,
        const std::vector<T>& data)
    {
        glBindBuffer(target, GetGLBuffer(name).mBuffer.mValue);
        glBufferSubData(
            target,
            offset,
            data.size() * sizeof(T),
            &data.front());
    }

    void BindAttribArrayGL(const GLBuffer&);
    
    void BindArraysGL();

    void SetAttribDivisor(const std::string& name, unsigned divisor)
    {
        const auto location = GetGLBuffer(name).mLocation;
        glEnableVertexAttribArray(location.mValue);
        glVertexAttribDivisor(location.mValue, divisor);
    }

private:
    std::unordered_map<std::string, GLBuffer> mBuffers;

    GLuint mElementBuffer;

    // disable when moving from
    bool mActive;
};

enum class FilterMode
{
    Nearest,
    LinearMipmap,
};

// Wrap mode for the LinearMipmap upload path (Task 3.1). GUI screens don't tile
// -> ClampToEdge (the TextureBuffer default, so the Sprites/backgrounds path from
// increment 1 is unchanged). Tiled terrain/world passes Repeat. The Nearest path
// keeps its hardcoded GL_REPEAT, so WrapMode only affects LinearMipmap callers.
enum class WrapMode
{
    Repeat,
    ClampToEdge,
};

class TextureBuffer
{
public:
    static constexpr auto sMaxTextures = 256;

    TextureBuffer(GLenum textureType);
    TextureBuffer(TextureBuffer&& other) noexcept;
    TextureBuffer& operator=(TextureBuffer&& other) noexcept;
    TextureBuffer(const TextureBuffer&) = delete;
    TextureBuffer& operator=(const TextureBuffer&) = delete;

    ~TextureBuffer();
    

    void BindGL() const;
    void UnbindGL() const;
    GLuint GetId() const;
    
    void MakeDepthBuffer(unsigned width, unsigned height);
    void MakePickBuffer(unsigned width, unsigned height);
    void MakeTexture2DArray();

    void LoadTexturesGL(
        const std::vector<Texture>& textures,
        unsigned maxDim,
        FilterMode filter = FilterMode::Nearest,
        WrapMode wrap = WrapMode::ClampToEdge);

private:
    GLuint mTextureBuffer;
    GLenum mTextureType;
    bool mActive;
};

class PixelPackBuffer
{
public:
    explicit PixelPackBuffer();
    PixelPackBuffer(PixelPackBuffer&& other) noexcept;
    PixelPackBuffer& operator=(PixelPackBuffer&& other) noexcept;
    PixelPackBuffer(const PixelPackBuffer&) = delete;
    PixelPackBuffer& operator=(const PixelPackBuffer&) = delete;

    ~PixelPackBuffer();

    void BindGL() const;
    void UnbindGL() const;
    GLuint GetId() const;

    template <typename T>
    void Allocate(GLenum usage)
    {
        BindGL();
        glBufferData(GL_PIXEL_PACK_BUFFER, sizeof(T), nullptr, usage);
        UnbindGL();
    }

    void* Map(GLenum access) const;
    void Unmap() const;

private:
    static GLuint GenBufferGL();

    GLuint mBuffer;
    bool mActive;
};

// Task 3.3-D: streaming upload PBO. Mirrors PixelPackBuffer (above) but targets
// GL_PIXEL_UNPACK_BUFFER and takes a byte size (the upload path sizes the PBO
// from maxDim*maxDim*4, not from a struct type). No Map/Unmap: the D path fills
// via SubData and reads via glTexSubImage3D(data=nullptr) -- a pure
// driver-managed DMA path with no client-pointer mapping and no fence sync.
class PixelUnpackBuffer
{
public:
    explicit PixelUnpackBuffer();
    PixelUnpackBuffer(PixelUnpackBuffer&& other) noexcept;
    PixelUnpackBuffer& operator=(PixelUnpackBuffer&& other) noexcept;
    PixelUnpackBuffer(const PixelUnpackBuffer&) = delete;
    PixelUnpackBuffer& operator=(const PixelUnpackBuffer&) = delete;

    ~PixelUnpackBuffer();

    void BindGL() const;
    void UnbindGL() const;
    GLuint GetId() const;

    // Size-based (byte) orphan + reserve. glBufferData with a null data pointer
    // re-specifies the store, orphans any prior contents and lets the driver
    // hand back fresh storage instead of syncing on a buffer the GPU may still
    // be reading from the previous layer. Called per layer for the streaming
    // orphan-and-refill pattern. Mirrors PixelPackBuffer::Allocate (which is
    // sized by sizeof(T)); the upload path sizes the PBO by byte count directly.
    void Allocate(GLsizeiptr size, GLenum usage);

    // Stage `size` bytes from `data` at `offset` into the bound PBO (no
    // re-specify -- fills storage reserved by Allocate). Paired with a
    // following glTexSubImage3D(data=nullptr) read from the bound PBO.
    void SubData(GLintptr offset, GLsizeiptr size, const void* data);

private:
    static GLuint GenBufferGL();

    GLuint mBuffer;
    bool mActive;
};

// RAII bind/unbind for the unpack PBO around the glTexSubImage3D(data=nullptr)
// read. That read pulls texels from whichever buffer is bound to
// GL_PIXEL_UNPACK_BUFFER, so the PBO must be bound for the call -- and must be
// unbound on scope exit: a left-bound unpack PBO would make a later
// client-pointer glTexSubImage3D interpret the client pointer as a byte offset
// into the still-bound PBO (corruption). Construct binds, destruct unbinds.
class BoundPixelUnpackBuffer
{
public:
    explicit BoundPixelUnpackBuffer(const PixelUnpackBuffer& pbo);
    ~BoundPixelUnpackBuffer();
    BoundPixelUnpackBuffer(const BoundPixelUnpackBuffer&) = delete;
    BoundPixelUnpackBuffer& operator=(const BoundPixelUnpackBuffer&) = delete;
private:
    const PixelUnpackBuffer& mPbo;
};

}

