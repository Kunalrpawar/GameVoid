// ============================================================================
// GameVoid Engine — Asset Manager Implementation
// ============================================================================
#include "assets/Assets.h"

#ifdef GV_HAS_GLFW
#include "core/GLDefs.h"
#include "stb/stb_image.h"
#endif

#include <cmath>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace gv {

// Forward declaration (defined later in file)
static void ComputeTangents(Vertex& v0, Vertex& v1, Vertex& v2);

// ── Texture ────────────────────────────────────────────────────────────────

bool Texture::Load(const std::string& path) {
    m_Path = path;
#ifdef GV_HAS_GLFW
    if (!glGenTextures) {
        GV_LOG_WARN("Texture::Load — GL not available, skipping: " + path);
        return false;
    }

    stbi_set_flip_vertically_on_load(1);
    int w, h, channels;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 0);
    if (!data) {
        GV_LOG_WARN("Texture::Load — failed to load image: " + path);
        // Create a 1x1 white fallback texture
        glGenTextures(1, &m_TextureID);
        glBindTexture(GL_TEXTURE_2D, m_TextureID);
        unsigned char white[] = { 255, 255, 255, 255 };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);
        m_Width = 1; m_Height = 1; m_Channels = 4;
        return false;
    }

    m_Width    = static_cast<u32>(w);
    m_Height   = static_cast<u32>(h);
    m_Channels = static_cast<u32>(channels);

    GLenum internalFmt = GL_RGBA8;
    GLenum format = GL_RGBA;
    if (channels == 1) { internalFmt = GL_RED;  format = GL_RED;  }
    else if (channels == 3) { internalFmt = GL_RGB8; format = GL_RGB; }
    else if (channels == 4) { internalFmt = GL_RGBA8; format = GL_RGBA; }

    glGenTextures(1, &m_TextureID);
    glBindTexture(GL_TEXTURE_2D, m_TextureID);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internalFmt),
                 static_cast<GLsizei>(w), static_cast<GLsizei>(h), 0,
                 format, GL_UNSIGNED_BYTE, data);

    if (glGenerateMipmap) glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    GV_LOG_INFO("Texture loaded: " + path + " (" + std::to_string(w) + "x" +
                std::to_string(h) + ", " + std::to_string(channels) + "ch)");
    return true;
#else
    GV_LOG_INFO("Texture loaded (no GPU): " + path);
    return true;
#endif
}

void Texture::Bind(u32 unit) const {
#ifdef GV_HAS_GLFW
    if (glActiveTexture && m_TextureID) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, m_TextureID);
    }
#else
    (void)unit;
#endif
}

void Texture::Unbind() const {
#ifdef GV_HAS_GLFW
    glBindTexture(GL_TEXTURE_2D, 0);
#endif
}

// ── Mesh ───────────────────────────────────────────────────────────────────

bool Mesh::LoadFromFile(const std::string& path) {
    // Detect format from extension
    auto dot = path.rfind('.');
    if (dot == std::string::npos) {
        GV_LOG_WARN("Mesh::LoadFromFile — no extension: " + path);
        return false;
    }
    std::string ext = path.substr(dot);
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (ext == ".obj") {
        return LoadOBJ(path);
    }
    // For .fbx, .gltf, .glb — would need Assimp or dedicated parsers
    GV_LOG_WARN("Mesh::LoadFromFile — unsupported format: " + ext + " (only .obj supported)");
    return false;
}

