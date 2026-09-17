#include "GLResources.h"

using namespace juce::gl;

namespace pad::gfx
{
    //==============================================================================
    void MeshData::append (const MeshData& other, const Mat4& transform)
    {
        const auto base = (juce::uint32) vertices.size();

        for (auto v : other.vertices)
        {
            const auto p = transform.transformPoint ({ v.px, v.py, v.pz });
            const auto n = normalise (transform.transformDir ({ v.nx, v.ny, v.nz }));
            vertices.push_back ({ p.x, p.y, p.z, n.x, n.y, n.z, v.u, v.v });
        }

        for (auto i : other.indices)
            indices.push_back (base + i);
    }

    //==============================================================================
    void GpuMesh::upload (const MeshData& data)
    {
        release();

        if (data.isEmpty())
            return;

        glGenVertexArrays (1, &vao);
        glBindVertexArray (vao);

        glGenBuffers (1, &vbo);
        glBindBuffer (GL_ARRAY_BUFFER, vbo);
        glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (data.vertices.size() * sizeof (Vertex)), data.vertices.data(), GL_STATIC_DRAW);

        glGenBuffers (1, &ibo);
        glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData (GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr) (data.indices.size() * sizeof (juce::uint32)), data.indices.data(), GL_STATIC_DRAW);

        const auto stride = (GLsizei) sizeof (Vertex);
        glEnableVertexAttribArray (0);
        glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, stride, (void*) offsetof (Vertex, px));
        glEnableVertexAttribArray (1);
        glVertexAttribPointer (1, 3, GL_FLOAT, GL_FALSE, stride, (void*) offsetof (Vertex, nx));
        glEnableVertexAttribArray (2);
        glVertexAttribPointer (2, 2, GL_FLOAT, GL_FALSE, stride, (void*) offsetof (Vertex, u));

        glBindVertexArray (0);
        count = (GLsizei) data.indices.size();
    }

    void GpuMesh::draw() const
    {
        if (! isValid())
            return;

        glBindVertexArray (vao);
        glDrawElements (GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
    }

    void GpuMesh::release()
    {
        if (ibo != 0) glDeleteBuffers (1, &ibo);
        if (vbo != 0) glDeleteBuffers (1, &vbo);
        if (vao != 0) glDeleteVertexArrays (1, &vao);
        vao = vbo = ibo = 0;
        count = 0;
    }

    //==============================================================================
    static GLuint compileStage (GLenum type, const char* source, juce::String& error)
    {
        const GLuint shader = glCreateShader (type);
        glShaderSource (shader, 1, &source, nullptr);
        glCompileShader (shader);

        GLint ok = 0;
        glGetShaderiv (shader, GL_COMPILE_STATUS, &ok);

        if (ok == 0)
        {
            char log[2048] = {};
            glGetShaderInfoLog (shader, sizeof (log) - 1, nullptr, log);
            error << (type == GL_VERTEX_SHADER ? "vertex: " : "fragment: ") << log;
            glDeleteShader (shader);
            return 0;
        }

        return shader;
    }

    bool ShaderProgram::build (const char* vs, const char* fs, juce::String& error)
    {
        release();

        const auto v = compileStage (GL_VERTEX_SHADER, vs, error);
        const auto f = compileStage (GL_FRAGMENT_SHADER, fs, error);

        if (v == 0 || f == 0)
        {
            if (v != 0) glDeleteShader (v);
            if (f != 0) glDeleteShader (f);
            return false;
        }

        program = glCreateProgram();
        glAttachShader (program, v);
        glAttachShader (program, f);
        glBindAttribLocation (program, 0, "aPos");
        glBindAttribLocation (program, 1, "aNormal");
        glBindAttribLocation (program, 2, "aUV");
        glBindFragDataLocation (program, 0, "fragColor");
        glLinkProgram (program);
        glDeleteShader (v);
        glDeleteShader (f);

        GLint ok = 0;
        glGetProgramiv (program, GL_LINK_STATUS, &ok);

        if (ok == 0)
        {
            char log[2048] = {};
            glGetProgramInfoLog (program, sizeof (log) - 1, nullptr, log);
            error << "link: " << log;
            release();
            return false;
        }

        return true;
    }

    void ShaderProgram::use() const { glUseProgram (program); }

    void ShaderProgram::release()
    {
        if (program != 0)
            glDeleteProgram (program);

        program = 0;
        uniformCache.clear();
    }

    GLint ShaderProgram::uniform (const char* name)
    {
        auto it = uniformCache.find (name);
        if (it != uniformCache.end())
            return it->second;

        const auto loc = glGetUniformLocation (program, name);
        uniformCache.emplace (name, loc);
        return loc;
    }

    void ShaderProgram::set (const char* n, float v)            { glUniform1f (uniform (n), v); }
    void ShaderProgram::set (const char* n, int v)              { glUniform1i (uniform (n), v); }
    void ShaderProgram::set (const char* n, Vec3 v)             { glUniform3f (uniform (n), v.x, v.y, v.z); }
    void ShaderProgram::set (const char* n, float a, float b)   { glUniform2f (uniform (n), a, b); }
    void ShaderProgram::set (const char* n, float a, float b, float c, float d) { glUniform4f (uniform (n), a, b, c, d); }
    void ShaderProgram::set (const char* n, const Mat4& m)      { glUniformMatrix4fv (uniform (n), 1, GL_FALSE, m.m.data()); }
    void ShaderProgram::setArray (const char* n, const float* v, int count) { glUniform1fv (uniform (n), count, v); }

    //==============================================================================
    void Texture2D::upload (const juce::uint8* data, int width, int height, int channels, bool mipmaps, int anisotropy)
    {
        const bool sameShape = (id != 0 && width == w && height == h && channels == ch);

        if (! sameShape)
        {
            release();
            glGenTextures (1, &id);
        }

        w = width; h = height; ch = channels;

        glBindTexture (GL_TEXTURE_2D, id);
        glPixelStorei (GL_UNPACK_ALIGNMENT, 1);

        const auto internalFormat = channels == 1 ? (GLenum) GL_R8 : (GLenum) GL_RGBA8;
        const auto format         = channels == 1 ? (GLenum) GL_RED : (GLenum) GL_RGBA;

        if (sameShape)
            glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, width, height, format, GL_UNSIGNED_BYTE, data);
        else
            glTexImage2D (GL_TEXTURE_2D, 0, (GLint) internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data);

        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        if (mipmaps)
        {
            glGenerateMipmap (GL_TEXTURE_2D);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

            if (anisotropy > 1)
            {
                constexpr GLenum maxAnisotropyExt = 0x84FE; // GL_TEXTURE_MAX_ANISOTROPY(_EXT), core since 4.6
                glTexParameterf (GL_TEXTURE_2D, maxAnisotropyExt, (GLfloat) anisotropy);
                glGetError(); // harmless if unsupported
            }
        }
        else
        {
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }
    }

    void Texture2D::bind (int unit) const
    {
        glActiveTexture ((GLenum) ((int) GL_TEXTURE0 + unit));
        glBindTexture (GL_TEXTURE_2D, id);
    }

    void Texture2D::release()
    {
        if (id != 0)
            glDeleteTextures (1, &id);
        id = 0;
    }
}
