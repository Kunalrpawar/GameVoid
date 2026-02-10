// ============================================================================
// GameVoid Engine — OpenGL Renderer Implementation
// ============================================================================
#include "renderer/Renderer.h"
#include "renderer/Camera.h"
#include "core/Scene.h"

namespace gv {

// ── OpenGLRenderer ─────────────────────────────────────────────────────────

bool OpenGLRenderer::Init(u32 width, u32 height, const std::string& title) {
    m_Width  = width;
    m_Height = height;

    // In production:
    //   1. glfwInit()
    //   2. glfwCreateWindow(width, height, title.c_str(), ...)
    //   3. glfwMakeContextCurrent(window)
    //   4. gladLoadGLLoader(...)   (or GLEW)
    //   5. glViewport(0, 0, width, height)
    //   6. glEnable(GL_DEPTH_TEST)

    m_Initialised = true;
    GV_LOG_INFO("OpenGLRenderer initialised (" + std::to_string(width) + "x" +
                std::to_string(height) + ") — " + title);
    return true;
}

void OpenGLRenderer::Shutdown() {
    if (!m_Initialised) return;
    // glfwDestroyWindow(m_Window);
    // glfwTerminate();
    m_Initialised = false;
    GV_LOG_INFO("OpenGLRenderer shut down.");
}

void OpenGLRenderer::Clear(f32 r, f32 g, f32 b, f32 a) {
    // glClearColor(r, g, b, a);
    // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    (void)r; (void)g; (void)b; (void)a;
}

void OpenGLRenderer::BeginFrame() {
    // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLRenderer::EndFrame() {
    // glfwSwapBuffers(m_Window);
}

void OpenGLRenderer::RenderScene(Scene& scene, Camera& camera) {
    // 1. Compute view & projection matrices from the camera
    Mat4 view = camera.GetViewMatrix();
    Mat4 proj = camera.GetProjectionMatrix();

    // 2. Iterate objects, bind their mesh/material, set MVP uniform, draw
    for (auto& obj : scene.GetAllObjects()) {
        if (!obj->IsActive()) continue;
        Mat4 model = obj->GetTransform().GetModelMatrix();
        // Mat4 mvp = proj * view * model;
        // shader.SetMat4("u_MVP", mvp);
        // mesh->Bind(); glDrawElements(...); mesh->Unbind();
        (void)model; (void)view; (void)proj;
    }
}

bool OpenGLRenderer::WindowShouldClose() const {
    // return glfwWindowShouldClose(m_Window);
    return false;   // skeleton always says "keep running"
}

void OpenGLRenderer::PollEvents() {
    // glfwPollEvents();
}

// ── Shader ─────────────────────────────────────────────────────────────────

bool Shader::Compile(const std::string& vertexSrc, const std::string& fragmentSrc) {
    // glCreateShader, glShaderSource, glCompileShader, glCreateProgram, glLinkProgram …
    GV_LOG_INFO("Shader '" + m_Name + "' compiled (placeholder).");
    (void)vertexSrc; (void)fragmentSrc;
    return true;
}

void Shader::Bind()   const { /* glUseProgram(m_ProgramID); */ }
void Shader::Unbind() const { /* glUseProgram(0); */ }

void Shader::SetFloat(const std::string& name, f32 value)       { (void)name; (void)value; }
void Shader::SetInt(const std::string& name, i32 value)         { (void)name; (void)value; }
void Shader::SetVec3(const std::string& name, const Vec3& value){ (void)name; (void)value; }
void Shader::SetMat4(const std::string& name, const Mat4& value){ (void)name; (void)value; }

} // namespace gv