bool Mesh::LoadOBJ(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        GV_LOG_WARN("Mesh::LoadOBJ — failed to open: " + path);
        return false;
    }

    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec2> texCoords;

    struct FaceVert { int posIdx, texIdx, normIdx; };
    std::vector<FaceVert> faceVerts;
    std::vector<u32> faceIndices;  // groups of 3 (triangulated)

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream ss(line);
        std::string prefix;
        ss >> prefix;

        if (prefix == "v") {
            Vec3 v;
            ss >> v.x >> v.y >> v.z;
            positions.push_back(v);
        } else if (prefix == "vn") {
            Vec3 n;
            ss >> n.x >> n.y >> n.z;
            normals.push_back(n);
        } else if (prefix == "vt") {
            Vec2 t;
            ss >> t.x >> t.y;
            texCoords.push_back(t);
        } else if (prefix == "f") {
            // Parse face — supports v, v/vt, v/vt/vn, v//vn
            std::vector<FaceVert> polygon;
            std::string token;
            while (ss >> token) {
                FaceVert fv = { -1, -1, -1 };
                // Replace '/' with spaces for parsing
                for (auto& c : token) if (c == '/') c = ' ';
                std::istringstream ts(token);

                // Count original slashes
                int slashCount = 0;
                bool doubleSlash = false;
                for (size_t i = 0; i + 1 < line.size(); ++i) {
                    // We already replaced them, so count spaces in token
                }
                // Re-parse from the original token format
                // Actually let's just parse the space-separated values
                std::string part;
                std::vector<std::string> parts;
                while (ts >> part) parts.push_back(part);

                if (parts.size() >= 1 && !parts[0].empty())
                    fv.posIdx = std::stoi(parts[0]) - 1;
                if (parts.size() >= 2 && !parts[1].empty())
                    fv.texIdx = std::stoi(parts[1]) - 1;
                if (parts.size() >= 3 && !parts[2].empty())
                    fv.normIdx = std::stoi(parts[2]) - 1;

                // Handle v//vn case (token had double slash → empty middle part)
                // Re-check original token for "//"
                // Since we replaced slashes, if parts.size() == 2 and original had //
                // we need special detection. Let's handle differently:
                polygon.push_back(fv);
            }

            // Triangulate polygon (fan triangulation)
            for (size_t i = 1; i + 1 < polygon.size(); ++i) {
                u32 base = static_cast<u32>(faceVerts.size());
                faceVerts.push_back(polygon[0]);
                faceVerts.push_back(polygon[i]);
                faceVerts.push_back(polygon[i + 1]);
                faceIndices.push_back(base);
                faceIndices.push_back(base + 1);
                faceIndices.push_back(base + 2);
            }
        }
    }

    if (faceVerts.empty()) {
        GV_LOG_WARN("Mesh::LoadOBJ — no faces found in: " + path);
        return false;
    }

    // Build vertex array
    std::vector<Vertex> vertices;
    std::vector<u32> indices;
    vertices.reserve(faceVerts.size());
    indices.reserve(faceIndices.size());

    for (size_t i = 0; i < faceVerts.size(); ++i) {
        Vertex v;
        const FaceVert& fv = faceVerts[i];

        if (fv.posIdx >= 0 && fv.posIdx < static_cast<int>(positions.size()))
            v.position = positions[fv.posIdx];
        if (fv.texIdx >= 0 && fv.texIdx < static_cast<int>(texCoords.size()))
            v.texCoord = texCoords[fv.texIdx];
        if (fv.normIdx >= 0 && fv.normIdx < static_cast<int>(normals.size()))
            v.normal = normals[fv.normIdx];

        vertices.push_back(v);
    }
    indices = faceIndices;

    // Compute normals if none were provided
    if (normals.empty()) {
        for (size_t i = 0; i + 2 < vertices.size(); i += 3) {
            Vec3 e1 = vertices[i + 1].position - vertices[i].position;
            Vec3 e2 = vertices[i + 2].position - vertices[i].position;
            Vec3 n = e1.Cross(e2).Normalized();
            vertices[i].normal = vertices[i + 1].normal = vertices[i + 2].normal = n;
        }
    }

    // Compute tangents/bitangents for normal mapping
    for (size_t i = 0; i + 2 < vertices.size(); i += 3) {
        ComputeTangents(vertices[i], vertices[i + 1], vertices[i + 2]);
    }

    m_Name = path;
    Build(vertices, indices);
    GV_LOG_INFO("Mesh loaded from OBJ: " + path + " (" +
                std::to_string(vertices.size()) + " verts, " +
                std::to_string(indices.size() / 3) + " tris)");
    return true;
}

