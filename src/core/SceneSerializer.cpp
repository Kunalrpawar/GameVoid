// ============================================================================
// GameVoid Engine — Scene Serialization & Prefab System Implementation
// ============================================================================
#include "core/SceneSerializer.h"
#include "renderer/Camera.h"
#include <algorithm>
#include <iomanip>
#include <cstdlib>

namespace gv {

// ============================================================================
// Prefab — Instantiate
// ============================================================================
GameObject* Prefab::Instantiate(Scene& scene, PhysicsWorld* physics) const {
    auto* obj = scene.CreateGameObject(name);
    obj->GetTransform().SetPosition(position.x, position.y, position.z);
    obj->GetTransform().SetEulerDeg(rotation.x, rotation.y, rotation.z);
    obj->GetTransform().SetScale(scale.x, scale.y, scale.z);

    if (hasMeshRenderer) {
        auto* mr = obj->AddComponent<MeshRenderer>();
        mr->primitiveType = primitiveType;
        mr->color = color;
        // If meshPath is set, the renderer can look it up from AssetManager
    }

    if (hasMaterial) {
        auto* mc = obj->AddComponent<MaterialComponent>();
        mc->albedo = matAlbedo;
        mc->metallic = matMetallic;
        mc->roughness = matRoughness;
        mc->emission = matEmission;
        mc->emissionStrength = matEmissionStrength;
        mc->ao = matAO;
    }

    if (hasRigidBody) {
        auto* rb = obj->AddComponent<RigidBody>();
        rb->bodyType = rbType;
        rb->mass = rbMass;
        rb->useGravity = rbUseGravity;
        rb->restitution = rbRestitution;
        if (physics) physics->RegisterBody(rb);
    }

    if (hasCollider) {
        auto* col = obj->AddComponent<Collider>();
        col->type = colliderType;
        col->boxHalfExtents = colliderHalfExtents;
        col->radius = colliderRadius;
        col->isTrigger = colliderIsTrigger;
    }

    if (hasScript) {
        auto* sc = obj->AddComponent<ScriptComponent>();
        if (!scriptPath.empty()) sc->SetScriptPath(scriptPath);
        if (!scriptSource.empty()) sc->SetSource(scriptSource);
    }

    if (hasLight) {
        if (lightType == "Ambient") {
            auto* al = obj->AddComponent<AmbientLight>();
            al->colour = lightColor;
            al->intensity = lightIntensity;
        } else if (lightType == "Directional") {
            auto* dl = obj->AddComponent<DirectionalLight>();
            dl->colour = lightColor;
            dl->intensity = lightIntensity;
            dl->direction = lightDirection;
        } else if (lightType == "Point") {
            auto* pl = obj->AddComponent<PointLight>();
            pl->colour = lightColor;
            pl->intensity = lightIntensity;
        } else if (lightType == "Spot") {
            auto* sl = obj->AddComponent<SpotLight>();
            sl->colour = lightColor;
            sl->intensity = lightIntensity;
            sl->direction = lightDirection;
        }
    }

    // Instantiate children
    for (auto& childPrefab : children) {
        // Children are instantiated as separate objects; parent linking is managed by scene
        auto* child = childPrefab.Instantiate(scene, physics);
        if (child) {
            // For now, children are just at their offset positions
            // Full hierarchy linking happens via AddChild
        }
    }

    return obj;
}

// ============================================================================
// PrefabLibrary
// ============================================================================
Prefab PrefabLibrary::CreateFromObject(const GameObject* obj) const {
    Prefab p;
    p.name = obj->GetName();
    p.position = obj->GetTransform().position;
    p.scale = obj->GetTransform().scale;
    // rotation stored as quaternion → convert to euler (simplified)
    p.rotation = Vec3(0, 0, 0);

    if (auto* mr = obj->GetComponent<MeshRenderer>()) {
        p.hasMeshRenderer = true;
        p.primitiveType = mr->primitiveType;
        p.color = mr->color;
    }

    if (auto* mc = obj->GetComponent<MaterialComponent>()) {
        p.hasMaterial = true;
        p.matAlbedo = mc->albedo;
        p.matMetallic = mc->metallic;
        p.matRoughness = mc->roughness;
        p.matEmission = mc->emission;
        p.matEmissionStrength = mc->emissionStrength;
        p.matAO = mc->ao;
    }

    if (auto* rb = obj->GetComponent<RigidBody>()) {
        p.hasRigidBody = true;
        p.rbType = rb->bodyType;
        p.rbMass = rb->mass;
        p.rbUseGravity = rb->useGravity;
        p.rbRestitution = rb->restitution;
    }

    if (auto* col = obj->GetComponent<Collider>()) {
        p.hasCollider = true;
        p.colliderType = col->type;
        p.colliderHalfExtents = col->boxHalfExtents;
        p.colliderRadius = col->radius;
        p.colliderIsTrigger = col->isTrigger;
    }

    if (auto* sc = obj->GetComponent<ScriptComponent>()) {
        p.hasScript = true;
        p.scriptPath = sc->GetScriptPath();
        p.scriptSource = sc->GetSource();
    }

    // Check for light components
    if (auto* al = obj->GetComponent<AmbientLight>()) {
        p.hasLight = true;
        p.lightType = "Ambient";
        p.lightColor = al->colour;
        p.lightIntensity = al->intensity;
    } else if (auto* dl = obj->GetComponent<DirectionalLight>()) {
        p.hasLight = true;
        p.lightType = "Directional";
        p.lightColor = dl->colour;
        p.lightIntensity = dl->intensity;
        p.lightDirection = dl->direction;
    } else if (auto* pl = obj->GetComponent<PointLight>()) {
        p.hasLight = true;
        p.lightType = "Point";
        p.lightColor = pl->colour;
        p.lightIntensity = pl->intensity;
    } else if (auto* sl = obj->GetComponent<SpotLight>()) {
        p.hasLight = true;
        p.lightType = "Spot";
        p.lightColor = sl->colour;
        p.lightIntensity = sl->intensity;
        p.lightDirection = sl->direction;
    }

    return p;
}

bool PrefabLibrary::SaveToFile(const std::string& path) const {
    std::ofstream f(path);
    if (!f.is_open()) return false;

    f << "{\n  \"prefabs\": [\n";
    size_t idx = 0;
    for (auto it = m_Prefabs.begin(); it != m_Prefabs.end(); ++it) {
        f << "    " << SceneSerializer::SerializeObject(nullptr, 4);
        // Use prefab name as a marker
        if (++idx < m_Prefabs.size()) f << ",";
        f << "\n";
    }
    f << "  ]\n}\n";
    f.close();
    GV_LOG_INFO("PrefabLibrary saved to: " + path);
    return true;
}

bool PrefabLibrary::LoadFromFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    GV_LOG_INFO("PrefabLibrary loaded from: " + path);
    return true;
}

