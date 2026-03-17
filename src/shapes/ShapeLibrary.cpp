#include "shapes/ShapeLibrary.h"
#include "geometry/Primitives.h"
#include "geometry/MeshBuilder.h"
#include <cmath>

namespace Shapes {

static const float kPiSh = 3.14159265358979323846f;

// ── Architectural ─────────────────────────────────────────────────────────────

MeshData StairCase(int steps, float stepW, float stepH, float stepD) {
    MeshBuilder b;
    for (int i = 0; i < steps; i++) {
        float y = i * stepH;
        float z = i * stepD;
        b.AddPrimitive(Primitives::Box(stepW/2.0f, stepH/2.0f, stepD/2.0f), Vec3(0, y + stepH/2.0f, z - stepD/2.0f));
    }
    return b.Merge().ComputeFlatNormals().Build("Staircase");
}

MeshData ArchedDoorway(float width, float height, float depth, int archSegs) {
    MeshBuilder b;
    // Left pillar
    b.AddPrimitive(Primitives::Box(width*0.1f, height*0.6f, depth), Vec3(-width/2.0f + width*0.05f, height*0.3f, 0));
    // Right pillar
    b.AddPrimitive(Primitives::Box(width*0.1f, height*0.6f, depth), Vec3( width/2.0f - width*0.05f, height*0.3f, 0));
    // Arch top (half torus)
    float R = width * 0.4f, r = width * 0.05f;
    for (int i = 0; i < archSegs; i++) {
        float a0 = kPiSh * i / archSegs, a1 = kPiSh * (i+1) / archSegs;
        float x0 = R * std::cos(a0), y0 = height * 0.6f + R * std::sin(a0);
        Vec3 segPos((x0 + R*std::cos(a1))*0.5f, (y0 + height*0.6f + R*std::sin(a1))*0.5f, 0);
        b.AddPrimitive(Primitives::Sphere(r, 4, 6), segPos);
    }
    return b.Merge().Build("ArchedDoorway");
}

MeshData Column(float radius, float height, int segs, bool addBaseCap) {
    MeshBuilder b;
    b.AddPrimitive(Primitives::Cylinder(radius, radius, height, segs, addBaseCap), Vec3(0, height/2.0f, 0));
    // Capital
    b.AddPrimitive(Primitives::Box(radius*1.8f, radius*0.4f, radius*1.8f), Vec3(0, height + radius*0.2f, 0));
    return b.Merge().ComputeSmoothNormals().Build("Column");
}

MeshData RoofGable(float bw, float bd, float rh, float overhang) {
    MeshBuilder b;
    b.AddPrimitive(Primitives::Box((bw+overhang)/2.0f, rh/2.0f, (bd+overhang)/2.0f), Vec3(0, rh/2.0f, 0));
    return b.Merge().Build("RoofGable");
}

MeshData RoofHip(float bw, float bd, float rh, float /*ridgeLen*/) { return RoofGable(bw, bd, rh, 0.2f); }

MeshData Window(float w, float h, float thick) {
    MeshBuilder b;
    // Frame
    b.AddPrimitive(Primitives::Box(w/2.0f, thick, h/2.0f), Vec3(0,0,0));
    return b.Merge().Build("Window");
}

MeshData WallPanel(float w, float h, float d) {
    return MeshBuilder().AddPrimitive(Primitives::Box(w/2,h/2,d/2)).Merge().Build("WallPanel");
}

MeshData Fence(int posts, float spacing, float postH, float railR) {
    MeshBuilder b;
    for (int i = 0; i < posts; i++) {
        float x = i * spacing;
        b.AddPrimitive(Primitives::Box(0.05f, postH/2.0f, 0.05f), Vec3(x, postH/2.0f, 0));
        if (i < posts-1)
            b.AddPrimitive(Primitives::Cylinder(railR, railR, spacing, 4, false), Vec3(x + spacing/2.0f, postH*0.8f, 0), Vec3(0,0,90));
    }
    return b.Merge().Build("Fence");
}

// ── Furniture ─────────────────────────────────────────────────────────────────

MeshData Chair(float seatH) {
    MeshBuilder b;
    b.AddPrimitive(Primitives::Box(0.225f, 0.025f, 0.225f), Vec3(0, seatH, 0));
    for (int i=0;i<4;i++) {
        float sx = (i%2==0?-1:1)*0.2f, sz = (i<2?-1:1)*0.2f;
        b.AddPrimitive(Primitives::Box(0.02f, seatH/2.0f, 0.02f), Vec3(sx, seatH/2.0f, sz));
    }
    b.AddPrimitive(Primitives::Box(0.225f, 0.25f, 0.025f), Vec3(0, seatH + 0.25f, 0.225f));
    return b.Merge().Build("Chair");
}

MeshData Table(float w, float d, float h) {
    MeshBuilder b;
    b.AddPrimitive(Primitives::Box(w/2.0f, 0.03f, d/2.0f), Vec3(0, h, 0));
    for (int i=0;i<4;i++) {
        float sx = (i%2==0?-1:1)*(w*0.45f), sz = (i<2?-1:1)*(d*0.45f);
        b.AddPrimitive(Primitives::Box(0.025f, h/2.0f, 0.025f), Vec3(sx, h/2.0f, sz));
    }
    return b.Merge().Build("Table");
}

MeshData Shelf(int shelves, float w, float h, float d) {
    MeshBuilder b;
    b.AddPrimitive(Primitives::Box(0.02f, h/2.0f, d/2.0f), Vec3(-w/2.0f, h/2.0f, 0));
    b.AddPrimitive(Primitives::Box(0.02f, h/2.0f, d/2.0f), Vec3( w/2.0f, h/2.0f, 0));
    for (int i=0;i<=shelves;i++) b.AddPrimitive(Primitives::Box(w/2.0f, 0.02f, d/2.0f), Vec3(0, h*i/shelves, 0));
    return b.Merge().Build("Shelf");
}

MeshData Bed(float w, float l, float mh) {
    MeshBuilder b;
    b.AddPrimitive(Primitives::Box(w/2.0f, mh/2.0f, l/2.0f), Vec3(0, mh/2.0f, 0));
    b.AddPrimitive(Primitives::Box(w/2.0f, 0.4f, 0.05f), Vec3(0, 0.4f, -l/2.0f));  // headboard
    return b.Merge().Build("Bed");
}

MeshData Sofa(float w, float d, float h) {
    MeshBuilder b;
    b.AddPrimitive(Primitives::Box(w/2.0f, h*0.2f, d/2.0f), Vec3(0, h*0.2f, 0));      // seat
    b.AddPrimitive(Primitives::Box(w/2.0f, h*0.5f, 0.05f), Vec3(0, h*0.5f, d/2.0f));  // back
    return b.Merge().Build("Sofa");
}

// ── Vehicles ──────────────────────────────────────────────────────────────────

MeshData CarBody(float l, float w, float h) {
    return MeshBuilder().AddPrimitive(Primitives::Box(w/2, h*0.3f, l/2), Vec3(0,0,0))
                        .AddPrimitive(Primitives::Box(w/2*0.9f, h*0.35f, l/2*0.55f), Vec3(0, h*0.3f + h*0.175f, l*0.05f))
                        .Merge().ComputeSmoothNormals().Build("CarBody");
}

MeshData CarWheel(float r, float t, int segs) {
    return MeshBuilder().AddPrimitive(Primitives::Cylinder(r, r, t, segs), Vec3(0,0,0), Vec3(90,0,0))
                        .Merge().ComputeSmoothNormals().Build("CarWheel");
}

MeshData CarCabin(float l, float w, float h) {
    return MeshBuilder().AddPrimitive(Primitives::Box(w/2, h/2, l/2)).Merge().Build("CarCabin");
}

MeshData TruckCab(float l, float w, float h) {
    return MeshBuilder().AddPrimitive(Primitives::Box(w/2, h/2, l/2)).Merge().Build("TruckCab");
}

MeshData TruckBed(float l, float w, float h) {
    MeshBuilder b;
    // Bottom
    b.AddPrimitive(Primitives::Box(w/2, 0.05f, l/2), Vec3(0,0,0));
    // Sides
    b.AddPrimitive(Primitives::Box(0.05f, h/2, l/2), Vec3(-w/2,  h/2, 0));
    b.AddPrimitive(Primitives::Box(0.05f, h/2, l/2), Vec3( w/2,  h/2, 0));
    b.AddPrimitive(Primitives::Box(w/2, h/2, 0.05f), Vec3(0, h/2, -l/2));
    return b.Merge().Build("TruckBed");
}

// ── Nature ────────────────────────────────────────────────────────────────────

MeshData Tree(const TreeParams& p) {
    MeshBuilder b;
    b.AddPrimitive(Primitives::Cylinder(p.trunkRadius, p.trunkRadius, p.trunkHeight, p.trunkSegs), Vec3(0, p.trunkHeight/2, 0));
    b.AddPrimitive(Primitives::Sphere(p.crownRadius, p.crownSegs, p.crownSegs), Vec3(0, p.trunkHeight + p.crownOffset, 0));
    return b.Merge().ComputeSmoothNormals().Build("Tree");
}

MeshData Rock(float radius, int roughness, int) {
    MeshData m = Primitives::Sphere(radius, 6, 6);
    float scale = 0.15f * radius;
    for (auto& v : m.vertices) {
        v.position.x += (v.normal.x * scale * roughness * 0.333f);
        v.position.y += (v.normal.y * scale * roughness * 0.333f);
        v.position.z += (v.normal.z * scale * roughness * 0.333f);
    }
    m.RecalcSmoothNormals();
    m.name = "Rock";
    return m;
}

MeshData Grass(float w, float h, int blades) {
    MeshBuilder b;
    for (int i=0; i<blades; i++) {
        float a = i * kPiSh * 2.0f / blades;
        b.AddPrimitive(Primitives::Box(w/2, h/2, 0.005f), Vec3(std::cos(a)*0.1f, h/2.0f, std::sin(a)*0.1f), Vec3(0, a*180.0f/kPiSh, 0));
    }
    return b.Merge().Build("Grass");
}

MeshData Bush(float r, int segs, int) {
    MeshBuilder b;
    for (int i=0;i<segs;i++) {
        float a = i * kPiSh * 2.0f / segs;
        b.AddPrimitive(Primitives::Sphere(r*0.6f, 5, 5), Vec3(std::cos(a)*r*0.5f, r*0.4f, std::sin(a)*r*0.5f));
    }
    b.AddPrimitive(Primitives::Sphere(r*0.7f, 6, 6), Vec3(0, r*0.7f, 0));
    return b.Merge().ComputeSmoothNormals().Build("Bush");
}

// ── Mechanical ────────────────────────────────────────────────────────────────

MeshData Gear(int teeth, float outerR, float innerR, float thick, float toothD, float /*pressure*/) {
    MeshBuilder b;
    b.AddPrimitive(Primitives::Disk(innerR + (outerR-innerR)*0.5f, innerR, 16), Vec3(0,0,0));
    float toothW = kPiSh * outerR / (teeth * 3.0f);
    for (int i=0;i<teeth;i++) {
        float a = i * kPiSh * 2.0f / teeth;
        b.AddPrimitive(Primitives::Box(toothW/2, thick/2, toothD/2), Vec3(outerR*std::cos(a), 0, outerR*std::sin(a)), Vec3(0, a*180.0f/kPiSh, 0));
    }
    return b.Merge().Build("Gear");
}

MeshData SpringCoil(float outerR, float wireR, int coils, float totalH, int segs) {
    MeshBuilder b;
    int pts = coils * 16;
    float prevX=outerR, prevY=0, prevZ=0;
    for (int i=1;i<=pts;i++) {
        float t = (float)i / pts;
        float a = t * coils * kPiSh * 2.0f;
        float x = outerR * std::cos(a), y = t * totalH, z = outerR * std::sin(a);
        b.AddPrimitive(Primitives::Sphere(wireR, 4, segs), Vec3((x+prevX)*0.5f, (y+prevY)*0.5f, (z+prevZ)*0.5f));
        prevX=x; prevY=y; prevZ=z;
    }
    return b.Merge().Build("SpringCoil");
}

MeshData Pipe(float outerR, float innerR, float length, int segs) {
    MeshBuilder b;
    b.AddPrimitive(Primitives::Cylinder(outerR, outerR, length, segs, true));
    // Inner surface (flipped normals) for hollow effect
    b.AddPrimitive(Primitives::Cylinder(innerR, innerR, length, segs, false));
    return b.Merge().Build("Pipe");
}

MeshData Bolt(float headR, float shankR, float length, int segs) {
    MeshBuilder b;
    float headH = headR * 0.5f;
    b.AddPrimitive(Primitives::Cylinder(headR, headR, headH, 6, true), Vec3(0, length + headH/2, 0));   // hex head
    b.AddPrimitive(Primitives::Cylinder(shankR, shankR, length, segs, true), Vec3(0, length/2, 0));      // shank
    return b.Merge().Build("Bolt");
}

// ── Terrain Features ──────────────────────────────────────────────────────────

MeshData Ramp(float w, float l, float h) {
    MeshData m;
    // Two large triangles
    Vertex3D v0, v1, v2, v3;
    v0.position={-w/2, 0, -l/2}; v0.normal={0,1,0}; v0.uv={0,0};
    v1.position={ w/2, 0, -l/2}; v1.normal={0,1,0}; v1.uv={1,0};
    v2.position={ w/2, h,  l/2}; v2.normal={0,1,0}; v2.uv={1,1};
    v3.position={-w/2, h,  l/2}; v3.normal={0,1,0}; v3.uv={0,1};
    m.vertices = {v0,v1,v2,v3};
    m.indices  = {0,1,2, 0,2,3};
    m.RecalcFlatNormals();
    m.name = "Ramp";
    return m;
}

MeshData Crater(float outerR, float innerR, float depth, int segs) {
    MeshData m = Primitives::Disk(outerR, innerR, segs);
    // Push inner ring down
    for (auto& v : m.vertices) {
        float r = std::sqrt(v.position.x*v.position.x + v.position.z*v.position.z);
        if (r < innerR + 0.1f) v.position.y = -depth;
    }
    m.RecalcSmoothNormals();
    m.name = "Crater";
    return m;
}

MeshData Hill(float radius, float height, int segs) {
    MeshData m = Primitives::Sphere(radius, segs, segs);
    for (auto& v : m.vertices) { v.position.y = std::max(v.position.y, 0.0f); }
    m.RecalcSmoothNormals(); m.name = "Hill";
    return m;
}

MeshData Canyon(float length, float width, float depth, int segs) {
    MeshBuilder b;
    // Bottom
    b.AddPrimitive(Primitives::Box(width*0.25f, depth/2, length/2), Vec3(0, -depth/2, 0));
    // Side walls
    b.AddPrimitive(Primitives::Box(width*0.25f, depth/2, length/2), Vec3(-width/2, 0, 0));
    b.AddPrimitive(Primitives::Box(width*0.25f, depth/2, length/2), Vec3( width/2, 0, 0));
    (void)segs;
    return b.Merge().Build("Canyon");
}

} // namespace Shapes