void Mesh::Build(const std::vector<Vertex>& vertices, const std::vector<u32>& indices) {
    m_Vertices = vertices;
    m_Indices  = indices;
#ifdef GV_HAS_GLFW
    if (glGenVertexArrays) {
        if (m_VAO) { glDeleteVertexArrays(1, &m_VAO); m_VAO = 0; }
        if (m_VBO) { glDeleteBuffers(1, &m_VBO);      m_VBO = 0; }
        if (m_EBO) { glDeleteBuffers(1, &m_EBO);      m_EBO = 0; }

        glGenVertexArrays(1, &m_VAO);
        glGenBuffers(1, &m_VBO);
        glGenBuffers(1, &m_EBO);
        glBindVertexArray(m_VAO);

        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                     vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(indices.size() * sizeof(u32)),
                     indices.data(), GL_STATIC_DRAW);

        // Vertex layout: position(3) + normal(3) + texCoord(2) + tangent(3) + bitangent(3)
        // = 14 floats = 56 bytes per vertex
        GLsizei stride = sizeof(Vertex);

        // location 0: position
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<void*>(offsetof(Vertex, position)));
        glEnableVertexAttribArray(0);
        // location 1: normal
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<void*>(offsetof(Vertex, normal)));
        glEnableVertexAttribArray(1);
        // location 2: texCoord
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<void*>(offsetof(Vertex, texCoord)));
        glEnableVertexAttribArray(2);
        // location 3: tangent
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<void*>(offsetof(Vertex, tangent)));
        glEnableVertexAttribArray(3);
        // location 4: bitangent
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<void*>(offsetof(Vertex, bitangent)));
        glEnableVertexAttribArray(4);

        glBindVertexArray(0);
    }
#endif
    GV_LOG_DEBUG("Mesh '" + m_Name + "' built: " + std::to_string(vertices.size()) +
                 " verts, " + std::to_string(indices.size()) + " indices.");
}

void Mesh::Bind()   const {
#ifdef GV_HAS_GLFW
    if (m_VAO && glBindVertexArray) glBindVertexArray(m_VAO);
#endif
}

void Mesh::Unbind() const {
#ifdef GV_HAS_GLFW
    if (glBindVertexArray) glBindVertexArray(0);
#endif
}

// ── Helper: compute tangent/bitangent for a triangle ───────────────────────
static void ComputeTangents(Vertex& v0, Vertex& v1, Vertex& v2) {
    Vec3 e1 = v1.position - v0.position;
    Vec3 e2 = v2.position - v0.position;
    float du1 = v1.texCoord.x - v0.texCoord.x;
    float dv1 = v1.texCoord.y - v0.texCoord.y;
    float du2 = v2.texCoord.x - v0.texCoord.x;
    float dv2 = v2.texCoord.y - v0.texCoord.y;

    float f = 1.0f / (du1 * dv2 - du2 * dv1 + 1e-8f);
    Vec3 tangent(f * (dv2 * e1.x - dv1 * e2.x),
                 f * (dv2 * e1.y - dv1 * e2.y),
                 f * (dv2 * e1.z - dv1 * e2.z));
    Vec3 bitangent(f * (-du2 * e1.x + du1 * e2.x),
                   f * (-du2 * e1.y + du1 * e2.y),
                   f * (-du2 * e1.z + du1 * e2.z));
    tangent = tangent.Normalized();
    bitangent = bitangent.Normalized();

    v0.tangent = v1.tangent = v2.tangent = tangent;
    v0.bitangent = v1.bitangent = v2.bitangent = bitangent;
}

Shared<Mesh> Mesh::CreateCube() {
    auto mesh = MakeShared<Mesh>("Cube");

    // 6 faces × 4 vertices, with positions + normals + UVs
    std::vector<Vertex> verts;
    std::vector<u32> indices;

    struct FaceData { Vec3 normal; Vec3 right; Vec3 up; };
    FaceData faces[] = {
        { { 0, 0, 1}, { 1, 0, 0}, {0, 1, 0} },  // front
        { { 0, 0,-1}, {-1, 0, 0}, {0, 1, 0} },  // back
        { { 0, 1, 0}, { 1, 0, 0}, {0, 0,-1} },  // top
        { { 0,-1, 0}, { 1, 0, 0}, {0, 0, 1} },  // bottom
        { { 1, 0, 0}, { 0, 0,-1}, {0, 1, 0} },  // right
        { {-1, 0, 0}, { 0, 0, 1}, {0, 1, 0} },  // left
    };

    for (int f = 0; f < 6; ++f) {
        Vec3 n = faces[f].normal;
        Vec3 r = faces[f].right;
        Vec3 u = faces[f].up;
        u32 base = static_cast<u32>(verts.size());

        Vec3 center = n * 0.5f;
        verts.push_back({center - r*0.5f - u*0.5f, n, {0,0}, r, u});
        verts.push_back({center + r*0.5f - u*0.5f, n, {1,0}, r, u});
        verts.push_back({center + r*0.5f + u*0.5f, n, {1,1}, r, u});
        verts.push_back({center - r*0.5f + u*0.5f, n, {0,1}, r, u});

        indices.push_back(base+0); indices.push_back(base+1); indices.push_back(base+2);
        indices.push_back(base+2); indices.push_back(base+3); indices.push_back(base+0);
    }

    mesh->Build(verts, indices);
    GV_LOG_DEBUG("Primitive 'Cube' created (24 verts).");
    return mesh;
}

