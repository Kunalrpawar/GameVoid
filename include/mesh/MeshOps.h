#pragma once
#include "geometry/Primitives.h"
#include "core/Math.h"
#include <vector>
#include <functional>

// ─────────────────────────────────────────────────────────────────────────────
//  MeshOps.h  —  Post-process operations on MeshData
//
//  All operations return NEW MeshData (non-destructive) unless the in-place
//  variant (suffix I) is used.
// ─────────────────────────────────────────────────────────────────────────────

namespace MeshOps {

    // ── Combine ──────────────────────────────────────────────────────────────
    // Merge multiple meshes into one; indices are re-based per mesh.
    MeshData Combine(const std::vector<MeshData>& meshes);

    // Same but also apply a transform to each mesh before merging
    MeshData CombineTransformed(const std::vector<std::pair<MeshData, Mat4>>& pairs);

    // ── Weld ─────────────────────────────────────────────────────────────────
    // Merge vertices that are within `tolerance` of each other in position.
    MeshData Weld(const MeshData& mesh, float tolerance = 1e-5f);

    // ── Subdivision ──────────────────────────────────────────────────────────
    // Loop subdivision  (smooth, for triangular meshes — doubles tri count each pass)
    MeshData SubdivideLoop(const MeshData& mesh, int passes = 1);

    // Catmull-Clark subdivision (works on arbitrary polygons, converted from tris)
    MeshData SubdivideCatmullClark(const MeshData& mesh, int passes = 1);

    // Midpoint subdivision with flat interpolation (no smoothing)
    MeshData SubdivideMidpoint(const MeshData& mesh, int passes = 1);

    // ── Smooth / Relax ────────────────────────────────────────────────────────
    // Laplacian smoothing: move each vertex toward the average of its neighbours.
    //   strength    0–1 (0 = no change, 1 = fully average)
    //   iterations  number of passes
    //   pinBoundary keep boundary vertices fixed if true
    MeshData LaplacianSmooth(const MeshData& mesh, float strength = 0.5f,
                              int iterations = 3, bool pinBoundary = true);

    // ── Decimation ────────────────────────────────────────────────────────────
    // Reduce triangle count to approx. targetTriCount using edge-collapse.
    MeshData Decimate(const MeshData& mesh, int targetTriCount);

    // ── Extrude ───────────────────────────────────────────────────────────────
    // Extrude all faces by `amount` along their normals.
    MeshData ExtrudeNormals(const MeshData& mesh, float amount);

    // Extrude a closed polygon profile along a path curve
    struct PathPoint { Vec3 pos; float twist = 0.0f; float scale = 1.0f; };
    MeshData SweepProfile(const std::vector<Vec2>& profile2D,
                           const std::vector<PathPoint>& path,
                           bool closedPath = false);

    // ── Boolean helpers ───────────────────────────────────────────────────────
    // Clip mesh by a plane (halfspace: normal dotted with (P - point) > 0 kept)
    MeshData ClipByPlane(const MeshData& mesh, Vec3 planeNormal, Vec3 planePoint);

    // ── UV mapping ────────────────────────────────────────────────────────────
    // Project UVs onto the XZ plane (top-down atlas)
    MeshData ProjectUVsXZ(const MeshData& mesh, float tileU = 1.0f, float tileV = 1.0f);

    // Box/cubic UV projection
    MeshData ProjectUVsBox(const MeshData& mesh, float tileScale = 1.0f);

    // ── Per-vertex operations ─────────────────────────────────────────────────
    MeshData TransformVertices(const MeshData& mesh, const Mat4& mat);
    MeshData ApplyNoise(const MeshData& mesh,
                         std::function<float(Vec3)> noiseFn, float strength);

} // namespace MeshOps