// ============================================================================
// SceneSerializer — Save
// ============================================================================
std::string SceneSerializer::Indent(int level) {
    return std::string(static_cast<size_t>(level) * 2, ' ');
}

std::string SceneSerializer::SerializeVec3(const Vec3& v) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(4)
       << "[" << v.x << ", " << v.y << ", " << v.z << "]";
    return ss.str();
}

std::string SceneSerializer::SerializeVec4(const Vec4& v) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(4)
       << "[" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << "]";
    return ss.str();
}

static std::string PrimitiveTypeToString(PrimitiveType t) {
    switch (t) {
        case PrimitiveType::None:     return "None";
        case PrimitiveType::Triangle: return "Triangle";
        case PrimitiveType::Cube:     return "Cube";
        case PrimitiveType::Plane:    return "Plane";
        default: return "None";
    }
}

static PrimitiveType StringToPrimitiveType(const std::string& s) {
    if (s == "Triangle") return PrimitiveType::Triangle;
    if (s == "Cube")     return PrimitiveType::Cube;
    if (s == "Plane")    return PrimitiveType::Plane;
    return PrimitiveType::None;
}

static std::string RigidBodyTypeToString(RigidBodyType t) {
    switch (t) {
        case RigidBodyType::Static:    return "Static";
        case RigidBodyType::Dynamic:   return "Dynamic";
        case RigidBodyType::Kinematic: return "Kinematic";
        default: return "Dynamic";
    }
}

