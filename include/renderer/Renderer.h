// ============================================================================
// GameVoid Engine — Renderer Interface
// ============================================================================
// Abstract rendering back-end.  The skeleton ships with an OpenGL
// implementation (OpenGLRenderer); a Vulkan back-end can be added later by
// deriving from the same interface.
// ============================================================================
#pragma once

#include "core/Types.h"
#include "core/Math.h"
#include <string>
#include <vector>

#ifdef GV_HAS_GLFW
class Window;  // avoid including Window.h here
#endif

namespace gv {

// Forward declarations
class Camera;
class Scene;
class Window;

/// Gizmo modes (shared between editor and renderer).
enum class GizmoMode { Translate, Rotate, Scale };

/// Built-in primitive type used by MeshRenderer (also declared in MeshRenderer.h).
enum class PrimitiveType : int;

/// Abstract rendering API.
class IRenderer {
public:
    virtual ~IRenderer() = default;

    /// Initialise the graphics context / window.
    virtual bool Init(u32 width, u32 height, const std::string& title) = 0;

    /// Shutdown and release GPU resources.
    virtual void Shutdown() = 0;

    /// Clear the colour and depth buffers.
    virtual void Clear(f32 r, f32 g, f32 b, f32 a) = 0;

    /// Begin a new frame.
    virtual void BeginFrame() = 0;

    /// End & present the frame.
    virtual void EndFrame() = 0;

    /// Draw the entire scene from the perspective of the given camera.
    virtual void RenderScene(Scene& scene, Camera& camera) = 0;

    // ── Drawing primitives (2D & 3D) ───────────────────────────────────────
    /// Draw a 2D rectangle (screen-space).  Used for UI overlays, sprites.
    virtual void DrawRect(f32 x, f32 y, f32 w, f32 h, const Vec4& colour);

    /// Draw a 2D textured quad.
    virtual void DrawTexture(u32 textureID, f32 x, f32 y, f32 w, f32 h);

    /// Draw a wireframe / solid 3D shape for debugging (box, sphere).
    virtual void DrawDebugBox(const Vec3& center, const Vec3& halfExtents, const Vec4& colour);
    virtual void DrawDebugSphere(const Vec3& center, f32 radius, const Vec4& colour);
    virtual void DrawDebugLine(const Vec3& from, const Vec3& to, const Vec4& colour);

    // ── Lighting pass ──────────────────────────────────────────────────────
    virtual void ApplyLighting(Scene& scene) { (void)scene; }

    /// Check whether the window should close.
    virtual bool WindowShouldClose() const = 0;

    /// Poll input & window events.
    virtual void PollEvents() = 0;

    /// Get the window dimensions.
    virtual u32 GetWidth()  const = 0;
    virtual u32 GetHeight() const = 0;

    /// Set the active camera for debug draw calls
    virtual void SetDebugCamera(Camera* cam) { m_DebugCamera = cam; }

protected:
    Camera* m_DebugCamera = nullptr;
};

// ============================================================================
// OpenGL Renderer
// ============================================================================
class OpenGLRenderer : public IRenderer {
public:
    OpenGLRenderer() = default;
    ~OpenGLRenderer() override { Shutdown(); }

    /// Provide the Window that owns the GL context (call before Init).
    void SetWindow(Window* window) { m_Window = window; }

    bool Init(u32 width, u32 height, const std::string& title) override;
    void Shutdown() override;
    void Clear(f32 r, f32 g, f32 b, f32 a) override;
    void BeginFrame() override;
    void EndFrame() override;
    void RenderScene(Scene& scene, Camera& camera) override;
    void ApplyLighting(Scene& scene) override;
    bool WindowShouldClose() const override;
    void PollEvents() override;
    u32  GetWidth()  const override { return m_Width; }
    u32  GetHeight() const override { return m_Height; }

