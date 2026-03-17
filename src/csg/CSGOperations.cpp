#include "csg/CSGOperations.h"
// ─── CSG stubs ────────────────────────────────────────────────────────────────
// Full BSP-tree CSG is complex; these stubs return A until full implementation.

namespace CSG {
    MeshData Union(const MeshData& A, const MeshData& /*B*/)        { return A; }
    MeshData Difference(const MeshData& A, const MeshData& /*B*/)   { return A; }
    MeshData Intersection(const MeshData& A, const MeshData& /*B*/) { return A; }
}

MeshData HolePunch::Drill(const MeshData& mesh, const HoleSpec& /*spec*/) { return mesh; }
MeshData HolePunch::DrillMultiple(const MeshData& mesh, const std::vector<HoleSpec>& /*specs*/) { return mesh; }
MeshData HolePunch::CutProfilePath(const MeshData& mesh, const std::vector<Vec2>&, Vec3, Vec3, Vec3) { return mesh; }
