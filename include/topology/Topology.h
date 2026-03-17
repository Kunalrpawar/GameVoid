#pragma once
#include "geometry/Primitives.h"
#include "core/Math.h"
#include <vector>
#include <unordered_map>

namespace gv {

// ─────────────────────────────────────────────────────────────────────────────
//  Topology.h  —  Half-edge mesh representation and topological analysis.
//
//  A half-edge mesh encodes adjacency explicitly, enabling O(1) neighbour
//  traversal, genus computation, manifold checks and boundary detection.
// ─────────────────────────────────────────────────────────────────────────────

struct HalfEdge {
    int vertex;   // index into vertex array (tip of this half-edge)
    int twin;     // index of opposite half-edge (-1 if boundary)
    int next;     // index of next half-edge around the same face
    int face;     // index of owning face (-1 if boundary half-edge)
};

struct HEFace {
    int edge;     // index of any half-edge on this face
    Vec3 normal;  // cached face normal
};

struct HEMesh {
    std::vector<Vec3>     vertices;
    std::vector<HalfEdge> halfEdges;
    std::vector<HEFace>   faces;

    // Build from indexed triangle list
    static HEMesh FromMeshData(const MeshData& md);

    // Convert back to indexed triangle list
    MeshData ToMeshData() const;

    // ── Queries ──────────────────────────────────────────────────────────────
    bool IsManifold()    const;   // true if every edge has exactly one twin
    bool IsClosed()      const;   // true if no boundary edges exist
    int  Genus()         const;   // Euler characteristic: V - E + F = 2 - 2g
    int  BoundaryLoopCount() const;

    // Per-vertex valence (number of incident edges)
    int  Valence(int vertexIdx) const;

    // Collect one-ring neighbours of vertex
    std::vector<int> OneRing(int vertexIdx) const;

    // Return all boundary edge chains as ordered vertex sequences
    std::vector<std::vector<int>> BoundaryLoops() const;

    // Count isolated vertices (no incident edges)
    int IsolatedVertexCount() const;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Manifold repair
// ─────────────────────────────────────────────────────────────────────────────
namespace TopologyRepair {
    // Fill all boundary loops with fan-triangulation caps
    HEMesh FillHoles(const HEMesh& mesh);

    // Remove duplicate vertices within epsilon
    HEMesh WeldVertices(const HEMesh& mesh, float eps = 1e-5f);

    // Remove degenerate (zero-area) triangles
    HEMesh RemoveDegenerates(const HEMesh& mesh, float areaEps = 1e-8f);

    // Flip face windings so all normals point outward (flood-fill consistency)
    HEMesh MakeConsistentWinding(const HEMesh& mesh);
}

// ─────────────────────────────────────────────────────────────────────────────
//  MeshAnalyzer  —  Diagnostics / stats on a triangular mesh
// ─────────────────────────────────────────────────────────────────────────────
struct MeshStats {
    int    vertexCount;
    int    triangleCount;
    int    edgeCount;
    int    genus;
    int    boundaryLoops;
    bool   isManifold;
    bool   isClosed;
    float  surfaceArea;   // m²
    float  volume;        // m³  (only meaningful for closed manifold)
    Vec3   centroid;
    Vec3   aabbMin, aabbMax;
};

class MeshAnalyzer {
public:
    static MeshStats   Analyze(const MeshData& mesh);
    static float       SurfaceArea(const MeshData& mesh);
    static float       Volume(const MeshData& mesh);        // signed, assumes closed
    static Vec3        Centroid(const MeshData& mesh);

    // Returns average dihedral angle deviation from flat (0 = flat plane)
    static float       AverageCurvature(const HEMesh& he);

    // Detect self-intersecting triangle pairs (expensive O(n²) naive check)
    static std::vector<std::pair<int,int>> FindSelfIntersections(const MeshData& mesh);
};

} // namespace gv
