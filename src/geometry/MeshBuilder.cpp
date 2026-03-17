#include "geometry/MeshBuilder.h"
#include <cmath>
#include <algorithm>

static const float kPiMB = 3.14159265358979323846f;

// Apply transform to a copy of a mesh
MeshData MeshBuilder::TransformMesh(MeshData mesh, Vec3 t, Vec3 rDeg, Vec3 s) {
    float rx = rDeg.x * kPiMB / 180.0f;
    float ry = rDeg.y * kPiMB / 180.0f;
    float rz = rDeg.z * kPiMB / 180.0f;

    auto rotX = [&](Vec3 v) -> Vec3 { return {v.x, v.y*std::cos(rx)-v.z*std::sin(rx), v.y*std::sin(rx)+v.z*std::cos(rx)}; };
    auto rotY = [&](Vec3 v) -> Vec3 { return {v.x*std::cos(ry)+v.z*std::sin(ry), v.y, -v.x*std::sin(ry)+v.z*std::cos(ry)}; };
    auto rotZ = [&](Vec3 v) -> Vec3 { return {v.x*std::cos(rz)-v.y*std::sin(rz), v.x*std::sin(rz)+v.y*std::cos(rz), v.z}; };

    for (auto& v : mesh.vertices) {
        v.position.x *= s.x; v.position.y *= s.y; v.position.z *= s.z;
        v.position = rotX(v.position); v.position = rotY(v.position); v.position = rotZ(v.position);
        v.position.x += t.x; v.position.y += t.y; v.position.z += t.z;
        v.normal = rotX(v.normal); v.normal = rotY(v.normal); v.normal = rotZ(v.normal);
    }
    return mesh;
}

MeshBuilder& MeshBuilder::AddPrimitive(MeshData mesh, Vec3 t, Vec3 r, Vec3 s) {
    m_Entries.push_back({std::move(mesh), t, r, s});
    m_HasMerged = false;
    return *this;
}

MeshBuilder& MeshBuilder::Merge() {
    m_Merged = {};
    for (auto& e : m_Entries) {
        MeshData baked = TransformMesh(e.mesh, e.translation, e.rotationDeg, e.scale);
        uint32_t off = (uint32_t)m_Merged.vertices.size();
        for (auto& v : baked.vertices) m_Merged.vertices.push_back(v);
        for (auto idx : baked.indices) m_Merged.indices.push_back(idx + off);
    }
    m_HasMerged = true;
    return *this;
}

MeshBuilder& MeshBuilder::ComputeFlatNormals()   { if (!m_HasMerged) Merge(); m_Merged.RecalcFlatNormals();   return *this; }
MeshBuilder& MeshBuilder::ComputeSmoothNormals() { if (!m_HasMerged) Merge(); m_Merged.RecalcSmoothNormals(); return *this; }
MeshBuilder& MeshBuilder::FlipNormals()          { if (!m_HasMerged) Merge(); m_Merged.FlipNormals();         return *this; }

MeshBuilder& MeshBuilder::Translate(Vec3 offset) {
    if (!m_HasMerged) Merge();
    for (auto& v : m_Merged.vertices) { v.position.x += offset.x; v.position.y += offset.y; v.position.z += offset.z; }
    return *this;
}

MeshBuilder& MeshBuilder::Scale(Vec3 f) {
    if (!m_HasMerged) Merge();
    for (auto& v : m_Merged.vertices) { v.position.x *= f.x; v.position.y *= f.y; v.position.z *= f.z; }
    return *this;
}

MeshBuilder& MeshBuilder::RotateEuler(Vec3 deg) {
    if (!m_HasMerged) Merge();
    float rx = deg.x*kPiMB/180.0f, ry = deg.y*kPiMB/180.0f, rz = deg.z*kPiMB/180.0f;
    auto rotX = [rx](Vec3 v) -> Vec3 { return {v.x, v.y*std::cos(rx)-v.z*std::sin(rx), v.y*std::sin(rx)+v.z*std::cos(rx)}; };
    auto rotY = [ry](Vec3 v) -> Vec3 { return {v.x*std::cos(ry)+v.z*std::sin(ry), v.y, -v.x*std::sin(ry)+v.z*std::cos(ry)}; };
    auto rotZ = [rz](Vec3 v) -> Vec3 { return {v.x*std::cos(rz)-v.y*std::sin(rz), v.x*std::sin(rz)+v.y*std::cos(rz), v.z}; };
    for (auto& v : m_Merged.vertices) {
        v.position = rotX(v.position); v.position = rotY(v.position); v.position = rotZ(v.position);
        v.normal = rotX(v.normal); v.normal = rotY(v.normal); v.normal = rotZ(v.normal);
    }
    return *this;
}

MeshBuilder& MeshBuilder::ForEachVertex(std::function<void(Vertex3D&)> fn) {
    if (!m_HasMerged) Merge();
    for (auto& v : m_Merged.vertices) fn(v);
    return *this;
}

MeshBuilder& MeshBuilder::WeldVertices(float tol) {
    if (!m_HasMerged) Merge();
    MeshData out;
    for (auto& v : m_Merged.vertices) {
        uint32_t found = (uint32_t)out.vertices.size();
        for (uint32_t i = 0; i < (uint32_t)out.vertices.size(); i++) {
            Vec3 d{v.position.x-out.vertices[i].position.x, v.position.y-out.vertices[i].position.y, v.position.z-out.vertices[i].position.z};
            if (d.x*d.x+d.y*d.y+d.z*d.z < tol*tol) { found=i; break; }
        }
        if (found == (uint32_t)out.vertices.size()) out.vertices.push_back(v);
        out.indices.push_back(found);
    }
    // Remap original indices
    std::vector<uint32_t> remap;
    for (uint32_t old = 0; old < (uint32_t)m_Merged.vertices.size(); old++) {
        for (uint32_t ni = 0; ni < (uint32_t)out.vertices.size(); ni++) {
            Vec3 d{m_Merged.vertices[old].position.x-out.vertices[ni].position.x, m_Merged.vertices[old].position.y-out.vertices[ni].position.y, m_Merged.vertices[old].position.z-out.vertices[ni].position.z};
            if (d.x*d.x+d.y*d.y+d.z*d.z < tol*tol) { remap.push_back(ni); break; }
        }
    }
    out.indices = m_Merged.indices;
    if (remap.size() == m_Merged.vertices.size())
        for (auto& idx : out.indices) idx = remap[idx];
    m_Merged = std::move(out);
    return *this;
}

MeshBuilder& MeshBuilder::Center() {
    if (!m_HasMerged) Merge();
    Vec3 sum{0,0,0};
    for (auto& v : m_Merged.vertices) { sum.x+=v.position.x; sum.y+=v.position.y; sum.z+=v.position.z; }
    if (!m_Merged.vertices.empty()) {
        float n = (float)m_Merged.vertices.size();
        sum.x/=n; sum.y/=n; sum.z/=n;
        for (auto& v : m_Merged.vertices) { v.position.x-=sum.x; v.position.y-=sum.y; v.position.z-=sum.z; }
    }
    return *this;
}

MeshData MeshBuilder::Build(const std::string& name) const {
    if (!m_HasMerged) const_cast<MeshBuilder*>(this)->Merge();
    MeshData out = m_Merged;
    if (!name.empty()) out.name = name;
    return out;
}
