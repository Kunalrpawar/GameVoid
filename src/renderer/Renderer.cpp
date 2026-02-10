// ============================================================================
// GameVoid Engine -- OpenGL Renderer Implementation
// ============================================================================
#include "renderer/Renderer.h"
#include "renderer/Camera.h"
#include "renderer/Lighting.h"
#include "renderer/MeshRenderer.h"
#include "core/Scene.h"
#include "core/GameObject.h"

#ifdef GV_HAS_GLFW
#include "core/GLDefs.h"
#include "core/Window.h"
#include <cmath>
#endif

namespace gv {

// ── OpenGLRenderer ─────────────────────────────────────────────────────────

bool OpenGLRenderer::Init(u32 width, u32 height, const std::string& title) {
    m_Width  = width;
    m_Height = height;

#ifdef GV_HAS_GLFW
    if (m_Window && m_Window->IsInitialised()) {
        // Load GL 2.0+ function pointers (GL context already current via Window)
        if (!gvLoadGL()) {
            GV_LOG_ERROR("OpenGLRenderer: some GL 3.3 functions could not be loaded.");
        }

        // Initial GL state
        glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Log driver info
        const char* glVendor   = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        const char* glRenderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        const char* glVersion  = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        GV_LOG_INFO("GL Vendor:   " + std::string(glVendor   ? glVendor   : "?"));
        GV_LOG_INFO("GL Renderer: " + std::string(glRenderer ? glRenderer : "?"));
        GV_LOG_INFO("GL Version:  " + std::string(glVersion  ? glVersion  : "?"));

        InitDemo();  // built-in spinning triangle
    }
#endif

    m_Initialised = true;
    GV_LOG_INFO("OpenGLRenderer initialised (" + std::to_string(width) + "x" +
                std::to_string(height) + ") — " + title);
    return true;
}

void OpenGLRenderer::Shutdown() {
    if (!m_Initialised) return;
#ifdef GV_HAS_GLFW
    CleanupDemo();
#endif
    m_Initialised = false;
    GV_LOG_INFO("OpenGLRenderer shut down.");
}

void OpenGLRenderer::Clear(f32 r, f32 g, f32 b, f32 a) {
#ifdef GV_HAS_GLFW
    if (m_Window && m_Window->IsInitialised()) {
        glClearColor(r, g, b, a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        return;
    }
#endif
    (void)r; (void)g; (void)b; (void)a;
}

void OpenGLRenderer::BeginFrame() {
    // Clear is called separately — nothing needed here
}

void OpenGLRenderer::EndFrame() {
#ifdef GV_HAS_GLFW
    if (m_Window) m_Window->SwapBuffers();
#endif
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
#ifdef GV_HAS_GLFW
    if (m_Window) return m_Window->ShouldClose();
#endif
    return false;
}

void OpenGLRenderer::PollEvents() {
#ifdef GV_HAS_GLFW
    if (m_Window) m_Window->PollEvents();
#endif
}

// ── Demo triangle (built-in visual test) ───────────────────────────────────
// Renders a spinning RGB triangle to prove the GL context works.
// Drawn by Engine::Run() when in window mode.

#ifdef GV_HAS_GLFW

static const char* s_DemoVertSrc =
    "#version 330 core\n"
    "layout(location = 0) in vec3 aPos;\n"
    "layout(location = 1) in vec3 aColor;\n"
    "out vec3 vColor;\n"
    "uniform mat4 u_Model;\n"
    "void main() {\n"
    "    gl_Position = u_Model * vec4(aPos, 1.0);\n"
    "    vColor = aColor;\n"
    "}\n";

static const char* s_DemoFragSrc =
    "#version 330 core\n"
    "in vec3 vColor;\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "    FragColor = vec4(vColor, 1.0);\n"
    "}\n";

static GLuint CompileDemoStage(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetShaderInfoLog(shader, 512, nullptr, buf);
        GV_LOG_ERROR("Demo shader compile: " + std::string(buf));
    }
    return shader;
}

void OpenGLRenderer::InitDemo() {
    if (!glCreateShader) return;

    GLuint vs = CompileDemoStage(GL_VERTEX_SHADER,   s_DemoVertSrc);
    GLuint fs = CompileDemoStage(GL_FRAGMENT_SHADER, s_DemoFragSrc);

    m_DemoShader = glCreateProgram();
    glAttachShader(m_DemoShader, vs);
    glAttachShader(m_DemoShader, fs);
    glLinkProgram(m_DemoShader);

    GLint linked = 0;
    glGetProgramiv(m_DemoShader, GL_LINK_STATUS, &linked);
    if (!linked) {
        char buf[512];
        glGetProgramInfoLog(m_DemoShader, 512, nullptr, buf);
        GV_LOG_ERROR("Demo shader link: " + std::string(buf));
    }
    glDeleteShader(vs);
    glDeleteShader(fs);

    // RGB triangle  — position (x,y,z) + colour (r,g,b)
    float verts[] = {
         0.0f,  0.6f, 0.0f,   1.0f, 0.2f, 0.3f,
        -0.5f, -0.4f, 0.0f,   0.2f, 1.0f, 0.3f,
         0.5f, -0.4f, 0.0f,   0.2f, 0.3f, 1.0f,
    };

    glGenVertexArrays(1, &m_DemoVAO);
    glGenBuffers(1, &m_DemoVBO);
    glBindVertexArray(m_DemoVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_DemoVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(verts)),
                 verts, GL_STATIC_DRAW);

