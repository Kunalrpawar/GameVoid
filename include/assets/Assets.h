// ============================================================================
// GameVoid Engine — Asset Management
// ============================================================================
// Handles loading and caching of textures, 3D meshes, and materials.
// ============================================================================
#pragma once

#include "core/Types.h"
#include "core/Math.h"
#include <string>

namespace gv {

// ============================================================================
// Texture
// ============================================================================
/// Represents a 2D texture loaded from disk (PNG, JPG, BMP, etc.).
class Texture {
public:
    Texture() = default;
    explicit Texture(const std::string& path) : m_Path(path) {}

    /// Load the image file and upload to GPU memory (placeholder).
    bool Load(const std::string& path);

    /// Bind to a given texture unit for rendering.
    void Bind(u32 unit = 0) const;
    void Unbind() const;

    u32 GetID()   const { return m_TextureID; }
    u32 GetWidth()  const { return m_Width; }
    u32 GetHeight() const { return m_Height; }
    const std::string& GetPath() const { return m_Path; }

private:
    std::string m_Path;
    u32 m_TextureID = 0;
    u32 m_Width  = 0;
    u32 m_Height = 0;
    u32 m_Channels = 0;
};

// ============================================================================
// Vertex & Mesh
// ============================================================================
/// Single vertex with position, normal, and UV.
struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec2 texCoord;
};

/// A mesh is a collection of vertices and indices uploaded to the GPU.
class Mesh {
public:
    Mesh() = default;
    explicit Mesh(const std::string& name) : m_Name(name) {}

    /// Load from a file (OBJ, FBX, glTF) via an asset importer.
    bool LoadFromFile(const std::string& path);

    /// Build from raw vertex/index data (e.g. procedural geometry).
    void Build(const std::vector<Vertex>& vertices, const std::vector<u32>& indices);

    /// Bind VAO for rendering.
    void Bind() const;
    void Unbind() const;

    u32 GetVertexCount() const { return static_cast<u32>(m_Vertices.size()); }
    u32 GetIndexCount()  const { return static_cast<u32>(m_Indices.size()); }
    const std::string& GetName() const { return m_Name; }

    // ── Built-in primitives (placeholder factories) ────────────────────────
    static Shared<Mesh> CreateCube();
    static Shared<Mesh> CreateSphere(u32 segments = 32, u32 rings = 16);
    static Shared<Mesh> CreatePlane(f32 width = 10.0f, f32 depth = 10.0f);
    static Shared<Mesh> CreateQuad();    // unit quad for 2D / sprites

private:
    std::string         m_Name;
    std::vector<Vertex> m_Vertices;
    std::vector<u32>    m_Indices;

    // GPU handles (OpenGL)
    u32 m_VAO = 0, m_VBO = 0, m_EBO = 0;
};

// ============================================================================
// Material
// ============================================================================
/// A simple PBR-ish material: diffuse/albedo, specular, normal map, etc.
class Material {
public:
    Material() = default;
    explicit Material(const std::string& name) : m_Name(name) {}

    // ── Colour properties ──────────────────────────────────────────────────
    Vec3 albedo      { 1, 1, 1 };   // base colour
    Vec3 specular    { 1, 1, 1 };
    f32  shininess   = 32.0f;
    f32  metallic    = 0.0f;
    f32  roughness   = 0.5f;
    Vec4 tintColour  { 1, 1, 1, 1 };

    // ── Textures ───────────────────────────────────────────────────────────
    Shared<Texture> diffuseMap;
    Shared<Texture> normalMap;
    Shared<Texture> specularMap;

    /// Upload material uniforms to the active shader.
    void Apply() const;

    const std::string& GetName() const { return m_Name; }

private:
    std::string m_Name;
};

// ============================================================================
// Asset Manager
// ============================================================================
/// Central cache so the same texture / mesh is not loaded twice.
class AssetManager {
public:
    AssetManager() = default;

    /// Load (or retrieve from cache) a texture.
    Shared<Texture> LoadTexture(const std::string& path);

    /// Load (or retrieve from cache) a mesh.
    Shared<Mesh> LoadMesh(const std::string& path);

    /// Create a named material (not file-backed, constructed programmatically).
    Shared<Material> CreateMaterial(const std::string& name);

    /// Retrieve a previously created material by name.
    Shared<Material> GetMaterial(const std::string& name) const;

    /// Release all cached resources.
    void Clear();

private:
    std::unordered_map<std::string, Shared<Texture>>  m_Textures;
    std::unordered_map<std::string, Shared<Mesh>>     m_Meshes;
    std::unordered_map<std::string, Shared<Material>> m_Materials;
};

} // namespace gv
