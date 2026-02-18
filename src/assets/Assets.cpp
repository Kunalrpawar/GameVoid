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
    if (ext == ".gltf" || ext == ".glb") {
        GLTFLoadResult gltfResult;
        if (!LoadGLTF(path, gltfResult)) {
            GV_LOG_WARN("Mesh::LoadFromFile — glTF/GLB load failed: " + path);
            return false;
        }
        // Convert SkinnedVertex → Vertex
        std::vector<Vertex> verts;
        verts.reserve(gltfResult.vertices.size());
        for (auto& sv : gltfResult.vertices) {
            Vertex v;
            v.position  = sv.position;
            v.normal    = sv.normal;
            v.texCoord  = sv.texCoord;
            v.tangent   = sv.tangent;
            v.bitangent = sv.bitangent;
            verts.push_back(v);
        }
        // Compute tangents if not provided
        for (size_t i = 0; i + 2 < verts.size(); i += 3) {
            ComputeTangents(verts[i], verts[i + 1], verts[i + 2]);
        }
        m_Name = path;
        Build(verts, gltfResult.indices);
        GV_LOG_INFO("Mesh loaded from glTF: " + path + " (" +
                    std::to_string(verts.size()) + " verts, " +
                    std::to_string(gltfResult.indices.size() / 3) + " tris)");
        return true;
    }
    GV_LOG_WARN("Mesh::LoadFromFile — unsupported format: " + ext + " (supported: .obj, .gltf, .glb)");
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
// glTF 2.0 / GLB Loader
// ============================================================================
// Supports both .gltf (JSON + .bin) and .glb (binary container) formats.
// Parses accessors, buffer views, and mesh primitives to extract geometry.
// ============================================================================

// ── Tiny JSON helpers (no external dependency) ─────────────────────────────

// Skip whitespace
static size_t JsonSkipWS(const std::string& s, size_t p) {
    while (p < s.size() && (s[p]==' '||s[p]=='\t'||s[p]=='\n'||s[p]=='\r')) ++p;
    return p;
}

// Parse a JSON number at position p, return value and advance p
static double JsonParseNumber(const std::string& s, size_t& p) {
    p = JsonSkipWS(s, p);
    size_t start = p;
    if (p < s.size() && (s[p]=='-'||s[p]=='+')) ++p;
    while (p < s.size() && (std::isdigit((unsigned char)s[p])||s[p]=='.'||s[p]=='e'||s[p]=='E'||s[p]=='+'||s[p]=='-')) ++p;
    return std::atof(s.substr(start, p - start).c_str());
}

// Parse a JSON string at position p (p should point to opening "), advance p past closing quote
static std::string JsonParseString(const std::string& s, size_t& p) {
    p = JsonSkipWS(s, p);
    if (p >= s.size() || s[p] != '"') return "";
    ++p;
    std::string result;
    while (p < s.size() && s[p] != '"') {
        if (s[p] == '\\' && p + 1 < s.size()) { result += s[p+1]; p += 2; }
        else { result += s[p]; ++p; }
    }
    if (p < s.size()) ++p;  // skip closing quote
    return result;
}

// Find the value of a key in the current JSON object scope starting at `start`
static size_t JsonFindKey(const std::string& s, size_t start, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t p = s.find(needle, start);
    if (p == std::string::npos) return std::string::npos;
    p += needle.size();
    p = JsonSkipWS(s, p);
    if (p < s.size() && s[p] == ':') ++p;
    return JsonSkipWS(s, p);
}

// Find matching bracket/brace end starting at p (p points to '[' or '{')
static size_t JsonFindMatchingEnd(const std::string& s, size_t p) {
    if (p >= s.size()) return std::string::npos;
    char open = s[p], close = (open == '[') ? ']' : '}';
    int depth = 1;
    ++p;
    bool inStr = false;
    while (p < s.size() && depth > 0) {
        if (s[p] == '"' && (p == 0 || s[p-1] != '\\')) inStr = !inStr;
        if (!inStr) {
            if (s[p] == open) ++depth;
            if (s[p] == close) --depth;
        }
        ++p;
    }
    return p;
}

