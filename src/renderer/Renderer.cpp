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
#include <vector>
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

        InitDemo();          // spinning demo triangle
        InitSceneShader();   // flat-colour MVP shader
        InitPrimitives();    // built-in triangle & cube & plane VAOs
        InitSkybox();        // procedural skybox shader
        InitLineShader();    // line/gizmo + highlight shaders
        InitGrid();          // grid VAO
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
    CleanupScene();
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
#ifdef GV_HAS_GLFW
    if (!m_SceneShader) {
        // No GL — fall back to placeholder below
    } else {
        // 1. Compute view & projection matrices from the camera
        Mat4 view = camera.GetViewMatrix();
        Mat4 proj = camera.GetProjectionMatrix();

        glUseProgram(m_SceneShader);
        GLint locModel = glGetUniformLocation(m_SceneShader, "u_Model");
        GLint locView  = glGetUniformLocation(m_SceneShader, "u_View");
        GLint locProj  = glGetUniformLocation(m_SceneShader, "u_Proj");
        GLint locColor = glGetUniformLocation(m_SceneShader, "u_Color");

        glUniformMatrix4fv(locView, 1, GL_FALSE, view.m);
        glUniformMatrix4fv(locProj, 1, GL_FALSE, proj.m);

        // ── Upload lighting uniforms from scene ───────────────────────────
        GLint locLightDir     = glGetUniformLocation(m_SceneShader, "u_LightDir");
        GLint locLightColor   = glGetUniformLocation(m_SceneShader, "u_LightColor");
        GLint locAmbientColor = glGetUniformLocation(m_SceneShader, "u_AmbientColor");
        GLint locCamPos       = glGetUniformLocation(m_SceneShader, "u_CamPos");
        GLint locLightOn      = glGetUniformLocation(m_SceneShader, "u_LightingEnabled");

        // Defaults (overridden by actual scene lights)
        Vec3 lightDir(0.3f, 1.0f, 0.5f);
        Vec3 lightColor(1.0f, 1.0f, 1.0f);
        Vec3 ambientColor(0.15f, 0.15f, 0.15f);

        for (auto& obj : scene.GetAllObjects()) {
            if (!obj->IsActive()) continue;
            if (auto* dl = obj->GetComponent<DirectionalLight>()) {
                // Negate: component stores direction light travels,
                // shader needs direction TOWARD the light
                lightDir = -(dl->direction.Normalized());
                lightColor = dl->colour * dl->intensity;
            }
            if (auto* al = obj->GetComponent<AmbientLight>()) {
                ambientColor = al->colour * al->intensity;
            }
        }

        Vec3 camPos = camera.GetOwner()->GetTransform().position;
        glUniform3f(locLightDir,     lightDir.x,     lightDir.y,     lightDir.z);
        glUniform3f(locLightColor,   lightColor.x,   lightColor.y,   lightColor.z);
        glUniform3f(locAmbientColor, ambientColor.x, ambientColor.y, ambientColor.z);
        glUniform3f(locCamPos,       camPos.x,       camPos.y,       camPos.z);
        glUniform1i(locLightOn, m_LightingEnabled ? 1 : 0);

        i32 drawCount = 0;

        // 2. Iterate all GameObjects that have a MeshRenderer
        for (auto& obj : scene.GetAllObjects()) {
            if (!obj->IsActive()) continue;

            MeshRenderer* mr = obj->GetComponent<MeshRenderer>();
            if (!mr) continue;
            if (mr->primitiveType == PrimitiveType::None) continue;

            // 3. Compute model matrix from the object's Transform
            Mat4 model = obj->GetTransform().GetModelMatrix();
            glUniformMatrix4fv(locModel, 1, GL_FALSE, model.m);

            // 4. Upload flat colour
            glUniform4f(locColor, mr->color.x, mr->color.y, mr->color.z, mr->color.w);

            // 5. Draw the built-in primitive
            if (mr->primitiveType == PrimitiveType::Triangle) {
                glBindVertexArray(m_TriVAO);
                glDrawArrays(GL_TRIANGLES, 0, 3);
                glBindVertexArray(0);
            } else if (mr->primitiveType == PrimitiveType::Cube) {
                glBindVertexArray(m_CubeVAO);
                glDrawElements(GL_TRIANGLES, m_CubeIndexCount, GL_UNSIGNED_INT,
                               reinterpret_cast<void*>(0));
                glBindVertexArray(0);
            } else if (mr->primitiveType == PrimitiveType::Plane) {
                glBindVertexArray(m_PlaneVAO);
                glDrawElements(GL_TRIANGLES, m_PlaneIndexCount, GL_UNSIGNED_INT,
                               reinterpret_cast<void*>(0));
                glBindVertexArray(0);
            }
            drawCount++;
        }

        glUseProgram(0);

        // Log draw count once on first frame only.
        static bool loggedOnce = false;
        if (!loggedOnce) {
            GV_LOG_INFO("RenderScene: drew " + std::to_string(drawCount) + " object(s).");
            loggedOnce = true;
        }

        return;
    }