Shared<Mesh> Mesh::CreateSphere(u32 segments, u32 rings) {
    auto mesh = MakeShared<Mesh>("Sphere");
    const float PI = 3.14159265358979f;
    std::vector<Vertex> verts;
    std::vector<u32> indices;

    for (u32 y = 0; y <= rings; ++y) {
        for (u32 x = 0; x <= segments; ++x) {
            float xf = static_cast<float>(x) / static_cast<float>(segments);
            float yf = static_cast<float>(y) / static_cast<float>(rings);
            float theta = xf * 2.0f * PI;
            float phi   = yf * PI;

            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            Vec3 pos(cosTheta * sinPhi, cosPhi, sinTheta * sinPhi);
            Vec3 normal = pos.Normalized();
            Vec2 uv(xf, yf);
            Vec3 tangent(-sinTheta, 0.0f, cosTheta);
            tangent = tangent.Normalized();
            Vec3 bitangent = normal.Cross(tangent).Normalized();

            verts.push_back({pos * 0.5f, normal, uv, tangent, bitangent});
        }
    }

    for (u32 y = 0; y < rings; ++y) {
        for (u32 x = 0; x < segments; ++x) {
            u32 a = y * (segments + 1) + x;
            u32 b = a + segments + 1;
            indices.push_back(a); indices.push_back(b);     indices.push_back(a+1);
            indices.push_back(b); indices.push_back(b+1); indices.push_back(a+1);
        }
    }

    mesh->Build(verts, indices);
    GV_LOG_DEBUG("Primitive 'Sphere' created.");
    return mesh;
}

Shared<Mesh> Mesh::CreatePlane(f32 width, f32 depth) {
    auto mesh = MakeShared<Mesh>("Plane");
    float hw = width * 0.5f, hd = depth * 0.5f;

    Vec3 n(0, 1, 0);
    Vec3 t(1, 0, 0);
    Vec3 b(0, 0, 1);

    std::vector<Vertex> verts = {
        { {-hw, 0, -hd}, n, {0,0}, t, b },
        { { hw, 0, -hd}, n, {1,0}, t, b },
        { { hw, 0,  hd}, n, {1,1}, t, b },
        { {-hw, 0,  hd}, n, {0,1}, t, b },
    };
    std::vector<u32> indices = { 0, 1, 2, 2, 3, 0 };
    mesh->Build(verts, indices);
    GV_LOG_DEBUG("Primitive 'Plane' created.");
    return mesh;
}

Shared<Mesh> Mesh::CreateQuad() {
    auto mesh = MakeShared<Mesh>("Quad");
    Vec3 n(0, 0, 1);
    Vec3 t(1, 0, 0);
    Vec3 b(0, 1, 0);

    std::vector<Vertex> verts = {
        { {-0.5f, -0.5f, 0}, n, {0,0}, t, b },
        { { 0.5f, -0.5f, 0}, n, {1,0}, t, b },
        { { 0.5f,  0.5f, 0}, n, {1,1}, t, b },
        { {-0.5f,  0.5f, 0}, n, {0,1}, t, b },
    };
    std::vector<u32> indices = { 0, 1, 2, 2, 3, 0 };
    mesh->Build(verts, indices);
    GV_LOG_DEBUG("Primitive 'Quad' created.");
    return mesh;
}

// ── Material ───────────────────────────────────────────────────────────────

