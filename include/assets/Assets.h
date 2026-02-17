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
/// Single vertex with position, normal, UV, tangent, and bitangent.
struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec2 texCoord;
    Vec3 tangent;    // for normal mapping (TBN matrix)
    Vec3 bitangent;  // for normal mapping (TBN matrix)
};

/// Extended vertex with bone influences for skeletal animation (GPU skinning).
struct SkinnedVertex {
    Vec3 position;
    Vec3 normal;
    Vec2 texCoord;
    Vec3 tangent;
    Vec3 bitangent;
    i32  boneIDs[4]   = { -1, -1, -1, -1 };
    f32  boneWeights[4] = { 0, 0, 0, 0 };
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

    /// Internal OBJ file parser.
    bool LoadOBJ(const std::string& path);
};

// ============================================================================
// Skinned Mesh (for GPU Skeletal Animation)
// ============================================================================
/// A mesh with bone weight data, uploaded to the GPU for hardware skinning.
class SkinnedMesh {
public:
    SkinnedMesh() = default;
    explicit SkinnedMesh(const std::string& name) : m_Name(name) {}

    /// Build from raw skinned vertex/index data.
    void Build(const std::vector<SkinnedVertex>& vertices, const std::vector<u32>& indices);

    void Bind() const;
    void Unbind() const;

    u32 GetVertexCount() const { return static_cast<u32>(m_Vertices.size()); }
    u32 GetIndexCount()  const { return static_cast<u32>(m_Indices.size()); }
    const std::string& GetName() const { return m_Name; }

private:
    std::string              m_Name;
    std::vector<SkinnedVertex> m_Vertices;
    std::vector<u32>         m_Indices;
    u32 m_VAO = 0, m_VBO = 0, m_EBO = 0;
};

// ============================================================================
// Minimal glTF 2.0 Loader
// ============================================================================
/// Loads meshes and skeletal data from .gltf / .glb files.
/// Provides a simplified interface — no external dependencies (Assimp-free).
struct GLTFLoadResult {
    std::vector<SkinnedVertex> vertices;
    std::vector<u32>           indices;
    bool                       hasBones = false;
    // Bone data (names, hierarchy, inverse bind matrices) can be extracted
    // into a Skeleton object externally.
};

/// Load geometry from a glTF/glb file. Returns true on success.
bool LoadGLTF(const std::string& path, GLTFLoadResult& result);

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

// ============================================================================
// Asset Loader  (convenience facade)
// ============================================================================
/// Stateless utility that wraps common load-from-disk patterns.
/// Use AssetManager for caching; use AssetLoader for one-shot loads and
/// format detection.
class AssetLoader {
public:
    /// Detect the asset type from a file extension and load via AssetManager.
    /// Supported extensions:  .png .jpg .bmp .tga  -> Texture
    ///                        .obj .fbx .gltf .glb -> Mesh
    /// Returns true on success.
    static bool LoadAsset(AssetManager& mgr, const std::string& path);

    /// Load a texture and return it directly (bypasses cache).
    static Shared<Texture> LoadTexture(const std::string& path);

    /// Load a 3D model and return it directly (bypasses cache).
    static Shared<Mesh> LoadModel(const std::string& path);

    /// Load a sprite sheet (texture + metadata).  Placeholder for future use.
    static Shared<Texture> LoadSprite(const std::string& path);

    /// Determine the file type from extension.
    enum class FileType { Unknown, Texture, Model, Script, Audio };
    static FileType DetectFileType(const std::string& path);
};

} // namespace gv