#endif

    // Placeholder path (CLI-only build or no GL)
    (void)scene; (void)camera;
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

// ── Scene-rendering shader & primitives ────────────────────────────────────
// A minimal flat-colour shader: transforms vertices by Model*View*Proj,
// colours every fragment with a uniform colour.

static const char* s_SceneVertSrc =
    "#version 330 core\n"
    "layout(location = 0) in vec3 aPos;\n"
    "layout(location = 1) in vec3 aNormal;\n"
    "uniform mat4 u_Model;\n"
    "uniform mat4 u_View;\n"
    "uniform mat4 u_Proj;\n"
    "out vec3 vWorldPos;\n"
    "out vec3 vNormal;\n"
    "void main() {\n"
    "    vec4 worldPos = u_Model * vec4(aPos, 1.0);\n"
    "    gl_Position = u_Proj * u_View * worldPos;\n"
    "    vWorldPos = worldPos.xyz;\n"
    "    vNormal = mat3(u_Model) * aNormal;\n"
    "}\n";

static const char* s_SceneFragSrc =
    "#version 330 core\n"
    "in vec3 vWorldPos;\n"
    "in vec3 vNormal;\n"
    "out vec4 FragColor;\n"
    "uniform vec4 u_Color;\n"
    "uniform vec3 u_LightDir;\n"
    "uniform vec3 u_LightColor;\n"
    "uniform vec3 u_AmbientColor;\n"
    "uniform vec3 u_CamPos;\n"
    "uniform int  u_LightingEnabled;\n"
    "void main() {\n"
    "    if (u_LightingEnabled == 0) {\n"
    "        FragColor = u_Color;\n"
    "        return;\n"
    "    }\n"
    "    vec3 N = normalize(vNormal);\n"
    "    vec3 L = normalize(u_LightDir);\n"
    "    // Ambient\n"
    "    vec3 ambient = u_AmbientColor * u_Color.rgb;\n"
    "    // Diffuse (Lambert)\n"
    "    float diff = max(dot(N, L), 0.0);\n"
    "    vec3 diffuse = diff * u_LightColor * u_Color.rgb;\n"
    "    // Specular (Blinn-Phong)\n"
    "    vec3 V = normalize(u_CamPos - vWorldPos);\n"
    "    vec3 H = normalize(L + V);\n"
    "    float spec = pow(max(dot(N, H), 0.0), 32.0);\n"
    "    vec3 specular = spec * u_LightColor * 0.5;\n"
    "    FragColor = vec4(ambient + diffuse + specular, u_Color.a);\n"
    "}\n";

