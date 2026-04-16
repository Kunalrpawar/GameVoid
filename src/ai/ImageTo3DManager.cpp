// ============================================================================
// GameVoid Engine — Image to 3D Pipeline Manager (Implementation)
// ============================================================================
// HTTP communication with the Python AI server + scene integration.
// Uses WinInet on Windows (same pattern as AIManager::HttpPost).
// ============================================================================

#include "ai/ImageTo3DManager.h"
#include "core/Scene.h"
#include "core/GameObject.h"
#include "renderer/MeshRenderer.h"
#include "assets/Assets.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")
#endif

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace gv {

// ── JSON Helpers (lightweight, no external library) ─────────────────────────

static std::string JsonEscapeStr(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;
        }
    }
    return out;
}

std::string ImageTo3DManager::ExtractJsonString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";

    // Find the colon after the key
    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return "";

    // Skip whitespace
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;

    if (pos >= json.size() || json[pos] != '"') return "";
    pos++; // skip opening quote

    std::string result;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            pos++;
            switch (json[pos]) {
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                case '/':  result += '/';  break;
                default:   result += json[pos]; break;
            }
        } else {
            result += json[pos];
        }
        pos++;
    }
    return result;
}

int ImageTo3DManager::ExtractJsonInt(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return 0;
    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return 0;
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;

    std::string num;
    while (pos < json.size() && (json[pos] == '-' || (json[pos] >= '0' && json[pos] <= '9')))
        num += json[pos++];
    if (num.empty()) return 0;
    try { return std::stoi(num); } catch (...) { return 0; }
}

float ImageTo3DManager::ExtractJsonFloat(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return 0.0f;
    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return 0.0f;
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;

    std::string num;
    while (pos < json.size() && (json[pos] == '-' || json[pos] == '.' ||
           (json[pos] >= '0' && json[pos] <= '9') || json[pos] == 'e' || json[pos] == 'E'))
        num += json[pos++];
    if (num.empty()) return 0.0f;
    try { return std::stof(num); } catch (...) { return 0.0f; }
}

bool ImageTo3DManager::ExtractJsonBool(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return false;
    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return false;
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos + 4 <= json.size() && json.substr(pos, 4) == "true") return true;
    return false;
}

// ── HTTP Helpers (WinInet) ──────────────────────────────────────────────────

std::string ImageTo3DManager::HttpGet(const std::string& host, u32 port, const std::string& path) const {
#ifdef _WIN32
    HINTERNET hInternet = InternetOpenA("GameVoid/1.0", INTERNET_OPEN_TYPE_PRECONFIG,
                                        nullptr, nullptr, 0);
    if (!hInternet) {
        GV_LOG_ERROR("ImageTo3D: InternetOpen failed");
        return "";
    }

    HINTERNET hConnect = InternetConnectA(hInternet, host.c_str(),
                                          static_cast<INTERNET_PORT>(port),
                                          nullptr, nullptr,
                                          INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        GV_LOG_ERROR("ImageTo3D: InternetConnect failed");
        return "";
    }

    DWORD flags = INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD;
    HINTERNET hRequest = HttpOpenRequestA(hConnect, "GET", path.c_str(),
                                          nullptr, nullptr, nullptr, flags, 0);
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        GV_LOG_ERROR("ImageTo3D: HttpOpenRequest failed");
        return "";
    }

    // Set a short timeout for health checks
    DWORD timeout = 5000; // 5 seconds
    InternetSetOptionA(hRequest, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionA(hRequest, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));

    if (!HttpSendRequestA(hRequest, nullptr, 0, nullptr, 0)) {
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "";
    }

    std::string response;
    char buf[4096];
    DWORD bytesRead = 0;
    while (InternetReadFile(hRequest, buf, sizeof(buf) - 1, &bytesRead) && bytesRead > 0) {
        buf[bytesRead] = '\0';
        response.append(buf, bytesRead);
        bytesRead = 0;
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return response;
#else
    (void)host; (void)port; (void)path;
    GV_LOG_WARN("ImageTo3D: HTTP not implemented on this platform");
    return "";
#endif
}

