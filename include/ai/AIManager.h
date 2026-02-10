// ============================================================================
// GameVoid Engine — AI Integration (Google Gemini)
// ============================================================================
// Provides a hook to call the Google Gemini API for generating game assets
// (textures, meshes described as text, scripts) and creating levels or
// objects from a natural-language text prompt.
//
// Prerequisites (production):
//   • libcurl or cpp-httplib for HTTP requests
//   • nlohmann/json for JSON parsing
// The skeleton defines the public API; HTTP plumbing is stubbed out.
// ============================================================================
#pragma once

#include "core/Types.h"
#include "core/Math.h"
#include <string>
#include <vector>
#include <functional>

namespace gv {

// Forward declarations
class Scene;
class GameObject;

// ============================================================================
// AI Configuration
// ============================================================================
struct AIConfig {
    std::string apiKey;                        // Google Gemini API key
    std::string model   = "gemini-2.0-flash";  // Model identifier
    std::string baseUrl = "https://generativelanguage.googleapis.com/v1beta/models/";
    f32 temperature     = 0.7f;
    u32 maxTokens       = 4096;
};

// ============================================================================
// AI Response
// ============================================================================
struct AIResponse {
    bool        success = false;
    std::string rawJSON;           // complete API response
    std::string text;              // extracted text content
    std::string errorMessage;
};

// ============================================================================
// AI Manager
// ============================================================================
/// High-level interface for AI-assisted content creation.
class AIManager {
public:
    AIManager() = default;
    ~AIManager() = default;

    // ── Configuration ──────────────────────────────────────────────────────
    void SetConfig(const AIConfig& config) { m_Config = config; }
    const AIConfig& GetConfig() const      { return m_Config; }

    /// Convenience: set just the API key.
    void SetAPIKey(const std::string& key) { m_Config.apiKey = key; }

    // ── Raw prompt ─────────────────────────────────────────────────────────
    /// Send an arbitrary text prompt to Gemini and return the response.
    AIResponse SendPrompt(const std::string& prompt) const;

    // ── Asset generation ───────────────────────────────────────────────────
    /// Ask the AI to generate a Lua/Python script for a given behaviour.
    /// Returns the script source code as a string.
    std::string GenerateScript(const std::string& behaviourDescription) const;

    /// Ask the AI to describe a procedural mesh or texture; the response can
    /// be parsed to create in-engine geometry.
    std::string GenerateAssetDescription(const std::string& assetPrompt) const;

    // ── Level generation ───────────────────────────────────────────────────
    /// From a free-form text prompt, generate a list of object descriptions
    /// (name, position, scale, mesh type, material, etc.) that the engine
    /// can instantiate in a Scene.
    struct ObjectBlueprint {
        std::string name;
        std::string meshType;       // "cube", "sphere", "plane", …
        Vec3 position{ 0, 0, 0 };
        Vec3 rotation{ 0, 0, 0 };   // Euler degrees
        Vec3 scale   { 1, 1, 1 };
        std::string materialName;
        std::string scriptSnippet;   // optional Lua/Python code
    };

    std::vector<ObjectBlueprint> GenerateLevel(const std::string& levelPrompt) const;

    /// Instantiate the blueprints from GenerateLevel() into a live Scene.
    void PopulateScene(Scene& scene, const std::vector<ObjectBlueprint>& blueprints) const;

    // ── Async variant (callback-based; placeholder) ────────────────────────
    using ResponseCallback = std::function<void(const AIResponse&)>;
    void SendPromptAsync(const std::string& prompt, ResponseCallback cb) const;

private:
    /// Build the full request URL for the configured model.
    std::string BuildRequestURL() const;

    /// Perform the actual HTTP POST (stubbed in skeleton).
    AIResponse HttpPost(const std::string& url, const std::string& jsonBody) const;

    AIConfig m_Config;
};

} // namespace gv
