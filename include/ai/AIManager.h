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
    std::string configFilePath = "gamevoid_config.ini"; // stores API key securely
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
    AIConfig& GetConfigMut()               { return m_Config; }

    /// Convenience: set just the API key.
    void SetAPIKey(const std::string& key) { m_Config.apiKey = key; }

    /// Initialise the AI subsystem with an API key (Godot-style entry point).
    /// Loads/saves the key from/to the config file.
    void Init(const std::string& apiKey);

    /// Save the current API key to config file.
    void SaveConfigToFile() const;

    /// Load the API key from config file (returns true if found).
    bool LoadConfigFromFile();

    /// Returns true if the API key is set and non-empty.
    bool IsReady() const { return !m_Config.apiKey.empty(); }

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

    // ── Object generation from prompt ─────────────────────────────────────
    /// The primary AI-to-game-world function: takes a natural language prompt
    /// (e.g. "a red barrel with physics") and creates a fully configured
    /// GameObject in the given Scene, attaching mesh, material, physics, and
    /// an optional AI-generated script.  Returns the new object pointer.
    GameObject* GenerateObjectFromPrompt(const std::string& prompt, Scene& scene) const;

    // ── Scene generation (used by AI Generator panel) ──────────────────────
    /// Result of a scene-generation request: list of blueprints + status.
    struct SceneGenResult {
        bool success = false;
        std::string errorMessage;
        std::string rawResponse;           // raw AI text for logging
        std::vector<ObjectBlueprint> objects;
    };

    /// Build the system prompt that constrains AI output to valid JSON.
    std::string BuildSceneGenPrompt(const std::string& userPrompt) const;

    /// Parse the AI response text into ObjectBlueprints.
    /// Handles markdown code fences, partial JSON, etc.
    static SceneGenResult ParseSceneGenResponse(const std::string& text);

    /// High-level: send prompt, parse, return blueprints.
    SceneGenResult GenerateSceneFromPrompt(const std::string& userPrompt) const;

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
