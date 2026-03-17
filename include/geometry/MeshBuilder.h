#pragma once
#include "Primitives.h"
#include "core/Math.h"
#include <vector>
#include <functional>

namespace gv {

// MeshBuilder.h: Fluent API for assembling complex meshes from primitives.
// Usage example:
//    MeshData car = MeshBuilder()
//        .AddPrimitive(Primitives::Box(2.0f, 0.4f, 4.0f), Vec3(0,0,0))   // body
//        .AddPrimitive(Primitives::Box(1.5f, 0.5f, 2.0f), Vec3(0,0.45f,0.3f)) // cabin
//        .AddPrimitive(Primitives::Cylinder(0.35f,0.35f,0.2f,16), Vec3( 0.9f,0,1.3f)) // wheel
//        .Merge()
//        .ComputeSmoothNormals()
//        .Build();
//

class MeshBuilder {
public:
    struct Entry {
        MeshData mesh;
        ::gv::Vec3     translation;
        ::gv::Vec3     rotationDeg;  // XYZ Euler
        ::gv::Vec3     scale;
    };

    MeshBuilder() = default;

    // Add a primitive with an optional transform
    MeshBuilder& AddPrimitive(MeshData mesh,
                              ::gv::Vec3 translation = ::gv::Vec3(0,0,0),
                              ::gv::Vec3 rotationDeg = ::gv::Vec3(0,0,0),
                              ::gv::Vec3 scale       = ::gv::Vec3(1,1,1));

    // Merge all entries into a single MeshData (indices re-based)
    MeshBuilder& Merge();

    // Post-process on the merged result
    MeshBuilder& ComputeFlatNormals();
    MeshBuilder& ComputeSmoothNormals();
    MeshBuilder& FlipNormals();
    MeshBuilder& Translate(::gv::Vec3 offset);
    MeshBuilder& Scale(::gv::Vec3 factor);
    MeshBuilder& RotateEuler(::gv::Vec3 deg);  // applies to whole combined mesh

    // Apply a per-vertex lambda: void(Vertex3D&)
    MeshBuilder& ForEachVertex(std::function<void(Vertex3D&)> fn);

    // Weld duplicate vertices by position within tolerance
    MeshBuilder& WeldVertices(float tolerance = 1e-5f);

    // Center the mesh at its centroid
    MeshBuilder& Center();

    // Retrieve the final mesh
    MeshData Build(const std::string& name = "") const;

private:
    std::vector<Entry> m_Entries;
    MeshData           m_Merged;
    bool               m_HasMerged = false;

    static MeshData TransformMesh(MeshData mesh, ::gv::Vec3 t, ::gv::Vec3 rDeg, ::gv::Vec3 s);
};

} // namespace gv
