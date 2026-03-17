#include "vehicle/VehicleAssembly.h"
#include "vehicle/CarController3D.h"
#include "core/Scene.h"
#include "core/GameObject.h"
#include "physics/Physics.h"
#include "renderer/MeshRenderer.h"
#include <cmath>

using namespace gv;

/* ─── VehicleBlueprint pre-built templates ─────────────────────────────── */

VehicleBlueprint VehicleBlueprint::MakeSedanTemplate() {
    VehicleBlueprint bp;
    bp.vehicleName    = "sedan";
    bp.rootPosition   = Vec3(0, 0.4f, 0);
    bp.initialHeading = 0.0f;

    // Car body (root — gets physics)
    VehiclePartDef body;
    body.name = "car_body"; body.offset = Vec3(0,0,0);
    body.scale = Vec3(1.8f, 0.4f, 4.2f); body.color = Vec4(0.2f, 0.5f, 0.9f, 1);
    body.isWheel = false; bp.parts.push_back(body);

    // Cabin
    VehiclePartDef cabin;
    cabin.name = "car_cabin"; cabin.offset = Vec3(0, 0.45f, 0.3f);
    cabin.scale = Vec3(1.6f, 0.5f, 2.2f); cabin.color = Vec4(0.25f, 0.25f, 0.3f, 1);
    cabin.isWheel = false; bp.parts.push_back(cabin);

    // Wheels (visual only, no physics)
    auto makeWheel = [&](const char* n, Vec3 o, bool isW) {
        VehiclePartDef w;
        w.name = n; w.offset = o; w.rotationDeg = Vec3(90,0,0);
        w.scale = Vec3(0.35f, 0.35f, 0.2f); w.color = Vec4(0.15f, 0.15f, 0.15f, 1);
        w.isWheel = isW; w.wheelRadius = 0.35f;
        bp.parts.push_back(w);
    };
    makeWheel("front_left_wheel",  Vec3(-0.95f, -0.4f,  1.4f), true);
    makeWheel("front_right_wheel", Vec3( 0.95f, -0.4f,  1.4f), true);
    makeWheel("rear_left_wheel",   Vec3(-0.95f, -0.4f, -1.4f), true);
    makeWheel("rear_right_wheel",  Vec3( 0.95f, -0.4f, -1.4f), true);
    return bp;
}

VehicleBlueprint VehicleBlueprint::MakeTruckTemplate() {
    VehicleBlueprint bp = MakeSedanTemplate();
    bp.vehicleName = "truck";
    // Scale body larger for truck
    bp.parts[0].scale = Vec3(2.0f, 0.5f, 6.0f);
    return bp;
}

VehicleBlueprint VehicleBlueprint::MakeSportsCarTemplate() {
    VehicleBlueprint bp = MakeSedanTemplate();
    bp.vehicleName = "sportscar";
    bp.parts[0].color = Vec4(0.9f, 0.1f, 0.1f, 1);  // red
    return bp;
}

/* ─── VehicleAssembler ─────────────────────────────────────────────────────*/

GameObject* VehicleAssembler::Spawn(Scene* scene, const VehicleBlueprint& blueprint,
                                     PhysicsWorld* physics) {
    if (!scene) return nullptr;

    GameObject* root = nullptr;

    for (const auto& part : blueprint.parts) {
        auto* obj = scene->CreateGameObject(part.name);
        Vec3 worldPos = blueprint.rootPosition + part.offset;
        obj->GetTransform().SetPosition(worldPos.x, worldPos.y, worldPos.z);
        obj->GetTransform().SetEulerDeg(part.rotationDeg.x, part.rotationDeg.y, part.rotationDeg.z);
        obj->GetTransform().SetScale(part.scale.x, part.scale.y, part.scale.z);

        auto* mr = obj->AddComponent<MeshRenderer>();
        mr->primitiveType = PrimitiveType::Cube;
        mr->color = part.color;

        if (part.name == blueprint.vehicleName + "_body" || part.name == "car_body") {
            auto* rb = obj->AddComponent<RigidBody>();
            rb->useGravity    = true;
            rb->angularDrag   = 0.95f;
            rb->drag          = 0.2f;
            auto* col = obj->AddComponent<Collider>();
            col->type = ColliderType::Box;
            col->boxHalfExtents = Vec3(part.scale.x * 0.5f, part.scale.y * 0.5f, part.scale.z * 0.5f);
            if (physics) physics->RegisterBody(rb);
            obj->AddComponent<CarController3D>();
            root = obj;
        }
    }
    return root;
}

void VehicleAssembler::AttachWheels(GameObject* /*carRoot*/, const VehicleBlueprint& /*bp*/) {
    // Wheel parenting stubbed until hierarchy system is implemented
}

void VehicleAssembler::UpdateWheelSpin(GameObject* /*carRoot*/, float /*speed*/, float /*dt*/) {
    // Wheel visual spin stubbed; implement per-frame transform rotation
}
