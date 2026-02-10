// ============================================================================
// GameVoid Engine — OpenGL Function Loader
// ============================================================================
// Resolves GL 2.0+ / 3.3 Core function pointers using glfwGetProcAddress.
// Call gvLoadGL() AFTER glfwMakeContextCurrent().
// ============================================================================
#ifdef GV_HAS_GLFW

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "core/GLDefs.h"

// ── Define all function-pointer globals ────────────────────────────────────

// Shaders
PFN_glCreateShader           glCreateShader           = nullptr;
PFN_glDeleteShader           glDeleteShader           = nullptr;
PFN_glShaderSource           glShaderSource           = nullptr;
PFN_glCompileShader          glCompileShader          = nullptr;
PFN_glGetShaderiv            glGetShaderiv            = nullptr;
PFN_glGetShaderInfoLog       glGetShaderInfoLog       = nullptr;

// Programs
PFN_glCreateProgram          glCreateProgram          = nullptr;
PFN_glDeleteProgram          glDeleteProgram          = nullptr;
PFN_glAttachShader           glAttachShader           = nullptr;
PFN_glDetachShader           glDetachShader           = nullptr;
PFN_glLinkProgram            glLinkProgram            = nullptr;
PFN_glUseProgram             glUseProgram             = nullptr;
PFN_glGetProgramiv           glGetProgramiv           = nullptr;
PFN_glGetProgramInfoLog      glGetProgramInfoLog      = nullptr;

// Uniforms
PFN_glGetUniformLocation     glGetUniformLocation     = nullptr;
PFN_glUniform1i              glUniform1i              = nullptr;
PFN_glUniform1f              glUniform1f              = nullptr;
PFN_glUniform2f              glUniform2f              = nullptr;
PFN_glUniform3f              glUniform3f              = nullptr;
PFN_glUniform4f              glUniform4f              = nullptr;
PFN_glUniform3fv             glUniform3fv             = nullptr;
PFN_glUniform4fv             glUniform4fv             = nullptr;
PFN_glUniformMatrix4fv       glUniformMatrix4fv       = nullptr;

// VAO
PFN_glGenVertexArrays        glGenVertexArrays        = nullptr;
PFN_glDeleteVertexArrays     glDeleteVertexArrays     = nullptr;
PFN_glBindVertexArray        glBindVertexArray        = nullptr;

// VBO
PFN_glGenBuffers             glGenBuffers             = nullptr;
PFN_glDeleteBuffers          glDeleteBuffers          = nullptr;
PFN_glBindBuffer             glBindBuffer             = nullptr;
PFN_glBufferData             glBufferData             = nullptr;
PFN_glBufferSubData          glBufferSubData          = nullptr;

// Vertex attributes
PFN_glVertexAttribPointer    glVertexAttribPointer    = nullptr;
PFN_glEnableVertexAttribArray  glEnableVertexAttribArray  = nullptr;
PFN_glDisableVertexAttribArray glDisableVertexAttribArray = nullptr;

// Texture
PFN_glActiveTexture          glActiveTexture          = nullptr;
PFN_glGenerateMipmap         glGenerateMipmap         = nullptr;

// ── Loader implementation ──────────────────────────────────────────────────

#define GV_LOAD(name) \
    name = (PFN_##name)glfwGetProcAddress(#name); \
    if (!name) ok = false

bool gvLoadGL() {
    bool ok = true;

    // Shaders
    GV_LOAD(glCreateShader);
    GV_LOAD(glDeleteShader);
    GV_LOAD(glShaderSource);
    GV_LOAD(glCompileShader);
    GV_LOAD(glGetShaderiv);
    GV_LOAD(glGetShaderInfoLog);

    // Programs
    GV_LOAD(glCreateProgram);
    GV_LOAD(glDeleteProgram);
    GV_LOAD(glAttachShader);
    GV_LOAD(glDetachShader);
    GV_LOAD(glLinkProgram);
    GV_LOAD(glUseProgram);
    GV_LOAD(glGetProgramiv);
    GV_LOAD(glGetProgramInfoLog);

    // Uniforms
    GV_LOAD(glGetUniformLocation);
    GV_LOAD(glUniform1i);
    GV_LOAD(glUniform1f);
    GV_LOAD(glUniform2f);
    GV_LOAD(glUniform3f);
    GV_LOAD(glUniform4f);
    GV_LOAD(glUniform3fv);
    GV_LOAD(glUniform4fv);
    GV_LOAD(glUniformMatrix4fv);

    // VAO
    GV_LOAD(glGenVertexArrays);
    GV_LOAD(glDeleteVertexArrays);
    GV_LOAD(glBindVertexArray);

    // VBO
    GV_LOAD(glGenBuffers);
    GV_LOAD(glDeleteBuffers);
    GV_LOAD(glBindBuffer);
    GV_LOAD(glBufferData);
    GV_LOAD(glBufferSubData);

    // Vertex attributes
    GV_LOAD(glVertexAttribPointer);
    GV_LOAD(glEnableVertexAttribArray);
    GV_LOAD(glDisableVertexAttribArray);

    // Texture
    GV_LOAD(glActiveTexture);
    GV_LOAD(glGenerateMipmap);

    return ok;
}

#undef GV_LOAD

#else  // !GV_HAS_GLFW

bool gvLoadGL() { return false; }

#endif // GV_HAS_GLFW
