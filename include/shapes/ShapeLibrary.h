#pragma once
#include "geometry/Primitives.h"
#include "geometry/MeshBuilder.h"
#include "core/Math.h"
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  ShapeLibrary.h  —  High-level complex shape factories built from primitives
//
//  Each function returns a ready-to-upload MeshData with smooth normals.
//  Dimensions are in metres unless noted.
// ─────────────────────────────────────────────────────────────────────────────

namespace Shapes {

    // ── Architectural ─────────────────────────────────────────────────────────
    MeshData StairCase(int steps, float stepW, float stepH, float stepD);
    MeshData ArchedDoorway(float width, float height, float depth, int archSegs = 12);
    MeshData Column(float radius, float height, int segs = 16, bool addBaseCap = true);
    MeshData RoofGable(float baseW, float baseD, float ridgeH, float overhang = 0.3f);
    MeshData RoofHip(float baseW, float baseD, float ridgeH, float ridgeLen);
    MeshData Window(float width, float height, float frameThickness = 0.05f);
    MeshData WallPanel(float width, float height, float depth = 0.2f);
    MeshData Fence(int posts, float postSpacing, float postH, float railRadius = 0.03f);

    // ── Furniture ─────────────────────────────────────────────────────────────
    MeshData Chair(float seatH = 0.45f);
    MeshData Table(float w = 1.2f, float d = 0.6f, float h = 0.75f);
    MeshData Shelf(int shelves, float w = 1.0f, float h = 1.8f, float d = 0.3f);
    MeshData Bed(float w = 1.4f, float l = 2.0f, float mattressH = 0.2f);
    MeshData Sofa(float w = 2.0f, float d = 0.8f, float h = 0.8f);

    // ── Vehicles ──────────────────────────────────────────────────────────────
    MeshData CarBody(float length = 4.2f, float width = 1.8f, float height = 1.4f);
    MeshData CarWheel(float radius = 0.35f, float thickness = 0.2f, int segs = 20);
    MeshData CarCabin(float l = 2.2f, float w = 1.7f, float h = 0.7f);
    MeshData TruckCab(float l = 2.8f, float w = 2.0f, float h = 2.1f);
    MeshData TruckBed(float l = 4.0f, float w = 2.0f, float h = 0.5f);

    // ── Nature ────────────────────────────────────────────────────────────────
    struct TreeParams {
        float trunkRadius = 0.15f, trunkHeight = 2.5f;
        float crownRadius = 1.5f,  crownOffset = 2.0f;
        int   trunkSegs   = 8,     crownSegs   = 10;
    };
    MeshData Tree(const TreeParams& p = {});
    MeshData Rock(float radius = 1.0f, int roughness = 3, int seed = 0);
    MeshData Grass(float width = 0.05f, float height = 0.5f, int blades = 8);
    MeshData Bush(float radius = 0.6f, int segs = 6, int seed = 0);

    // ── Mechanical / Tech ────────────────────────────────────────────────────
    MeshData Gear(int teeth, float outerR, float innerR, float thickness,
                  float toothDepth, float pressureAngleDeg = 20.0f);
    MeshData SpringCoil(float outerR, float wireR, int coils, float totalH, int segs = 8);
    MeshData Pipe(float outerR, float innerR, float length, int radSegs = 16);
    MeshData Bolt(float headR, float shankR, float length, int segs = 12);

    // ── Terrain Features ─────────────────────────────────────────────────────
    MeshData Ramp(float width, float length, float height);
    MeshData Crater(float outerR, float innerR, float craterDepth, int segs = 24);
    MeshData Hill(float radius, float height, int segs = 16);
    MeshData Canyon(float length, float width, float depth, int segs = 20);

} // namespace Shapes
