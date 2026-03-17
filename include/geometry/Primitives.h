#pragma once
#include "core/Math.h"
#include "core/Types.h"
#include <vector>
#include <string>
#include <cstdint>
#include <cmath>

namespace gv {

// Primitives.h: Base geometric primitive definitions.
// All primitives produce indexed triangle meshes (positions + normals + UVs).

struct Vertex3D {
    ::gv::Vec3 position;
    ::gv::Vec3 normal;
    ::gv::Vec2 uv;
};

struct MeshData {
    std::vector<Vertex3D>   vertices;
    std::vector<uint32_t>   indices;  // triangles
    std::string             name;

    // Recalculate flat normals from triangle windings
    void RecalcFlatNormals();
    // Recalculate smooth normals (averaged per vertex)
    void RecalcSmoothNormals();
    // Flip all normals
    void FlipNormals();
    // Compute axis-aligned bounding box
    void ComputeAABB(::gv::Vec3& outMin, ::gv::Vec3& outMax) const;
    // Count triangles
    size_t TriangleCount() const { return indices.size() / 3; }
};

// Primitive builders
namespace Primitives {

    // Box  (hx, hy, hz = half-extents on each axis)
    MeshData Box(float hx = 0.5f, float hy = 0.5f, float hz = 0.5f, int subdivisions = 0);

    // Sphere (radius, latSegments, lonSegments) — UV-sphere
    MeshData Sphere(float radius = 1.0f, int latSegs = 16, int lonSegs = 16);

    // Cylinder (top/bottom radius, height, radial segments, cap segments)
    MeshData Cylinder(float radiusTop = 1.0f, float radiusBot = 1.0f,
                      float height = 2.0f, int radSegs = 16, bool caps = true);

    // Cone  (base radius, height, segments)
    MeshData Cone(float radius = 1.0f, float height = 2.0f, int segs = 16);

    // Torus  (outer radius R, tube radius r, major segs, minor segs)
    MeshData Torus(float R = 1.0f, float r = 0.25f, int majorSegs = 24, int minorSegs = 12);

    // Plane  (width, depth, subdivisions each axis)
    MeshData Plane(float width = 2.0f, float depth = 2.0f, int segsX = 1, int segsZ = 1);

    // Capsule  (radius, cylindrical height, segments)
    MeshData Capsule(float radius = 0.5f, float height = 1.0f, int segs = 16);

    // Pyramid  (base width, base depth, apex height)
    MeshData Pyramid(float baseW = 1.0f, float baseD = 1.0f, float apexH = 2.0f);

    // Disk  (outer, inner radius for ring; inner=0 for solid disk)
    MeshData Disk(float outerR = 1.0f, float innerR = 0.0f, int segs = 16);

    // Arrow  (shaft radius, head radius, total length, head fraction 0-1)
    MeshData Arrow(float shaftR = 0.05f, float headR = 0.15f,
                   float length = 1.0f, float headFraction = 0.3f);

} // namespace Primitives

} // namespace gv
