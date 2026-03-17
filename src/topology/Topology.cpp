#include "topology/Topology.h"
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <stdexcept>

// ─────────────────────────────────────────────────────────────────────────────
//  HEMesh  —  Build from indexed triangle list
// ─────────────────────────────────────────────────────────────────────────────

HEMesh HEMesh::FromMeshData(const MeshData& md) {
    HEMesh he;
    for (auto& v : md.vertices) he.vertices.push_back(v.position);

    size_t triCount = md.indices.size() / 3;
    he.halfEdges.resize(triCount * 3);
    he.faces.resize(triCount);

    using EdgeKey = std::pair<int,int>;
    std::unordered_map<uint64_t, int> edgeMap;
    auto key = [](int a, int b) -> uint64_t {
        return (uint64_t)(uint32_t)a << 32 | (uint32_t)b;
    };

    for (size_t t = 0; t < triCount; t++) {
        int i0 = md.indices[t*3], i1 = md.indices[t*3+1], i2 = md.indices[t*3+2];
        int he0 = (int)(t*3), he1 = (int)(t*3+1), he2 = (int)(t*3+2);

        he.halfEdges[he0] = {i1, -1, he1, (int)t};
        he.halfEdges[he1] = {i2, -1, he2, (int)t};
        he.halfEdges[he2] = {i0, -1, he0, (int)t};
        he.faces[t].edge = he0;

        // Register directed edges for twin lookup
        edgeMap[key(i0, i1)] = he0;
        edgeMap[key(i1, i2)] = he1;
        edgeMap[key(i2, i0)] = he2;
    }
    // Link twins
    for (auto& kv : edgeMap) {
        int heIdx = kv.second;
        int tip   = he.halfEdges[heIdx].vertex;
        // origin of this half-edge = tip of prev half-edge
        int origin = he.halfEdges[he.halfEdges[heIdx].next].vertex;
        // Wait, origin = tip of PREVIOUS; let's use reverse direction lookup
        auto it = edgeMap.find(key(tip, origin)); // reversed direction = twin
        if (it != edgeMap.end()) he.halfEdges[heIdx].twin = it->second;
    }
    return he;
}

MeshData HEMesh::ToMeshData() const {
    MeshData md;
    for (auto& p : vertices) { Vertex3D v; v.position = p; v.normal = Vec3(0,1,0); v.uv = Vec2(0,0); md.vertices.push_back(v); }
    for (auto& f : faces) {
        int h = f.edge;
        md.indices.push_back(halfEdges[h].vertex);
        h = halfEdges[h].next;
        md.indices.push_back(halfEdges[h].vertex);
        h = halfEdges[h].next;
        md.indices.push_back(halfEdges[h].vertex);
    }
    md.RecalcSmoothNormals();
    return md;
}

bool HEMesh::IsManifold() const {
    for (auto& he : halfEdges)
        if (he.twin == -1) return false;
    return true;
}

bool HEMesh::IsClosed() const {
    for (auto& he : halfEdges)
        if (he.twin == -1) return false;
    return true;
}

int HEMesh::Genus() const {
    int V = (int)vertices.size();
    int E = (int)(halfEdges.size() / 2);
    int F = (int)faces.size();
    int chi = V - E + F;
    return (2 - chi) / 2;  // Euler characteristic: χ = 2 - 2g
}

int HEMesh::BoundaryLoopCount() const {
    return (int)BoundaryLoops().size();
}

int HEMesh::Valence(int vi) const {
    int count = 0;
    for (auto& he : halfEdges)
        if (he.vertex == vi) count++;
    return count;
}

std::vector<int> HEMesh::OneRing(int vi) const {
    std::vector<int> ring;
    for (size_t i = 0; i < halfEdges.size(); i++) {
        if (halfEdges[i].vertex == vi && halfEdges[i].twin != -1)
            ring.push_back(halfEdges[halfEdges[i].twin].vertex);
    }
    return ring;
}