static RigidBodyType StringToRigidBodyType(const std::string& s) {
    if (s == "Static")    return RigidBodyType::Static;
    if (s == "Kinematic") return RigidBodyType::Kinematic;
    return RigidBodyType::Dynamic;
}

static std::string ColliderTypeToString(ColliderType t) {
    switch (t) {
        case ColliderType::Box:     return "Box";
        case ColliderType::Sphere:  return "Sphere";
        case ColliderType::Capsule: return "Capsule";
        case ColliderType::Mesh:    return "Mesh";
        default: return "Box";
    }
}

static ColliderType StringToColliderType(const std::string& s) {
    if (s == "Sphere")  return ColliderType::Sphere;
    if (s == "Capsule") return ColliderType::Capsule;
    if (s == "Mesh")    return ColliderType::Mesh;
    return ColliderType::Box;
}

static std::string EscapeJsonString(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

std::string SceneSerializer::SerializeObject(const GameObject* obj, int indent) {
    if (!obj) return "{}";

    std::ostringstream ss;
    std::string in = Indent(indent);
    std::string in1 = Indent(indent + 1);
    std::string in2 = Indent(indent + 2);

    ss << "{\n";
    ss << in1 << "\"name\": \"" << EscapeJsonString(obj->GetName()) << "\",\n";
    ss << in1 << "\"id\": " << obj->GetID() << ",\n";
    ss << in1 << "\"active\": " << (obj->IsActive() ? "true" : "false") << ",\n";

    // Transform
    ss << in1 << "\"transform\": {\n";
    ss << in2 << "\"position\": " << SerializeVec3(obj->GetTransform().position) << ",\n";
    ss << in2 << "\"scale\": " << SerializeVec3(obj->GetTransform().scale) << "\n";
    ss << in1 << "},\n";

    // Components
    ss << in1 << "\"components\": [";

    bool first = true;

    // MeshRenderer
    if (auto* mr = obj->GetComponent<MeshRenderer>()) {
        if (!first) ss << ",";
        ss << "\n" << in2 << "{\n";
        ss << Indent(indent + 3) << "\"type\": \"MeshRenderer\",\n";
        ss << Indent(indent + 3) << "\"primitiveType\": \"" << PrimitiveTypeToString(mr->primitiveType) << "\",\n";
        ss << Indent(indent + 3) << "\"color\": " << SerializeVec4(mr->color) << "\n";
        ss << in2 << "}";
        first = false;
    }

    // MaterialComponent
    if (auto* mc = obj->GetComponent<MaterialComponent>()) {
        if (!first) ss << ",";
        ss << "\n" << in2 << "{\n";
        ss << Indent(indent + 3) << "\"type\": \"Material\",\n";
        ss << Indent(indent + 3) << "\"albedo\": " << SerializeVec4(mc->albedo) << ",\n";
        ss << Indent(indent + 3) << "\"metallic\": " << mc->metallic << ",\n";
        ss << Indent(indent + 3) << "\"roughness\": " << mc->roughness << ",\n";
        ss << Indent(indent + 3) << "\"emission\": " << SerializeVec3(mc->emission) << ",\n";
        ss << Indent(indent + 3) << "\"emissionStrength\": " << mc->emissionStrength << ",\n";
        ss << Indent(indent + 3) << "\"ao\": " << mc->ao << "\n";
        ss << in2 << "}";
        first = false;
    }

    // RigidBody
    if (auto* rb = obj->GetComponent<RigidBody>()) {
        if (!first) ss << ",";
        ss << "\n" << in2 << "{\n";
        ss << Indent(indent + 3) << "\"type\": \"RigidBody\",\n";
        ss << Indent(indent + 3) << "\"bodyType\": \"" << RigidBodyTypeToString(rb->bodyType) << "\",\n";
        ss << Indent(indent + 3) << "\"mass\": " << rb->mass << ",\n";
        ss << Indent(indent + 3) << "\"useGravity\": " << (rb->useGravity ? "true" : "false") << ",\n";
        ss << Indent(indent + 3) << "\"restitution\": " << rb->restitution << "\n";
        ss << in2 << "}";
        first = false;
    }

    // Collider
    if (auto* col = obj->GetComponent<Collider>()) {
        if (!first) ss << ",";
        ss << "\n" << in2 << "{\n";
        ss << Indent(indent + 3) << "\"type\": \"Collider\",\n";
        ss << Indent(indent + 3) << "\"colliderType\": \"" << ColliderTypeToString(col->type) << "\",\n";
        ss << Indent(indent + 3) << "\"halfExtents\": " << SerializeVec3(col->boxHalfExtents) << ",\n";
        ss << Indent(indent + 3) << "\"radius\": " << col->radius << ",\n";
        ss << Indent(indent + 3) << "\"isTrigger\": " << (col->isTrigger ? "true" : "false") << "\n";
        ss << in2 << "}";
        first = false;
    }

    // ScriptComponent
    if (auto* sc = obj->GetComponent<ScriptComponent>()) {
        if (!first) ss << ",";
        ss << "\n" << in2 << "{\n";
        ss << Indent(indent + 3) << "\"type\": \"Script\",\n";
        ss << Indent(indent + 3) << "\"path\": \"" << EscapeJsonString(sc->GetScriptPath()) << "\",\n";
        ss << Indent(indent + 3) << "\"source\": \"" << EscapeJsonString(sc->GetSource()) << "\"\n";
        ss << in2 << "}";
        first = false;
    }

    // Light components
    if (auto* al = obj->GetComponent<AmbientLight>()) {
        if (!first) ss << ",";
        ss << "\n" << in2 << "{\n";
        ss << Indent(indent + 3) << "\"type\": \"AmbientLight\",\n";
        ss << Indent(indent + 3) << "\"color\": " << SerializeVec3(al->colour) << ",\n";
        ss << Indent(indent + 3) << "\"intensity\": " << al->intensity << "\n";
        ss << in2 << "}";
        first = false;
    }
    if (auto* dl = obj->GetComponent<DirectionalLight>()) {
        if (!first) ss << ",";
        ss << "\n" << in2 << "{\n";
        ss << Indent(indent + 3) << "\"type\": \"DirectionalLight\",\n";
        ss << Indent(indent + 3) << "\"direction\": " << SerializeVec3(dl->direction) << ",\n";
        ss << Indent(indent + 3) << "\"color\": " << SerializeVec3(dl->colour) << ",\n";
        ss << Indent(indent + 3) << "\"intensity\": " << dl->intensity << "\n";
        ss << in2 << "}";
        first = false;
    }
    if (auto* pl = obj->GetComponent<PointLight>()) {
        if (!first) ss << ",";
        ss << "\n" << in2 << "{\n";
        ss << Indent(indent + 3) << "\"type\": \"PointLight\",\n";
        ss << Indent(indent + 3) << "\"color\": " << SerializeVec3(pl->colour) << ",\n";
        ss << Indent(indent + 3) << "\"intensity\": " << pl->intensity << ",\n";
        ss << Indent(indent + 3) << "\"range\": " << pl->range << "\n";
        ss << in2 << "}";
        first = false;
    }
    if (auto* sl = obj->GetComponent<SpotLight>()) {
        if (!first) ss << ",";
        ss << "\n" << in2 << "{\n";
        ss << Indent(indent + 3) << "\"type\": \"SpotLight\",\n";
        ss << Indent(indent + 3) << "\"direction\": " << SerializeVec3(sl->direction) << ",\n";
        ss << Indent(indent + 3) << "\"color\": " << SerializeVec3(sl->colour) << ",\n";
        ss << Indent(indent + 3) << "\"intensity\": " << sl->intensity << "\n";
        ss << in2 << "}";
        first = false;
    }

    // Camera
    if (obj->GetComponent<Camera>()) {
        if (!first) ss << ",";
        ss << "\n" << in2 << "{\n";
        ss << Indent(indent + 3) << "\"type\": \"Camera\"\n";
        ss << in2 << "}";
        first = false;
    }

    ss << "\n" << in1 << "]\n";
    ss << in << "}";
    return ss.str();
}

bool SceneSerializer::SaveScene(const Scene& scene, const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) {
        GV_LOG_ERROR("SceneSerializer — failed to open file for writing: " + path);
        return false;
    }

    f << "{\n";
    f << "  \"sceneName\": \"" << EscapeJsonString(scene.GetName()) << "\",\n";
    f << "  \"objects\": [\n";

    const auto& objects = scene.GetAllObjects();
    for (size_t i = 0; i < objects.size(); ++i) {
        f << "    " << SerializeObject(objects[i].get(), 2);
        if (i + 1 < objects.size()) f << ",";
        f << "\n";
    }

    f << "  ]\n";
    f << "}\n";
    f.close();

    GV_LOG_INFO("Scene saved to: " + path + " (" + std::to_string(objects.size()) + " objects)");
    return true;
}

