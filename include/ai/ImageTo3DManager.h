// ============================================================================
// GameVoid Engine — Image to 3D Pipeline Manager
// ============================================================================
// Manages the complete pipeline of converting a user-supplied image into a
// textured 3D model that is loaded into the engine as a GameObject.
//
// Architecture:
//   C++ (this) ──HTTP──> Python Flask server (ai_server/)
//   The Python server handles AI inference (Gemini Vision + TripoSR/MiDaS)
//   and returns OBJ file paths that this class loads via the existing
//   Mesh::LoadOBJ() pipeline.
// ============================================================================
#pragma once

#include "core/Types.h"
#include "core/Math.h"
#include <string>
#include <vector>

namespace gv {

// Forward declarations
class Scene;
class GameObject;
class AssetManager;

// ============================================================================
// Generation Request
// ============================================================================
struct ImageTo3DRequest {
    std::string imagePath;                           // Local file path to source image
    std::string objectName;                          // Optional name override (auto-detected if empty)
    std::string method      = "auto";                // "auto", "triposr", or "midas"
    std::string serverHost  = "127.0.0.1";           // Python server host
    u32         serverPort  = 5000;                   // Python server port
    bool        useSelection = false;                // If true, generate from selected image region
    f32         selectionMinX = 0.0f;                // Normalized [0..1]
    f32         selectionMinY = 0.0f;                // Normalized [0..1]
    f32         selectionMaxX = 1.0f;                // Normalized [0..1]
    f32         selectionMaxY = 1.0f;                // Normalized [0..1]
    bool        useSmartPointSelection = false;      // If true, server segments object from click point
    f32         smartPointX = 0.5f;                  // Normalized [0..1]
    f32         smartPointY = 0.5f;                  // Normalized [0..1]

    // Multi-point SAM segmentation (Meta SAM-style)
    std::vector<Vec2> samPositivePoints;              // Points ON the object (include)
    std::vector<Vec2> samNegativePoints;              // Points to EXCLUDE
    std::string       maskedImagePath;                // Pre-segmented image path (from /segment)
};

// ============================================================================
// Generation Result
// ============================================================================
struct ImageTo3DResult {
    bool        success         = false;
    std::string objectName;                          // Detected/provided object name
    std::string category;                            // Object category (furniture, vehicle, etc.)
    std::string description;                         // Short description from Gemini
    std::string objFilePath;                         // Path to generated .obj file
    std::string textureFilePath;                     // Path to generated texture .png
    u32         vertexCount     = 0;
    u32         faceCount       = 0;
    f32         generationTime  = 0.0f;              // Seconds
    std::string generationMethod;                    // "triposr" or "midas"
    std::string errorMessage;
};

// ============================================================================
// Analysis Result (image classification only)
// ============================================================================
struct ImageAnalysisResult {
    bool        success = false;
    std::string objectName;
    std::string category;
    std::string description;
    std::string errorMessage;
};

// ============================================================================
// SAM Segmentation Result
// ============================================================================
struct SegmentResult {
    bool        success = false;
    std::string maskedImagePath;                     // Cropped RGBA image with background removed
    std::string previewImagePath;                    // Overlay visualization path
    u32         maskPixelCount = 0;
    u32         totalPixels    = 0;
    f32         coveragePercent = 0.0f;
    std::string errorMessage;
};

// ============================================================================
// Image To 3D Manager
// ============================================================================
/// Orchestrates the Image → 3D Model → Scene pipeline.
/// Communicates with the local Python AI server via HTTP.
class ImageTo3DManager {
public:
    ImageTo3DManager() = default;
    ~ImageTo3DManager() = default;

    // ── Server Management ──────────────────────────────────────────────────

    /// Check if the Python AI server is running and responsive.
    bool IsServerRunning(const std::string& host = "127.0.0.1", u32 port = 5000) const;

    /// Attempt to start the Python AI server as a background process.
    /// Returns true if the server was started (or was already running).
    bool StartServer();

    // ── Pipeline Steps ─────────────────────────────────────────────────────

    /// Step 1: Analyze an image to identify the object (Gemini Vision).
    /// Calls POST /analyze on the Python server.
    ImageAnalysisResult AnalyzeImage(const std::string& imagePath,
                                     const std::string& host = "127.0.0.1",
                                     u32 port = 5000) const;

    /// Step 2: Generate a 3D model from an image.
    /// Calls POST /generate_from_path on the Python server.
    /// This is the main entry point for the full pipeline.
    ImageTo3DResult GenerateFromImage(const ImageTo3DRequest& request) const;

    /// Step 3: Load the generated model into the scene as a GameObject.
    /// Creates a new GameObject with MeshRenderer + Material + Texture.
    GameObject* LoadIntoScene(const ImageTo3DResult& result,
                              Scene& scene,
                              AssetManager& assets) const;

    // ── Convenience: Full Pipeline ─────────────────────────────────────────

    /// Run the complete pipeline: image → analyze → generate → load into scene.
    /// Returns the created GameObject, or nullptr on failure.
    GameObject* ImageToGameObject(const std::string& imagePath,
                                  Scene& scene,
                                  AssetManager& assets,
                                  const std::string& method = "auto");

    // ── SAM Segmentation ──────────────────────────────────────────────────

    /// Segment an object using click points (calls /segment on Python server).
    /// Returns a SegmentResult with the path to the masked image.
    SegmentResult SegmentImageAtPoints(const std::string& imagePath,
                                       const std::vector<Vec2>& positivePoints,
                                       const std::vector<Vec2>& negativePoints,
                                       const std::string& host = "127.0.0.1",
                                       u32 port = 5000) const;

    /// Get a preview visualization of the segmentation mask.
    SegmentResult SegmentPreview(const std::string& imagePath,
                                 const std::vector<Vec2>& positivePoints,
                                 const std::vector<Vec2>& negativePoints,
                                 const std::string& host = "127.0.0.1",
                                 u32 port = 5000) const;

    // ── Configuration ──────────────────────────────────────────────────────

    /// Set the output directory for generated models.
    void SetOutputDir(const std::string& dir) { m_OutputDir = dir; }
    const std::string& GetOutputDir() const   { return m_OutputDir; }

private:
    /// Internal HTTP helpers (reuses WinInet pattern from AIManager).
    std::string HttpGet(const std::string& host, u32 port, const std::string& path) const;
    std::string HttpPostJson(const std::string& host, u32 port,
                              const std::string& path, const std::string& jsonBody) const;

    /// Simple JSON value extraction helpers (no external JSON library needed).
    static std::string ExtractJsonString(const std::string& json, const std::string& key);
    static int         ExtractJsonInt(const std::string& json, const std::string& key);
    static float       ExtractJsonFloat(const std::string& json, const std::string& key);
    static bool        ExtractJsonBool(const std::string& json, const std::string& key);

    std::string m_OutputDir = "generated_models";
};

} // namespace gv
