#include "generator/SceneBlueprint.h"
#include "core/Scene.h"
#include "core/GameObject.h"
#include "physics/Physics.h"
#include "renderer/MeshRenderer.h"
#include "vehicle/CarController3D.h"
#include <sstream>

// ─── SceneBlueprint3D ─────────────────────────────────────────────────────────

std::string SceneBlueprint3D::Serialize() const {
    std::ostringstream o;
    o << "scene:" << sceneName << "\n";
    for (auto& obj : objects) {
        o << "obj:" << obj.name << " type:" << obj.typeName
          << " pos:" << obj.position.x << "," << obj.position.y << "," << obj.position.z
          << " phys:" << obj.physicsRole << "\n";
    }
    return o.str();
}

SceneBlueprint3D SceneBlueprint3D::Deserialize(const std::string& /*data*/) {
    return {};  // Parsing stubbed; implement when format is finalised
}

SceneBlueprint3D SceneBlueprint3D::EmptyCity() {
    SceneBlueprint3D bp; bp.sceneName = "city";
    // Ground
    ObjectBlueprint3D ground; ground.name = "ground"; ground.typeName = "ground";
    ground.position = Vec3(0,0,0); ground.scale = Vec3(100, 0.2f, 100);
    ground.physicsRole = "static"; ground.color = Vec4(0.45f, 0.45f, 0.45f, 1);
    bp.objects.push_back(ground);
    return bp;
}

SceneBlueprint3D SceneBlueprint3D::CarParkScene() {
    SceneBlueprint3D bp = EmptyCity(); bp.sceneName = "car_park";
    ObjectBlueprint3D car; car.name = "car_body"; car.typeName = "car";
    car.position = Vec3(0, 1, 0); car.physicsRole = "dynamic"; car.controller = "car";
    bp.objects.push_back(car);
    return bp;
}

SceneBlueprint3D SceneBlueprint3D::RacingTrack() {
    return CarParkScene();
}

// ─── SceneGenerator3D ────────────────────────────────────────────────────────

void SceneGenerator3D::Spawn(const SceneBlueprint3D& blueprint) {
    if (!m_Scene) return;
    int total = (int)blueprint.objects.size();
    int count = 0;
    for (auto& obj : blueprint.objects) {
        SpawnObject(obj);
        if (m_ProgressCb) m_ProgressCb(++count, total);
    }
}

GameObject* SceneGenerator3D::SpawnObject(const ObjectBlueprint3D& bp, GameObject* /*parent*/) {
    if (!m_Scene) return nullptr;
    auto* obj = m_Scene->CreateGameObject(bp.name);
    obj->GetTransform().SetPosition(bp.position.x, bp.position.y, bp.position.z);
    obj->GetTransform().SetEulerDeg(bp.rotationDeg.x, bp.rotationDeg.y, bp.rotationDeg.z);
    obj->GetTransform().SetScale(bp.scale.x, bp.scale.y, bp.scale.z);

    auto* mr = obj->AddComponent<MeshRenderer>();
    mr->primitiveType = PrimitiveType::Cube;
    mr->color = bp.color;

    if (bp.physicsRole == "dynamic") {
        auto* rb = obj->AddComponent<RigidBody>(); rb->useGravity = true; rb->angularDrag = 0.95f;
        obj->AddComponent<Collider>()->type = ColliderType::Box;
        if (m_Physics) m_Physics->RegisterBody(rb);
        if (bp.controller == "car") obj->AddComponent<CarController3D>();
    } else if (bp.physicsRole == "static") {
        auto* rb = obj->AddComponent<RigidBody>(); rb->bodyType = RigidBodyType::Static; rb->useGravity = false;
        obj->AddComponent<Collider>()->type = ColliderType::Box;
        if (m_Physics) m_Physics->RegisterBody(rb);
    }

    for (auto& child : bp.children) SpawnObject(child, obj);
    return obj;
}