void OpenGLRenderer::InitSceneShader() {
    if (!glCreateShader) return;

    GLuint vs = CompileDemoStage(GL_VERTEX_SHADER,   s_SceneVertSrc);
    GLuint fs = CompileDemoStage(GL_FRAGMENT_SHADER, s_SceneFragSrc);

    m_SceneShader = glCreateProgram();
    glAttachShader(m_SceneShader, vs);
    glAttachShader(m_SceneShader, fs);
    glLinkProgram(m_SceneShader);

    GLint linked = 0;
    glGetProgramiv(m_SceneShader, GL_LINK_STATUS, &linked);
    if (!linked) {
        char buf[512];
        glGetProgramInfoLog(m_SceneShader, 512, nullptr, buf);
        GV_LOG_ERROR("Scene shader link: " + std::string(buf));
    }
    glDeleteShader(vs);
    glDeleteShader(fs);

    GV_LOG_INFO("Scene shader compiled (Phong lighting).");
}

void OpenGLRenderer::InitPrimitives() {
    if (!glGenVertexArrays) return;

    // ── Triangle (2D / flat) ───────────────────────────────────────────────
    // position (xyz) + normal (xyz)
    float triVerts[] = {
        // positions           normals (all face +Z)
         0.0f,  0.5f, 0.0f,   0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,
    };

    glGenVertexArrays(1, &m_TriVAO);
    glGenBuffers(1, &m_TriVBO);
    glBindVertexArray(m_TriVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_TriVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(triVerts)),
                 triVerts, GL_STATIC_DRAW);
    // aPos (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    // aNormal (location 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    GV_LOG_INFO("Built-in triangle primitive created.");

    // ── Cube (3D, 36 vertices with per-face normals, indexed) ──────────────
    // 6 faces × 4 vertices each = 24 unique vertices (position + normal)
    float cubeVerts[] = {
        // Front face (+Z)
        -0.5f, -0.5f,  0.5f,   0, 0, 1,
         0.5f, -0.5f,  0.5f,   0, 0, 1,
         0.5f,  0.5f,  0.5f,   0, 0, 1,
        -0.5f,  0.5f,  0.5f,   0, 0, 1,
        // Back face (-Z)
        -0.5f, -0.5f, -0.5f,   0, 0,-1,
        -0.5f,  0.5f, -0.5f,   0, 0,-1,
         0.5f,  0.5f, -0.5f,   0, 0,-1,
         0.5f, -0.5f, -0.5f,   0, 0,-1,
        // Top face (+Y)
        -0.5f,  0.5f, -0.5f,   0, 1, 0,
        -0.5f,  0.5f,  0.5f,   0, 1, 0,
         0.5f,  0.5f,  0.5f,   0, 1, 0,
         0.5f,  0.5f, -0.5f,   0, 1, 0,
        // Bottom face (-Y)
        -0.5f, -0.5f, -0.5f,   0,-1, 0,
         0.5f, -0.5f, -0.5f,   0,-1, 0,
         0.5f, -0.5f,  0.5f,   0,-1, 0,
        -0.5f, -0.5f,  0.5f,   0,-1, 0,
        // Right face (+X)
         0.5f, -0.5f, -0.5f,   1, 0, 0,
         0.5f,  0.5f, -0.5f,   1, 0, 0,
         0.5f,  0.5f,  0.5f,   1, 0, 0,
         0.5f, -0.5f,  0.5f,   1, 0, 0,
        // Left face (-X)
        -0.5f, -0.5f, -0.5f,  -1, 0, 0,
        -0.5f, -0.5f,  0.5f,  -1, 0, 0,
        -0.5f,  0.5f,  0.5f,  -1, 0, 0,
        -0.5f,  0.5f, -0.5f,  -1, 0, 0,
    };

    unsigned int cubeIndices[] = {
         0,  1,  2,   2,  3,  0,   // front
         4,  5,  6,   6,  7,  4,   // back
         8,  9, 10,  10, 11,  8,   // top
        12, 13, 14,  14, 15, 12,   // bottom
        16, 17, 18,  18, 19, 16,   // right
        20, 21, 22,  22, 23, 20,   // left
    };
    m_CubeIndexCount = 36;

    glGenVertexArrays(1, &m_CubeVAO);
    glGenBuffers(1, &m_CubeVBO);
    glGenBuffers(1, &m_CubeEBO);
    glBindVertexArray(m_CubeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_CubeVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(cubeVerts)),
                 cubeVerts, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_CubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(cubeIndices)),
                 cubeIndices, GL_STATIC_DRAW);

    // aPos (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    // aNormal (location 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    GV_LOG_INFO("Built-in cube primitive created (24 verts, 36 indices).");

    // ── Plane (XZ, 1×1 centred at origin, faces +Y) ───────────────────────
    float planeVerts[] = {
        // positions           normals
        -0.5f, 0.0f, -0.5f,   0, 1, 0,
         0.5f, 0.0f, -0.5f,   0, 1, 0,
         0.5f, 0.0f,  0.5f,   0, 1, 0,
        -0.5f, 0.0f,  0.5f,   0, 1, 0,
    };
    unsigned int planeIdx[] = { 0, 1, 2, 2, 3, 0 };
    m_PlaneIndexCount = 6;

    glGenVertexArrays(1, &m_PlaneVAO);
    glGenBuffers(1, &m_PlaneVBO);
    glGenBuffers(1, &m_PlaneEBO);
    glBindVertexArray(m_PlaneVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_PlaneVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(planeVerts)),
                 planeVerts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_PlaneEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(planeIdx)),
                 planeIdx, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    GV_LOG_INFO("Built-in plane primitive created.");
}

