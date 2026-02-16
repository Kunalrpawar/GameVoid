// ============================================================================
// GameVoid Engine -- OpenGL Renderer Implementation
// ============================================================================
// PBR rendering, shadow mapping, point/spot lights, post-processing (bloom,
// tone mapping, FXAA), debug draw, instanced rendering, sprite rendering.
// ============================================================================
#include "renderer/Renderer.h"
#include "renderer/Camera.h"
#include "renderer/Lighting.h"
#include "renderer/MeshRenderer.h"
#include "renderer/MaterialComponent.h"
#include "renderer/Frustum.h"
#include "assets/Assets.h"
#include "core/Scene.h"
#include "core/GameObject.h"

#ifdef GV_HAS_GLFW
#include "core/GLDefs.h"
#include "core/Window.h"
#include <cmath>
#include <vector>
#include <algorithm>
#endif

namespace gv {

// ── IRenderer default implementations (no-ops unless overridden) ───────────
void IRenderer::DrawRect(f32 x, f32 y, f32 w, f32 h, const Vec4& c)
    { (void)x;(void)y;(void)w;(void)h;(void)c; }
void IRenderer::DrawTexture(u32 t, f32 x, f32 y, f32 w, f32 h)
    { (void)t;(void)x;(void)y;(void)w;(void)h; }
void IRenderer::DrawDebugBox(const Vec3& c, const Vec3& h, const Vec4& col)
    { (void)c;(void)h;(void)col; }
void IRenderer::DrawDebugSphere(const Vec3& c, f32 r, const Vec4& col)
    { (void)c;(void)r;(void)col; }
void IRenderer::DrawDebugLine(const Vec3& f, const Vec3& t, const Vec4& col)
    { (void)f;(void)t;(void)col; }

// ── OpenGLRenderer ─────────────────────────────────────────────────────────

bool OpenGLRenderer::Init(u32 width, u32 height, const std::string& title) {
    m_Width  = width;
    m_Height = height;

#ifdef GV_HAS_GLFW
    if (m_Window && m_Window->IsInitialised()) {
        if (!gvLoadGL()) {
            GV_LOG_ERROR("OpenGLRenderer: some GL 3.3 functions could not be loaded.");
        }

        glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        const char* glVendor   = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        const char* glRenderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        const char* glVersion  = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        GV_LOG_INFO("GL Vendor:   " + std::string(glVendor   ? glVendor   : "?"));
        GV_LOG_INFO("GL Renderer: " + std::string(glRenderer ? glRenderer : "?"));
        GV_LOG_INFO("GL Version:  " + std::string(glVersion  ? glVersion  : "?"));

        InitDemo();
        InitSceneShader();
        InitPrimitives();
        InitSkybox();
        InitLineShader();
        InitGrid();
        InitShadowMap();
        InitPostProcessing();
        InitScreenQuad();
        InitSpriteRenderer();
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
    CleanupShadowMap();
    CleanupPostProcessing();
    CleanupSpriteRenderer();
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
    // Clear is called separately
}

void OpenGLRenderer::EndFrame() {
#ifdef GV_HAS_GLFW
    if (m_Window) m_Window->SwapBuffers();
#endif
}

// ============================================================================
// RenderScene — Main rendering pipeline
// ============================================================================
void OpenGLRenderer::RenderScene(Scene& scene, Camera& camera) {
#ifdef GV_HAS_GLFW
    if (!m_SceneShader) {
        (void)scene; (void)camera;
        return;
    }

    // ── 1. Collect light data ──────────────────────────────────────────────
    Vec3 lightDir(0.3f, 1.0f, 0.5f);
    Vec3 lightColor(1.0f, 1.0f, 1.0f);
    Vec3 ambientColor(0.15f, 0.15f, 0.15f);
    Vec3 camPos = camera.GetOwner()->GetTransform().position;

    // Point lights (max 8)
    struct PointLightData {
        Vec3 position, colour;
        f32 intensity, constant, linear, quadratic, range;
    };
    std::vector<PointLightData> pointLights;

    // Spot lights (max 4)
    struct SpotLightData {
        Vec3 position, direction, colour;
        f32 intensity, innerCos, outerCos;
    };
    std::vector<SpotLightData> spotLights;

    for (auto& obj : scene.GetAllObjects()) {
        if (!obj->IsActive()) continue;
        if (auto* dl = obj->GetComponent<DirectionalLight>()) {
            lightDir = -(dl->direction.Normalized());
            lightColor = dl->colour * dl->intensity;
        }
        if (auto* al = obj->GetComponent<AmbientLight>()) {
            ambientColor = al->colour * al->intensity;
        }
        if (auto* pl = obj->GetComponent<PointLight>()) {
            if (pointLights.size() < 8) {
                pointLights.push_back({
                    obj->GetTransform().position, pl->colour,
                    pl->intensity, pl->constant, pl->linear, pl->quadratic, pl->range
                });
            }
        }
        if (auto* sl = obj->GetComponent<SpotLight>()) {
            if (spotLights.size() < 4) {
                const float PI = 3.14159265358979f;
                spotLights.push_back({
                    obj->GetTransform().position,
                    sl->direction.Normalized(),
                    sl->colour,
                    sl->intensity,
                    std::cos(sl->innerCutoff * PI / 180.0f),
                    std::cos(sl->outerCutoff * PI / 180.0f)
                });
            }
        }
    }

    // ── 2. Shadow pass (directional light) ─────────────────────────────────
    if (m_ShadowsEnabled && m_ShadowShader && m_ShadowFBO) {
        RenderShadowPass(scene, lightDir);
    }

    // ── 3. Main PBR pass ───────────────────────────────────────────────────
    Mat4 view = camera.GetViewMatrix();
    Mat4 proj = camera.GetProjectionMatrix();
    Mat4 viewProj = proj * view;

    // Extract frustum planes for culling
    Frustum frustum;
    frustum.ExtractFromVP(viewProj);

    glUseProgram(m_SceneShader);

    // Matrices
    GLint locView  = glGetUniformLocation(m_SceneShader, "u_View");
    GLint locProj  = glGetUniformLocation(m_SceneShader, "u_Proj");
    glUniformMatrix4fv(locView, 1, GL_FALSE, view.m);
    glUniformMatrix4fv(locProj, 1, GL_FALSE, proj.m);

    // Camera
    glUniform3f(glGetUniformLocation(m_SceneShader, "u_CamPos"),
                camPos.x, camPos.y, camPos.z);
    glUniform1i(glGetUniformLocation(m_SceneShader, "u_LightingEnabled"),
                m_LightingEnabled ? 1 : 0);

    // Directional light
    glUniform3f(glGetUniformLocation(m_SceneShader, "u_LightDir"),
                lightDir.x, lightDir.y, lightDir.z);
    glUniform3f(glGetUniformLocation(m_SceneShader, "u_LightColor"),
                lightColor.x, lightColor.y, lightColor.z);
    glUniform3f(glGetUniformLocation(m_SceneShader, "u_AmbientColor"),
                ambientColor.x, ambientColor.y, ambientColor.z);

    // Point lights
    i32 numPL = static_cast<i32>(pointLights.size());
    glUniform1i(glGetUniformLocation(m_SceneShader, "u_NumPointLights"), numPL);
    for (i32 i = 0; i < numPL; ++i) {
        std::string p = "u_PointLights[" + std::to_string(i) + "]";
        glUniform3f(glGetUniformLocation(m_SceneShader, (p + ".position").c_str()),
                    pointLights[i].position.x, pointLights[i].position.y, pointLights[i].position.z);
        glUniform3f(glGetUniformLocation(m_SceneShader, (p + ".color").c_str()),
                    pointLights[i].colour.x, pointLights[i].colour.y, pointLights[i].colour.z);
        glUniform1f(glGetUniformLocation(m_SceneShader, (p + ".intensity").c_str()),
                    pointLights[i].intensity);
        glUniform1f(glGetUniformLocation(m_SceneShader, (p + ".constant").c_str()),
                    pointLights[i].constant);
        glUniform1f(glGetUniformLocation(m_SceneShader, (p + ".linear").c_str()),
                    pointLights[i].linear);
        glUniform1f(glGetUniformLocation(m_SceneShader, (p + ".quadratic").c_str()),
                    pointLights[i].quadratic);
        glUniform1f(glGetUniformLocation(m_SceneShader, (p + ".range").c_str()),
                    pointLights[i].range);
    }

    // Spot lights
    i32 numSL = static_cast<i32>(spotLights.size());
    glUniform1i(glGetUniformLocation(m_SceneShader, "u_NumSpotLights"), numSL);
    for (i32 i = 0; i < numSL; ++i) {
        std::string p = "u_SpotLights[" + std::to_string(i) + "]";
        glUniform3f(glGetUniformLocation(m_SceneShader, (p + ".position").c_str()),
                    spotLights[i].position.x, spotLights[i].position.y, spotLights[i].position.z);
        glUniform3f(glGetUniformLocation(m_SceneShader, (p + ".direction").c_str()),
                    spotLights[i].direction.x, spotLights[i].direction.y, spotLights[i].direction.z);
        glUniform3f(glGetUniformLocation(m_SceneShader, (p + ".color").c_str()),
                    spotLights[i].colour.x, spotLights[i].colour.y, spotLights[i].colour.z);
        glUniform1f(glGetUniformLocation(m_SceneShader, (p + ".intensity").c_str()),
                    spotLights[i].intensity);
        glUniform1f(glGetUniformLocation(m_SceneShader, (p + ".innerCos").c_str()),
                    spotLights[i].innerCos);
        glUniform1f(glGetUniformLocation(m_SceneShader, (p + ".outerCos").c_str()),
                    spotLights[i].outerCos);
    }

    // Shadow map
    if (m_ShadowsEnabled && m_ShadowMap) {
        glActiveTexture(GL_TEXTURE0 + 5);
        glBindTexture(GL_TEXTURE_2D, m_ShadowMap);
        glUniform1i(glGetUniformLocation(m_SceneShader, "u_ShadowMap"), 5);
        glUniformMatrix4fv(glGetUniformLocation(m_SceneShader, "u_LightSpaceMatrix"),
                           1, GL_FALSE, m_LightSpaceMatrix.m);
        glUniform1i(glGetUniformLocation(m_SceneShader, "u_ShadowsEnabled"), 1);
    } else {
        glUniform1i(glGetUniformLocation(m_SceneShader, "u_ShadowsEnabled"), 0);
    }

    // ── 4. Draw all objects with MeshRenderer ──────────────────────────────
    GLint locModel = glGetUniformLocation(m_SceneShader, "u_Model");
    GLint locColor = glGetUniformLocation(m_SceneShader, "u_Color");
    // PBR material uniforms
    GLint locMetallic  = glGetUniformLocation(m_SceneShader, "u_Metallic");
    GLint locRoughness = glGetUniformLocation(m_SceneShader, "u_Roughness");
    GLint locEmission  = glGetUniformLocation(m_SceneShader, "u_Emission");
    GLint locAO        = glGetUniformLocation(m_SceneShader, "u_AO");
    GLint locHasAlbedo = glGetUniformLocation(m_SceneShader, "u_HasAlbedoMap");
    GLint locHasNormal = glGetUniformLocation(m_SceneShader, "u_HasNormalMap");

    i32 drawCount = 0;
    i32 culledCount = 0;

    for (auto& obj : scene.GetAllObjects()) {
        if (!obj->IsActive()) continue;

        MeshRenderer* mr = obj->GetComponent<MeshRenderer>();
        if (!mr) continue;
        // Skip if no primitive type AND no custom mesh
        if (mr->primitiveType == PrimitiveType::None && !mr->GetMesh()) continue;

        // Frustum culling: test object bounding sphere
        Vec3 objPos = obj->GetTransform().GetWorldPosition();
        f32  objScale = obj->GetTransform().scale.x; // approximate with x scale
        if (!frustum.TestObject(objPos, objScale)) {
            culledCount++;
            continue;
        }

        Mat4 model = obj->GetTransform().GetModelMatrix();
        glUniformMatrix4fv(locModel, 1, GL_FALSE, model.m);

        // Check for MaterialComponent (PBR overrides)
        auto* matComp = obj->GetComponent<MaterialComponent>();
        if (matComp) {
            glUniform4f(locColor, matComp->albedo.x, matComp->albedo.y,
                        matComp->albedo.z, matComp->albedo.w);
            glUniform1f(locMetallic,  matComp->metallic);
            glUniform1f(locRoughness, matComp->roughness);
            Vec3 em = matComp->emission * matComp->emissionStrength;
            glUniform3f(locEmission, em.x, em.y, em.z);
            glUniform1f(locAO, matComp->ao);

            // Texture maps
            if (matComp->albedoMap) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, matComp->albedoMap);
                glUniform1i(glGetUniformLocation(m_SceneShader, "u_AlbedoMap"), 0);
                glUniform1i(locHasAlbedo, 1);
            } else {
                glUniform1i(locHasAlbedo, 0);
            }
            if (matComp->normalMap) {
                glActiveTexture(GL_TEXTURE0 + 1);
                glBindTexture(GL_TEXTURE_2D, matComp->normalMap);
                glUniform1i(glGetUniformLocation(m_SceneShader, "u_NormalMap"), 1);
                glUniform1i(locHasNormal, 1);
            } else {
                glUniform1i(locHasNormal, 0);
            }
        } else {
            glUniform4f(locColor, mr->color.x, mr->color.y, mr->color.z, mr->color.w);
            glUniform1f(locMetallic,  0.0f);
            glUniform1f(locRoughness, 0.5f);
            glUniform3f(locEmission,  0.0f, 0.0f, 0.0f);
            glUniform1f(locAO, 1.0f);
            glUniform1i(locHasAlbedo, 0);
            glUniform1i(locHasNormal, 0);
        }

        // Draw the built-in primitive or custom mesh
        if (mr->GetMesh() && mr->GetMesh()->GetIndexCount() > 0) {
            // Custom loaded mesh (e.g. OBJ)
            mr->GetMesh()->Bind();
            glDrawElements(GL_TRIANGLES,
                           static_cast<GLsizei>(mr->GetMesh()->GetIndexCount()),
                           GL_UNSIGNED_INT, reinterpret_cast<void*>(0));
            mr->GetMesh()->Unbind();
        } else if (mr->primitiveType == PrimitiveType::Triangle) {
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

    // ── 5. Flush debug draw ────────────────────────────────────────────────
    FlushDebugDraw(camera);

    static bool loggedOnce = false;
    if (!loggedOnce) {
        GV_LOG_INFO("RenderScene: drew " + std::to_string(drawCount) +
                    " object(s) with PBR pipeline.");
        loggedOnce = true;
    }

    return;
#else
    (void)scene; (void)camera;
#endif
}

void OpenGLRenderer::ApplyLighting(Scene& scene) {
    (void)scene;
    // Lighting is now uploaded directly in RenderScene
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

// ============================================================================
// Demo triangle
// ============================================================================
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

static GLuint CompileShaderStage(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, buf);
        GV_LOG_ERROR("Shader compile: " + std::string(buf));
    }
    return shader;
}

static GLuint LinkProgram(GLuint vs, GLuint fs) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) {
        char buf[1024];
        glGetProgramInfoLog(prog, 1024, nullptr, buf);
        GV_LOG_ERROR("Shader link: " + std::string(buf));
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

void OpenGLRenderer::InitDemo() {
    if (!glCreateShader) return;

    GLuint vs = CompileShaderStage(GL_VERTEX_SHADER,   s_DemoVertSrc);
    GLuint fs = CompileShaderStage(GL_FRAGMENT_SHADER, s_DemoFragSrc);
    m_DemoShader = LinkProgram(vs, fs);

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
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
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
    float model[16] = { c,s,0,0, -s,c,0,0, 0,0,1,0, 0,0,0,1 };

    glUseProgram(m_DemoShader);
    glUniformMatrix4fv(glGetUniformLocation(m_DemoShader, "u_Model"), 1, GL_FALSE, model);
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

// ============================================================================
// PBR Scene Shader
// ============================================================================
// Cook-Torrance BRDF with GGX distribution, Smith geometry, Fresnel-Schlick.
// Supports: directional light, up to 8 point lights, up to 4 spot lights,
// shadow mapping, normal mapping, albedo/roughness/metallic textures.

static const char* s_PBR_VertSrc =
    "#version 330 core\n"
    "layout(location = 0) in vec3 aPos;\n"
    "layout(location = 1) in vec3 aNormal;\n"
    "layout(location = 2) in vec2 aTexCoord;\n"
    "layout(location = 3) in vec3 aTangent;\n"
    "layout(location = 4) in vec3 aBitangent;\n"
    "\n"
    "uniform mat4 u_Model;\n"
    "uniform mat4 u_View;\n"
    "uniform mat4 u_Proj;\n"
    "uniform mat4 u_LightSpaceMatrix;\n"
    "\n"
    "out vec3 vWorldPos;\n"
    "out vec3 vNormal;\n"
    "out vec2 vTexCoord;\n"
    "out vec4 vLightSpacePos;\n"
    "out mat3 vTBN;\n"
    "\n"
    "void main() {\n"
    "    vec4 worldPos = u_Model * vec4(aPos, 1.0);\n"
    "    gl_Position = u_Proj * u_View * worldPos;\n"
    "    vWorldPos = worldPos.xyz;\n"
    "    mat3 normalMat = mat3(u_Model);\n"
    "    vNormal = normalize(normalMat * aNormal);\n"
    "    vTexCoord = aTexCoord;\n"
    "    vLightSpacePos = u_LightSpaceMatrix * worldPos;\n"
    "    vec3 T = normalize(normalMat * aTangent);\n"
    "    vec3 B = normalize(normalMat * aBitangent);\n"
    "    vec3 N = vNormal;\n"
    "    vTBN = mat3(T, B, N);\n"
    "}\n";

static const char* s_PBR_FragSrc =
    "#version 330 core\n"
    "in vec3 vWorldPos;\n"
    "in vec3 vNormal;\n"
    "in vec2 vTexCoord;\n"
    "in vec4 vLightSpacePos;\n"
    "in mat3 vTBN;\n"
    "\n"
    "layout(location = 0) out vec4 FragColor;\n"
    "\n"
    "uniform vec4 u_Color;\n"
    "uniform float u_Metallic;\n"
    "uniform float u_Roughness;\n"
    "uniform vec3  u_Emission;\n"
    "uniform float u_AO;\n"
    "\n"
    "// Texture maps\n"
    "uniform sampler2D u_AlbedoMap;\n"
    "uniform sampler2D u_NormalMap;\n"
    "uniform int u_HasAlbedoMap;\n"
    "uniform int u_HasNormalMap;\n"
    "\n"
    "// Directional light\n"
    "uniform vec3 u_LightDir;\n"
    "uniform vec3 u_LightColor;\n"
    "uniform vec3 u_AmbientColor;\n"
    "uniform vec3 u_CamPos;\n"
    "uniform int  u_LightingEnabled;\n"
    "\n"
    "// Point lights\n"
    "struct PointLight {\n"
    "    vec3 position;\n"
    "    vec3 color;\n"
    "    float intensity;\n"
    "    float constant;\n"
    "    float linear;\n"
    "    float quadratic;\n"
    "    float range;\n"
    "};\n"
    "#define MAX_POINT_LIGHTS 8\n"
    "uniform PointLight u_PointLights[MAX_POINT_LIGHTS];\n"
    "uniform int u_NumPointLights;\n"
    "\n"
    "// Spot lights\n"
    "struct SpotLight {\n"
    "    vec3 position;\n"
    "    vec3 direction;\n"
    "    vec3 color;\n"
    "    float intensity;\n"
    "    float innerCos;\n"
    "    float outerCos;\n"
    "};\n"
    "#define MAX_SPOT_LIGHTS 4\n"
    "uniform SpotLight u_SpotLights[MAX_SPOT_LIGHTS];\n"
    "uniform int u_NumSpotLights;\n"
    "\n"
    "// Shadow mapping\n"
    "uniform sampler2D u_ShadowMap;\n"
    "uniform int u_ShadowsEnabled;\n"
    "\n"
    "const float PI = 3.14159265359;\n"
    "\n"
    "// ── PBR functions ──────────────────────────\n"
    "float DistributionGGX(vec3 N, vec3 H, float roughness) {\n"
    "    float a  = roughness * roughness;\n"
    "    float a2 = a * a;\n"
    "    float NdotH  = max(dot(N, H), 0.0);\n"
    "    float NdotH2 = NdotH * NdotH;\n"
    "    float denom = NdotH2 * (a2 - 1.0) + 1.0;\n"
    "    return a2 / (PI * denom * denom + 0.0001);\n"
    "}\n"
    "\n"
    "float GeometrySchlickGGX(float NdotV, float roughness) {\n"
    "    float r = roughness + 1.0;\n"
    "    float k = (r*r) / 8.0;\n"
    "    return NdotV / (NdotV * (1.0 - k) + k);\n"
    "}\n"
    "\n"
    "float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {\n"
    "    float NdotV = max(dot(N, V), 0.0);\n"
    "    float NdotL = max(dot(N, L), 0.0);\n"
    "    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);\n"
    "}\n"
    "\n"
    "vec3 FresnelSchlick(float cosTheta, vec3 F0) {\n"
    "    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);\n"
    "}\n"
    "\n"
    "// ── Shadow calculation ──────────────────────\n"
    "float ShadowCalc(vec4 lsPos, vec3 N, vec3 L) {\n"
    "    vec3 projCoords = lsPos.xyz / lsPos.w;\n"
    "    projCoords = projCoords * 0.5 + 0.5;\n"
    "    if (projCoords.z > 1.0) return 0.0;\n"
    "    float currentDepth = projCoords.z;\n"
    "    float bias = max(0.005 * (1.0 - dot(N, L)), 0.001);\n"
    "    // PCF soft shadows (3x3 kernel)\n"
    "    float shadow = 0.0;\n"
    "    vec2 texelSize = 1.0 / textureSize(u_ShadowMap, 0);\n"
    "    for (int x = -1; x <= 1; ++x) {\n"
    "        for (int y = -1; y <= 1; ++y) {\n"
    "            float pcfDepth = texture(u_ShadowMap, projCoords.xy + vec2(x,y)*texelSize).r;\n"
    "            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;\n"
    "        }\n"
    "    }\n"
    "    return shadow / 9.0;\n"
    "}\n"
    "\n"
    "// ── Cook-Torrance BRDF for a single light ──\n"
    "vec3 CookTorrance(vec3 N, vec3 V, vec3 L, vec3 lightColor,\n"
    "                  vec3 albedo, float metallic, float roughness) {\n"
    "    vec3 H = normalize(V + L);\n"
    "    vec3 F0 = mix(vec3(0.04), albedo, metallic);\n"
    "    float NDF = DistributionGGX(N, H, roughness);\n"
    "    float G   = GeometrySmith(N, V, L, roughness);\n"
    "    vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);\n"
    "    vec3 numerator = NDF * G * F;\n"
    "    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;\n"
    "    vec3 specular = numerator / denominator;\n"
    "    vec3 kS = F;\n"
    "    vec3 kD = (1.0 - kS) * (1.0 - metallic);\n"
    "    float NdotL = max(dot(N, L), 0.0);\n"
    "    return (kD * albedo / PI + specular) * lightColor * NdotL;\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    // Get albedo\n"
    "    vec3 albedo = u_Color.rgb;\n"
    "    float alpha = u_Color.a;\n"
    "    if (u_HasAlbedoMap != 0) {\n"
    "        vec4 texColor = texture(u_AlbedoMap, vTexCoord);\n"
    "        albedo *= texColor.rgb;\n"
    "        alpha *= texColor.a;\n"
    "    }\n"
    "\n"
    "    if (u_LightingEnabled == 0) {\n"
    "        FragColor = vec4(albedo + u_Emission, alpha);\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    // Normal (with optional normal map)\n"
    "    vec3 N = normalize(vNormal);\n"
    "    if (u_HasNormalMap != 0) {\n"
    "        vec3 tangentNormal = texture(u_NormalMap, vTexCoord).xyz * 2.0 - 1.0;\n"
    "        N = normalize(vTBN * tangentNormal);\n"
    "    }\n"
    "    vec3 V = normalize(u_CamPos - vWorldPos);\n"
    "\n"
    "    float metallic = u_Metallic;\n"
    "    float roughness = max(u_Roughness, 0.04);\n"
    "\n"
    "    // ── Directional light ────────────────────\n"
    "    vec3 L = normalize(u_LightDir);\n"
    "    vec3 Lo = CookTorrance(N, V, L, u_LightColor, albedo, metallic, roughness);\n"
    "\n"
    "    // Shadow\n"
    "    float shadow = 0.0;\n"
    "    if (u_ShadowsEnabled != 0) {\n"
    "        shadow = ShadowCalc(vLightSpacePos, N, L);\n"
    "    }\n"
    "    Lo *= (1.0 - shadow);\n"
    "\n"
    "    // ── Point lights ─────────────────────────\n"
    "    for (int i = 0; i < u_NumPointLights; ++i) {\n"
    "        vec3 pL = u_PointLights[i].position - vWorldPos;\n"
    "        float dist = length(pL);\n"
    "        if (dist > u_PointLights[i].range) continue;\n"
    "        pL = normalize(pL);\n"
    "        float atten = u_PointLights[i].intensity /\n"
    "            (u_PointLights[i].constant + u_PointLights[i].linear * dist +\n"
    "             u_PointLights[i].quadratic * dist * dist);\n"
    "        Lo += CookTorrance(N, V, pL, u_PointLights[i].color * atten,\n"
    "                           albedo, metallic, roughness);\n"
    "    }\n"
    "\n"
    "    // ── Spot lights ──────────────────────────\n"
    "    for (int i = 0; i < u_NumSpotLights; ++i) {\n"
    "        vec3 sL = normalize(u_SpotLights[i].position - vWorldPos);\n"
    "        float theta = dot(sL, normalize(-u_SpotLights[i].direction));\n"
    "        float epsilon = u_SpotLights[i].innerCos - u_SpotLights[i].outerCos;\n"
    "        float spotIntensity = clamp((theta - u_SpotLights[i].outerCos) / epsilon, 0.0, 1.0);\n"
    "        if (spotIntensity > 0.0) {\n"
    "            float dist = length(u_SpotLights[i].position - vWorldPos);\n"
    "            float atten = u_SpotLights[i].intensity / (1.0 + 0.09*dist + 0.032*dist*dist);\n"
    "            Lo += CookTorrance(N, V, sL, u_SpotLights[i].color * atten * spotIntensity,\n"
    "                               albedo, metallic, roughness);\n"
    "        }\n"
    "    }\n"
    "\n"
    "    // Ambient (simplified IBL approximation)\n"
    "    vec3 F0 = mix(vec3(0.04), albedo, metallic);\n"
    "    vec3 kS = FresnelSchlick(max(dot(N, V), 0.0), F0);\n"
    "    vec3 kD = (1.0 - kS) * (1.0 - metallic);\n"
    "    vec3 ambient = (kD * albedo * u_AmbientColor) * u_AO;\n"
    "\n"
    "    vec3 color = ambient + Lo + u_Emission;\n"
    "    FragColor = vec4(color, alpha);\n"
    "}\n";

void OpenGLRenderer::InitSceneShader() {
    if (!glCreateShader) return;

    GLuint vs = CompileShaderStage(GL_VERTEX_SHADER,   s_PBR_VertSrc);
    GLuint fs = CompileShaderStage(GL_FRAGMENT_SHADER, s_PBR_FragSrc);
    m_SceneShader = LinkProgram(vs, fs);

    GV_LOG_INFO("PBR scene shader compiled (Cook-Torrance BRDF).");
}

// ============================================================================
// Built-in Primitives (with UVs and tangents for PBR / normal mapping)
// ============================================================================
void OpenGLRenderer::InitPrimitives() {
    if (!glGenVertexArrays) return;

    // Vertex layout: position(3) + normal(3) + texCoord(2) + tangent(3) + bitangent(3)
    // = 14 floats, 56 bytes per vertex
    const int STRIDE = 14;

    // ── Triangle ───────────────────────────────────────────────────────────
    float triVerts[] = {
        // pos                 normal              uv        tangent             bitangent
         0.0f,  0.5f, 0.0f,   0,0,1,   0.5f,1,   1,0,0,   0,1,0,
        -0.5f, -0.5f, 0.0f,   0,0,1,   0,0,      1,0,0,   0,1,0,
         0.5f, -0.5f, 0.0f,   0,0,1,   1,0,      1,0,0,   0,1,0,
    };

    glGenVertexArrays(1, &m_TriVAO);
    glGenBuffers(1, &m_TriVBO);
    glBindVertexArray(m_TriVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_TriVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(triVerts)),
                 triVerts, GL_STATIC_DRAW);
    for (int i = 0; i < 5; ++i) {
        int sz = (i == 2) ? 2 : 3;
        int off = 0;
        if (i == 1) off = 3; else if (i == 2) off = 6; else if (i == 3) off = 8; else if (i == 4) off = 11;
        glVertexAttribPointer(i, sz, GL_FLOAT, GL_FALSE, STRIDE * sizeof(float),
                              reinterpret_cast<void*>(off * sizeof(float)));
        glEnableVertexAttribArray(i);
    }
    glBindVertexArray(0);
    GV_LOG_INFO("Built-in triangle primitive created.");

    // ── Cube ───────────────────────────────────────────────────────────────
    struct CubeFace { float nx,ny,nz; float tx,ty,tz; float bx,by,bz;
                      float verts[4][3]; float uvs[4][2]; };
    CubeFace cubeFaces[] = {
        // front +Z
        { 0,0,1, 1,0,0, 0,1,0,
          {{-0.5f,-0.5f,0.5f},{0.5f,-0.5f,0.5f},{0.5f,0.5f,0.5f},{-0.5f,0.5f,0.5f}},
          {{0,0},{1,0},{1,1},{0,1}} },
        // back -Z
        { 0,0,-1, -1,0,0, 0,1,0,
          {{-0.5f,-0.5f,-0.5f},{-0.5f,0.5f,-0.5f},{0.5f,0.5f,-0.5f},{0.5f,-0.5f,-0.5f}},
          {{1,0},{1,1},{0,1},{0,0}} },
        // top +Y
        { 0,1,0, 1,0,0, 0,0,-1,
          {{-0.5f,0.5f,-0.5f},{-0.5f,0.5f,0.5f},{0.5f,0.5f,0.5f},{0.5f,0.5f,-0.5f}},
          {{0,0},{0,1},{1,1},{1,0}} },
        // bottom -Y
        { 0,-1,0, 1,0,0, 0,0,1,
          {{-0.5f,-0.5f,-0.5f},{0.5f,-0.5f,-0.5f},{0.5f,-0.5f,0.5f},{-0.5f,-0.5f,0.5f}},
          {{0,0},{1,0},{1,1},{0,1}} },
        // right +X
        { 1,0,0, 0,0,-1, 0,1,0,
          {{0.5f,-0.5f,-0.5f},{0.5f,0.5f,-0.5f},{0.5f,0.5f,0.5f},{0.5f,-0.5f,0.5f}},
          {{0,0},{0,1},{1,1},{1,0}} },
        // left -X
        { -1,0,0, 0,0,1, 0,1,0,
          {{-0.5f,-0.5f,-0.5f},{-0.5f,-0.5f,0.5f},{-0.5f,0.5f,0.5f},{-0.5f,0.5f,-0.5f}},
          {{0,0},{1,0},{1,1},{0,1}} },
    };

    std::vector<float> cubeData;
    std::vector<unsigned int> cubeIdx;
    for (int f = 0; f < 6; ++f) {
        unsigned int base = static_cast<unsigned int>(cubeData.size() / STRIDE);
        for (int v = 0; v < 4; ++v) {
            cubeData.insert(cubeData.end(), {cubeFaces[f].verts[v][0], cubeFaces[f].verts[v][1], cubeFaces[f].verts[v][2]});
            cubeData.insert(cubeData.end(), {cubeFaces[f].nx, cubeFaces[f].ny, cubeFaces[f].nz});
            cubeData.insert(cubeData.end(), {cubeFaces[f].uvs[v][0], cubeFaces[f].uvs[v][1]});
            cubeData.insert(cubeData.end(), {cubeFaces[f].tx, cubeFaces[f].ty, cubeFaces[f].tz});
            cubeData.insert(cubeData.end(), {cubeFaces[f].bx, cubeFaces[f].by, cubeFaces[f].bz});
        }
        cubeIdx.insert(cubeIdx.end(), {base, base+1, base+2, base+2, base+3, base});
    }
    m_CubeIndexCount = static_cast<i32>(cubeIdx.size());

    glGenVertexArrays(1, &m_CubeVAO);
    glGenBuffers(1, &m_CubeVBO);
    glGenBuffers(1, &m_CubeEBO);
    glBindVertexArray(m_CubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_CubeVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(cubeData.size() * sizeof(float)),
                 cubeData.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_CubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(cubeIdx.size() * sizeof(unsigned int)),
                 cubeIdx.data(), GL_STATIC_DRAW);
    for (int i = 0; i < 5; ++i) {
        int sz = (i == 2) ? 2 : 3;
        int off = 0;
        if (i == 1) off = 3; else if (i == 2) off = 6; else if (i == 3) off = 8; else if (i == 4) off = 11;
        glVertexAttribPointer(i, sz, GL_FLOAT, GL_FALSE, STRIDE * sizeof(float),
                              reinterpret_cast<void*>(off * sizeof(float)));
        glEnableVertexAttribArray(i);
    }
    glBindVertexArray(0);
    GV_LOG_INFO("Built-in cube primitive created (24 verts, PBR layout).");

    // ── Plane ──────────────────────────────────────────────────────────────
    float planeVerts[] = {
        // pos               normal   uv     tangent    bitangent
        -0.5f,0,-0.5f,  0,1,0,  0,0,  1,0,0,  0,0,1,
         0.5f,0,-0.5f,  0,1,0,  1,0,  1,0,0,  0,0,1,
         0.5f,0, 0.5f,  0,1,0,  1,1,  1,0,0,  0,0,1,
        -0.5f,0, 0.5f,  0,1,0,  0,1,  1,0,0,  0,0,1,
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
    for (int i = 0; i < 5; ++i) {
        int sz = (i == 2) ? 2 : 3;
        int off = 0;
        if (i == 1) off = 3; else if (i == 2) off = 6; else if (i == 3) off = 8; else if (i == 4) off = 11;
        glVertexAttribPointer(i, sz, GL_FLOAT, GL_FALSE, STRIDE * sizeof(float),
                              reinterpret_cast<void*>(off * sizeof(float)));
        glEnableVertexAttribArray(i);
    }
    glBindVertexArray(0);
    GV_LOG_INFO("Built-in plane primitive created (PBR layout).");
}

// ============================================================================
// Shadow Mapping
// ============================================================================
static const char* s_ShadowVertSrc =
    "#version 330 core\n"
    "layout(location = 0) in vec3 aPos;\n"
    "uniform mat4 u_LightSpaceMatrix;\n"
    "uniform mat4 u_Model;\n"
    "void main() {\n"
    "    gl_Position = u_LightSpaceMatrix * u_Model * vec4(aPos, 1.0);\n"
    "}\n";

static const char* s_ShadowFragSrc =
    "#version 330 core\n"
    "void main() {\n"
    "    // Depth is written automatically\n"
    "}\n";

void OpenGLRenderer::InitShadowMap() {
    if (!glGenFramebuffers) return;

    GLuint vs = CompileShaderStage(GL_VERTEX_SHADER, s_ShadowVertSrc);
    GLuint fs = CompileShaderStage(GL_FRAGMENT_SHADER, s_ShadowFragSrc);
    m_ShadowShader = LinkProgram(vs, fs);

    // Create depth texture
    glGenTextures(1, &m_ShadowMap);
    glBindTexture(GL_TEXTURE_2D, m_ShadowMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
                 static_cast<GLsizei>(m_ShadowMapSize), static_cast<GLsizei>(m_ShadowMapSize),
                 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    if (glTexParameterfv) glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Create FBO
    glGenFramebuffers(1, &m_ShadowFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_ShadowFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_ShadowMap, 0);
    // No color attachment
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        GV_LOG_ERROR("Shadow map FBO incomplete!");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    GV_LOG_INFO("Shadow map initialised (" + std::to_string(m_ShadowMapSize) + "x" +
                std::to_string(m_ShadowMapSize) + ").");
}

void OpenGLRenderer::RenderShadowPass(Scene& scene, const Vec3& lightDir) {
    // Save current FBO and viewport so we can restore after shadow pass
    GLint prevFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT_ENUM, prevViewport);

    // Compute light-space matrix (orthographic projection for directional light)
    float extent = 30.0f;
    Mat4 lightProj = Mat4::Ortho(-extent, extent, -extent, extent, 0.1f, 100.0f);
    Vec3 lightPos = lightDir * 30.0f;  // Place light far away in light direction
    Mat4 lightView = Mat4::LookAt(lightPos, Vec3(0, 0, 0), Vec3(0, 1, 0));
    m_LightSpaceMatrix = lightProj * lightView;

    glViewport(0, 0, static_cast<GLsizei>(m_ShadowMapSize), static_cast<GLsizei>(m_ShadowMapSize));
    glBindFramebuffer(GL_FRAMEBUFFER, m_ShadowFBO);
    glClear(GL_DEPTH_BUFFER_BIT);

    glUseProgram(m_ShadowShader);
    GLint locLSM = glGetUniformLocation(m_ShadowShader, "u_LightSpaceMatrix");
    GLint locModel = glGetUniformLocation(m_ShadowShader, "u_Model");
    glUniformMatrix4fv(locLSM, 1, GL_FALSE, m_LightSpaceMatrix.m);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);  // Peter-panning fix

    for (auto& obj : scene.GetAllObjects()) {
        if (!obj->IsActive()) continue;
        MeshRenderer* mr = obj->GetComponent<MeshRenderer>();
        if (!mr) continue;
        if (mr->primitiveType == PrimitiveType::None && !mr->GetMesh()) continue;

        Mat4 model = obj->GetTransform().GetModelMatrix();
        glUniformMatrix4fv(locModel, 1, GL_FALSE, model.m);

        if (mr->GetMesh() && mr->GetMesh()->GetIndexCount() > 0) {
            // Custom loaded mesh shadow
            mr->GetMesh()->Bind();
            glDrawElements(GL_TRIANGLES,
                           static_cast<GLsizei>(mr->GetMesh()->GetIndexCount()),
                           GL_UNSIGNED_INT, nullptr);
            mr->GetMesh()->Unbind();
        } else if (mr->primitiveType == PrimitiveType::Cube) {
            glBindVertexArray(m_CubeVAO);
            glDrawElements(GL_TRIANGLES, m_CubeIndexCount, GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
        } else if (mr->primitiveType == PrimitiveType::Plane) {
            glBindVertexArray(m_PlaneVAO);
            glDrawElements(GL_TRIANGLES, m_PlaneIndexCount, GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
        } else if (mr->primitiveType == PrimitiveType::Triangle) {
            glBindVertexArray(m_TriVAO);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindVertexArray(0);
        }
    }

    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);

    // Restore the previously-bound FBO and viewport (critical for editor viewport)
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFBO));
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    glUseProgram(0);
}

