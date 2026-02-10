// ============================================================================
// GameVoid Engine — Asset Manager Implementation
// ============================================================================
#include "assets/Assets.h"

namespace gv {

// ── Texture ────────────────────────────────────────────────────────────────

bool Texture::Load(const std::string& path) {
    m_Path = path;
    // In production: stb_image or similar
    //   unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 0);
    //   glGenTextures(1, &m_TextureID);
    //   glBindTexture(GL_TEXTURE_2D, m_TextureID);
    //   glTexImage2D(...)
    //   stbi_image_free(data);
    GV_LOG_INFO("Texture loaded (placeholder): " + path);
    return true;
}

void Texture::Bind(u32 unit) const {
    // glActiveTexture(GL_TEXTURE0 + unit);
    // glBindTexture(GL_TEXTURE_2D, m_TextureID);
    (void)unit;
}

void Texture::Unbind() const {
    // glBindTexture(GL_TEXTURE_2D, 0);
}

// ── Mesh ───────────────────────────────────────────────────────────────────

bool Mesh::LoadFromFile(const std::string& path) {
    // In production: use Assimp
    //   Assimp::Importer importer;
    //   const aiScene* scene = importer.ReadFile(path, ...);
    //   extract vertices, normals, UVs, indices → Build(...)
    GV_LOG_INFO("Mesh loaded from file (placeholder): " + path);
    return true;
}

void Mesh::Build(const std::vector<Vertex>& vertices, const std::vector<u32>& indices) {
    m_Vertices = vertices;
    m_Indices  = indices;
    // glGenVertexArrays(1, &m_VAO);
    // glGenBuffers(1, &m_VBO);
    // glGenBuffers(1, &m_EBO);
    // upload to GPU …
    GV_LOG_DEBUG("Mesh '" + m_Name + "' built: " + std::to_string(vertices.size()) +
                 " verts, " + std::to_string(indices.size()) + " indices.");
}

void Mesh::Bind()   const { /* glBindVertexArray(m_VAO); */ }
void Mesh::Unbind() const { /* glBindVertexArray(0); */ }

Shared<Mesh> Mesh::CreateCube() {
    auto mesh = MakeShared<Mesh>("Cube");
    // 24 vertices (4 per face × 6 faces), 36 indices
    // Placeholder — populate with unit-cube geometry.
    GV_LOG_DEBUG("Primitive 'Cube' created.");
    return mesh;
}

Shared<Mesh> Mesh::CreateSphere(u32 segments, u32 rings) {
    auto mesh = MakeShared<Mesh>("Sphere");
    (void)segments; (void)rings;
    GV_LOG_DEBUG("Primitive 'Sphere' created.");
    return mesh;
}

Shared<Mesh> Mesh::CreatePlane(f32 width, f32 depth) {
    auto mesh = MakeShared<Mesh>("Plane");
    (void)width; (void)depth;
    GV_LOG_DEBUG("Primitive 'Plane' created.");
    return mesh;
}

Shared<Mesh> Mesh::CreateQuad() {
    auto mesh = MakeShared<Mesh>("Quad");
    GV_LOG_DEBUG("Primitive 'Quad' created.");
    return mesh;
}

// ── Material ───────────────────────────────────────────────────────────────

void Material::Apply() const {
    // Bind textures, upload uniform values to the active shader.
    // shader.SetVec3("u_Albedo", albedo);
    // shader.SetFloat("u_Metallic", metallic);
    // shader.SetFloat("u_Roughness", roughness);
    // if (diffuseMap)  diffuseMap->Bind(0);
    // if (normalMap)   normalMap->Bind(1);
    // if (specularMap) specularMap->Bind(2);
}

// ── AssetManager ───────────────────────────────────────────────────────────

Shared<Texture> AssetManager::LoadTexture(const std::string& path) {
    auto it = m_Textures.find(path);
    if (it != m_Textures.end()) return it->second;

    auto tex = MakeShared<Texture>(path);
    tex->Load(path);
    m_Textures[path] = tex;
    return tex;
}

Shared<Mesh> AssetManager::LoadMesh(const std::string& path) {
    auto it = m_Meshes.find(path);
    if (it != m_Meshes.end()) return it->second;

    auto mesh = MakeShared<Mesh>(path);
    mesh->LoadFromFile(path);
    m_Meshes[path] = mesh;
    return mesh;
}

Shared<Material> AssetManager::CreateMaterial(const std::string& name) {
    auto mat = MakeShared<Material>(name);
    m_Materials[name] = mat;
    return mat;
}

Shared<Material> AssetManager::GetMaterial(const std::string& name) const {
    auto it = m_Materials.find(name);
    return (it != m_Materials.end()) ? it->second : nullptr;
}

void AssetManager::Clear() {
    m_Textures.clear();
    m_Meshes.clear();
    m_Materials.clear();
    GV_LOG_INFO("AssetManager — all cached resources released.");
}

// ── AssetLoader ────────────────────────────────────────────────────────────

AssetLoader::FileType AssetLoader::DetectFileType(const std::string& path) {
    auto dot = path.rfind('.');
    if (dot == std::string::npos) return FileType::Unknown;
    std::string ext = path.substr(dot);
    // lowercase
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
        ext == ".bmp" || ext == ".tga" || ext == ".hdr")
        return FileType::Texture;
    if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb")
        return FileType::Model;
    if (ext == ".lua" || ext == ".py" || ext == ".js")
        return FileType::Script;
    if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac")
        return FileType::Audio;
    return FileType::Unknown;
}

bool AssetLoader::LoadAsset(AssetManager& mgr, const std::string& path) {
    FileType ft = DetectFileType(path);
    switch (ft) {
        case FileType::Texture: {
            auto tex = mgr.LoadTexture(path);
            GV_LOG_INFO("AssetLoader — loaded texture: " + path);
            return tex != nullptr;
        }
        case FileType::Model: {
            auto mesh = mgr.LoadMesh(path);
            GV_LOG_INFO("AssetLoader — loaded model: " + path);
            return mesh != nullptr;
        }
        default:
            GV_LOG_WARN("AssetLoader — unsupported or unknown file type: " + path);
            return false;
    }
}

Shared<Texture> AssetLoader::LoadTexture(const std::string& path) {
    auto tex = MakeShared<Texture>(path);
    if (!tex->Load(path)) {
        GV_LOG_WARN("AssetLoader — failed to load texture: " + path);
        return nullptr;
    }
    GV_LOG_INFO("AssetLoader — texture loaded (no cache): " + path);
    return tex;
}

Shared<Mesh> AssetLoader::LoadModel(const std::string& path) {
    auto mesh = MakeShared<Mesh>(path);
    if (!mesh->LoadFromFile(path)) {
        GV_LOG_WARN("AssetLoader — failed to load model: " + path);
        return nullptr;
    }
    GV_LOG_INFO("AssetLoader — model loaded (no cache): " + path);
    return mesh;
}

Shared<Texture> AssetLoader::LoadSprite(const std::string& path) {
    // Sprite loading is essentially texture loading with future metadata support
    GV_LOG_INFO("AssetLoader — sprite load (delegates to texture): " + path);
    return LoadTexture(path);
}

} // namespace gv