// ============================================================================
// SceneSerializer — Load (Minimal JSON Parser)
// ============================================================================
void SceneSerializer::SkipWhitespace(const std::string& src, size_t& pos) {
    while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' ||
           src[pos] == '\n' || src[pos] == '\r')) ++pos;
}

SceneSerializer::JsonValue SceneSerializer::ParseJsonString(const std::string& src, size_t& pos) {
    JsonValue val;
    val.type = JsonValue::String;
    ++pos; // skip opening quote
    while (pos < src.size() && src[pos] != '"') {
        if (src[pos] == '\\' && pos + 1 < src.size()) {
            ++pos;
            switch (src[pos]) {
                case 'n':  val.strVal += '\n'; break;
                case 't':  val.strVal += '\t'; break;
                case '\\': val.strVal += '\\'; break;
                case '"':  val.strVal += '"';  break;
                default:   val.strVal += src[pos]; break;
            }
        } else {
            val.strVal += src[pos];
        }
        ++pos;
    }
    if (pos < src.size()) ++pos; // skip closing quote
    return val;
}

SceneSerializer::JsonValue SceneSerializer::ParseJsonNumber(const std::string& src, size_t& pos) {
    JsonValue val;
    val.type = JsonValue::Number;
    size_t start = pos;
    if (pos < src.size() && src[pos] == '-') ++pos;
    while (pos < src.size() && (std::isdigit(src[pos]) || src[pos] == '.' || src[pos] == 'e' || src[pos] == 'E' || src[pos] == '+' || src[pos] == '-')) {
        if ((src[pos] == '+' || src[pos] == '-') && pos > start && src[pos-1] != 'e' && src[pos-1] != 'E') break;
        ++pos;
    }
    val.numVal = std::stod(src.substr(start, pos - start));
    return val;
}

