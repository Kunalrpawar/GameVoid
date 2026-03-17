#pragma once
#include "geometry/Primitives.h"
#include "core/Math.h"
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  CSGOperations.h  —  Constructive Solid Geometry
//
//  Operations: Union, Difference, Intersection, HolePunch
//
//  Internally uses a BSP-tree approach:
//    1. Split faces of A by each plane of B
//    2. Classify fragments as inside / outside / coincident
//    3. Keep / discard based on operation type
//    4. Re-merge and re-weld result
//
//  All inputs should be closed manifold meshes for correct results.
// ─────────────────────────────────────────────────────────────────────────────

namespace CSG {

    // A ∪ B  — combine both solids, remove internal faces
    MeshData Union(const MeshData& A, const MeshData& B);

    // A − B  — subtract B from A, creating a cavity
    MeshData Difference(const MeshData& A, const MeshData& B);

    // A ∩ B  — retain only the overlapping region
    MeshData Intersection(const MeshData& A, const MeshData& B);

} // namespace CSG

// ─────────────────────────────────────────────────────────────────────────────
//  HolePunch  —  Drill through-holes and arbitrary cutouts into solid meshes
// ─────────────────────────────────────────────────────────────────────────────

struct HoleSpec {
    enum class Shape {
        Circle,
        Rectangle,
        Polygon,    // arbitrary planar polygon, specify vertices below
    };

    Shape       shape        = Shape::Circle;
    Vec3        center;                    // world-space center of hole axis
    Vec3        normal        = Vec3(0,1,0); // hole drilling direction
    float       radius        = 0.25f;     // Circle only
    float       width         = 0.5f;      // Rectangle only (local X)
    float       height        = 0.5f;      // Rectangle only (local Y)
    int         radialSegs    = 16;        // Circle tessellation resolution
    float       depth         = 999.0f;   // Max drill depth (use large value for through-holes)
    bool        bevelEdge     = false;     // Round hole edge (chamfer)
    float       bevelWidth    = 0.05f;
    std::vector<Vec2> polygonPoints;       // Polygon shape only
};

class HolePunch {
public:
    // Drill a single hole through the mesh
    static MeshData Drill(const MeshData& mesh, const HoleSpec& spec);

    // Drill multiple holes in one pass (more efficient than repeated Drill calls)
    static MeshData DrillMultiple(const MeshData& mesh, const std::vector<HoleSpec>& specs);

    // Cut an arbitrary planar cross-section (like a cookie cutter pressed down)
    static MeshData CutProfilePath(const MeshData& mesh,
                                    const std::vector<Vec2>& profilePoints,
                                    Vec3 axisDir, Vec3 startPoint, Vec3 endPoint);
};