// Extract the i-th element from a JSON array (0-indexed). Returns start..end range in the string.
static bool JsonArrayElement(const std::string& s, size_t arrayStart, int index, size_t& elemStart, size_t& elemEnd) {
    size_t p = JsonSkipWS(s, arrayStart);
    if (p >= s.size() || s[p] != '[') return false;
    ++p;
    int cur = 0;
    while (p < s.size() && s[p] != ']') {
        p = JsonSkipWS(s, p);
        size_t start = p;
        // Find end of this element
        if (s[p] == '{' || s[p] == '[') {
            size_t end = JsonFindMatchingEnd(s, p);
            if (cur == index) { elemStart = start; elemEnd = end; return true; }
            p = end;
        } else if (s[p] == '"') {
            ++p;
            while (p < s.size() && s[p] != '"') { if (s[p]=='\\') ++p; ++p; }
            if (p < s.size()) ++p;
            if (cur == index) { elemStart = start; elemEnd = p; return true; }
        } else {
            while (p < s.size() && s[p] != ',' && s[p] != ']' && s[p] != '}') ++p;
            if (cur == index) { elemStart = start; elemEnd = p; return true; }
        }
        p = JsonSkipWS(s, p);
        if (p < s.size() && s[p] == ',') ++p;
        ++cur;
    }
    return false;
}

// Count elements in a JSON array
static int JsonArrayCount(const std::string& s, size_t arrayStart) {
    int count = 0;
    size_t es, ee;
    while (JsonArrayElement(s, arrayStart, count, es, ee)) ++count;
    return count;
}

// Get an integer value from key in an object substring
static int JsonObjGetInt(const std::string& s, size_t objStart, size_t objEnd, const std::string& key, int def = -1) {
    std::string sub = s.substr(objStart, objEnd - objStart);
    size_t p = JsonFindKey(sub, 0, key);
    if (p == std::string::npos) return def;
    return static_cast<int>(JsonParseNumber(sub, p));
}

static std::string JsonObjGetString(const std::string& s, size_t objStart, size_t objEnd, const std::string& key) {
    std::string sub = s.substr(objStart, objEnd - objStart);
    size_t p = JsonFindKey(sub, 0, key);
    if (p == std::string::npos) return "";
    return JsonParseString(sub, p);
}

// ── Accessor info struct ───────────────────────────────────────────────────
struct GltfAccessor {
    int bufferView   = -1;
    int byteOffset   = 0;
    int componentType = 0;   // 5120=byte, 5121=ubyte, 5122=short, 5123=ushort, 5125=uint, 5126=float
    int count        = 0;
    std::string type;        // "SCALAR", "VEC2", "VEC3", "VEC4", "MAT4"
};

struct GltfBufferView {
    int buffer     = 0;
    int byteOffset = 0;
    int byteLength = 0;
    int byteStride = 0;
};