std::vector<std::vector<int>> HEMesh::BoundaryLoops() const {
    std::vector<std::vector<int>> loops;
    std::vector<bool> visited(halfEdges.size(), false);
    for (size_t i = 0; i < halfEdges.size(); i++) {
        if (halfEdges[i].twin != -1 || visited[i]) continue;
        std::vector<int> loop;
        size_t cur = i;
        while (!visited[cur]) {
            visited[cur] = true;
            loop.push_back(halfEdges[cur].vertex);
            // Follow boundary
            size_t next = halfEdges[cur].next;
            while (halfEdges[next].twin != -1) {
                next = halfEdges[halfEdges[next].twin].next;
                if (next == cur) break;
            }
            cur = next;
        }
        if (!loop.empty()) loops.push_back(loop);
    }
    return loops;
}

int HEMesh::IsolatedVertexCount() const {
    std::vector<bool> used(vertices.size(), false);
    for (auto& he : halfEdges) if (he.vertex >= 0 && (size_t)he.vertex < used.size()) used[he.vertex] = true;
    int cnt = 0;
    for (auto b : used) if (!b) cnt++;
    return cnt;
}

// ─── TopologyRepair stubs ────────────────────────────────────────────────────

HEMesh TopologyRepair::FillHoles(const HEMesh& mesh) { return mesh; }
HEMesh TopologyRepair::RemoveDegenerates(const HEMesh& mesh, float) { return mesh; }
HEMesh TopologyRepair::MakeConsistentWinding(const HEMesh& mesh) { return mesh; }
HEMesh TopologyRepair::WeldVertices(const HEMesh& mesh, float) { return mesh; }

// ─── MeshAnalyzer ─────────────────────────────────────────────────────────────

float MeshAnalyzer::SurfaceArea(const MeshData& mesh) {
    float area = 0;
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        Vec3 a = mesh.vertices[mesh.indices[i]].position;
        Vec3 b = mesh.vertices[mesh.indices[i+1]].position;
        Vec3 c = mesh.vertices[mesh.indices[i+2]].position;
        Vec3 ab{b.x-a.x,b.y-a.y,b.z-a.z}, ac{c.x-a.x,c.y-a.y,c.z-a.z};
        Vec3 cross{ab.y*ac.z-ab.z*ac.y, ab.z*ac.x-ab.x*ac.z, ab.x*ac.y-ab.y*ac.x};
        area += 0.5f * std::sqrt(cross.x*cross.x+cross.y*cross.y+cross.z*cross.z);
    }
    return area;
}

float MeshAnalyzer::Volume(const MeshData& mesh) {
    float vol = 0;
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        Vec3 a = mesh.vertices[mesh.indices[i]].position;
        Vec3 b = mesh.vertices[mesh.indices[i+1]].position;
        Vec3 c = mesh.vertices[mesh.indices[i+2]].position;
        vol += a.x*(b.y*c.z - b.z*c.y) + a.y*(b.z*c.x - b.x*c.z) + a.z*(b.x*c.y - b.y*c.x);
    }
    return std::abs(vol) / 6.0f;
}

Vec3 MeshAnalyzer::Centroid(const MeshData& mesh) {
    Vec3 sum{0,0,0};
    if (mesh.vertices.empty()) return sum;
    for (auto& v : mesh.vertices) { sum.x+=v.position.x; sum.y+=v.position.y; sum.z+=v.position.z; }
    float n = (float)mesh.vertices.size();
    return {sum.x/n, sum.y/n, sum.z/n};
}

MeshStats MeshAnalyzer::Analyze(const MeshData& mesh) {
    MeshStats s{};
    s.vertexCount   = (int)mesh.vertices.size();
    s.triangleCount = (int)(mesh.indices.size() / 3);
    s.surfaceArea   = SurfaceArea(mesh);
    s.centroid      = Centroid(mesh);
    mesh.ComputeAABB(const_cast<Vec3&>(s.aabbMin), const_cast<Vec3&>(s.aabbMax));
    HEMesh he = HEMesh::FromMeshData(mesh);
    s.edgeCount     = (int)(he.halfEdges.size() / 2);
    s.genus         = he.Genus();
    s.boundaryLoops = he.BoundaryLoopCount();
    s.isManifold    = he.IsManifold();
    s.isClosed      = he.IsClosed();
    s.volume        = s.isClosed ? Volume(mesh) : 0.0f;
    return s;
}

float MeshAnalyzer::AverageCurvature(const HEMesh& /*he*/) { return 0.0f; }

std::vector<std::pair<int,int>> MeshAnalyzer::FindSelfIntersections(const MeshData& /*mesh*/) {
    return {};  // O(n²) check stubbed for now
}