std::string ImageTo3DManager::HttpPostJson(const std::string& host, u32 port,
                                            const std::string& path,
                                            const std::string& jsonBody) const {
#ifdef _WIN32
    HINTERNET hInternet = InternetOpenA("GameVoid/1.0", INTERNET_OPEN_TYPE_PRECONFIG,
                                        nullptr, nullptr, 0);
    if (!hInternet) {
        GV_LOG_ERROR("ImageTo3D: InternetOpen failed");
        return "";
    }

    HINTERNET hConnect = InternetConnectA(hInternet, host.c_str(),
                                          static_cast<INTERNET_PORT>(port),
                                          nullptr, nullptr,
                                          INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        GV_LOG_ERROR("ImageTo3D: InternetConnect failed");
        return "";
    }

    DWORD flags = INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD;
    HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", path.c_str(),
                                          nullptr, nullptr, nullptr, flags, 0);
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        GV_LOG_ERROR("ImageTo3D: HttpOpenRequest failed");
        return "";
    }

    // Set a much longer timeout for generation (can take 3-5 minutes on CPU)
    DWORD timeout = 600000; // 10 minutes
    InternetSetOptionA(hRequest, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionA(hRequest, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionA(hRequest, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));

    const char* headers = "Content-Type: application/json\r\n";
    BOOL sent = HttpSendRequestA(hRequest, headers, static_cast<DWORD>(strlen(headers)),
                                  (LPVOID)jsonBody.c_str(),
                                  static_cast<DWORD>(jsonBody.size()));
    if (!sent) {
        DWORD err = GetLastError();
        GV_LOG_ERROR("ImageTo3D: HttpSendRequest failed (error " + std::to_string(err) + ")");
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "";
    }

    std::string response;
    char buf[4096];
    DWORD bytesRead = 0;
    while (InternetReadFile(hRequest, buf, sizeof(buf) - 1, &bytesRead) && bytesRead > 0) {
        buf[bytesRead] = '\0';
        response.append(buf, bytesRead);
        bytesRead = 0;
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return response;
#else
    (void)host; (void)port; (void)path; (void)jsonBody;
    GV_LOG_WARN("ImageTo3D: HTTP not implemented on this platform");
    return "";
#endif
}

// ── Server Management ───────────────────────────────────────────────────────

bool ImageTo3DManager::IsServerRunning(const std::string& host, u32 port) const {
    std::string resp = HttpGet(host, port, "/health");
    if (resp.empty()) return false;

    // Check if response contains "ok"
    return resp.find("\"ok\"") != std::string::npos;
}

bool ImageTo3DManager::StartServer() {
#ifdef _WIN32
    GV_LOG_INFO("ImageTo3D: Attempting to start Python AI server...");

    // Build the command to start the server
    std::string scriptPath = "ai_server\\server.py";

    // Try the venv Python first, then system Python
    std::string venvPython = "ai_server\\.venv\\Scripts\\python.exe";
    std::string cmd;

    // Check if venv exists
    std::ifstream venvCheck(venvPython);
    if (venvCheck.good()) {
        cmd = "start /B \"\" \"" + venvPython + "\" \"" + scriptPath + "\"";
    } else {
        cmd = "start /B \"\" python \"" + scriptPath + "\"";
    }
    venvCheck.close();

    // Launch as a background process
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_MINIMIZE;

    PROCESS_INFORMATION pi = {};

    std::string fullCmd = "cmd.exe /C " + cmd;
    BOOL created = CreateProcessA(
        nullptr,
        const_cast<char*>(fullCmd.c_str()),
        nullptr, nullptr, FALSE,
        CREATE_NEW_CONSOLE | CREATE_NEW_PROCESS_GROUP,
        nullptr, nullptr,
        &si, &pi
    );

    if (created) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        GV_LOG_INFO("ImageTo3D: Server process started. Waiting for it to be ready...");

        // Wait up to 15 seconds for the server to respond
        for (int i = 0; i < 30; i++) {
            Sleep(500);
            if (IsServerRunning()) {
                GV_LOG_INFO("ImageTo3D: Server is ready!");
                return true;
            }
        }
        GV_LOG_WARN("ImageTo3D: Server started but not responding after 15 seconds.");
        return false;
    } else {
        GV_LOG_ERROR("ImageTo3D: Failed to start server (error " +
                     std::to_string(GetLastError()) + ")");
        return false;
    }
#else
    GV_LOG_WARN("ImageTo3D: Server auto-start not implemented on this platform");
    return false;
#endif
}