    // ── Debug draw (overrides) ─────────────────────────────────────────────
    void DrawDebugBox(const Vec3& center, const Vec3& halfExtents, const Vec4& colour) override;
    void DrawDebugSphere(const Vec3& center, f32 radius, const Vec4& colour) override;
    void DrawDebugLine(const Vec3& from, const Vec3& to, const Vec4& colour) override;
    void DrawRect(f32 x, f32 y, f32 w, f32 h, const Vec4& colour) override;
    void DrawTexture(u32 textureID, f32 x, f32 y, f32 w, f32 h) override;

    // ── Post-processing ────────────────────────────────────────────────────
    void SetBloomEnabled(bool e)       { m_BloomEnabled = e; }
    void SetToneMappingEnabled(bool e) { m_ToneMappingEnabled = e; }
    void SetFXAAEnabled(bool e)        { m_FXAAEnabled = e; }
    bool IsBloomEnabled() const        { return m_BloomEnabled; }
    bool IsToneMappingEnabled() const  { return m_ToneMappingEnabled; }
    bool IsFXAAEnabled() const         { return m_FXAAEnabled; }
    void SetBloomThreshold(f32 t)      { m_BloomThreshold = t; }
    void SetBloomIntensity(f32 i)      { m_BloomIntensity = i; }
    void SetExposure(f32 e)            { m_Exposure = e; }
    f32  GetExposure() const           { return m_Exposure; }

    // ── Shadow mapping ─────────────────────────────────────────────────────
    void SetShadowsEnabled(bool e)     { m_ShadowsEnabled = e; }
    bool IsShadowsEnabled() const      { return m_ShadowsEnabled; }

#ifdef GV_HAS_GLFW
    /// Draw the built-in demo triangle (call from Engine game loop).
    void RenderDemo(f32 dt);
#endif

    // ── Lighting toggle ──────────────────────────────────────────────────────
    void SetLightingEnabled(bool e) { m_LightingEnabled = e; }
    bool IsLightingEnabled() const  { return m_LightingEnabled; }

private:
    u32  m_Width  = 0;
    u32  m_Height = 0;
    bool m_Initialised = false;
    bool m_LightingEnabled = true;
    Window* m_Window = nullptr;

    // ── Post-processing settings ───────────────────────────────────────────
    bool m_BloomEnabled       = false;
    bool m_ToneMappingEnabled = true;
    bool m_FXAAEnabled        = false;
    f32  m_BloomThreshold     = 1.0f;
    f32  m_BloomIntensity     = 0.3f;
    f32  m_Exposure           = 1.0f;

    // ── Shadow settings ────────────────────────────────────────────────────
    bool m_ShadowsEnabled = true;

#ifdef GV_HAS_GLFW
    // Built-in demo triangle (proves GL context works)
    u32 m_DemoVAO    = 0;
    u32 m_DemoVBO    = 0;
    u32 m_DemoShader = 0;
    void InitDemo();
    void CleanupDemo();

    // ── Scene-rendering resources (PBR) ────────────────────────────────────
    u32 m_SceneShader = 0;   // PBR shader program

    // Built-in primitives (triangle + cube + plane):
    u32 m_TriVAO = 0, m_TriVBO = 0;
    u32 m_CubeVAO = 0, m_CubeVBO = 0, m_CubeEBO = 0;
    i32 m_CubeIndexCount = 0;
    u32 m_PlaneVAO = 0, m_PlaneVBO = 0, m_PlaneEBO = 0;
    i32 m_PlaneIndexCount = 0;

    // ── Shadow Mapping ─────────────────────────────────────────────────────
    u32 m_ShadowFBO = 0;
    u32 m_ShadowMap = 0;       // depth texture
    u32 m_ShadowShader = 0;    // depth-only pass shader
    u32 m_ShadowMapSize = 2048;
    Mat4 m_LightSpaceMatrix;
    void InitShadowMap();
    void RenderShadowPass(Scene& scene, const Vec3& lightDir);
    void CleanupShadowMap();