    // aPos  (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    // aColor (location 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    GV_LOG_INFO("Demo triangle created.");
}

void OpenGLRenderer::RenderDemo(f32 dt) {
    if (!m_DemoShader || !m_DemoVAO) return;

    static f32 angle = 0.0f;
    angle += dt * 0.8f;

    float c = std::cos(angle);
    float s = std::sin(angle);
    // Column-major rotation around Z
    float model[16] = {
         c,  s, 0, 0,
        -s,  c, 0, 0,
         0,  0, 1, 0,
         0,  0, 0, 1,
    };

    glUseProgram(m_DemoShader);
    GLint loc = glGetUniformLocation(m_DemoShader, "u_Model");
    glUniformMatrix4fv(loc, 1, GL_FALSE, model);

    glBindVertexArray(m_DemoVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glUseProgram(0);
}

void OpenGLRenderer::CleanupDemo() {
    if (m_DemoVAO)    { glDeleteVertexArrays(1, &m_DemoVAO); m_DemoVAO = 0; }
    if (m_DemoVBO)    { glDeleteBuffers(1, &m_DemoVBO);      m_DemoVBO = 0; }
    if (m_DemoShader) { glDeleteProgram(m_DemoShader);        m_DemoShader = 0; }
}

#endif // GV_HAS_GLFW

// ── Shader ─────────────────────────────────────────────────────────────────

bool Shader::Compile(const std::string& vertexSrc, const std::string& fragmentSrc) {
#ifdef GV_HAS_GLFW
    if (glCreateShader) {
        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        const char* vSrc = vertexSrc.c_str();
        glShaderSource(vs, 1, &vSrc, nullptr);
        glCompileShader(vs);

        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        const char* fSrc = fragmentSrc.c_str();
        glShaderSource(fs, 1, &fSrc, nullptr);
        glCompileShader(fs);

        m_ProgramID = glCreateProgram();
        glAttachShader(m_ProgramID, vs);
        glAttachShader(m_ProgramID, fs);
        glLinkProgram(m_ProgramID);

        GLint linked = 0;
        glGetProgramiv(m_ProgramID, GL_LINK_STATUS, &linked);
        if (!linked) {
            char buf[512];
            glGetProgramInfoLog(m_ProgramID, 512, nullptr, buf);
            GV_LOG_ERROR("Shader '" + m_Name + "' link error: " + std::string(buf));
        }
        glDeleteShader(vs);
        glDeleteShader(fs);
        GV_LOG_INFO("Shader '" + m_Name + "' compiled.");
        return linked != 0;
    }
#endif
    GV_LOG_INFO("Shader '" + m_Name + "' compiled (placeholder).");
    (void)vertexSrc; (void)fragmentSrc;
    return true;
}

void Shader::Bind() const {
#ifdef GV_HAS_GLFW
    if (glUseProgram) glUseProgram(m_ProgramID);
#endif
}

void Shader::Unbind() const {
#ifdef GV_HAS_GLFW
    if (glUseProgram) glUseProgram(0);
#endif
}

void Shader::SetFloat(const std::string& name, f32 value) {
#ifdef GV_HAS_GLFW
    if (glGetUniformLocation && glUniform1f)
        glUniform1f(glGetUniformLocation(m_ProgramID, name.c_str()), value);
#else
    (void)name; (void)value;
#endif
}

void Shader::SetInt(const std::string& name, i32 value) {
#ifdef GV_HAS_GLFW
    if (glGetUniformLocation && glUniform1i)
        glUniform1i(glGetUniformLocation(m_ProgramID, name.c_str()), value);
#else
    (void)name; (void)value;
#endif
}

void Shader::SetVec3(const std::string& name, const Vec3& value) {
#ifdef GV_HAS_GLFW
    if (glGetUniformLocation && glUniform3f)
        glUniform3f(glGetUniformLocation(m_ProgramID, name.c_str()),
                    value.x, value.y, value.z);
#else
    (void)name; (void)value;
#endif
}

void Shader::SetMat4(const std::string& name, const Mat4& value) {
#ifdef GV_HAS_GLFW
    if (glGetUniformLocation && glUniformMatrix4fv)
        glUniformMatrix4fv(glGetUniformLocation(m_ProgramID, name.c_str()),
                           1, GL_FALSE, value.m);
#else
    (void)name; (void)value;
#endif
}

} // namespace gv