void OpenGLRenderer::CleanupScene() {
    if (m_TriVAO)      { glDeleteVertexArrays(1, &m_TriVAO);   m_TriVAO = 0; }
    if (m_TriVBO)      { glDeleteBuffers(1, &m_TriVBO);         m_TriVBO = 0; }
    if (m_CubeVAO)     { glDeleteVertexArrays(1, &m_CubeVAO);  m_CubeVAO = 0; }
    if (m_CubeVBO)     { glDeleteBuffers(1, &m_CubeVBO);        m_CubeVBO = 0; }
    if (m_CubeEBO)     { glDeleteBuffers(1, &m_CubeEBO);        m_CubeEBO = 0; }
    if (m_PlaneVAO)    { glDeleteVertexArrays(1, &m_PlaneVAO); m_PlaneVAO = 0; }
    if (m_PlaneVBO)    { glDeleteBuffers(1, &m_PlaneVBO);       m_PlaneVBO = 0; }
    if (m_PlaneEBO)    { glDeleteBuffers(1, &m_PlaneEBO);       m_PlaneEBO = 0; }
    if (m_SceneShader) { glDeleteProgram(m_SceneShader);        m_SceneShader = 0; }
    if (m_SkyShader)   { glDeleteProgram(m_SkyShader);          m_SkyShader = 0; }
    if (m_SkyVAO)      { glDeleteVertexArrays(1, &m_SkyVAO);   m_SkyVAO = 0; }
    if (m_SkyVBO)      { glDeleteBuffers(1, &m_SkyVBO);         m_SkyVBO = 0; }
    if (m_LineShader)  { glDeleteProgram(m_LineShader);          m_LineShader = 0; }
    if (m_LineVAO)     { glDeleteVertexArrays(1, &m_LineVAO);   m_LineVAO = 0; }
    if (m_LineVBO)     { glDeleteBuffers(1, &m_LineVBO);         m_LineVBO = 0; }
    if (m_GridVAO)     { glDeleteVertexArrays(1, &m_GridVAO);   m_GridVAO = 0; }
    if (m_GridVBO)     { glDeleteBuffers(1, &m_GridVBO);         m_GridVBO = 0; }
    if (m_HighlightShader) { glDeleteProgram(m_HighlightShader); m_HighlightShader = 0; }
}

