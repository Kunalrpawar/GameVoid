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

namespace gv {

// Forward declarations
class Camera;
class Scene;

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
    virtual void DrawRect(f32 x, f32 y, f32 w, f32 h, const Vec4& colour) {
        (void)x; (void)y; (void)w; (void)h; (void)colour;
    }

    /// Draw a 2D textured quad.
    virtual void DrawTexture(u32 textureID, f32 x, f32 y, f32 w, f32 h) {
        (void)textureID; (void)x; (void)y; (void)w; (void)h;
    }

    /// Draw a wireframe / solid 3D shape for debugging (box, sphere).
    virtual void DrawDebugBox(const Vec3& center, const Vec3& halfExtents, const Vec4& colour) {
        (void)center; (void)halfExtents; (void)colour;
    }
    virtual void DrawDebugSphere(const Vec3& center, f32 radius, const Vec4& colour) {
        (void)center; (void)radius; (void)colour;
    }
    virtual void DrawDebugLine(const Vec3& from, const Vec3& to, const Vec4& colour) {
        (void)from; (void)to; (void)colour;
    }

    // ── Lighting pass ──────────────────────────────────────────────────────
    /// Collect all light components from the scene and upload their data to
    /// the active shader as uniforms.  Called internally by RenderScene().
    virtual void ApplyLighting(Scene& scene) { (void)scene; }

    /// Check whether the window should close.
    virtual bool WindowShouldClose() const = 0;

    /// Poll input & window events.
    virtual void PollEvents() = 0;

    /// Get the window dimensions.
    virtual u32 GetWidth()  const = 0;
    virtual u32 GetHeight() const = 0;
};

// ============================================================================
// OpenGL Renderer (skeleton)
// ============================================================================
class OpenGLRenderer : public IRenderer {
public:
    OpenGLRenderer() = default;
    ~OpenGLRenderer() override { Shutdown(); }

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

private:
    u32  m_Width  = 0;
    u32  m_Height = 0;
    bool m_Initialised = false;

    // In a real build this would store a GLFWwindow*, shader programs, etc.
    // void* m_Window = nullptr;
};

// ============================================================================
// Shader (placeholder)
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

    /// Set uniform values (placeholders — real impl uses glUniform*).
    void SetFloat(const std::string& name, f32 value);
    void SetInt(const std::string& name, i32 value);
    void SetVec3(const std::string& name, const Vec3& value);
    void SetMat4(const std::string& name, const Mat4& value);

    const std::string& GetName() const { return m_Name; }

private:
    std::string m_Name;
    u32 m_ProgramID = 0;   // OpenGL program handle
};

} // namespace gv
