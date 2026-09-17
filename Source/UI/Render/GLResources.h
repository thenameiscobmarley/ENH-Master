#pragma once

#include <juce_opengl/juce_opengl.h>
#include "GLMath.h"

namespace pad::gfx
{
    struct Vertex
    {
        float px, py, pz;
        float nx, ny, nz;
        float u, v;
    };

    /** CPU-side geometry. */
    struct MeshData
    {
        std::vector<Vertex>       vertices;
        std::vector<juce::uint32> indices;

        juce::uint32 addVertex (Vec3 p, Vec3 n, float u, float v)
        {
            vertices.push_back ({ p.x, p.y, p.z, n.x, n.y, n.z, u, v });
            return (juce::uint32) vertices.size() - 1;
        }

        void addTriangle (juce::uint32 a, juce::uint32 b, juce::uint32 c)  { indices.insert (indices.end(), { a, b, c }); }
        void addQuad (juce::uint32 a, juce::uint32 b, juce::uint32 c, juce::uint32 d) { addTriangle (a, b, c); addTriangle (a, c, d); }

        /** Appends another mesh, transforming positions and normals. */
        void append (const MeshData& other, const Mat4& transform = Mat4::identity());

        bool isEmpty() const noexcept { return indices.empty(); }
    };

    /** GPU mesh: VAO + VBO + IBO. Must be used on the GL thread. */
    class GpuMesh
    {
    public:
        GpuMesh() = default;
        ~GpuMesh() { jassert (vao == 0); }

        void upload (const MeshData&);
        void draw() const;
        void release();

        bool isValid() const noexcept { return vao != 0 && count > 0; }

    private:
        GLuint vao = 0, vbo = 0, ibo = 0;
        GLsizei count = 0;

        JUCE_DECLARE_NON_COPYABLE (GpuMesh)
    };

    /** Minimal shader program with fixed attribute slots (aPos=0, aNormal=1, aUV=2). */
    class ShaderProgram
    {
    public:
        ~ShaderProgram() { jassert (program == 0); }

        bool build (const char* vertexSource, const char* fragmentSource, juce::String& errorOut);
        void use() const;
        void release();

        GLint uniform (const char* name);

        void set (const char* name, float v);
        void set (const char* name, int v);
        void set (const char* name, Vec3 v);
        void set (const char* name, float a, float b);
        void set (const char* name, float a, float b, float c, float d);
        void set (const char* name, const Mat4& m);

    private:
        GLuint program = 0;
        std::unordered_map<std::string, GLint> uniformCache;
    };

    /** 2D texture uploaded from raw 8-bit channel data, with mipmaps. */
    class Texture2D
    {
    public:
        ~Texture2D() { jassert (id == 0); }

        /** channels: 1 (R8) or 4 (RGBA8). */
        void upload (const juce::uint8* data, int width, int height, int channels, bool mipmaps, int anisotropy);
        void bind (int unit) const;
        void release();

        bool isValid() const noexcept { return id != 0; }

    private:
        GLuint id = 0;
        int w = 0, h = 0, ch = 0;
    };
}