bool LoadGLTF(const std::string& path, GLTFLoadResult& result) {
    std::string json;
    std::vector<u8> binData;

    // ── Detect .glb vs .gltf ───────────────────────────────────────────────
    std::string ext = path.substr(path.rfind('.'));
    for (auto& c : ext) c = static_cast<char>(std::tolower((unsigned char)c));

    if (ext == ".glb") {
        // ── GLB binary container ────────────────────────────────────────────
        // Format: 12-byte header + chunk0 (JSON) + chunk1 (BIN)
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            GV_LOG_ERROR("GLB loader — cannot open: " + path);
            return false;
        }
        std::vector<u8> fileData((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
        file.close();

        if (fileData.size() < 12) {
            GV_LOG_ERROR("GLB loader — file too small: " + path);
            return false;
        }

        // Header: magic(4) + version(4) + length(4)
        u32 magic   = *reinterpret_cast<u32*>(&fileData[0]);
        u32 version = *reinterpret_cast<u32*>(&fileData[4]);
        // u32 totalLen = *reinterpret_cast<u32*>(&fileData[8]);
        (void)version;

        if (magic != 0x46546C67) {  // "glTF" in little-endian
            GV_LOG_ERROR("GLB loader — invalid magic number: " + path);
            return false;
        }

        // Parse chunks
        size_t offset = 12;
        while (offset + 8 <= fileData.size()) {
            u32 chunkLen  = *reinterpret_cast<u32*>(&fileData[offset]);
            u32 chunkType = *reinterpret_cast<u32*>(&fileData[offset + 4]);
            offset += 8;

            if (offset + chunkLen > fileData.size()) break;

            if (chunkType == 0x4E4F534A) {  // "JSON"
                json.assign(reinterpret_cast<char*>(&fileData[offset]), chunkLen);
            } else if (chunkType == 0x004E4942) {  // "BIN\0"
                binData.assign(fileData.begin() + offset, fileData.begin() + offset + chunkLen);
            }
            offset += chunkLen;
        }

        if (json.empty()) {
            GV_LOG_ERROR("GLB loader — no JSON chunk found: " + path);
            return false;
        }
        GV_LOG_INFO("GLB loader — parsed " + std::to_string(json.size()) + " bytes JSON, " +
                    std::to_string(binData.size()) + " bytes BIN from: " + path);
    } else {
        // ── .gltf (JSON + external .bin) ────────────────────────────────────
        std::ifstream file(path);
        if (!file.is_open()) {
            GV_LOG_ERROR("glTF loader — cannot open: " + path);
            return false;
        }
        std::stringstream ss;
        ss << file.rdbuf();
        json = ss.str();
        file.close();

        // Find buffer URI
        size_t buffersPos = JsonFindKey(json, 0, "buffers");
        if (buffersPos != std::string::npos) {
            size_t es, ee;
            if (JsonArrayElement(json, buffersPos, 0, es, ee)) {
                std::string uri = JsonObjGetString(json, es, ee, "uri");
                if (!uri.empty()) {
                    std::string dir = path;
                    size_t sl = dir.find_last_of("/\\");
                    dir = (sl != std::string::npos) ? dir.substr(0, sl + 1) : "";
                    std::string binPath = dir + uri;

                    std::ifstream binFile(binPath, std::ios::binary);
                    if (binFile.is_open()) {
                        binData.assign((std::istreambuf_iterator<char>(binFile)),
                                        std::istreambuf_iterator<char>());
                        binFile.close();
                        GV_LOG_INFO("glTF loader — loaded buffer: " + binPath +
                                    " (" + std::to_string(binData.size()) + " bytes)");
                    } else {
                        GV_LOG_ERROR("glTF loader — cannot open buffer: " + binPath);
                        return false;
                    }
                }
            }
        }
    }

    if (binData.empty()) {
        GV_LOG_ERROR("glTF loader — no binary data found: " + path);
        return false;
    }

    // ── Parse buffer views ──────────────────────────────────────────────────
    std::vector<GltfBufferView> bufferViews;
    {
        size_t bvPos = JsonFindKey(json, 0, "bufferViews");
        if (bvPos != std::string::npos) {
            int count = JsonArrayCount(json, bvPos);
            for (int i = 0; i < count; ++i) {
                size_t es, ee;
                if (!JsonArrayElement(json, bvPos, i, es, ee)) break;
                GltfBufferView bv;
                bv.buffer     = JsonObjGetInt(json, es, ee, "buffer", 0);
                bv.byteOffset = JsonObjGetInt(json, es, ee, "byteOffset", 0);
                bv.byteLength = JsonObjGetInt(json, es, ee, "byteLength", 0);
                bv.byteStride = JsonObjGetInt(json, es, ee, "byteStride", 0);
                bufferViews.push_back(bv);
            }
        }
    }

    // ── Parse accessors ─────────────────────────────────────────────────────
    std::vector<GltfAccessor> accessors;
    {
        size_t accPos = JsonFindKey(json, 0, "accessors");
        if (accPos != std::string::npos) {
            int count = JsonArrayCount(json, accPos);
            for (int i = 0; i < count; ++i) {
                size_t es, ee;
                if (!JsonArrayElement(json, accPos, i, es, ee)) break;
                GltfAccessor acc;
                acc.bufferView    = JsonObjGetInt(json, es, ee, "bufferView", -1);
                acc.byteOffset    = JsonObjGetInt(json, es, ee, "byteOffset", 0);
                acc.componentType = JsonObjGetInt(json, es, ee, "componentType", 5126);
                acc.count         = JsonObjGetInt(json, es, ee, "count", 0);
                acc.type          = JsonObjGetString(json, es, ee, "type");
                accessors.push_back(acc);
            }
        }
    }

    // ── Helper: read accessor data from buffer ──────────────────────────────
    auto getAccessorData = [&](int accIdx) -> const u8* {
        if (accIdx < 0 || accIdx >= static_cast<int>(accessors.size())) return nullptr;
        auto& acc = accessors[accIdx];
        if (acc.bufferView < 0 || acc.bufferView >= static_cast<int>(bufferViews.size())) return nullptr;
        auto& bv = bufferViews[acc.bufferView];
        size_t offset = static_cast<size_t>(bv.byteOffset) + static_cast<size_t>(acc.byteOffset);
        if (offset >= binData.size()) return nullptr;
        return &binData[offset];
    };

    // ── Parse meshes — extract first primitive ──────────────────────────────
    size_t meshesPos = JsonFindKey(json, 0, "meshes");
    if (meshesPos == std::string::npos) {
        GV_LOG_WARN("glTF loader — no meshes found: " + path);
        return false;
    }

    size_t meshEs, meshEe;
    if (!JsonArrayElement(json, meshesPos, 0, meshEs, meshEe)) {
        GV_LOG_WARN("glTF loader — empty meshes array: " + path);
        return false;
    }

    // Find "primitives" array in the first mesh
    size_t primPos = JsonFindKey(json, meshEs, "primitives");
    if (primPos == std::string::npos || primPos >= meshEe) {
        GV_LOG_WARN("glTF loader — no primitives in mesh: " + path);
        return false;
    }

    size_t primEs, primEe;
    if (!JsonArrayElement(json, primPos, 0, primEs, primEe)) {
        GV_LOG_WARN("glTF loader — empty primitives: " + path);
        return false;
    }

    // Find attribute accessors
    size_t attrPos = JsonFindKey(json, primEs, "attributes");
    int posAccIdx   = -1;
    int normAccIdx  = -1;
    int uvAccIdx    = -1;
    int idxAccIdx   = -1;

    if (attrPos != std::string::npos && attrPos < primEe) {
        size_t attrEnd = JsonFindMatchingEnd(json, attrPos);
        posAccIdx  = JsonObjGetInt(json, attrPos, attrEnd, "POSITION", -1);
        normAccIdx = JsonObjGetInt(json, attrPos, attrEnd, "NORMAL", -1);
        uvAccIdx   = JsonObjGetInt(json, attrPos, attrEnd, "TEXCOORD_0", -1);
    }
    idxAccIdx = JsonObjGetInt(json, primEs, primEe, "indices", -1);

    // ── Read position data ──────────────────────────────────────────────────
    if (posAccIdx < 0 || posAccIdx >= static_cast<int>(accessors.size())) {
        GV_LOG_WARN("glTF loader — no POSITION accessor: " + path);
        return false;
    }

    int vertexCount = accessors[posAccIdx].count;
    result.vertices.resize(vertexCount);
    result.hasBones = false;

    const u8* posPtr = getAccessorData(posAccIdx);
    if (posPtr) {
        auto& bv = bufferViews[accessors[posAccIdx].bufferView];
        int stride = (bv.byteStride > 0) ? bv.byteStride : static_cast<int>(3 * sizeof(float));
        for (int i = 0; i < vertexCount; ++i) {
            const float* fp = reinterpret_cast<const float*>(posPtr + i * stride);
            result.vertices[i].position = Vec3(fp[0], fp[1], fp[2]);
        }
    }

    // ── Read normals ────────────────────────────────────────────────────────
    if (normAccIdx >= 0 && normAccIdx < static_cast<int>(accessors.size())) {
        const u8* normPtr = getAccessorData(normAccIdx);
        if (normPtr) {
            auto& bv = bufferViews[accessors[normAccIdx].bufferView];
            int stride = (bv.byteStride > 0) ? bv.byteStride : static_cast<int>(3 * sizeof(float));
            for (int i = 0; i < vertexCount; ++i) {
                const float* fp = reinterpret_cast<const float*>(normPtr + i * stride);
                result.vertices[i].normal = Vec3(fp[0], fp[1], fp[2]);
            }
        }
    } else {
        for (int i = 0; i < vertexCount; ++i)
            result.vertices[i].normal = Vec3(0, 1, 0);
    }

    // ── Read texcoords ──────────────────────────────────────────────────────
    if (uvAccIdx >= 0 && uvAccIdx < static_cast<int>(accessors.size())) {
        const u8* uvPtr = getAccessorData(uvAccIdx);
        if (uvPtr) {
            auto& bv = bufferViews[accessors[uvAccIdx].bufferView];
            int stride = (bv.byteStride > 0) ? bv.byteStride : static_cast<int>(2 * sizeof(float));
            for (int i = 0; i < vertexCount; ++i) {
                const float* fp = reinterpret_cast<const float*>(uvPtr + i * stride);
                result.vertices[i].texCoord = Vec2(fp[0], fp[1]);
            }
        }
    }

    // ── Read indices ────────────────────────────────────────────────────────
    if (idxAccIdx >= 0 && idxAccIdx < static_cast<int>(accessors.size())) {
        auto& idxAcc = accessors[idxAccIdx];
        const u8* idxPtr = getAccessorData(idxAccIdx);
        if (idxPtr) {
            result.indices.reserve(idxAcc.count);
            for (int i = 0; i < idxAcc.count; ++i) {
                u32 idx = 0;
                switch (idxAcc.componentType) {
                    case 5121: // UNSIGNED_BYTE
                        idx = static_cast<u32>(idxPtr[i]);
                        break;
                    case 5123: // UNSIGNED_SHORT
                        idx = static_cast<u32>(reinterpret_cast<const u16*>(idxPtr)[i]);
                        break;
                    case 5125: // UNSIGNED_INT
                        idx = reinterpret_cast<const u32*>(idxPtr)[i];
                        break;
                    default:
                        idx = static_cast<u32>(i);
                        break;
                }
                result.indices.push_back(idx);
            }
        }
    } else {
        // No indices — generate sequential
        for (int i = 0; i < vertexCount; ++i)
            result.indices.push_back(static_cast<u32>(i));
    }

    // Compute normals if not provided
    if (normAccIdx < 0) {
        for (size_t i = 0; i + 2 < result.indices.size(); i += 3) {
            u32 i0 = result.indices[i], i1 = result.indices[i+1], i2 = result.indices[i+2];
            if (i0 < result.vertices.size() && i1 < result.vertices.size() && i2 < result.vertices.size()) {
                Vec3 e1 = result.vertices[i1].position - result.vertices[i0].position;
                Vec3 e2 = result.vertices[i2].position - result.vertices[i0].position;
                Vec3 n = e1.Cross(e2).Normalized();
                result.vertices[i0].normal = result.vertices[i1].normal = result.vertices[i2].normal = n;
            }
        }
    }

    GV_LOG_INFO("glTF loader — loaded " + std::to_string(vertexCount) +
                " vertices, " + std::to_string(result.indices.size() / 3) +
                " triangles from: " + path);
    return true;
}

} // namespace gv