// ── Pipeline Steps ──────────────────────────────────────────────────────────

ImageAnalysisResult ImageTo3DManager::AnalyzeImage(const std::string& imagePath,
                                                     const std::string& host,
                                                     u32 port) const {
    ImageAnalysisResult result;

    GV_LOG_INFO("ImageTo3D: Analyzing image: " + imagePath);

    // Build JSON body with the image path
    std::string json = R"({"image_path":")" + JsonEscapeStr(imagePath) + R"("})";

    // We use generate_from_path but we can also set up an analyze_from_path endpoint.
    // For now, the analysis is done as part of generation. Let's call the server
    // with just analysis intent by POSTing to /analyze with the file path.
    // Since /analyze expects multipart, we'll use a simple workaround:
    // Send the image path in JSON and let the server read it locally.

    // The server's /generate_from_path already does analysis as part of generation.
    // For standalone analysis, we'll use a direct Gemini call via AIManager instead.
    // But to keep this self-contained, let's try the server.

    std::string resp = HttpPostJson(host, port, "/analyze_from_path", json);

    if (resp.empty()) {
        // Fallback: if /analyze_from_path doesn't exist, that's okay
        // The full generate pipeline does its own analysis
        result.errorMessage = "Server not responding or analysis endpoint unavailable";
        GV_LOG_WARN("ImageTo3D: " + result.errorMessage);
        return result;
    }

    result.success = ExtractJsonBool(resp, "success");
    result.objectName = ExtractJsonString(resp, "object");
    result.category = ExtractJsonString(resp, "category");
    result.description = ExtractJsonString(resp, "description");
    if (!result.success) {
        result.errorMessage = ExtractJsonString(resp, "error");
    }

    GV_LOG_INFO("ImageTo3D: Analysis result — " + result.objectName +
                " (" + result.category + ")");
    return result;
}