// ── Procedural Skybox ──────────────────────────────────────────────────────

static const char* s_SkyVertSrc =
    "#version 330 core\n"
    "layout(location = 0) in vec3 aPos;\n"
    "uniform mat4 u_VP;\n"
    "out vec3 vDir;\n"
    "void main() {\n"
    "    vDir = aPos;\n"
    "    vec4 pos = u_VP * vec4(aPos, 1.0);\n"
    "    gl_Position = pos.xyww;\n"  // depth = 1.0 (behind everything)
    "}\n";

static const char* s_SkyFragSrc =
    "#version 330 core\n"
    "in vec3 vDir;\n"
    "out vec4 FragColor;\n"
    "uniform float u_Time;\n"
    "void main() {\n"
    "    vec3 dir = normalize(vDir);\n"
    "    float t = dir.y * 0.5 + 0.5;\n"                     // 0 at horizon, 1 at zenith
    "    vec3 horizonCol = vec3(0.7, 0.82, 0.92);\n"
    "    vec3 zenithCol  = vec3(0.2, 0.35, 0.75);\n"
    "    vec3 groundCol  = vec3(0.25, 0.22, 0.2);\n"
    "    vec3 sky = mix(horizonCol, zenithCol, clamp(t, 0.0, 1.0));\n"
    "    if (dir.y < 0.0) sky = mix(horizonCol, groundCol, clamp(-dir.y*4.0, 0.0, 1.0));\n"
    // simple sun disc
    "    float sunAngle = u_Time * 0.02;\n"
    "    vec3 sunDir = normalize(vec3(cos(sunAngle)*0.4, 0.8, sin(sunAngle)*0.4));\n"
    "    float sunDot = max(dot(dir, sunDir), 0.0);\n"
    "    sky += vec3(1.0, 0.9, 0.7) * pow(sunDot, 256.0) * 2.0;\n"  // sun
    "    sky += vec3(1.0, 0.85, 0.6) * pow(sunDot, 8.0) * 0.15;\n"  // glow
    // wispy clouds
    "    float cx = dir.x / max(dir.y, 0.01) * 2.0 + u_Time * 0.01;\n"
    "    float cz = dir.z / max(dir.y, 0.01) * 2.0;\n"
    "    float cloud = sin(cx*1.2)*sin(cz*1.5)*0.5+0.5;\n"
    "    cloud = smoothstep(0.45, 0.65, cloud);\n"
    "    if (dir.y > 0.0) sky = mix(sky, vec3(1.0), cloud * 0.35 * clamp(dir.y*3.0, 0.0, 1.0));\n"
    "    FragColor = vec4(sky, 1.0);\n"
    "}\n";

void OpenGLRenderer::InitSkybox() {
    if (!glCreateShader) return;
    GLuint vs = CompileDemoStage(GL_VERTEX_SHADER, s_SkyVertSrc);
    GLuint fs = CompileDemoStage(GL_FRAGMENT_SHADER, s_SkyFragSrc);
    m_SkyShader = glCreateProgram();
    glAttachShader(m_SkyShader, vs);
    glAttachShader(m_SkyShader, fs);
    glLinkProgram(m_SkyShader);
    glDeleteShader(vs); glDeleteShader(fs);

    // Unit cube for skybox
    float skyVerts[] = {
        -1,1,-1, -1,-1,-1, 1,-1,-1, 1,-1,-1, 1,1,-1, -1,1,-1,
        -1,-1,1, -1,-1,-1, -1,1,-1, -1,1,-1, -1,1,1, -1,-1,1,
        1,-1,-1, 1,-1,1, 1,1,1, 1,1,1, 1,1,-1, 1,-1,-1,
        -1,-1,1, -1,1,1, 1,1,1, 1,1,1, 1,-1,1, -1,-1,1,
        -1,1,-1, 1,1,-1, 1,1,1, 1,1,1, -1,1,1, -1,1,-1,
        -1,-1,-1, -1,-1,1, 1,-1,1, 1,-1,1, 1,-1,-1, -1,-1,-1,
    };
    glGenVertexArrays(1, &m_SkyVAO);
    glGenBuffers(1, &m_SkyVBO);
    glBindVertexArray(m_SkyVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_SkyVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(skyVerts)), skyVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    GV_LOG_INFO("Procedural skybox initialised.");
}