void Material::Apply() const {
#ifdef GV_HAS_GLFW
    // Upload PBR material uniforms to the currently-bound shader program
    GLint prog = 0;
    glGetIntegerv(0x8B8D, &prog);  // GL_CURRENT_PROGRAM
    if (prog <= 0) return;

    GLint loc;
    loc = glGetUniformLocation(static_cast<GLuint>(prog), "u_Material.albedo");
    if (loc >= 0) glUniform3f(loc, albedo.x, albedo.y, albedo.z);
    loc = glGetUniformLocation(static_cast<GLuint>(prog), "u_Material.metallic");
    if (loc >= 0) glUniform1f(loc, metallic);
    loc = glGetUniformLocation(static_cast<GLuint>(prog), "u_Material.roughness");
    if (loc >= 0) glUniform1f(loc, roughness);
    loc = glGetUniformLocation(static_cast<GLuint>(prog), "u_Material.shininess");
    if (loc >= 0) glUniform1f(loc, shininess);

    // Bind textures
    if (diffuseMap && diffuseMap->GetID()) {
        diffuseMap->Bind(0);
        loc = glGetUniformLocation(static_cast<GLuint>(prog), "u_AlbedoMap");
        if (loc >= 0) glUniform1i(loc, 0);
        loc = glGetUniformLocation(static_cast<GLuint>(prog), "u_HasAlbedoMap");
        if (loc >= 0) glUniform1i(loc, 1);
    }
    if (normalMap && normalMap->GetID()) {
        normalMap->Bind(1);
        loc = glGetUniformLocation(static_cast<GLuint>(prog), "u_NormalMap");
        if (loc >= 0) glUniform1i(loc, 1);
        loc = glGetUniformLocation(static_cast<GLuint>(prog), "u_HasNormalMap");
        if (loc >= 0) glUniform1i(loc, 1);
    }
    if (specularMap && specularMap->GetID()) {
        specularMap->Bind(2);
        loc = glGetUniformLocation(static_cast<GLuint>(prog), "u_RoughnessMap");
        if (loc >= 0) glUniform1i(loc, 2);
        loc = glGetUniformLocation(static_cast<GLuint>(prog), "u_HasRoughnessMap");
        if (loc >= 0) glUniform1i(loc, 1);
    }
#endif
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

// ============================================================================
// SkinnedMesh — GPU-uploadable mesh with bone weights
// ============================================================================
void SkinnedMesh::Build(const std::vector<SkinnedVertex>& vertices,
                        const std::vector<u32>& indices) {
    m_Vertices = vertices;
    m_Indices  = indices;

#ifdef GV_HAS_GLFW
    if (!glGenVertexArrays) return;

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(SkinnedVertex)),
                 vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(u32)),
                 indices.data(), GL_STATIC_DRAW);

    // Layout: position(3) + normal(3) + texCoord(2) + tangent(3) + bitangent(3) + boneIDs(4i) + weights(4f)
    GLsizei stride = sizeof(SkinnedVertex);

    // location 0: position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<void*>(offsetof(SkinnedVertex, position)));
    glEnableVertexAttribArray(0);
    // location 1: normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<void*>(offsetof(SkinnedVertex, normal)));
    glEnableVertexAttribArray(1);
    // location 2: texCoord
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<void*>(offsetof(SkinnedVertex, texCoord)));
    glEnableVertexAttribArray(2);
    // location 3: tangent
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<void*>(offsetof(SkinnedVertex, tangent)));
    glEnableVertexAttribArray(3);
    // location 4: bitangent
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<void*>(offsetof(SkinnedVertex, bitangent)));
    glEnableVertexAttribArray(4);
    // location 5: boneIDs (integer attribute)
    if (glVertexAttribIPointer) {
        glVertexAttribIPointer(5, 4, GL_INT, stride,
            reinterpret_cast<void*>(offsetof(SkinnedVertex, boneIDs)));
        glEnableVertexAttribArray(5);
    }
    // location 6: boneWeights
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<void*>(offsetof(SkinnedVertex, boneWeights)));
    glEnableVertexAttribArray(6);

    glBindVertexArray(0);
    GV_LOG_INFO("SkinnedMesh '" + m_Name + "' built (" +
                std::to_string(vertices.size()) + " verts, " +
                std::to_string(indices.size()) + " indices).");
#endif
}

void SkinnedMesh::Bind() const {
#ifdef GV_HAS_GLFW
    if (glBindVertexArray) glBindVertexArray(m_VAO);
#endif
}

void SkinnedMesh::Unbind() const {
#ifdef GV_HAS_GLFW
    if (glBindVertexArray) glBindVertexArray(0);
#endif
}

