// ============================================================================
// GameVoid Engine — AI Manager Implementation (Google Gemini)
// ============================================================================
#include "ai/AIManager.h"
#include "core/Scene.h"
#include "core/GameObject.h"
#include "renderer/MeshRenderer.h"

namespace gv {

// ── Helpers ────────────────────────────────────────────────────────────────

std::string AIManager::BuildRequestURL() const {
    // e.g. https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent?key=API_KEY
    return m_Config.baseUrl + m_Config.model + ":generateContent?key=" + m_Config.apiKey;
}

AIResponse AIManager::HttpPost(const std::string& url, const std::string& jsonBody) const {
    AIResponse resp;
    // In production:
    //   CURL* curl = curl_easy_init();
    //   curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    //   curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());
    //   ... set headers (Content-Type: application/json) ...
    //   CURLcode res = curl_easy_perform(curl);
    //   resp.rawJSON = <response buffer>;
    //   resp.success = (res == CURLE_OK);
    //   curl_easy_cleanup(curl);
    GV_LOG_INFO("AIManager::HttpPost (placeholder) — URL: " + url.substr(0, 80) + "...");
    (void)jsonBody;
    resp.success = false;
    resp.errorMessage = "HTTP client not implemented in skeleton build.";
    return resp;
}

// ── Public API ─────────────────────────────────────────────────────────────

AIResponse AIManager::SendPrompt(const std::string& prompt) const {
    if (m_Config.apiKey.empty()) {
        AIResponse r;
        r.errorMessage = "No API key configured. Call SetAPIKey() first.";
        GV_LOG_WARN(r.errorMessage);
        return r;
    }

    // Build the JSON payload following the Gemini REST API format.
    std::string json =
        R"({"contents":[{"parts":[{"text":")" + prompt + R"("}]}],)"
        R"("generationConfig":{"temperature":)" + std::to_string(m_Config.temperature) +
        R"(,"maxOutputTokens":)" + std::to_string(m_Config.maxTokens) + R"(}})";

    std::string url = BuildRequestURL();
    return HttpPost(url, json);
}

std::string AIManager::GenerateScript(const std::string& behaviourDescription) const {
    std::string prompt =
        "You are a game engine scripting assistant. Generate a Lua script for the "
        "following behaviour. Only output the Lua code, no explanation.\n\n"
        "Behaviour: " + behaviourDescription;

    AIResponse resp = SendPrompt(prompt);
    return resp.success ? resp.text : ("-- AI generation failed: " + resp.errorMessage);
}

std::string AIManager::GenerateAssetDescription(const std::string& assetPrompt) const {
    std::string prompt =
        "Describe a 3D game asset in JSON format with fields: name, vertices (list of "
        "[x,y,z]), triangles (list of [i,j,k]), uvs, colour.\n\nAsset: " + assetPrompt;

    AIResponse resp = SendPrompt(prompt);
    return resp.success ? resp.text : "";
}

std::vector<AIManager::ObjectBlueprint> AIManager::GenerateLevel(const std::string& levelPrompt) const {
    std::string prompt =
        "Generate a game level as a JSON array of objects. Each object has: "
        "name (string), meshType (cube|sphere|plane), position [x,y,z], "
        "rotation [x,y,z] (degrees), scale [x,y,z], materialName (string).\n\n"
        "Level description: " + levelPrompt;

    AIResponse resp = SendPrompt(prompt);
    std::vector<ObjectBlueprint> blueprints;

    if (!resp.success) {
        GV_LOG_WARN("AI level generation failed: " + resp.errorMessage);
        return blueprints;
    }

    // In production: parse resp.text as JSON array → vector<ObjectBlueprint>.
    // Placeholder returns empty.
    GV_LOG_INFO("AI level generation returned " + std::to_string(blueprints.size()) + " objects.");
    return blueprints;
}

void AIManager::PopulateScene(Scene& scene, const std::vector<ObjectBlueprint>& blueprints) const {
    for (auto& bp : blueprints) {
        GameObject* obj = scene.CreateGameObject(bp.name);
        obj->GetTransform().SetPosition(bp.position.x, bp.position.y, bp.position.z);
        obj->GetTransform().SetEulerDeg(bp.rotation.x, bp.rotation.y, bp.rotation.z);
        obj->GetTransform().SetScale(bp.scale.x, bp.scale.y, bp.scale.z);

        // Attach a MeshRenderer with the appropriate primitive.
        // auto* mr = obj->AddComponent<MeshRenderer>();
        // if (bp.meshType == "cube")   mr->SetMesh(Mesh::CreateCube());
        // if (bp.meshType == "sphere") mr->SetMesh(Mesh::CreateSphere());
        // if (bp.meshType == "plane")  mr->SetMesh(Mesh::CreatePlane());

        GV_LOG_INFO("  Spawned '" + bp.name + "' (" + bp.meshType + ")");
    }
}

void AIManager::SendPromptAsync(const std::string& prompt, ResponseCallback cb) const {
    // In production: fire off the request on a worker thread, call cb on completion.
    // Skeleton: run synchronously.
    AIResponse resp = SendPrompt(prompt);
    if (cb) cb(resp);
}

} // namespace gv