ImageTo3DResult ImageTo3DManager::GenerateFromImage(const ImageTo3DRequest& request) const {
    ImageTo3DResult result;

    GV_LOG_INFO("ImageTo3D: Generating 3D model from: " + request.imagePath);
    GV_LOG_INFO("ImageTo3D: Method: " + request.method);

    // Check if file exists
    {
        std::ifstream check(request.imagePath);
        if (!check.good()) {
            result.errorMessage = "Image file not found: " + request.imagePath;
            GV_LOG_ERROR("ImageTo3D: " + result.errorMessage);
            return result;
        }
    }

    // Build JSON request body.
    // We keep image_path as the original source and pass mask/points explicitly so
    // the server can choose the best preprocessing path.
    std::string imgPath = request.imagePath;
    std::replace(imgPath.begin(), imgPath.end(), '\\', '/');

    std::string json = R"({"image_path":")" + JsonEscapeStr(imgPath) +
                       R"(","method":")" + request.method + R"(")";

    auto buildPointsJson = [](const std::string& key, const std::vector<Vec2>& points) -> std::string {
        if (points.empty()) return "";
        std::string out = ",\"" + key + "\":[";
        for (size_t i = 0; i < points.size(); ++i) {
            if (i > 0) out += ",";
            out += "[" + std::to_string(points[i].x) + "," + std::to_string(points[i].y) + "]";
        }
        out += "]";
        return out;
    };

    if (!request.objectName.empty()) {
        json += R"(,"name":")" + JsonEscapeStr(request.objectName) + R"(")";
    }
    if (!request.maskedImagePath.empty()) {
        std::string maskPath = request.maskedImagePath;
        std::replace(maskPath.begin(), maskPath.end(), '\\', '/');
        json += R"(,"mask_image_path":")" + JsonEscapeStr(maskPath) + R"(")";
    }
    json += buildPointsJson("positive_points", request.samPositivePoints);
    json += buildPointsJson("negative_points", request.samNegativePoints);
    if (request.useSelection) {
        json += R"(,"use_selection":true)";
        json += R"(,"selection_min_x":)" + std::to_string(request.selectionMinX);
        json += R"(,"selection_min_y":)" + std::to_string(request.selectionMinY);
        json += R"(,"selection_max_x":)" + std::to_string(request.selectionMaxX);
        json += R"(,"selection_max_y":)" + std::to_string(request.selectionMaxY);
    }
    if (request.useSmartPointSelection) {
        json += R"(,"use_smart_point":true)";
        json += R"(,"smart_point_x":)" + std::to_string(request.smartPointX);
        json += R"(,"smart_point_y":)" + std::to_string(request.smartPointY);
    }
    json += "}";

    GV_LOG_INFO("ImageTo3D: Sending request to server...");

    std::string resp = HttpPostJson(request.serverHost, request.serverPort,
                                     "/generate_from_path", json);

    if (resp.empty()) {
        result.errorMessage = "Server not responding. Is the AI server running? "
                              "Start it with: .\\ai_server\\setup_ai_server.ps1 -StartOnly";
        GV_LOG_ERROR("ImageTo3D: " + result.errorMessage);
        return result;
    }

    // Parse response
    result.success          = ExtractJsonBool(resp, "success");
    result.objectName       = ExtractJsonString(resp, "object_name");
    result.objFilePath      = ExtractJsonString(resp, "obj_path");
    result.textureFilePath  = ExtractJsonString(resp, "texture_path");
    result.vertexCount      = static_cast<u32>(ExtractJsonInt(resp, "vertex_count"));
    result.faceCount        = static_cast<u32>(ExtractJsonInt(resp, "face_count"));
    result.generationTime   = ExtractJsonFloat(resp, "generation_time");
    result.generationMethod = ExtractJsonString(resp, "method");

    if (!result.success) {
        result.errorMessage = ExtractJsonString(resp, "error");
        GV_LOG_ERROR("ImageTo3D: Generation failed — " + result.errorMessage);
    } else {
        GV_LOG_INFO("ImageTo3D: Model generated successfully!");
        GV_LOG_INFO("  OBJ:     " + result.objFilePath);
        GV_LOG_INFO("  Texture: " + result.textureFilePath);
        GV_LOG_INFO("  Verts:   " + std::to_string(result.vertexCount));
        GV_LOG_INFO("  Faces:   " + std::to_string(result.faceCount));
        GV_LOG_INFO("  Time:    " + std::to_string(result.generationTime) + "s");
        GV_LOG_INFO("  Method:  " + result.generationMethod);
    }

    return result;
}

// ── SAM Segmentation ────────────────────────────────────────────────────────

static std::string BuildPointsJson(const std::string& key, const std::vector<Vec2>& points) {
    if (points.empty()) return "";
    std::string arr = ",\"" + key + "\":[";
    for (size_t i = 0; i < points.size(); ++i) {
        if (i > 0) arr += ",";
        arr += "[" + std::to_string(points[i].x) + "," + std::to_string(points[i].y) + "]";
    }
    arr += "]";
    return arr;
}