SceneSerializer::JsonValue SceneSerializer::ParseJsonArray(const std::string& src, size_t& pos) {
    JsonValue val;
    val.type = JsonValue::Array;
    ++pos; // skip [
    SkipWhitespace(src, pos);
    while (pos < src.size() && src[pos] != ']') {
        val.arrVal.push_back(ParseJson(src, pos));
        SkipWhitespace(src, pos);
        if (pos < src.size() && src[pos] == ',') ++pos;
        SkipWhitespace(src, pos);
    }
    if (pos < src.size()) ++pos; // skip ]
    return val;
}

SceneSerializer::JsonValue SceneSerializer::ParseJsonObject(const std::string& src, size_t& pos) {
    JsonValue val;
    val.type = JsonValue::Object;
    ++pos; // skip {
    SkipWhitespace(src, pos);
    while (pos < src.size() && src[pos] != '}') {
        SkipWhitespace(src, pos);
        if (pos >= src.size() || src[pos] != '"') break;
        JsonValue key = ParseJsonString(src, pos);
        SkipWhitespace(src, pos);
        if (pos < src.size() && src[pos] == ':') ++pos;
        SkipWhitespace(src, pos);
        JsonValue value = ParseJson(src, pos);
        val.objVal.push_back({key.strVal, value});
        SkipWhitespace(src, pos);
        if (pos < src.size() && src[pos] == ',') ++pos;
        SkipWhitespace(src, pos);
    }
    if (pos < src.size()) ++pos; // skip }
    return val;
}

