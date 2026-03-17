#pragma once
#include "geometry/Primitives.h"
#include "geometry/MeshBuilder.h"
#include "core/Math.h"
#include <vector>
#include <string>
#include <memory>
#include <functional>

namespace gv {

// ObjectAssembly.h: compose complex objects from primitive parts.
// Call Flatten() to bake the tree into a single MeshData.

struct AssemblyNode {
    std::string             name;
    ::gv::Vec3              translation = ::gv::Vec3(0,0,0);
    ::gv::Vec3              rotationDeg = ::gv::Vec3(0,0,0);
    ::gv::Vec3              scale       = ::gv::Vec3(1,1,1);
    ::gv::Vec4              color       = ::gv::Vec4(1,1,1,1);

    // Leaf: holds mesh data directly
    bool                    hasMesh  = false;
    MeshData                mesh;

    // Children: sub-assemblies or primitive parts
    std::vector<std::shared_ptr<AssemblyNode>> children;

    // Convenience: add a primitive child
    AssemblyNode& AddChild(const std::string& childName, MeshData m,
                           ::gv::Vec3 t = ::gv::Vec3(0,0,0),
                           ::gv::Vec3 r = ::gv::Vec3(0,0,0),
                           ::gv::Vec3 s = ::gv::Vec3(1,1,1));

    // Flatten into world-space MeshData (applies all ancestor transforms)
    MeshData Flatten() const;

private:
    MeshData FlattenNode(const AssemblyNode& node, const ::gv::Mat4& parentWorld) const;
};

// AssemblyBlueprint: serializable description of one compound object.

struct PartEntry {
    std::string primType;   // "box", "sphere", "cylinder", "cone", "torus", "capsule"
    ::gv::Vec3  translation;
    ::gv::Vec3  rotationDeg;
    ::gv::Vec3  scale;
    ::gv::Vec4  color;
    // Primitive-specific params
    float p0 = 1.0f, p1 = 1.0f, p2 = 1.0f;  // e.g. radii, heights
    int   segs = 16;
};

struct AssemblyBlueprint {
    std::string             objectName;
    ::gv::Vec3              spawnPosition;
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