SegmentResult ImageTo3DManager::SegmentImageAtPoints(const std::string& imagePath,
                                                      const std::vector<Vec2>& positivePoints,
                                                      const std::vector<Vec2>& negativePoints,
                                                      const std::string& host,
                                                      u32 port) const {
    SegmentResult result;

    if (positivePoints.empty()) {
        result.errorMessage = "At least one positive point required";
        return result;
    }

    std::string imgPath = imagePath;
    std::replace(imgPath.begin(), imgPath.end(), '\\', '/');

    std::string json = R"({"image_path":")" + JsonEscapeStr(imgPath) + R"(")";
    json += BuildPointsJson("positive_points", positivePoints);
    json += BuildPointsJson("negative_points", negativePoints);
    json += "}";

    GV_LOG_INFO("ImageTo3D: Segmenting with " + std::to_string(positivePoints.size()) +
                " positive, " + std::to_string(negativePoints.size()) + " negative points");

    std::string resp = HttpPostJson(host, port, "/segment", json);
    if (resp.empty()) {
        result.errorMessage = "Server not responding for segmentation";
        return result;
    }

    result.success         = ExtractJsonBool(resp, "success");
    result.maskedImagePath = ExtractJsonString(resp, "masked_image_path");
    result.maskPixelCount  = static_cast<u32>(ExtractJsonInt(resp, "mask_pixel_count"));
    result.totalPixels     = static_cast<u32>(ExtractJsonInt(resp, "total_pixels"));
    result.coveragePercent = ExtractJsonFloat(resp, "coverage_percent");

    if (!result.success) {
        result.errorMessage = ExtractJsonString(resp, "error");
        GV_LOG_WARN("ImageTo3D: Segmentation failed — " + result.errorMessage);
    } else {
        GV_LOG_INFO("ImageTo3D: Segmented " + std::to_string(result.coveragePercent) + "% of image");
    }

    return result;
}

SegmentResult ImageTo3DManager::SegmentPreview(const std::string& imagePath,
                                                const std::vector<Vec2>& positivePoints,
                                                const std::vector<Vec2>& negativePoints,
                                                const std::string& host,
                                                u32 port) const {
    SegmentResult result;

    if (positivePoints.empty()) {
        result.errorMessage = "At least one positive point required";
        return result;
    }

    std::string imgPath = imagePath;
    std::replace(imgPath.begin(), imgPath.end(), '\\', '/');

    std::string json = R"({"image_path":")" + JsonEscapeStr(imgPath) + R"(")";
    json += BuildPointsJson("positive_points", positivePoints);
    json += BuildPointsJson("negative_points", negativePoints);
    json += "}";

    std::string resp = HttpPostJson(host, port, "/segment_preview", json);
    if (resp.empty()) {
        result.errorMessage = "Server not responding for preview";
        return result;
    }

    result.success          = ExtractJsonBool(resp, "success");
    result.previewImagePath = ExtractJsonString(resp, "preview_path");
    result.maskPixelCount   = static_cast<u32>(ExtractJsonInt(resp, "mask_pixel_count"));

    if (!result.success) {
        result.errorMessage = ExtractJsonString(resp, "error");
    }

    return result;
}

// ── Scene Integration ───────────────────────────────────────────────────────