    // ── Post-Processing Pipeline ───────────────────────────────────────────
    u32 m_HDR_FBO = 0;           // HDR framebuffer
    u32 m_HDR_ColorTex = 0;      // RGBA16F colour attachment
    u32 m_HDR_BrightTex = 0;     // brightness extraction for bloom
    u32 m_HDR_DepthRBO = 0;      // depth renderbuffer
    u32 m_BloomFBO[2] = {0, 0};  // ping-pong blur FBOs
    u32 m_BloomTex[2] = {0, 0};  // ping-pong blur textures
    u32 m_BrightPassShader = 0;
    u32 m_BlurShader = 0;
    u32 m_TonemapShader = 0;
    u32 m_FXAAShader = 0;
    u32 m_ScreenQuadVAO = 0, m_ScreenQuadVBO = 0;
    void InitPostProcessing();
    void BeginHDRPass();
    void EndHDRPass();
    void RenderPostProcessing();
    void CleanupPostProcessing();
    void InitScreenQuad();

    // ── Sprite/2D Rendering ────────────────────────────────────────────────
    u32 m_SpriteShader = 0;
    u32 m_SpriteVAO = 0, m_SpriteVBO = 0;
    void InitSpriteRenderer();
    void CleanupSpriteRenderer();

    // ── Debug Draw ─────────────────────────────────────────────────────────
    struct DebugLine { Vec3 from, to; Vec4 colour; };
    std::vector<DebugLine> m_DebugLines;
    void FlushDebugDraw(Camera& camera);

    // ── Skybox ─────────────────────────────────────────────────────────────
    u32 m_SkyShader = 0;
    u32 m_SkyVAO = 0, m_SkyVBO = 0;
    f32 m_SkyRotation = 0.0f;

    // ── Line / gizmo shader ────────────────────────────────────────────────
    u32 m_LineShader = 0;
    u32 m_LineVAO = 0, m_LineVBO = 0;

    // ── Selection highlight ────────────────────────────────────────────────
    u32 m_HighlightShader = 0;

    // ── Grid ───────────────────────────────────────────────────────────────
    u32 m_GridVAO = 0, m_GridVBO = 0;
    i32 m_GridVertCount = 0;

    void InitSceneShader();
    void InitPrimitives();
    void InitSkybox();
    void InitLineShader();
    void InitGrid();
    void CleanupScene();

public:
    // ── Additional rendering methods ───────────────────────────────────────
    void RenderSkybox(Camera& camera, f32 dt);
    void RenderGrid(Camera& camera);
    void RenderGizmo(Camera& camera, const Vec3& position, GizmoMode mode, i32 activeAxis = -1);
    void RenderHighlight(Camera& camera, const Mat4& model, PrimitiveType type);

    /// Get scene shader for external uniform uploads.
    u32 GetSceneShader() const { return m_SceneShader; }
#endif
};

// ============================================================================
// Shader
// ============================================================================
/// Represents a compiled GPU shader program (vertex + fragment).
class Shader {
public:
    Shader() = default;
    explicit Shader(const std::string& name) : m_Name(name) {}

    /// Load and compile shaders from source strings.
    bool Compile(const std::string& vertexSrc, const std::string& fragmentSrc);

    /// Activate this shader program for subsequent draw calls.
    void Bind() const;
    void Unbind() const;

    /// Set uniform values.
    void SetFloat(const std::string& name, f32 value);
    void SetInt(const std::string& name, i32 value);
    void SetVec3(const std::string& name, const Vec3& value);
    void SetVec4(const std::string& name, const Vec4& value);
    void SetMat4(const std::string& name, const Mat4& value);

    u32 GetProgramID() const { return m_ProgramID; }
    const std::string& GetName() const { return m_Name; }

private:
    std::string m_Name;
    u32 m_ProgramID = 0;   // OpenGL program handle
};

} // namespace gv