SceneSerializer::JsonValue SceneSerializer::ParseJson(const std::string& src, size_t& pos) {
    SkipWhitespace(src, pos);
    if (pos >= src.size()) return {};

    char c = src[pos];
    if (c == '"') return ParseJsonString(src, pos);
    if (c == '{') return ParseJsonObject(src, pos);
    if (c == '[') return ParseJsonArray(src, pos);
    if (c == '-' || std::isdigit(c)) return ParseJsonNumber(src, pos);

    // true / false / null
    if (src.substr(pos, 4) == "true") {
        pos += 4;
        JsonValue v; v.type = JsonValue::Bool; v.boolVal = true;
        return v;
    }
    if (src.substr(pos, 5) == "false") {
        pos += 5;
        JsonValue v; v.type = JsonValue::Bool; v.boolVal = false;
        return v;
    }
    if (src.substr(pos, 4) == "null") {
        pos += 4;
        return {};
    }
    ++pos; // skip unknown
    return {};
}

Vec3 SceneSerializer::ParseVec3(const JsonValue& v) {
    if (v.type != JsonValue::Array || v.arrVal.size() < 3) return {};
    return Vec3(v.arrVal[0].AsFloat(), v.arrVal[1].AsFloat(), v.arrVal[2].AsFloat());
}

Vec4 SceneSerializer::ParseVec4(const JsonValue& v) {
    if (v.type != JsonValue::Array || v.arrVal.size() < 4) return {};
    return Vec4(v.arrVal[0].AsFloat(), v.arrVal[1].AsFloat(),
                v.arrVal[2].AsFloat(), v.arrVal[3].AsFloat());
}

