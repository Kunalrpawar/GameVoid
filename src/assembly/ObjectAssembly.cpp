#include "assembly/ObjectAssembly.h"
#include "geometry/Primitives.h"
#include <cstdio>

// ─── AssemblyNode ─────────────────────────────────────────────────────────────

AssemblyNode& AssemblyNode::AddChild(const std::string& n, MeshData m, Vec3 t, Vec3 r, Vec3 s) {
    auto child = std::make_shared<AssemblyNode>();
    child->name = n; child->translation = t; child->rotationDeg = r; child->scale = s;
    child->hasMesh = true; child->mesh = std::move(m);
    children.push_back(child);
    return *child;
}

static Mat4 BuildMat4(Vec3 t, Vec3 /*r*/, Vec3 s) {
    Mat4 m = Mat4(1.0f);
    // Simplified: just scale and translate
    m[0][0] = s.x; m[1][1] = s.y; m[2][2] = s.z;
    m[3][0] = t.x; m[3][1] = t.y; m[3][2] = t.z;
    return m;
}

MeshData AssemblyNode::FlattenNode(const AssemblyNode& node, const Mat4& parentWorld) const {
    MeshData out;
    if (node.hasMesh) {
        for (auto& v : node.mesh.vertices) {
            Vertex3D nv = v;
            nv.position.x = parentWorld[0][0]*v.position.x + parentWorld[3][0];
            nv.position.y = parentWorld[1][1]*v.position.y + parentWorld[3][1];
            nv.position.z = parentWorld[2][2]*v.position.z + parentWorld[3][2];
            out.vertices.push_back(nv);
        }
        out.indices = node.mesh.indices;
    }
    for (auto& c : node.children) {
        Mat4 childWorld = BuildMat4(c->translation, c->rotationDeg, c->scale);
        MeshData childMesh = FlattenNode(*c, childWorld);
        uint32_t off = (uint32_t)out.vertices.size();
        for (auto& v : childMesh.vertices) out.vertices.push_back(v);
        for (auto idx : childMesh.indices) out.indices.push_back(idx + off);
    }
    return out;
}

MeshData AssemblyNode::Flatten() const {
    Mat4 identity = Mat4(1.0f);
    return FlattenNode(*this, identity);
}

// ─── AssemblyBlueprint ────────────────────────────────────────────────────────

std::shared_ptr<AssemblyNode> AssemblyBlueprint::Build() const {
    auto root = std::make_shared<AssemblyNode>();
    root->name = objectName;
    for (auto& p : parts) {
        MeshData m;
        if      (p.primType == "box")      m = Primitives::Box(p.p0, p.p1, p.p2);
        else if (p.primType == "sphere")   m = Primitives::Sphere(p.p0, p.segs, p.segs);
        else if (p.primType == "cylinder") m = Primitives::Cylinder(p.p0, p.p0, p.p1, p.segs);
        else if (p.primType == "cone")     m = Primitives::Cone(p.p0, p.p1, p.segs);
        else if (p.primType == "torus")    m = Primitives::Torus(p.p0, p.p1, p.segs, p.segs/2);
        else                               m = Primitives::Box(p.p0, p.p1, p.p2);
        root->AddChild(p.primType, std::move(m), p.translation, p.rotationDeg, p.scale);
    }
    return root;
}

std::string AssemblyBlueprint::Serialize() const { return "{}"; }
AssemblyBlueprint AssemblyBlueprint::Deserialize(const std::string&) { return {}; }

AssemblyBlueprint AssemblyBlueprint::Car(const std::string& name) {
    AssemblyBlueprint bp; bp.objectName = name;
    bp.parts.push_back({"box",  {0, 0,    0}, {0,0,0}, {1,1,1}, {0.8f,0.4f,1.0f,1}, 1.8f, 0.4f, 4.2f, 1});  // body
    bp.parts.push_back({"box",  {0, 0.45f, 0.3f}, {0,0,0}, {1,1,1}, {0.6f,0.7f,0.9f,1}, 1.6f, 0.5f, 2.2f, 1});  // cabin
    return bp;
}

AssemblyBlueprint AssemblyBlueprint::Building(int floors, const std::string& name) {
    AssemblyBlueprint bp; bp.objectName = name;
    for (int i = 0; i < floors; i++)
        bp.parts.push_back({"box", {0, i*3.0f, 0}, {0,0,0}, {1,1,1}, {0.9f,0.9f,0.85f,1}, 5.0f, 3.0f, 5.0f, 1});
    return bp;
}

AssemblyBlueprint AssemblyBlueprint::Truck(const std::string& n)  { return Car(n); }
AssemblyBlueprint AssemblyBlueprint::Tree(const std::string& n) {
    AssemblyBlueprint bp; bp.objectName = n;
    bp.parts.push_back({"cylinder", {0,-1.25f,0}, {0,0,0}, {1,1,1}, {0.5f,0.3f,0.1f,1}, 0.15f, 2.5f, 0, 8});
    bp.parts.push_back({"sphere",   {0, 1.0f, 0}, {0,0,0}, {1,1,1}, {0.2f,0.6f,0.15f,1}, 1.5f, 0, 0, 8});
    return bp;
}
AssemblyBlueprint AssemblyBlueprint::Table(const std::string& n) {
    AssemblyBlueprint bp; bp.objectName = n;
    bp.parts.push_back({"box", {0,0.35f,0},{0,0,0},{1,1,1},{0.7f,0.5f,0.3f,1}, 1.2f,0.05f,0.6f,1}); // top
    return bp;
}
