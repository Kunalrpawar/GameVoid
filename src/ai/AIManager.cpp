// ============================================================================
// GameVoid Engine — AI Manager Implementation (Google Gemini)
// ============================================================================
// Real HTTP integration via WinInet (Windows) or placeholder on other platforms.
// ============================================================================
#include "ai/AIManager.h"
#include "core/Scene.h"
#include "core/GameObject.h"
#include "renderer/MeshRenderer.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")
#endif

#include <fstream>
#include <sstream>
#include <algorithm>

namespace gv {

// ── Init / Config Persistence ──────────────────────────────────────────────

void AIManager::Init(const std::string& apiKey) {
    if (!apiKey.empty()) {
        m_Config.apiKey = apiKey;
        SaveConfigToFile();
        GV_LOG_INFO("AIManager::Init — API key set and saved to config.");
    } else {
        // Try loading from disk
        if (LoadConfigFromFile()) {
            GV_LOG_INFO("AIManager::Init — loaded API key from config file.");
        } else {
            GV_LOG_WARN("AIManager::Init — no API key provided or found on disk.");
        }
    }
}

void AIManager::SaveConfigToFile() const {
    std::ofstream ofs(m_Config.configFilePath);
    if (ofs.is_open()) {
        ofs << "[AI]\n";
        ofs << "apiKey=" << m_Config.apiKey << "\n";
        ofs << "model=" << m_Config.model << "\n";
        ofs << "temperature=" << m_Config.temperature << "\n";
        ofs << "maxTokens=" << m_Config.maxTokens << "\n";
        ofs.close();
        GV_LOG_INFO("AIManager — config saved to " + m_Config.configFilePath);
    }
}

bool AIManager::LoadConfigFromFile() {
    std::ifstream ifs(m_Config.configFilePath);
    if (!ifs.is_open()) return false;

    std::string line;
    while (std::getline(ifs, line)) {
        // skip comments / section headers
        if (line.empty() || line[0] == '[' || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        // trim whitespace
        while (!key.empty() && key.back() == ' ') key.pop_back();
        while (!val.empty() && val.front() == ' ') val.erase(val.begin());

        if (key == "apiKey")       m_Config.apiKey = val;
        else if (key == "model")   m_Config.model = val;
        else if (key == "temperature") { try { m_Config.temperature = std::stof(val); } catch (...) {} }
        else if (key == "maxTokens")   { try { m_Config.maxTokens = static_cast<u32>(std::stoul(val)); } catch (...) {} }
    }
    return !m_Config.apiKey.empty();
}

// ── Helpers ────────────────────────────────────────────────────────────────

std::string AIManager::BuildRequestURL() const {
    return m_Config.baseUrl + m_Config.model + ":generateContent?key=" + m_Config.apiKey;
}

// ── HTTP POST (WinInet on Windows, placeholder elsewhere) ──────────────────

AIResponse AIManager::HttpPost(const std::string& url, const std::string& jsonBody) const {
    AIResponse resp;

#ifdef _WIN32
    // ── Parse URL components ───────────────────────────────────────────────
    std::string host, path;
    {
        std::string u = url;
        size_t schemeEnd = u.find("://");
        if (schemeEnd != std::string::npos) u = u.substr(schemeEnd + 3);
        size_t slash = u.find('/');
        if (slash != std::string::npos) {
            host = u.substr(0, slash);
            path = u.substr(slash);
        } else {
            host = u;
            path = "/";
        }
    }

    HINTERNET hInternet = InternetOpenA("GameVoid/1.0", INTERNET_OPEN_TYPE_PRECONFIG,
                                        nullptr, nullptr, 0);
    if (!hInternet) {
        resp.errorMessage = "InternetOpen failed (error " + std::to_string(GetLastError()) + ").";
        GV_LOG_ERROR(resp.errorMessage);
        return resp;
    }

    HINTERNET hConnect = InternetConnectA(hInternet, host.c_str(),
                                          INTERNET_DEFAULT_HTTPS_PORT,
                                          nullptr, nullptr,
                                          INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        resp.errorMessage = "InternetConnect failed (error " + std::to_string(GetLastError()) + ").";
        GV_LOG_ERROR(resp.errorMessage);
        InternetCloseHandle(hInternet);
        return resp;
    }

    DWORD flags = INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD;
    HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", path.c_str(),
                                          nullptr, nullptr, nullptr, flags, 0);
    if (!hRequest) {
        resp.errorMessage = "HttpOpenRequest failed (error " + std::to_string(GetLastError()) + ").";
        GV_LOG_ERROR(resp.errorMessage);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return resp;
    }

    // Set content type header
    const char* headers = "Content-Type: application/json\r\n";
    BOOL bResult = HttpSendRequestA(hRequest, headers, (DWORD)strlen(headers),
                                    (LPVOID)jsonBody.c_str(), (DWORD)jsonBody.size());
    if (!bResult) {
        resp.errorMessage = "HttpSendRequest failed (error " + std::to_string(GetLastError()) + ").";
        GV_LOG_ERROR(resp.errorMessage);
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return resp;
    }

    // Read response body
    std::string responseBody;
    char buf[4096];
    DWORD bytesRead = 0;
    while (InternetReadFile(hRequest, buf, sizeof(buf) - 1, &bytesRead) && bytesRead > 0) {
        buf[bytesRead] = '\0';
        responseBody.append(buf, bytesRead);
        bytesRead = 0;
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    resp.rawJSON = responseBody;

    // ── Extract text from Gemini JSON response ─────────────────────────────
    // Response format: { "candidates": [{ "content": { "parts": [{ "text": "..." }] } }] }
    // Simple extraction without a JSON library:
    {
        std::string marker = "\"text\"";
        size_t pos = responseBody.find(marker);
        if (pos != std::string::npos) {
            pos = responseBody.find(':', pos);
            if (pos != std::string::npos) {
                pos = responseBody.find('"', pos + 1);
                if (pos != std::string::npos) {
                    pos++; // skip opening "
                    std::string extracted;
                    while (pos < responseBody.size()) {
                        if (responseBody[pos] == '\\' && pos + 1 < responseBody.size()) {
                            char next = responseBody[pos + 1];
                            if (next == '"')       { extracted += '"';  pos += 2; continue; }
                            else if (next == 'n')  { extracted += '\n'; pos += 2; continue; }
                            else if (next == '\\') { extracted += '\\'; pos += 2; continue; }
                            else if (next == 't')  { extracted += '\t'; pos += 2; continue; }
                            else { extracted += next; pos += 2; continue; }
                        }
                        if (responseBody[pos] == '"') break;
                        extracted += responseBody[pos++];
                    }
                    resp.text = extracted;
                    resp.success = true;
                }
            }
        }

        if (!resp.success) {
            // Check for error message in response
            size_t errPos = responseBody.find("\"message\"");
            if (errPos != std::string::npos) {
                errPos = responseBody.find('"', responseBody.find(':', errPos) + 1);
                if (errPos != std::string::npos) {
                    errPos++;
                    std::string errMsg;
                    while (errPos < responseBody.size() && responseBody[errPos] != '"') {
                        errMsg += responseBody[errPos++];
                    }
                    resp.errorMessage = "Gemini API error: " + errMsg;
                }
            }
            if (resp.errorMessage.empty())
                resp.errorMessage = "Failed to parse Gemini response.";
            GV_LOG_WARN(resp.errorMessage);
        }
    }

    GV_LOG_INFO("AIManager::HttpPost — response " + std::to_string(responseBody.size()) + " bytes, success=" + (resp.success ? "true" : "false"));
#else
    // Non-Windows placeholder
    GV_LOG_INFO("AIManager::HttpPost (placeholder) — URL: " + url.substr(0, 80) + "...");
    (void)jsonBody;
    resp.success = false;
    resp.errorMessage = "HTTP client not implemented on this platform. Build on Windows for WinInet support.";
#endif

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

// ── Scene Generation from Prompt ───────────────────────────────────────────

std::string AIManager::BuildSceneGenPrompt(const std::string& userPrompt) const {
    return
        "You are a game level designer AI. The user will describe a scene.\n"
        "Generate a JSON array of game objects. Each element is an object with:\n"
        "  \"name\": string,\n"
        "  \"meshType\": \"cube\" | \"triangle\" | \"plane\",\n"
        "  \"position\": [x, y, z],\n"
        "  \"rotation\": [rx, ry, rz]  (degrees),\n"
        "  \"scale\": [sx, sy, sz],\n"
        "  \"color\": [r, g, b, a]  (0-1 floats),\n"
        "  \"hasPhysics\": true/false\n"
        "\n"
        "RULES:\n"
        "- Output ONLY the JSON array, nothing else. No markdown, no comments.\n"
        "- Ground/floor at Y=0. Objects above Y=0.\n"
        "- Use 5-30 objects. Be creative with placement.\n"
        "- Use varied colours.\n"
        "- Gravity objects: hasPhysics=true.\n"
        "\n"
        "Scene description: " + userPrompt;
}

// Simple hand-written JSON array-of-objects parser for ObjectBlueprint.
// Handles: [{ ... }, { ... }] with string, number, bool, array fields.
// Robust against whitespace, trailing commas, and markdown code fences.
AIManager::SceneGenResult AIManager::ParseSceneGenResponse(const std::string& raw) {
    SceneGenResult result;
    result.rawResponse = raw;

    // Strip markdown code fences if present
    std::string text = raw;
    {
        auto pos1 = text.find("```");
        if (pos1 != std::string::npos) {
            auto pos2 = text.find('\n', pos1);
            if (pos2 != std::string::npos) text = text.substr(pos2 + 1);
        }
        auto pos3 = text.rfind("```");
        if (pos3 != std::string::npos) text = text.substr(0, pos3);
    }

    // Find the outermost [ ... ]
    auto arrStart = text.find('[');
    auto arrEnd   = text.rfind(']');
    if (arrStart == std::string::npos || arrEnd == std::string::npos || arrEnd <= arrStart) {
        result.errorMessage = "No JSON array found in AI response.";
        return result;
    }
    text = text.substr(arrStart, arrEnd - arrStart + 1);

    // Helper lambdas for parsing
    size_t pos = 1; // skip '['
    auto skipWS = [&]() {
        while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\n' ||
               text[pos] == '\r' || text[pos] == '\t' || text[pos] == ','))
            pos++;
    };

    auto parseString = [&]() -> std::string {
        std::string s;
        if (pos >= text.size() || text[pos] != '"') return s;
        pos++; // skip opening "
        while (pos < text.size() && text[pos] != '"') {
            if (text[pos] == '\\' && pos + 1 < text.size()) { pos++; }
            s += text[pos++];
        }
        if (pos < text.size()) pos++; // skip closing "
        return s;
    };

    auto parseNumber = [&]() -> f32 {
        skipWS();
        std::string num;
        while (pos < text.size() && (text[pos] == '-' || text[pos] == '.' ||
               (text[pos] >= '0' && text[pos] <= '9') || text[pos] == 'e' ||
               text[pos] == 'E' || text[pos] == '+'))
            num += text[pos++];
        if (num.empty()) return 0.0f;
        try { return std::stof(num); } catch (...) { return 0.0f; }
    };

    auto parseBool = [&]() -> bool {
        skipWS();
        if (pos + 4 <= text.size() && text.substr(pos, 4) == "true") { pos += 4; return true; }
        if (pos + 5 <= text.size() && text.substr(pos, 5) == "false") { pos += 5; return false; }
        return false;
    };

    auto parseNumArray = [&](f32* out, int count) {
        skipWS();
        if (pos < text.size() && text[pos] == '[') pos++; // skip [
        for (int i = 0; i < count; i++) {
            skipWS();
            out[i] = parseNumber();
            skipWS();
            if (pos < text.size() && text[pos] == ',') pos++;
        }
        skipWS();
        if (pos < text.size() && text[pos] == ']') pos++; // skip ]
    };

    // Parse objects
    while (pos < text.size()) {
        skipWS();
        if (pos >= text.size() || text[pos] == ']') break;
        if (text[pos] != '{') { pos++; continue; }
        pos++; // skip '{'

        ObjectBlueprint bp;
        bp.name = "AI_Object";
        bp.meshType = "cube";
        f32 color[4] = { 0.7f, 0.7f, 0.7f, 1.0f };
        bool hasPhysics = false;

        // Parse key-value pairs
        while (pos < text.size() && text[pos] != '}') {
            skipWS();
            if (pos >= text.size() || text[pos] == '}') break;
            std::string key = parseString();
            skipWS();
            if (pos < text.size() && text[pos] == ':') pos++; // skip ':'
            skipWS();

            if (key == "name")         bp.name = parseString();
            else if (key == "meshType") bp.meshType = parseString();
            else if (key == "position") { f32 v[3]; parseNumArray(v, 3); bp.position = Vec3(v[0], v[1], v[2]); }
            else if (key == "rotation") { f32 v[3]; parseNumArray(v, 3); bp.rotation = Vec3(v[0], v[1], v[2]); }
            else if (key == "scale")    { f32 v[3]; parseNumArray(v, 3); bp.scale = Vec3(v[0], v[1], v[2]); }
            else if (key == "color")    { parseNumArray(color, 4); }
            else if (key == "hasPhysics") hasPhysics = parseBool();
            else {
                // Skip unknown value
                if (pos < text.size() && text[pos] == '"') parseString();
                else if (pos < text.size() && text[pos] == '[') {
                    int depth = 1; pos++;
                    while (pos < text.size() && depth > 0) {
                        if (text[pos] == '[') depth++;
                        else if (text[pos] == ']') depth--;
                        pos++;
                    }
                } else if (pos < text.size() && text[pos] == '{') {
                    int depth = 1; pos++;
                    while (pos < text.size() && depth > 0) {
                        if (text[pos] == '{') depth++;
                        else if (text[pos] == '}') depth--;
                        pos++;
                    }
                } else {
                    while (pos < text.size() && text[pos] != ',' && text[pos] != '}') pos++;
                }
            }
            skipWS();
        }

        if (pos < text.size() && text[pos] == '}') pos++; // skip '}'

        // Store colour in materialName as "r,g,b,a" for later parsing
        bp.materialName = std::to_string(color[0]) + "," + std::to_string(color[1]) + ","
                        + std::to_string(color[2]) + "," + std::to_string(color[3]);
        // Store hasPhysics in scriptSnippet field as a flag
        bp.scriptSnippet = hasPhysics ? "physics" : "";

        result.objects.push_back(bp);
    }

    result.success = !result.objects.empty();
    if (!result.success)
        result.errorMessage = "Parsed 0 objects from AI response.";

    return result;
}

AIManager::SceneGenResult AIManager::GenerateSceneFromPrompt(const std::string& userPrompt) const {
    std::string fullPrompt = BuildSceneGenPrompt(userPrompt);
    AIResponse resp = SendPrompt(fullPrompt);

    if (!resp.success) {
        SceneGenResult r;
        r.errorMessage = resp.errorMessage;
        r.rawResponse = resp.rawJSON;
        return r;
    }

    return ParseSceneGenResponse(resp.text);
}

// == generateObjectFromPrompt ================================================
// The primary AI->game-world entry-point.

GameObject* AIManager::GenerateObjectFromPrompt(const std::string& prompt, Scene& scene) const {
    GV_LOG_INFO("AIManager::GenerateObjectFromPrompt -- \"" + prompt + "\"");

    // 1. Ask Gemini to describe a single game object in JSON
    std::string aiPrompt =
        "You are a game engine assistant. Given the following description, produce "
        "a single JSON object with fields: name (string), meshType (cube|sphere|plane), "
        "position [x,y,z], rotation [x,y,z] (degrees), scale [x,y,z], "
        "materialName (string), hasPhysics (bool), scriptSnippet (string, Lua code or empty). "
        "Only output JSON, nothing else.\n\nDescription: " + prompt;

    AIResponse resp = SendPrompt(aiPrompt);

    // 2. Parse AI response into an ObjectBlueprint.
    //    In production: use nlohmann::json to parse resp.text.
    //    Skeleton: create a sensible default so the function still works.
    ObjectBlueprint bp;
    if (resp.success) {
        // TODO: JSON parse resp.text -> bp fields
        GV_LOG_INFO("AI returned object description (parse not implemented in skeleton).");
    } else {
        GV_LOG_WARN("AI call failed (" + resp.errorMessage + "), using defaults.");
    }

    // Fallback defaults derived from the prompt keywords
    if (bp.name.empty())     bp.name = "AI_Object";
    if (bp.meshType.empty()) {
        if (prompt.find("sphere") != std::string::npos || prompt.find("ball") != std::string::npos)
            bp.meshType = "sphere";
        else if (prompt.find("plane") != std::string::npos || prompt.find("floor") != std::string::npos)
            bp.meshType = "plane";
        else
            bp.meshType = "cube";
    }

    // 3. Instantiate in the scene
    GameObject* obj = scene.CreateGameObject(bp.name);
    obj->GetTransform().SetPosition(bp.position.x, bp.position.y, bp.position.z);
    obj->GetTransform().SetEulerDeg(bp.rotation.x, bp.rotation.y, bp.rotation.z);
    obj->GetTransform().SetScale(bp.scale.x, bp.scale.y, bp.scale.z);

    // Attach a MeshRenderer
    obj->AddComponent<MeshRenderer>();
    // In production:
    //   if (bp.meshType == "cube")   mr->SetMesh(Mesh::CreateCube());
    //   if (bp.meshType == "sphere") mr->SetMesh(Mesh::CreateSphere());
    //   if (bp.meshType == "plane")  mr->SetMesh(Mesh::CreatePlane());

    GV_LOG_INFO("Generated object '" + bp.name + "' (" + bp.meshType + ") from AI prompt.");
    return obj;
}

} // namespace gv
