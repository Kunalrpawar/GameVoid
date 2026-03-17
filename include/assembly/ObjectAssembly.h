#pragma once
#include "geometry/Primitives.h"
#include "geometry/MeshBuilder.h"
#include "core/Math.h"
#include <vector>
#include <string>
#include <memory>
#include <functional>

namespace gv {

// ─────────────────────────────────────────────────────────────────────────────
//  ObjectAssembly.h  —  Compose complex objects from primitive parts
//
//  An AssemblyNode is one primitive with a local transform in the hierarchy.
//  Nodes can be nested (one assembly inside another), forming a tree.
//  The root node's local transform is identity; child transforms are relative.
//
//  Call Flatten() to bake the whole tree into a single MeshData ready for GPU.
// ─────────────────────────────────────────────────────────────────────────────

struct AssemblyNode {
    std::string             name;
    Vec3                    translation = Vec3(0,0,0);
    Vec3                    rotationDeg = Vec3(0,0,0);
    Vec3                    scale       = Vec3(1,1,1);
    Vec4                    color       = Vec4(1,1,1,1);

    // Leaf: holds mesh data directly
    bool                    hasMesh  = false;
    MeshData                mesh;

    // Children: sub-assemblies or primitive parts
    std::vector<std::shared_ptr<AssemblyNode>> children;

    // Convenience: add a primitive child
    AssemblyNode& AddChild(const std::string& childName, MeshData m,
                           Vec3 t = Vec3(0,0,0),
                           Vec3 r = Vec3(0,0,0),
                           Vec3 s = Vec3(1,1,1));

    // Flatten into world-space MeshData (applies all ancestor transforms)
    MeshData Flatten() const;

private:
    MeshData FlattenNode(const AssemblyNode& node, const Mat4& parentWorld) const;
};

// ─────────────────────────────────────────────────────────────────────────────
//  AssemblyBlueprint  —  Serializable description of one compound object.
//  AI systems generate these; the assembler instantiates them in the scene.
// ─────────────────────────────────────────────────────────────────────────────

struct PartEntry {
    std::string primType;   // "box", "sphere", "cylinder", "cone", "torus", "capsule"
    Vec3        translation;
    Vec3        rotationDeg;
    Vec3        scale;
    Vec4        color;
    // Primitive-specific params
    float p0 = 1.0f, p1 = 1.0f, p2 = 1.0f;  // e.g. radii, heights
    int   segs = 16;
};

struct AssemblyBlueprint {
    std::string             objectName;
    Vec3                    spawnPosition;
    float                   spawnHeadingDeg = 0.0f;
    std::vector<PartEntry>  parts;

    // Build the full AssemblyNode tree from this blueprint
    std::shared_ptr<AssemblyNode> Build() const;

    // Serialize / deserialize to a simple JSON-like text format
    std::string Serialize() const;
    static AssemblyBlueprint Deserialize(const std::string& text);

    // Pre-built named blueprints
    static AssemblyBlueprint Car(const std::string& name = "sedan");
    static AssemblyBlueprint Truck(const std::string& name = "truck");
    static AssemblyBlueprint Building(int floors = 3, const std::string& name = "building");
    static AssemblyBlueprint Tree(const std::string& name = "tree");
    static AssemblyBlueprint Table(const std::string& name = "table");
};

} // namespace gv