void OpenGLRenderer::RenderSkybox(Camera& camera, f32 dt) {
    if (!m_SkyShader || !m_SkyVAO) return;
    m_SkyRotation += dt;

    Mat4 view = camera.GetViewMatrix();
    // Remove translation from view matrix — skybox is always centered on camera
    view.m[12] = 0; view.m[13] = 0; view.m[14] = 0;
    Mat4 proj = camera.GetProjectionMatrix();
    Mat4 vp = proj * view;

    glDepthFunc(GL_LEQUAL);   // draw at depth=1.0
    glUseProgram(m_SkyShader);
    glUniformMatrix4fv(glGetUniformLocation(m_SkyShader, "u_VP"), 1, GL_FALSE, vp.m);
    glUniform1f(glGetUniformLocation(m_SkyShader, "u_Time"), m_SkyRotation);
    glBindVertexArray(m_SkyVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glUseProgram(0);
    glDepthFunc(GL_LESS);
}

// ── Line / Gizmo Shader ───────────────────────────────────────────────────

static const char* s_LineVertSrc =
    "#version 330 core\n"
    "layout(location = 0) in vec3 aPos;\n"
    "layout(location = 1) in vec3 aColor;\n"
    "uniform mat4 u_VP;\n"
    "out vec3 vColor;\n"
    "void main() {\n"
    "    vColor = aColor;\n"
    "    gl_Position = u_VP * vec4(aPos, 1.0);\n"
    "}\n";

static const char* s_LineFragSrc =
    "#version 330 core\n"
    "in vec3 vColor;\n"
    "out vec4 FragColor;\n"
    "void main() { FragColor = vec4(vColor, 1.0); }\n";

void OpenGLRenderer::InitLineShader() {
    if (!glCreateShader) return;
    GLuint vs = CompileDemoStage(GL_VERTEX_SHADER, s_LineVertSrc);
    GLuint fs = CompileDemoStage(GL_FRAGMENT_SHADER, s_LineFragSrc);
    m_LineShader = glCreateProgram();
    glAttachShader(m_LineShader, vs);
    glAttachShader(m_LineShader, fs);
    glLinkProgram(m_LineShader);
    glDeleteShader(vs); glDeleteShader(fs);

    // Dynamic VBO for line drawing (updated each frame)
    glGenVertexArrays(1, &m_LineVAO);
    glGenBuffers(1, &m_LineVBO);
    glBindVertexArray(m_LineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_LineVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(8192 * sizeof(float)), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), reinterpret_cast<void*>(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // Highlight shader: draws object slightly scaled with flat color + wireframe
    m_HighlightShader = m_SceneShader; // reuse scene shader — we'll just set u_Color
    GV_LOG_INFO("Line/gizmo shader initialised.");
}

void OpenGLRenderer::RenderGizmo(Camera& camera, const Vec3& pos, GizmoMode mode, i32 activeAxis) {
    if (!m_LineShader || !m_LineVAO) return;

    Mat4 vp = camera.GetProjectionMatrix() * camera.GetViewMatrix();
    glUseProgram(m_LineShader);
    glUniformMatrix4fv(glGetUniformLocation(m_LineShader, "u_VP"), 1, GL_FALSE, vp.m);

    f32 len = 1.5f;
    f32 hl = 0.15f; // arrowhead length

    // Line data: pos(3) + color(3) per vertex, 2 vertices per line
    // Rotation mode: 3 circles × 24 segments = 72 lines × 2 verts × 6 floats = 864
    float lines[864 + 72]; // extra room for translate/scale
    int idx = 0;

    auto pushLine = [&](Vec3 a, Vec3 b, Vec3 col) {
        lines[idx++] = a.x; lines[idx++] = a.y; lines[idx++] = a.z;
        lines[idx++] = col.x; lines[idx++] = col.y; lines[idx++] = col.z;
        lines[idx++] = b.x; lines[idx++] = b.y; lines[idx++] = b.z;
        lines[idx++] = col.x; lines[idx++] = col.y; lines[idx++] = col.z;
    };

    Vec3 xCol = (activeAxis == 0) ? Vec3(1, 1, 0) : Vec3(1, 0.2f, 0.2f);
    Vec3 yCol = (activeAxis == 1) ? Vec3(1, 1, 0) : Vec3(0.2f, 1, 0.2f);
    Vec3 zCol = (activeAxis == 2) ? Vec3(1, 1, 0) : Vec3(0.2f, 0.2f, 1);

    if (mode == GizmoMode::Translate) {
        // Arrows along each axis
        pushLine(pos, pos + Vec3(len, 0, 0), xCol);
        pushLine(pos + Vec3(len, 0, 0), pos + Vec3(len - hl, hl, 0), xCol);
        pushLine(pos, pos + Vec3(0, len, 0), yCol);
        pushLine(pos + Vec3(0, len, 0), pos + Vec3(0, len - hl, hl), yCol);
        pushLine(pos, pos + Vec3(0, 0, len), zCol);
        pushLine(pos + Vec3(0, 0, len), pos + Vec3(0, hl, len - hl), zCol);
    } else if (mode == GizmoMode::Scale) {
        // Lines with squares at end
        pushLine(pos, pos + Vec3(len, 0, 0), xCol);
        pushLine(pos + Vec3(len-hl, -hl, 0), pos + Vec3(len+hl, hl, 0), xCol);
        pushLine(pos, pos + Vec3(0, len, 0), yCol);
        pushLine(pos + Vec3(-hl, len-hl, 0), pos + Vec3(hl, len+hl, 0), yCol);
        pushLine(pos, pos + Vec3(0, 0, len), zCol);
        pushLine(pos + Vec3(0, -hl, len-hl), pos + Vec3(0, hl, len+hl), zCol);
    } else {
        // Rotate — circles (approximate with line segments)
        const int N = 24;
        for (int i = 0; i < N; ++i) {
            f32 a0 = (float)i / (float)N * 6.2832f;
            f32 a1 = (float)(i+1) / (float)N * 6.2832f;
            f32 r = 1.0f;
            // X circle (YZ plane)
            pushLine(pos + Vec3(0, std::cos(a0)*r, std::sin(a0)*r),
                     pos + Vec3(0, std::cos(a1)*r, std::sin(a1)*r), xCol);
            // Y circle (XZ plane)  
            pushLine(pos + Vec3(std::cos(a0)*r, 0, std::sin(a0)*r),
                     pos + Vec3(std::cos(a1)*r, 0, std::sin(a1)*r), yCol);
            // Z circle (XY plane)
            pushLine(pos + Vec3(std::cos(a0)*r, std::sin(a0)*r, 0),
                     pos + Vec3(std::cos(a1)*r, std::sin(a1)*r, 0), zCol);
        }
    }

    int vertCount = idx / 6;
    glBindVertexArray(m_LineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_LineVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(idx * sizeof(float)), lines);
    glLineWidth(2.5f);
    glDisable(GL_DEPTH_TEST);
    glDrawArrays(GL_LINES, 0, vertCount);
    glEnable(GL_DEPTH_TEST);
    glLineWidth(1.0f);
    glBindVertexArray(0);
    glUseProgram(0);
}

void OpenGLRenderer::RenderHighlight(Camera& camera, const Mat4& model, PrimitiveType type) {
    // Draw wireframe outline of the selected object
    if (!m_SceneShader) return;
    Mat4 view = camera.GetViewMatrix();
    Mat4 proj = camera.GetProjectionMatrix();

    glUseProgram(m_SceneShader);
    glUniformMatrix4fv(glGetUniformLocation(m_SceneShader, "u_Model"), 1, GL_FALSE, model.m);
    glUniformMatrix4fv(glGetUniformLocation(m_SceneShader, "u_View"),  1, GL_FALSE, view.m);
    glUniformMatrix4fv(glGetUniformLocation(m_SceneShader, "u_Proj"),  1, GL_FALSE, proj.m);
    glUniform4f(glGetUniformLocation(m_SceneShader, "u_Color"), 1.0f, 0.8f, 0.0f, 1.0f);
    glUniform1i(glGetUniformLocation(m_SceneShader, "u_LightingEnabled"), 0);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(2.0f);
    glDisable(GL_DEPTH_TEST);

    if (type == PrimitiveType::Cube) {
        glBindVertexArray(m_CubeVAO);
        glDrawElements(GL_TRIANGLES, m_CubeIndexCount, GL_UNSIGNED_INT, reinterpret_cast<void*>(0));
        glBindVertexArray(0);
    } else if (type == PrimitiveType::Plane) {
        glBindVertexArray(m_PlaneVAO);
        glDrawElements(GL_TRIANGLES, m_PlaneIndexCount, GL_UNSIGNED_INT, reinterpret_cast<void*>(0));
        glBindVertexArray(0);
    } else if (type == PrimitiveType::Triangle) {
        glBindVertexArray(m_TriVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
    }

    glEnable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glLineWidth(1.0f);
    glUseProgram(0);
}

// ── Grid (XZ plane) ───────────────────────────────────────────────────────

void OpenGLRenderer::InitGrid() {
    if (!glGenVertexArrays) return;

    const int halfSize = 20;
    const float step = 1.0f;
    std::vector<float> gridVerts;

    for (int i = -halfSize; i <= halfSize; ++i) {
        float fi = static_cast<float>(i) * step;
        float limit = static_cast<float>(halfSize) * step;
        float alpha = (i == 0) ? 0.5f : 0.22f;
        // Line along Z
        gridVerts.insert(gridVerts.end(), { fi, 0, -limit, alpha, alpha, alpha });
        gridVerts.insert(gridVerts.end(), { fi, 0,  limit, alpha, alpha, alpha });
        // Line along X
        gridVerts.insert(gridVerts.end(), { -limit, 0, fi, alpha, alpha, alpha });
        gridVerts.insert(gridVerts.end(), {  limit, 0, fi, alpha, alpha, alpha });
    }
    m_GridVertCount = static_cast<i32>(gridVerts.size() / 6);

    glGenVertexArrays(1, &m_GridVAO);
    glGenBuffers(1, &m_GridVBO);
    glBindVertexArray(m_GridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_GridVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(gridVerts.size() * sizeof(float)),
                 gridVerts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), reinterpret_cast<void*>(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    GV_LOG_INFO("Editor grid created.");
}

void OpenGLRenderer::RenderGrid(Camera& camera) {
    if (!m_LineShader || !m_GridVAO) return;
    Mat4 vp = camera.GetProjectionMatrix() * camera.GetViewMatrix();
    glUseProgram(m_LineShader);
    glUniformMatrix4fv(glGetUniformLocation(m_LineShader, "u_VP"), 1, GL_FALSE, vp.m);
    glBindVertexArray(m_GridVAO);
    glDrawArrays(GL_LINES, 0, m_GridVertCount);
    glBindVertexArray(0);
    glUseProgram(0);
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

void Shader::SetVec4(const std::string& name, const Vec4& value) {
#ifdef GV_HAS_GLFW
    if (glGetUniformLocation && glUniform4f)
        glUniform4f(glGetUniformLocation(m_ProgramID, name.c_str()),
                    value.x, value.y, value.z, value.w);
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
