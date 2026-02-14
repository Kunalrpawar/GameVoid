// ============================================================================
// GameVoid Engine — Scene Serialization & Prefab System
// ============================================================================
// JSON-based scene save/load and reusable prefab templates.
// ============================================================================
#pragma once

#include "core/Types.h"
#include "core/Scene.h"
#include "core/GameObject.h"
#include "renderer/MeshRenderer.h"
#include "renderer/MaterialComponent.h"
#include "renderer/Lighting.h"
#include "physics/Physics.h"
#include "scripting/ScriptEngine.h"
#include <string>
#include <fstream>
#include <sstream>

namespace gv {

// ============================================================================
// Prefab — a reusable object blueprint
// ============================================================================
struct Prefab {
    std::string name;
    Vec3 position{ 0, 0, 0 };
    Vec3 rotation{ 0, 0, 0 };  // Euler degrees
    Vec3 scale{ 1, 1, 1 };

    // Optional components
    bool hasMeshRenderer = false;
    PrimitiveType primitiveType = PrimitiveType::None;
    Vec4 color{ 0.8f, 0.3f, 0.2f, 1.0f };
    std::string meshPath;  // for loaded meshes

    bool hasMaterial = false;
    Vec4 matAlbedo{ 0.8f, 0.8f, 0.8f, 1.0f };
    f32  matMetallic = 0.0f;
    f32  matRoughness = 0.5f;
    Vec3 matEmission{ 0, 0, 0 };
    f32  matEmissionStrength = 0.0f;
    f32  matAO = 1.0f;

    bool hasRigidBody = false;
    RigidBodyType rbType = RigidBodyType::Dynamic;
    f32  rbMass = 1.0f;
    bool rbUseGravity = true;
    f32  rbRestitution = 0.3f;

    bool hasCollider = false;
    ColliderType colliderType = ColliderType::Box;
    Vec3 colliderHalfExtents{ 0.5f, 0.5f, 0.5f };
    f32  colliderRadius = 0.5f;
    bool colliderIsTrigger = false;

    bool hasScript = false;
    std::string scriptPath;
    std::string scriptSource;

    bool hasLight = false;
    std::string lightType;  // "Ambient", "Directional", "Point", "Spot"
    Vec3 lightColor{ 1, 1, 1 };
    f32  lightIntensity = 1.0f;
    Vec3 lightDirection{ 0, -1, 0 };

    // Children prefabs
    std::vector<Prefab> children;

    /// Instantiate this prefab into the given scene.
    GameObject* Instantiate(Scene& scene, PhysicsWorld* physics = nullptr) const;
};

// ============================================================================
// Prefab Library — manages named prefabs
// ============================================================================
class PrefabLibrary {
public:
    void Register(const std::string& name, const Prefab& prefab) {
        m_Prefabs[name] = prefab;
    }

    const Prefab* Get(const std::string& name) const {
        auto it = m_Prefabs.find(name);
        return (it != m_Prefabs.end()) ? &it->second : nullptr;
    }

    /// Create a prefab from an existing GameObject.
    Prefab CreateFromObject(const GameObject* obj) const;

    /// Save all prefabs to a file.
    bool SaveToFile(const std::string& path) const;

    /// Load prefabs from a file.
    bool LoadFromFile(const std::string& path);

    const std::unordered_map<std::string, Prefab>& GetAll() const { return m_Prefabs; }

    void Clear() { m_Prefabs.clear(); }

private:
    std::unordered_map<std::string, Prefab> m_Prefabs;
};

// ============================================================================
// Scene Serializer — JSON-based scene save/load
// ============================================================================
class SceneSerializer {
public:
    /// Serialize a scene to a JSON file.
    static bool SaveScene(const Scene& scene, const std::string& path);

    /// Deserialize a scene from a JSON file. Clears existing objects first.
    static bool LoadScene(Scene& scene, const std::string& path,
                          PhysicsWorld* physics = nullptr);

private:
    // ── JSON writing helpers ───────────────────────────────────────────────
    static std::string SerializeObject(const GameObject* obj, int indent = 2);
    static std::string SerializeVec3(const Vec3& v);
    static std::string SerializeVec4(const Vec4& v);
    static std::string Indent(int level);

    // ── JSON reading helpers (minimal hand-written parser) ─────────────────
    struct JsonValue {
        enum Type { Null, Bool, Number, String, Array, Object };
        Type type = Null;
        f64  numVal = 0;
        bool boolVal = false;
        std::string strVal;
        std::vector<JsonValue> arrVal;
        std::vector<std::pair<std::string, JsonValue>> objVal;

        // Accessors
        const JsonValue& operator[](const std::string& key) const {
            for (auto& p : objVal) if (p.first == key) return p.second;
            static JsonValue nil;
            return nil;
        }
        const JsonValue& operator[](size_t idx) const {
            return (idx < arrVal.size()) ? arrVal[idx] : *this;
        }
        f64 AsNum() const { return numVal; }
        f32 AsFloat() const { return static_cast<f32>(numVal); }
        const std::string& AsStr() const { return strVal; }
        bool AsBool() const { return boolVal; }
        bool Has(const std::string& key) const {
            for (auto& p : objVal) if (p.first == key) return true;
            return false;
        }
        size_t Size() const { return (type == Array) ? arrVal.size() : objVal.size(); }
    };

    static JsonValue ParseJson(const std::string& src, size_t& pos);
    static JsonValue ParseJsonString(const std::string& src, size_t& pos);
    static JsonValue ParseJsonNumber(const std::string& src, size_t& pos);
    static JsonValue ParseJsonArray(const std::string& src, size_t& pos);
    static JsonValue ParseJsonObject(const std::string& src, size_t& pos);
    static void SkipWhitespace(const std::string& src, size_t& pos);

    static void DeserializeObject(Scene& scene, const JsonValue& jObj,
                                  PhysicsWorld* physics);
    static Vec3 ParseVec3(const JsonValue& v);
    static Vec4 ParseVec4(const JsonValue& v);
};

} // namespace gv