void OpenGLRenderer::CleanupShadowMap() {
    if (m_ShadowFBO)    { glDeleteFramebuffers(1, &m_ShadowFBO); m_ShadowFBO = 0; }
    if (m_ShadowMap)    { glDeleteTextures(1, &m_ShadowMap);     m_ShadowMap = 0; }
    if (m_ShadowShader && m_ShadowShader != m_SceneShader) {
        glDeleteProgram(m_ShadowShader); m_ShadowShader = 0;
    }
}

// ============================================================================
// Post-Processing Pipeline (Bloom + Tone Mapping + FXAA)
// ============================================================================

// Screen quad for fullscreen passes
void OpenGLRenderer::InitScreenQuad() {
    if (!glGenVertexArrays) return;
    float quadVerts[] = {
        // pos(2)    uv(2)
        -1, -1,   0, 0,
         1, -1,   1, 0,
         1,  1,   1, 1,
        -1, -1,   0, 0,
         1,  1,   1, 1,
        -1,  1,   0, 1,
    };
    glGenVertexArrays(1, &m_ScreenQuadVAO);
    glGenBuffers(1, &m_ScreenQuadVBO);
    glBindVertexArray(m_ScreenQuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_ScreenQuadVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(quadVerts)),
                 quadVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float),
                          reinterpret_cast<void*>(2*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

static const char* s_ScreenQuadVert =
    "#version 330 core\n"
    "layout(location = 0) in vec2 aPos;\n"
    "layout(location = 1) in vec2 aTexCoord;\n"
    "out vec2 vTexCoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
    "    vTexCoord = aTexCoord;\n"
    "}\n";

// Bright-pass extraction (for bloom)
static const char* s_BrightPassFrag =
    "#version 330 core\n"
    "in vec2 vTexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D u_HDRBuffer;\n"
    "uniform float u_Threshold;\n"
    "void main() {\n"
    "    vec3 color = texture(u_HDRBuffer, vTexCoord).rgb;\n"
    "    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));\n"
    "    if (brightness > u_Threshold)\n"
    "        FragColor = vec4(color, 1.0);\n"
    "    else\n"
    "        FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
    "}\n";

// Gaussian blur (ping-pong)
static const char* s_BlurFrag =
    "#version 330 core\n"
    "in vec2 vTexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D u_Image;\n"
    "uniform int u_Horizontal;\n"
    "uniform float u_Weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);\n"
    "void main() {\n"
    "    vec2 texOffset = 1.0 / textureSize(u_Image, 0);\n"
    "    vec3 result = texture(u_Image, vTexCoord).rgb * u_Weight[0];\n"
    "    if (u_Horizontal != 0) {\n"
    "        for (int i = 1; i < 5; ++i) {\n"
    "            result += texture(u_Image, vTexCoord + vec2(texOffset.x*float(i), 0)).rgb * u_Weight[i];\n"
    "            result += texture(u_Image, vTexCoord - vec2(texOffset.x*float(i), 0)).rgb * u_Weight[i];\n"
    "        }\n"
    "    } else {\n"
    "        for (int i = 1; i < 5; ++i) {\n"
    "            result += texture(u_Image, vTexCoord + vec2(0, texOffset.y*float(i))).rgb * u_Weight[i];\n"
    "            result += texture(u_Image, vTexCoord - vec2(0, texOffset.y*float(i))).rgb * u_Weight[i];\n"
    "        }\n"
    "    }\n"
    "    FragColor = vec4(result, 1.0);\n"
    "}\n";

// Tone mapping + bloom composite (ACES + Reinhard option)
static const char* s_TonemapFrag =
    "#version 330 core\n"
    "in vec2 vTexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D u_HDRBuffer;\n"
    "uniform sampler2D u_BloomBlur;\n"
    "uniform float u_Exposure;\n"
    "uniform float u_BloomIntensity;\n"
    "uniform int u_BloomEnabled;\n"
    "\n"
    "vec3 ACESFilm(vec3 x) {\n"
    "    float a = 2.51;\n"
    "    float b = 0.03;\n"
    "    float c = 2.43;\n"
    "    float d = 0.59;\n"
    "    float e = 0.14;\n"
    "    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec3 hdr = texture(u_HDRBuffer, vTexCoord).rgb;\n"
    "    if (u_BloomEnabled != 0) {\n"
    "        vec3 bloom = texture(u_BloomBlur, vTexCoord).rgb;\n"
    "        hdr += bloom * u_BloomIntensity;\n"
    "    }\n"
    "    // Exposure + ACES tone mapping\n"
    "    vec3 mapped = ACESFilm(hdr * u_Exposure);\n"
    "    // Gamma correction\n"
    "    mapped = pow(mapped, vec3(1.0/2.2));\n"
    "    FragColor = vec4(mapped, 1.0);\n"
    "}\n";

// FXAA (fast approximate anti-aliasing)
static const char* s_FXAAFrag =
    "#version 330 core\n"
    "in vec2 vTexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D u_Screen;\n"
    "uniform vec2 u_InvScreenSize;\n"
    "\n"
    "#define FXAA_REDUCE_MIN (1.0/128.0)\n"
    "#define FXAA_REDUCE_MUL (1.0/8.0)\n"
    "#define FXAA_SPAN_MAX   8.0\n"
    "\n"
    "void main() {\n"
    "    vec3 rgbNW = texture(u_Screen, vTexCoord + vec2(-1, -1) * u_InvScreenSize).rgb;\n"
    "    vec3 rgbNE = texture(u_Screen, vTexCoord + vec2( 1, -1) * u_InvScreenSize).rgb;\n"
    "    vec3 rgbSW = texture(u_Screen, vTexCoord + vec2(-1,  1) * u_InvScreenSize).rgb;\n"
    "    vec3 rgbSE = texture(u_Screen, vTexCoord + vec2( 1,  1) * u_InvScreenSize).rgb;\n"
    "    vec3 rgbM  = texture(u_Screen, vTexCoord).rgb;\n"
    "\n"
    "    vec3 luma = vec3(0.299, 0.587, 0.114);\n"
    "    float lumaNW = dot(rgbNW, luma);\n"
    "    float lumaNE = dot(rgbNE, luma);\n"
    "    float lumaSW = dot(rgbSW, luma);\n"
    "    float lumaSE = dot(rgbSE, luma);\n"
    "    float lumaM  = dot(rgbM,  luma);\n"
    "\n"
    "    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));\n"
    "    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));\n"
    "\n"
    "    vec2 dir;\n"
    "    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));\n"
    "    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));\n"
    "\n"
    "    float dirReduce = max((lumaNW+lumaNE+lumaSW+lumaSE)*(0.25*FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);\n"
    "    float rcpDirMin = 1.0/(min(abs(dir.x), abs(dir.y)) + dirReduce);\n"
    "    dir = min(vec2(FXAA_SPAN_MAX), max(vec2(-FXAA_SPAN_MAX), dir*rcpDirMin)) * u_InvScreenSize;\n"
    "\n"
    "    vec3 rgbA = 0.5 * (texture(u_Screen, vTexCoord + dir*(1.0/3.0 - 0.5)).rgb\n"
    "                     + texture(u_Screen, vTexCoord + dir*(2.0/3.0 - 0.5)).rgb);\n"
    "    vec3 rgbB = rgbA * 0.5 + 0.25 * (texture(u_Screen, vTexCoord + dir*(-0.5)).rgb\n"
    "                                    + texture(u_Screen, vTexCoord + dir*0.5).rgb);\n"
    "    float lumaB = dot(rgbB, luma);\n"
    "    if (lumaB < lumaMin || lumaB > lumaMax)\n"
    "        FragColor = vec4(rgbA, 1.0);\n"
    "    else\n"
    "        FragColor = vec4(rgbB, 1.0);\n"
    "}\n";

void OpenGLRenderer::InitPostProcessing() {
    if (!glGenFramebuffers || !glCreateShader) return;

    // ── HDR framebuffer ────────────────────────────────────────────────────
    glGenFramebuffers(1, &m_HDR_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_HDR_FBO);

    // HDR color attachment (RGBA16F)
    glGenTextures(1, &m_HDR_ColorTex);
    glBindTexture(GL_TEXTURE_2D, m_HDR_ColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                 static_cast<GLsizei>(m_Width), static_cast<GLsizei>(m_Height),
                 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_HDR_ColorTex, 0);

    // Depth renderbuffer
    glGenRenderbuffers(1, &m_HDR_DepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_HDR_DepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
                          static_cast<GLsizei>(m_Width), static_cast<GLsizei>(m_Height));
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_HDR_DepthRBO);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        GV_LOG_ERROR("HDR FBO incomplete!");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ── Bloom ping-pong FBOs ───────────────────────────────────────────────
    for (int i = 0; i < 2; ++i) {
        glGenFramebuffers(1, &m_BloomFBO[i]);
        glGenTextures(1, &m_BloomTex[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFBO[i]);
        glBindTexture(GL_TEXTURE_2D, m_BloomTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                     static_cast<GLsizei>(m_Width / 2), static_cast<GLsizei>(m_Height / 2),
                     0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_BloomTex[i], 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ── Compile post-processing shaders ────────────────────────────────────
    GLuint screenVS = CompileShaderStage(GL_VERTEX_SHADER, s_ScreenQuadVert);

    GLuint bpFS = CompileShaderStage(GL_FRAGMENT_SHADER, s_BrightPassFrag);
    m_BrightPassShader = LinkProgram(CompileShaderStage(GL_VERTEX_SHADER, s_ScreenQuadVert), bpFS);

    GLuint blurFS = CompileShaderStage(GL_FRAGMENT_SHADER, s_BlurFrag);
    m_BlurShader = LinkProgram(CompileShaderStage(GL_VERTEX_SHADER, s_ScreenQuadVert), blurFS);

    GLuint tmFS = CompileShaderStage(GL_FRAGMENT_SHADER, s_TonemapFrag);
    m_TonemapShader = LinkProgram(CompileShaderStage(GL_VERTEX_SHADER, s_ScreenQuadVert), tmFS);

    GLuint fxFS = CompileShaderStage(GL_FRAGMENT_SHADER, s_FXAAFrag);
    m_FXAAShader = LinkProgram(CompileShaderStage(GL_VERTEX_SHADER, s_ScreenQuadVert), fxFS);

    (void)screenVS; // used inline above

    GV_LOG_INFO("Post-processing pipeline initialised (bloom + ACES tone mapping + FXAA).");
}

void OpenGLRenderer::BeginHDRPass() {
    if (!m_HDR_FBO) return;
    glBindFramebuffer(GL_FRAMEBUFFER, m_HDR_FBO);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLRenderer::EndHDRPass() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLRenderer::RenderPostProcessing() {
    if (!m_ScreenQuadVAO || !m_TonemapShader) return;

    glDisable(GL_DEPTH_TEST);

    GLuint finalTex = m_HDR_ColorTex;

    // ── Bloom pass ─────────────────────────────────────────────────────────
    if (m_BloomEnabled && m_BrightPassShader && m_BlurShader) {
        // 1. Extract bright pixels
        glViewport(0, 0, static_cast<GLsizei>(m_Width/2), static_cast<GLsizei>(m_Height/2));
        glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFBO[0]);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(m_BrightPassShader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_HDR_ColorTex);
        glUniform1i(glGetUniformLocation(m_BrightPassShader, "u_HDRBuffer"), 0);
        glUniform1f(glGetUniformLocation(m_BrightPassShader, "u_Threshold"), m_BloomThreshold);
        glBindVertexArray(m_ScreenQuadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // 2. Gaussian blur (ping-pong, 5 iterations)
        bool horizontal = true;
        for (int i = 0; i < 10; ++i) {
            glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFBO[horizontal ? 1 : 0]);
            glUseProgram(m_BlurShader);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_BloomTex[horizontal ? 0 : 1]);
            glUniform1i(glGetUniformLocation(m_BlurShader, "u_Image"), 0);
            glUniform1i(glGetUniformLocation(m_BlurShader, "u_Horizontal"), horizontal ? 1 : 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            horizontal = !horizontal;
        }
        glBindVertexArray(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, static_cast<GLsizei>(m_Width), static_cast<GLsizei>(m_Height));
    }

    // ── Tone mapping pass ──────────────────────────────────────────────────
    if (m_ToneMappingEnabled) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(m_TonemapShader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_HDR_ColorTex);
        glUniform1i(glGetUniformLocation(m_TonemapShader, "u_HDRBuffer"), 0);

        if (m_BloomEnabled) {
            glActiveTexture(GL_TEXTURE0 + 1);
            glBindTexture(GL_TEXTURE_2D, m_BloomTex[0]);
            glUniform1i(glGetUniformLocation(m_TonemapShader, "u_BloomBlur"), 1);
        }
        glUniform1f(glGetUniformLocation(m_TonemapShader, "u_Exposure"), m_Exposure);
        glUniform1f(glGetUniformLocation(m_TonemapShader, "u_BloomIntensity"), m_BloomIntensity);
        glUniform1i(glGetUniformLocation(m_TonemapShader, "u_BloomEnabled"), m_BloomEnabled ? 1 : 0);

        glBindVertexArray(m_ScreenQuadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

    // ── FXAA pass ──────────────────────────────────────────────────────────
    // Note: FXAA requires an additional FBO to read from. For simplicity,
    // we skip it when not explicitly enabled (it would need another intermediate texture).
    // The shader is ready for use when an additional FBO is set up.

    glEnable(GL_DEPTH_TEST);
    glUseProgram(0);
}

void OpenGLRenderer::CleanupPostProcessing() {
    if (m_HDR_FBO)       { glDeleteFramebuffers(1, &m_HDR_FBO);       m_HDR_FBO = 0; }
    if (m_HDR_ColorTex)  { glDeleteTextures(1, &m_HDR_ColorTex);      m_HDR_ColorTex = 0; }
    if (m_HDR_DepthRBO)  { glDeleteRenderbuffers(1, &m_HDR_DepthRBO); m_HDR_DepthRBO = 0; }
    for (int i = 0; i < 2; ++i) {
        if (m_BloomFBO[i]) { glDeleteFramebuffers(1, &m_BloomFBO[i]); m_BloomFBO[i] = 0; }
        if (m_BloomTex[i]) { glDeleteTextures(1, &m_BloomTex[i]);     m_BloomTex[i] = 0; }
    }
    if (m_BrightPassShader) { glDeleteProgram(m_BrightPassShader); m_BrightPassShader = 0; }
    if (m_BlurShader)       { glDeleteProgram(m_BlurShader);       m_BlurShader = 0; }
    if (m_TonemapShader)    { glDeleteProgram(m_TonemapShader);    m_TonemapShader = 0; }
    if (m_FXAAShader)       { glDeleteProgram(m_FXAAShader);       m_FXAAShader = 0; }
    if (m_ScreenQuadVAO)    { glDeleteVertexArrays(1, &m_ScreenQuadVAO); m_ScreenQuadVAO = 0; }
    if (m_ScreenQuadVBO)    { glDeleteBuffers(1, &m_ScreenQuadVBO);      m_ScreenQuadVBO = 0; }
}

// ============================================================================
// Sprite / 2D Rendering
// ============================================================================
static const char* s_SpriteVertSrc =
    "#version 330 core\n"
    "layout(location = 0) in vec2 aPos;\n"
    "layout(location = 1) in vec2 aTexCoord;\n"
    "uniform mat4 u_Projection;\n"
    "uniform mat4 u_Model;\n"
    "out vec2 vTexCoord;\n"
    "void main() {\n"
    "    gl_Position = u_Projection * u_Model * vec4(aPos, 0.0, 1.0);\n"
    "    vTexCoord = aTexCoord;\n"
    "}\n";

static const char* s_SpriteFragSrc =
    "#version 330 core\n"
    "in vec2 vTexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D u_Texture;\n"
    "uniform vec4 u_Tint;\n"
    "uniform int u_HasTexture;\n"
    "void main() {\n"
    "    if (u_HasTexture != 0)\n"
    "        FragColor = texture(u_Texture, vTexCoord) * u_Tint;\n"
    "    else\n"
    "        FragColor = u_Tint;\n"
    "}\n";

void OpenGLRenderer::InitSpriteRenderer() {
    if (!glCreateShader) return;

    GLuint vs = CompileShaderStage(GL_VERTEX_SHADER, s_SpriteVertSrc);
    GLuint fs = CompileShaderStage(GL_FRAGMENT_SHADER, s_SpriteFragSrc);
    m_SpriteShader = LinkProgram(vs, fs);

    // Unit quad: 2D position + UV
    float spriteVerts[] = {
        0, 0,  0, 0,
        1, 0,  1, 0,
        1, 1,  1, 1,
        0, 0,  0, 0,
        1, 1,  1, 1,
        0, 1,  0, 1,
    };

    glGenVertexArrays(1, &m_SpriteVAO);
    glGenBuffers(1, &m_SpriteVBO);
    glBindVertexArray(m_SpriteVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_SpriteVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(spriteVerts)),
                 spriteVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float),
                          reinterpret_cast<void*>(2*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    GV_LOG_INFO("Sprite renderer initialised.");
}

void OpenGLRenderer::CleanupSpriteRenderer() {
    if (m_SpriteVAO)    { glDeleteVertexArrays(1, &m_SpriteVAO); m_SpriteVAO = 0; }
    if (m_SpriteVBO)    { glDeleteBuffers(1, &m_SpriteVBO);      m_SpriteVBO = 0; }
    if (m_SpriteShader) { glDeleteProgram(m_SpriteShader);        m_SpriteShader = 0; }
}

void OpenGLRenderer::DrawRect(f32 x, f32 y, f32 w, f32 h, const Vec4& colour) {
    if (!m_SpriteShader || !m_SpriteVAO) return;

    // Orthographic projection for screen-space
    Mat4 proj = Mat4::Ortho(0, static_cast<f32>(m_Width), static_cast<f32>(m_Height), 0, -1, 1);

    // Model: translate + scale
    Mat4 model = Mat4::Identity();
    // Scale
    model.m[0] = w; model.m[5] = h;
    // Translate
    model.m[12] = x; model.m[13] = y;

    glDisable(GL_DEPTH_TEST);
    glUseProgram(m_SpriteShader);
    glUniformMatrix4fv(glGetUniformLocation(m_SpriteShader, "u_Projection"), 1, GL_FALSE, proj.m);
    glUniformMatrix4fv(glGetUniformLocation(m_SpriteShader, "u_Model"), 1, GL_FALSE, model.m);
    glUniform4f(glGetUniformLocation(m_SpriteShader, "u_Tint"), colour.x, colour.y, colour.z, colour.w);
    glUniform1i(glGetUniformLocation(m_SpriteShader, "u_HasTexture"), 0);

    glBindVertexArray(m_SpriteVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glUseProgram(0);
    glEnable(GL_DEPTH_TEST);
}

void OpenGLRenderer::DrawTexture(u32 textureID, f32 x, f32 y, f32 w, f32 h) {
    if (!m_SpriteShader || !m_SpriteVAO) return;

    Mat4 proj = Mat4::Ortho(0, static_cast<f32>(m_Width), static_cast<f32>(m_Height), 0, -1, 1);
    Mat4 model = Mat4::Identity();
    model.m[0] = w; model.m[5] = h;
    model.m[12] = x; model.m[13] = y;

    glDisable(GL_DEPTH_TEST);
    glUseProgram(m_SpriteShader);
    glUniformMatrix4fv(glGetUniformLocation(m_SpriteShader, "u_Projection"), 1, GL_FALSE, proj.m);
    glUniformMatrix4fv(glGetUniformLocation(m_SpriteShader, "u_Model"), 1, GL_FALSE, model.m);
    glUniform4f(glGetUniformLocation(m_SpriteShader, "u_Tint"), 1, 1, 1, 1);
    glUniform1i(glGetUniformLocation(m_SpriteShader, "u_HasTexture"), 1);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glUniform1i(glGetUniformLocation(m_SpriteShader, "u_Texture"), 0);

    glBindVertexArray(m_SpriteVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glUseProgram(0);
    glEnable(GL_DEPTH_TEST);
}

// ============================================================================
// Debug Draw
// ============================================================================
void OpenGLRenderer::DrawDebugLine(const Vec3& from, const Vec3& to, const Vec4& colour) {
    m_DebugLines.push_back({from, to, colour});
}

void OpenGLRenderer::DrawDebugBox(const Vec3& center, const Vec3& half, const Vec4& colour) {
    // 12 edges of a box
    Vec3 corners[8] = {
        center + Vec3(-half.x, -half.y, -half.z),
        center + Vec3( half.x, -half.y, -half.z),
        center + Vec3( half.x,  half.y, -half.z),
        center + Vec3(-half.x,  half.y, -half.z),
        center + Vec3(-half.x, -half.y,  half.z),
        center + Vec3( half.x, -half.y,  half.z),
        center + Vec3( half.x,  half.y,  half.z),
        center + Vec3(-half.x,  half.y,  half.z),
    };
    int edges[][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
    for (auto& e : edges)
        m_DebugLines.push_back({corners[e[0]], corners[e[1]], colour});
}

void OpenGLRenderer::DrawDebugSphere(const Vec3& center, f32 radius, const Vec4& colour) {
    // Approximate sphere with 3 axis-aligned circles (16 segments each)
    const int N = 16;
    const float PI2 = 6.28318530718f;
    for (int axis = 0; axis < 3; ++axis) {
        for (int i = 0; i < N; ++i) {
            float a0 = static_cast<float>(i) / N * PI2;
            float a1 = static_cast<float>(i+1) / N * PI2;
            Vec3 p0, p1;
            if (axis == 0) { // YZ circle
                p0 = center + Vec3(0, std::cos(a0)*radius, std::sin(a0)*radius);
                p1 = center + Vec3(0, std::cos(a1)*radius, std::sin(a1)*radius);
            } else if (axis == 1) { // XZ circle
                p0 = center + Vec3(std::cos(a0)*radius, 0, std::sin(a0)*radius);
                p1 = center + Vec3(std::cos(a1)*radius, 0, std::sin(a1)*radius);
            } else { // XY circle
                p0 = center + Vec3(std::cos(a0)*radius, std::sin(a0)*radius, 0);
                p1 = center + Vec3(std::cos(a1)*radius, std::sin(a1)*radius, 0);
            }
            m_DebugLines.push_back({p0, p1, colour});
        }
    }
}

void OpenGLRenderer::FlushDebugDraw(Camera& camera) {
    if (m_DebugLines.empty() || !m_LineShader || !m_LineVAO) return;

    Mat4 vp = camera.GetProjectionMatrix() * camera.GetViewMatrix();

    // Build line data: pos(3) + color(3) per vertex, 2 verts per line
    std::vector<float> data;
    data.reserve(m_DebugLines.size() * 12);
    for (auto& dl : m_DebugLines) {
        data.insert(data.end(), {dl.from.x, dl.from.y, dl.from.z,
                                  dl.colour.x, dl.colour.y, dl.colour.z});
        data.insert(data.end(), {dl.to.x, dl.to.y, dl.to.z,
                                  dl.colour.x, dl.colour.y, dl.colour.z});
    }

    glUseProgram(m_LineShader);
    glUniformMatrix4fv(glGetUniformLocation(m_LineShader, "u_VP"), 1, GL_FALSE, vp.m);

    glBindVertexArray(m_LineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_LineVBO);

    // Upload in chunks if needed
    GLsizeiptr dataSize = static_cast<GLsizeiptr>(data.size() * sizeof(float));
    if (dataSize <= 8192 * static_cast<GLsizeiptr>(sizeof(float))) {
        glBufferSubData(GL_ARRAY_BUFFER, 0, dataSize, data.data());
    } else {
        glBufferData(GL_ARRAY_BUFFER, dataSize, data.data(), GL_DYNAMIC_DRAW);
    }

    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_DebugLines.size() * 2));
    glLineWidth(1.0f);

    glBindVertexArray(0);
    glUseProgram(0);

    m_DebugLines.clear();
}

// ============================================================================
// Cleanup, Skybox, Grid, Gizmo, Highlight (same as before with minor fixes)
// ============================================================================

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
    if (m_HighlightShader && m_HighlightShader != m_SceneShader) {
        glDeleteProgram(m_HighlightShader); m_HighlightShader = 0;
    }
}

// ── Skybox ─────────────────────────────────────────────────────────────────

static const char* s_SkyVertSrc =
    "#version 330 core\n"
    "layout(location = 0) in vec3 aPos;\n"
    "uniform mat4 u_VP;\n"
    "out vec3 vDir;\n"
    "void main() {\n"
    "    vDir = aPos;\n"
    "    vec4 pos = u_VP * vec4(aPos, 1.0);\n"
    "    gl_Position = pos.xyww;\n"
    "}\n";

static const char* s_SkyFragSrc =
    "#version 330 core\n"
    "in vec3 vDir;\n"
    "out vec4 FragColor;\n"
    "uniform float u_Time;\n"
    "void main() {\n"
    "    vec3 dir = normalize(vDir);\n"
    "    float t = dir.y * 0.5 + 0.5;\n"
    "    vec3 horizonCol = vec3(0.7, 0.82, 0.92);\n"
    "    vec3 zenithCol  = vec3(0.2, 0.35, 0.75);\n"
    "    vec3 groundCol  = vec3(0.25, 0.22, 0.2);\n"
    "    vec3 sky = mix(horizonCol, zenithCol, clamp(t, 0.0, 1.0));\n"
    "    if (dir.y < 0.0) sky = mix(horizonCol, groundCol, clamp(-dir.y*4.0, 0.0, 1.0));\n"
    "    float sunAngle = u_Time * 0.02;\n"
    "    vec3 sunDir = normalize(vec3(cos(sunAngle)*0.4, 0.8, sin(sunAngle)*0.4));\n"
    "    float sunDot = max(dot(dir, sunDir), 0.0);\n"
    "    sky += vec3(1.0, 0.9, 0.7) * pow(sunDot, 256.0) * 2.0;\n"
    "    sky += vec3(1.0, 0.85, 0.6) * pow(sunDot, 8.0) * 0.15;\n"
    "    float cx = dir.x / max(dir.y, 0.01) * 2.0 + u_Time * 0.01;\n"
    "    float cz = dir.z / max(dir.y, 0.01) * 2.0;\n"
    "    float cloud = sin(cx*1.2)*sin(cz*1.5)*0.5+0.5;\n"
    "    cloud = smoothstep(0.45, 0.65, cloud);\n"
    "    if (dir.y > 0.0) sky = mix(sky, vec3(1.0), cloud * 0.35 * clamp(dir.y*3.0, 0.0, 1.0));\n"
    "    FragColor = vec4(sky, 1.0);\n"
    "}\n";

void OpenGLRenderer::InitSkybox() {
    if (!glCreateShader) return;
    GLuint vs = CompileShaderStage(GL_VERTEX_SHADER, s_SkyVertSrc);
    GLuint fs = CompileShaderStage(GL_FRAGMENT_SHADER, s_SkyFragSrc);
    m_SkyShader = LinkProgram(vs, fs);

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
    view.m[12] = 0; view.m[13] = 0; view.m[14] = 0;
    Mat4 proj = camera.GetProjectionMatrix();
    Mat4 vp = proj * view;

    glDepthFunc(GL_LEQUAL);
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
    GLuint vs = CompileShaderStage(GL_VERTEX_SHADER, s_LineVertSrc);
    GLuint fs = CompileShaderStage(GL_FRAGMENT_SHADER, s_LineFragSrc);
    m_LineShader = LinkProgram(vs, fs);

    glGenVertexArrays(1, &m_LineVAO);
    glGenBuffers(1, &m_LineVBO);
    glBindVertexArray(m_LineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_LineVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(65536 * sizeof(float)), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), reinterpret_cast<void*>(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    m_HighlightShader = m_SceneShader;
    GV_LOG_INFO("Line/gizmo shader initialised.");
}

void OpenGLRenderer::RenderGizmo(Camera& camera, const Vec3& pos, GizmoMode mode, i32 activeAxis) {
    if (!m_LineShader || !m_LineVAO) return;

    Mat4 vp = camera.GetProjectionMatrix() * camera.GetViewMatrix();
    glUseProgram(m_LineShader);
    glUniformMatrix4fv(glGetUniformLocation(m_LineShader, "u_VP"), 1, GL_FALSE, vp.m);

    f32 len = 1.5f;
    f32 hl = 0.15f;
    float lines[864 + 72];
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
        pushLine(pos, pos + Vec3(len, 0, 0), xCol);
        pushLine(pos + Vec3(len, 0, 0), pos + Vec3(len - hl, hl, 0), xCol);
        pushLine(pos, pos + Vec3(0, len, 0), yCol);
        pushLine(pos + Vec3(0, len, 0), pos + Vec3(0, len - hl, hl), yCol);
        pushLine(pos, pos + Vec3(0, 0, len), zCol);
        pushLine(pos + Vec3(0, 0, len), pos + Vec3(0, hl, len - hl), zCol);
    } else if (mode == GizmoMode::Scale) {
        pushLine(pos, pos + Vec3(len, 0, 0), xCol);
        pushLine(pos + Vec3(len-hl, -hl, 0), pos + Vec3(len+hl, hl, 0), xCol);
        pushLine(pos, pos + Vec3(0, len, 0), yCol);
        pushLine(pos + Vec3(-hl, len-hl, 0), pos + Vec3(hl, len+hl, 0), yCol);
        pushLine(pos, pos + Vec3(0, 0, len), zCol);
        pushLine(pos + Vec3(0, -hl, len-hl), pos + Vec3(0, hl, len+hl), zCol);
    } else {
        const int N = 24;
        for (int i = 0; i < N; ++i) {
            f32 a0 = (float)i / (float)N * 6.2832f;
            f32 a1 = (float)(i+1) / (float)N * 6.2832f;
            f32 r = 1.0f;
            pushLine(pos + Vec3(0, std::cos(a0)*r, std::sin(a0)*r),
                     pos + Vec3(0, std::cos(a1)*r, std::sin(a1)*r), xCol);
            pushLine(pos + Vec3(std::cos(a0)*r, 0, std::sin(a0)*r),
                     pos + Vec3(std::cos(a1)*r, 0, std::sin(a1)*r), yCol);
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
        glDrawElements(GL_TRIANGLES, m_CubeIndexCount, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    } else if (type == PrimitiveType::Plane) {
        glBindVertexArray(m_PlaneVAO);
        glDrawElements(GL_TRIANGLES, m_PlaneIndexCount, GL_UNSIGNED_INT, nullptr);
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

// ── Grid ───────────────────────────────────────────────────────────────────

void OpenGLRenderer::InitGrid() {
    if (!glGenVertexArrays) return;

    const int halfSize = 20;
    const float step = 1.0f;
    std::vector<float> gridVerts;

    for (int i = -halfSize; i <= halfSize; ++i) {
        float fi = static_cast<float>(i) * step;
        float limit = static_cast<float>(halfSize) * step;
        float alpha = (i == 0) ? 0.5f : 0.22f;
        gridVerts.insert(gridVerts.end(), { fi, 0, -limit, alpha, alpha, alpha });
        gridVerts.insert(gridVerts.end(), { fi, 0,  limit, alpha, alpha, alpha });
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

// ============================================================================
// Shader class implementation
// ============================================================================

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
