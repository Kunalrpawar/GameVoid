#pragma once
#include <vector>
#include <string>
#include "core/Math.h"
#include "core/Types.h"

// VehicleAssembly — describes how a multi-mesh vehicle is constructed from sub-parts.
// The root GameObject holds a single CarController3D + RigidBody (compound body).
// All sub-part GameObjects are purely visual — no individual physics shapes.

struct VehiclePartDef {
    std::string name;         // e.g. "front_left_wheel"
    Vec3        offset;       // relative to car root origin
    Vec3        rotationDeg;  // local rotation
    Vec3        scale;        // local scale
    Vec4        color;        // RGBA mesh tint
    bool        isWheel;      // enable auto-spin when car moves
    float       wheelRadius;  // used for visual spin rpm
};

struct VehicleBlueprint {
    std::string                  vehicleName;    // e.g. "sedan"
    Vec3                         rootPosition;   // world spawn position
    float                        initialHeading; // degrees
    std::vector<VehiclePartDef>  parts;

    // Pre-built sedan template (approx 5m x 1.4m x 2m bounding box)
    static VehicleBlueprint MakeSedanTemplate();

    // Pre-built truck template
    static VehicleBlueprint MakeTruckTemplate();

    // Pre-built sportscar template
    static VehicleBlueprint MakeSportsCarTemplate();
};

// VehicleAssembler — spawns the full hierarchy from a VehicleBlueprint.
// Returns the root car_body GameObject; callers should store it.
class Scene;
class GameObject;

class VehicleAssembler {
public:
    // Spawn vehicle into scene, return root car_body object
    static GameObject* Spawn(Scene* scene, const VehicleBlueprint& blueprint,
                              class PhysicsWorld* physics = nullptr);

    // Attach visual wheels as child objects and record them for spin updates
    static void AttachWheels(GameObject* carRoot, const VehicleBlueprint& blueprint);

    // Call each frame during play to spin wheel meshes based on car speed
    static void UpdateWheelSpin(GameObject* carRoot, float speed, float dt);
};