GameObject* ImageTo3DManager::LoadIntoScene(const ImageTo3DResult& result,
                                             Scene& scene,
                                             AssetManager& assets) const {
    if (!result.success || result.objFilePath.empty()) {
        GV_LOG_ERROR("ImageTo3D: Cannot load — generation result is invalid.");
        return nullptr;
    }

    GV_LOG_INFO("ImageTo3D: Loading model into scene: " + result.objFilePath);

    // 1. Load the mesh via AssetManager (uses existing OBJ loader)
    Shared<Mesh> mesh = assets.LoadMesh(result.objFilePath);
    if (!mesh) {
        GV_LOG_ERROR("ImageTo3D: Failed to load mesh: " + result.objFilePath);
        return nullptr;
    }

    // 2. Load the texture
    Shared<Texture> texture;
    if (!result.textureFilePath.empty()) {
        texture = assets.LoadTexture(result.textureFilePath);
        if (!texture) {
            GV_LOG_WARN("ImageTo3D: Failed to load texture: " + result.textureFilePath +
                        " — proceeding without texture");
        }
    }

    // 3. Create material
    std::string matName = "AI_" + result.objectName + "_Material";
    Shared<Material> material = assets.CreateMaterial(matName);
    if (material) {
        material->albedo = Vec3(1.0f, 1.0f, 1.0f);   // White base (texture provides colour)
        material->metallic = 0.0f;
        material->roughness = 0.7f;
        if (texture) {
            material->diffuseMap = texture;
        }
    }

    // 4. Create GameObject
    std::string objName = result.objectName.empty() ? "AI_Model" : ("AI_" + result.objectName);
    GameObject* obj = scene.CreateGameObject(objName);

    // 5. Compute mesh bounds and auto-scale to reasonable size
    Vec3 boundsMin, boundsMax;
    mesh->GetBounds(boundsMin, boundsMax);
    
    Vec3 boundsCenter = (boundsMin + boundsMax) * 0.5f;
    Vec3 boundsExtents = boundsMax - boundsMin;
    f32 maxExtent = std::max({boundsExtents.x, boundsExtents.y, boundsExtents.z});
    
    f32 targetSize = 2.0f;  // Scale to fit in 2x2x2 cube
    f32 scale = 1.0f;
    if (maxExtent > 0.001f) {
        scale = targetSize / maxExtent;
    }
    
    // Apply computed transform
    obj->GetTransform().SetScale(scale, scale, scale);
    obj->GetTransform().SetPosition(0.0f, 1.0f, 0.0f);
    
    GV_LOG_INFO("ImageTo3D: Bounds: min(" + std::to_string(boundsMin.x) + "," + std::to_string(boundsMin.y) + "," + 
                std::to_string(boundsMin.z) + ") max(" + std::to_string(boundsMax.x) + "," + 
                std::to_string(boundsMax.y) + "," + std::to_string(boundsMax.z) + ") extents: " + 
                std::to_string(boundsExtents.x) + "," + std::to_string(boundsExtents.y) + "," + 
                std::to_string(boundsExtents.z) + " — applied scale: " + std::to_string(scale));

    // 6. Attach MeshRenderer with loaded mesh and material
    auto* mr = obj->AddComponent<MeshRenderer>();
    mr->SetMesh(mesh);
    if (material) {
        mr->SetMaterial(material);
    }

    GV_LOG_INFO("ImageTo3D: Created GameObject '" + objName + "' with " +
                std::to_string(result.vertexCount) + " vertices in scene.");

    return obj;
}

// ── Convenience: Full Pipeline ──────────────────────────────────────────────

GameObject* ImageTo3DManager::ImageToGameObject(const std::string& imagePath,
                                                 Scene& scene,
                                                 AssetManager& assets,
                                                 const std::string& method) {
    GV_LOG_INFO("ImageTo3D: === Starting full Image-to-3D pipeline ===");
    GV_LOG_INFO("ImageTo3D: Input: " + imagePath);

    // Check server
    if (!IsServerRunning()) {
        GV_LOG_WARN("ImageTo3D: Server not running. Attempting to start...");
        if (!StartServer()) {
            GV_LOG_ERROR("ImageTo3D: Could not start AI server. "
                         "Please run: .\\ai_server\\setup_ai_server.ps1");
            return nullptr;
        }
    }

    // Generate
    ImageTo3DRequest request;
    request.imagePath = imagePath;
    request.method = method;

    ImageTo3DResult result = GenerateFromImage(request);
    if (!result.success) {
        GV_LOG_ERROR("ImageTo3D: Pipeline failed at generation step.");
        return nullptr;
    }

    // Load into scene
    GameObject* obj = LoadIntoScene(result, scene, assets);
    if (obj) {
        GV_LOG_INFO("ImageTo3D: === Pipeline complete! Object '" +
                    obj->GetName() + "' added to scene. ===");
    }

    return obj;
}

} // namespace gv