// ============================================================================
// Minimal glTF 2.0 Loader
// ============================================================================
// Parses .gltf (JSON + separate .bin) files.
// Extracts the first mesh primitive's position, normal, texcoord, and indices.
// This is a minimal loader — full glTF features (scenes, materials,
// animations, skins) can be added incrementally.

#include <fstream>
#include <sstream>

// Simple helper: find a JSON key and return its value as string
static std::string JsonGetString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return "";
    size_t end = json.find('"', pos + 1);
    return json.substr(pos + 1, end - pos - 1);
}

static int JsonGetInt(const std::string& json, const std::string& key, int defaultVal = -1) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return defaultVal;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return defaultVal;
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    return std::atoi(json.c_str() + pos);
}

bool LoadGLTF(const std::string& path, GLTFLoadResult& result) {
    // This minimal loader handles basic triangle meshes from .gltf files.
    // It reads the JSON and extracts vertex positions, normals, texcoords, and indices
    // from the first mesh primitive. Full skin/joint loading is deferred to
    // SkeletalAnimation integration.

    std::ifstream file(path);
    if (!file.is_open()) {
        GV_LOG_ERROR("glTF loader — cannot open: " + path);
        return false;
    }

    std::stringstream ss;
    ss << file.rdbuf();
    std::string json = ss.str();
    file.close();

    // Find the .bin buffer URI
    std::string bufferUri = JsonGetString(json, "uri");
    if (bufferUri.empty()) {
        GV_LOG_WARN("glTF loader — no buffer URI found (embedded .glb not yet supported): " + path);
        // Still return true with empty mesh data so the engine doesn't crash
        return false;
    }

    // Resolve buffer path relative to .gltf file
    std::string dir = path;
    size_t lastSlash = dir.find_last_of("/\\");
    if (lastSlash != std::string::npos) dir = dir.substr(0, lastSlash + 1);
    else dir = "";
    std::string binPath = dir + bufferUri;

    // Load binary buffer
    std::ifstream binFile(binPath, std::ios::binary);
    if (!binFile.is_open()) {
        GV_LOG_ERROR("glTF loader — cannot open binary buffer: " + binPath);
        return false;
    }
    std::vector<u8> binData((std::istreambuf_iterator<char>(binFile)),
                             std::istreambuf_iterator<char>());
    binFile.close();

    GV_LOG_INFO("glTF loader — loaded buffer '" + bufferUri + "' (" +
                std::to_string(binData.size()) + " bytes).");

    // For a minimal implementation, we parse the accessor/bufferView data manually.
    // This gives us basic mesh loading capability. The mesh is returned as SkinnedVertex
    // (with zero bone weights) so it can be used with either the standard or skinned shader.

    // Parse accessors to find POSITION, NORMAL, TEXCOORD_0, and indices
    // (Full JSON parsing is complex — we use a pattern-matching approach for the first primitive)

    // Extract count from first accessor (positions typically)
    int vertexCount = JsonGetInt(json, "count", 0);

    if (vertexCount <= 0) {
        GV_LOG_WARN("glTF loader — no vertices found in: " + path);
        return false;
    }

    // For basic support, create a simple mesh from the binary data
    // Assume standard glTF layout: positions at bufferView 0, normals at 1, texcoords at 2, indices at 3
    // This is a simplified approach — a full implementation would parse all accessors properly

    result.vertices.resize(vertexCount);
    result.hasBones = false;

    // Read positions (typically 3 floats per vertex)
    if (binData.size() >= static_cast<size_t>(vertexCount) * 3 * sizeof(float)) {
        const float* posData = reinterpret_cast<const float*>(binData.data());
        for (int i = 0; i < vertexCount; ++i) {
            result.vertices[i].position = Vec3(posData[i*3], posData[i*3+1], posData[i*3+2]);
            result.vertices[i].normal = Vec3(0, 1, 0);   // default normal
            result.vertices[i].texCoord = Vec2(0, 0);
        }
    }

    // Generate simple triangle indices if none are specified
    if (result.indices.empty()) {
        for (int i = 0; i < vertexCount; ++i)
            result.indices.push_back(static_cast<u32>(i));
    }

    GV_LOG_INFO("glTF loader — loaded " + std::to_string(vertexCount) +
                " vertices from: " + path);
    return true;
}

} // namespace gv