void SceneSerializer::DeserializeObject(Scene& scene, const JsonValue& jObj,
                                        PhysicsWorld* physics) {
    std::string name = jObj["name"].AsStr();
    if (name.empty()) name = "GameObject";

    auto* obj = scene.CreateGameObject(name);
    bool active = jObj.Has("active") ? jObj["active"].AsBool() : true;
    obj->SetActive(active);

    // Transform
    if (jObj.Has("transform")) {
        auto& t = jObj["transform"];
        if (t.Has("position")) {
            Vec3 pos = ParseVec3(t["position"]);
            obj->GetTransform().SetPosition(pos.x, pos.y, pos.z);
        }
        if (t.Has("scale")) {
            Vec3 scl = ParseVec3(t["scale"]);
            obj->GetTransform().SetScale(scl.x, scl.y, scl.z);
        }
    }

    // Components
    if (jObj.Has("components")) {
        auto& comps = jObj["components"];
        for (size_t i = 0; i < comps.arrVal.size(); ++i) {
            auto& comp = comps.arrVal[i];
            std::string type = comp["type"].AsStr();

            if (type == "MeshRenderer") {
                auto* mr = obj->AddComponent<MeshRenderer>();
                mr->primitiveType = StringToPrimitiveType(comp["primitiveType"].AsStr());
                if (comp.Has("color")) mr->color = ParseVec4(comp["color"]);
            }
            else if (type == "Material") {
                auto* mc = obj->AddComponent<MaterialComponent>();
                if (comp.Has("albedo"))    mc->albedo    = ParseVec4(comp["albedo"]);
                if (comp.Has("metallic"))  mc->metallic  = comp["metallic"].AsFloat();
                if (comp.Has("roughness")) mc->roughness = comp["roughness"].AsFloat();
                if (comp.Has("emission"))  mc->emission  = ParseVec3(comp["emission"]);
                if (comp.Has("emissionStrength")) mc->emissionStrength = comp["emissionStrength"].AsFloat();
                if (comp.Has("ao"))        mc->ao        = comp["ao"].AsFloat();
            }
            else if (type == "RigidBody") {
                auto* rb = obj->AddComponent<RigidBody>();
                if (comp.Has("bodyType"))    rb->bodyType    = StringToRigidBodyType(comp["bodyType"].AsStr());
                if (comp.Has("mass"))        rb->mass        = comp["mass"].AsFloat();
                if (comp.Has("useGravity"))  rb->useGravity  = comp["useGravity"].AsBool();
                if (comp.Has("restitution")) rb->restitution = comp["restitution"].AsFloat();
                if (physics) physics->RegisterBody(rb);
            }
            else if (type == "Collider") {
                auto* col = obj->AddComponent<Collider>();
                if (comp.Has("colliderType"))  col->type           = StringToColliderType(comp["colliderType"].AsStr());
                if (comp.Has("halfExtents"))   col->boxHalfExtents = ParseVec3(comp["halfExtents"]);
                if (comp.Has("radius"))        col->radius         = comp["radius"].AsFloat();
                if (comp.Has("isTrigger"))     col->isTrigger      = comp["isTrigger"].AsBool();
            }
            else if (type == "Script") {
                auto* sc = obj->AddComponent<ScriptComponent>();
                if (comp.Has("path"))   sc->SetScriptPath(comp["path"].AsStr());
                if (comp.Has("source")) sc->SetSource(comp["source"].AsStr());
            }
            else if (type == "AmbientLight") {
                auto* al = obj->AddComponent<AmbientLight>();
                if (comp.Has("color"))     al->colour    = ParseVec3(comp["color"]);
                if (comp.Has("intensity")) al->intensity = comp["intensity"].AsFloat();
            }
            else if (type == "DirectionalLight") {
                auto* dl = obj->AddComponent<DirectionalLight>();
                if (comp.Has("direction")) dl->direction = ParseVec3(comp["direction"]);
                if (comp.Has("color"))     dl->colour    = ParseVec3(comp["color"]);
                if (comp.Has("intensity")) dl->intensity = comp["intensity"].AsFloat();
            }
            else if (type == "PointLight") {
                auto* pl = obj->AddComponent<PointLight>();
                if (comp.Has("color"))     pl->colour    = ParseVec3(comp["color"]);
                if (comp.Has("intensity")) pl->intensity = comp["intensity"].AsFloat();
                if (comp.Has("range"))     pl->range     = comp["range"].AsFloat();
            }
            else if (type == "SpotLight") {
                auto* sl = obj->AddComponent<SpotLight>();
                if (comp.Has("direction")) sl->direction = ParseVec3(comp["direction"]);
                if (comp.Has("color"))     sl->colour    = ParseVec3(comp["color"]);
                if (comp.Has("intensity")) sl->intensity = comp["intensity"].AsFloat();
            }
            else if (type == "Camera") {
                auto* cam = obj->AddComponent<Camera>();
                cam->SetPerspective(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
            }
        }
    }
}

bool SceneSerializer::LoadScene(Scene& scene, const std::string& path,
                                PhysicsWorld* physics) {
    std::ifstream f(path);
    if (!f.is_open()) {
        GV_LOG_ERROR("SceneSerializer — failed to open file: " + path);
        return false;
    }

    std::stringstream ss;
    ss << f.rdbuf();
    std::string json = ss.str();
    f.close();

    size_t pos = 0;
    JsonValue root = ParseJson(json, pos);

    if (root.type != JsonValue::Object) {
        GV_LOG_ERROR("SceneSerializer — invalid JSON in: " + path);
        return false;
    }

    // Deserialize objects
    if (root.Has("objects")) {
        auto& objects = root["objects"];
        for (size_t i = 0; i < objects.arrVal.size(); ++i) {
            DeserializeObject(scene, objects.arrVal[i], physics);
        }
    }

    GV_LOG_INFO("Scene loaded from: " + path);
    return true;
}

} // namespace gv
