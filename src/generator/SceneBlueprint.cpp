#include "generator/SceneBlueprint.h"
#include "core/Scene.h"
#include "core/GameObject.h"
#include "physics/Physics.h"
#include "renderer/MeshRenderer.h"
#include "scripting/ScriptEngine.h"
#include "scripting/physics/ForceController.h"
#include "vehicle/CarController3D.h"
#include <sstream>
#include <algorithm>

namespace {

static std::string ToLowerCopy(const std::string& in) {
    std::string out = in;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

static PrimitiveType PrimitiveFromTypeName(const std::string& typeName) {
    const std::string t = ToLowerCopy(typeName);
    if (t == "triangle") return PrimitiveType::Triangle;
    if (t == "plane" || t == "ground" || t == "floor") return PrimitiveType::Plane;
    return PrimitiveType::Cube;
}

static std::string CanonicalPhysicsRole(const std::string& roleIn) {
    const std::string role = ToLowerCopy(roleIn);
    if (role == "dynamic") return "dynamic";
    if (role == "static") return "static";
    if (role == "kinematic") return "kinematic";
    return "none";
}

}

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
    mr->primitiveType = PrimitiveFromTypeName(bp.typeName);
    mr->color = bp.color;

    const std::string role = CanonicalPhysicsRole(bp.physicsRole);
    if (role != "none") {
        auto* rb = obj->AddComponent<RigidBody>();
        auto* col = obj->AddComponent<Collider>();
        col->type = ColliderType::Box;

        if (role == "dynamic") {
            rb->bodyType = RigidBodyType::Dynamic;
            rb->useGravity = true;
            rb->angularDrag = 0.95f;
        } else if (role == "static") {
            rb->bodyType = RigidBodyType::Static;
            rb->useGravity = false;
        } else if (role == "kinematic") {
            rb->bodyType = RigidBodyType::Kinematic;
            rb->useGravity = false;
        }

        if (m_Physics) m_Physics->RegisterBody(rb);
    }

    const std::string controller = ToLowerCopy(bp.controller);
    if (controller == "car") {
        obj->AddComponent<CarController3D>();
    } else if (controller == "force") {
        auto* fc = obj->AddComponent<ForceController>();
        fc->BindKeyToForce("W", ForceDirection::Forward, 35.0f);
        fc->BindKeyToForce("S", ForceDirection::Backward, 35.0f);
        fc->BindKeyToForce("A", ForceDirection::Left, 18.0f);
        fc->BindKeyToForce("D", ForceDirection::Right, 18.0f);
    }

    if (!bp.scriptTag.empty()) {
        auto* sc = obj->AddComponent<ScriptComponent>();
        std::string tag = bp.scriptTag;
        const std::string lowered = ToLowerCopy(tag);
        if (lowered.rfind("file:", 0) == 0) {
            sc->SetScriptPath(tag.substr(5));
        } else {
            sc->SetSource(bp.scriptTag);
        }
    }

    for (auto& child : bp.children) SpawnObject(child, obj);
    return obj;
}
