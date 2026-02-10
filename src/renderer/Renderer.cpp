// ============================================================================
// GameVoid Engine -- OpenGL Renderer Implementation
// ============================================================================
#include "renderer/Renderer.h"
#include "renderer/Camera.h"
#include "renderer/Lighting.h"
#include "renderer/MeshRenderer.h"
#include "core/Scene.h"
#include "core/GameObject.h"

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
    // 1. Upload lighting uniforms to the shader
    ApplyLighting(scene);

    // 2. Compute view & projection matrices from the camera
    Mat4 view = camera.GetViewMatrix();
    Mat4 proj = camera.GetProjectionMatrix();

    // 3. Iterate objects, bind their mesh/material, set MVP uniform, draw
    for (auto& obj : scene.GetAllObjects()) {
        if (!obj->IsActive()) continue;

        // Skip objects without a renderable component
        MeshRenderer* mr = obj->GetComponent<MeshRenderer>();
        SpriteRenderer* sr = obj->GetComponent<SpriteRenderer>();
        if (!mr && !sr) continue;

        Mat4 model = obj->GetTransform().GetModelMatrix();

        if (mr) {
            // 3D mesh path
            // shader.Bind();
            // shader.SetMat4(\"u_Model\", model);
            // shader.SetMat4(\"u_View\",  view);
            // shader.SetMat4(\"u_Proj\",  proj);
            // if (mr->GetMaterial()) mr->GetMaterial()->Apply();
            // if (mr->GetMesh())     { mr->GetMesh()->Bind(); glDrawElements(...); }
        }
        if (sr) {
            // 2D sprite path
            // spriteShader.Bind();
            // spriteShader.SetMat4(\"u_Model\", model);
            // Bind sr->texturePath texture, draw quad
        }

        (void)model; (void)view; (void)proj;
    }
}

void OpenGLRenderer::ApplyLighting(Scene& scene) {
    // Collect all light components from the active scene and upload to shader.
    //
    // In production the active shader would receive uniform arrays such as:
    //   u_AmbientColor, u_AmbientIntensity
    //   u_DirLightDir[], u_DirLightColor[], u_DirLightIntensity[]
    //   u_PointLightPos[], u_PointLightColor[], u_PointLightRange[]

    i32 dirIdx = 0, ptIdx = 0;

    for (auto& obj : scene.GetAllObjects()) {
        if (!obj->IsActive()) continue;

        // Ambient
        if (auto* ambient = obj->GetComponent<AmbientLight>()) {
            // shader.SetVec3(\"u_AmbientColor\", ambient->colour);
            // shader.SetFloat(\"u_AmbientIntensity\", ambient->intensity);
            (void)ambient;
        }

        // Directional
        if (auto* dir = obj->GetComponent<DirectionalLight>()) {
            std::string prefix = "u_DirLights[" + std::to_string(dirIdx++) + "]";
            // shader.SetVec3(prefix + \".direction\", dir->direction);
            // shader.SetVec3(prefix + \".color\",     dir->colour);
            // shader.SetFloat(prefix + \".intensity\", dir->intensity);
            (void)dir; (void)prefix;
        }

        // Point
        if (auto* pt = obj->GetComponent<PointLight>()) {
            std::string prefix = "u_PointLights[" + std::to_string(ptIdx++) + "]";
            // shader.SetVec3(prefix + \".position\", obj->GetTransform().position);
            // shader.SetVec3(prefix + \".color\",    pt->colour);
            // shader.SetFloat(prefix + \".intensity\", pt->intensity);
            // shader.SetFloat(prefix + \".constant\",  pt->constant);
            // shader.SetFloat(prefix + \".linear\",    pt->linear);
            // shader.SetFloat(prefix + \".quadratic\", pt->quadratic);
            // shader.SetFloat(prefix + \".range\",     pt->range);
            (void)pt; (void)prefix;
        }
    }

    // shader.SetInt(\"u_NumDirLights\",   dirIdx);
    // shader.SetInt(\"u_NumPointLights\", ptIdx);
    GV_LOG_TRACE("Lighting applied: " + std::to_string(dirIdx) + " dir, " +
                 std::to_string(ptIdx) + " point lights.");
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
