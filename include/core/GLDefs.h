// ============================================================================
// GameVoid Engine — Minimal OpenGL 3.3 Core Definitions
// ============================================================================
// Self-contained GL types, constants, and function declarations.
// GL 1.1 functions link from opengl32.lib; GL 2.0+ are loaded at runtime
// via gvLoadGL() (see GLLoader.cpp).
//
// Only compiled when GV_HAS_GLFW is defined (window mode build).
// ============================================================================
#pragma once

#ifdef GV_HAS_GLFW

#include <cstddef>  // ptrdiff_t

// ── Calling-convention macros ──────────────────────────────────────────────
#ifndef APIENTRY
  #ifdef _WIN32
    #define APIENTRY __stdcall
  #else
    #define APIENTRY
  #endif
#endif

#ifdef _WIN32
  #define GV_GL_IMPORT __declspec(dllimport)
#else
  #define GV_GL_IMPORT
#endif

// Prevent system GL headers from being included after this
#ifndef __gl_h_
#define __gl_h_
#endif
#ifndef __GL_H__
#define __GL_H__
#endif

// ── GL types ───────────────────────────────────────────────────────────────
typedef unsigned int   GLenum;
typedef unsigned int   GLbitfield;
typedef unsigned int   GLuint;
typedef int            GLint;
typedef int            GLsizei;
typedef unsigned char  GLboolean;
typedef char           GLchar;
typedef float          GLfloat;
typedef float          GLclampf;
typedef double         GLdouble;
typedef unsigned char  GLubyte;
typedef short          GLshort;
typedef unsigned short GLushort;
typedef void           GLvoid;
typedef ptrdiff_t      GLsizeiptr;
typedef ptrdiff_t      GLintptr;

// ── GL constants ───────────────────────────────────────────────────────────
#define GL_FALSE                    0
#define GL_TRUE                     1
#define GL_NO_ERROR                 0

// Buffer bits
#define GL_DEPTH_BUFFER_BIT         0x00000100
#define GL_STENCIL_BUFFER_BIT       0x00000400
#define GL_COLOR_BUFFER_BIT         0x00004000

// Enable caps
#define GL_DEPTH_TEST               0x0B71
#define GL_BLEND                    0x0BE2
#define GL_CULL_FACE                0x0B44
#define GL_SCISSOR_TEST             0x0C11
#define GL_MULTISAMPLE              0x809D

// Blend factors
#define GL_ZERO                     0
#define GL_ONE                      1
#define GL_SRC_ALPHA                0x0302
#define GL_ONE_MINUS_SRC_ALPHA      0x0303
#define GL_SRC_COLOR                0x0300
#define GL_ONE_MINUS_SRC_COLOR      0x0301

// Depth functions
#define GL_NEVER                    0x0200
#define GL_LESS                     0x0201
#define GL_EQUAL                    0x0202
#define GL_LEQUAL                   0x0203
#define GL_GREATER                  0x0204
#define GL_NOTEQUAL                 0x0205
#define GL_GEQUAL                   0x0206
#define GL_ALWAYS                   0x0207

// Cull face
#define GL_FRONT                    0x0404
#define GL_BACK                     0x0405
#define GL_FRONT_AND_BACK           0x0408
#define GL_CW                       0x0900
#define GL_CCW                      0x0901

// Shader types
#define GL_VERTEX_SHADER            0x8B31
#define GL_FRAGMENT_SHADER          0x8B30

// Shader / program queries
#define GL_COMPILE_STATUS           0x8B81
#define GL_LINK_STATUS              0x8B82
#define GL_INFO_LOG_LENGTH          0x8B84

// Buffer targets
#define GL_ARRAY_BUFFER             0x8892
#define GL_ELEMENT_ARRAY_BUFFER     0x8893

// Buffer usage
#define GL_STATIC_DRAW              0x88E4
#define GL_DYNAMIC_DRAW             0x88E8
#define GL_STREAM_DRAW              0x88E0

// Draw modes
#define GL_POINTS                   0x0000
#define GL_LINES                    0x0001
#define GL_LINE_LOOP                0x0002
#define GL_LINE_STRIP               0x0003
#define GL_TRIANGLES                0x0004
#define GL_TRIANGLE_STRIP           0x0005
#define GL_TRIANGLE_FAN             0x0006

// Data types
#define GL_BYTE                     0x1400
#define GL_UNSIGNED_BYTE            0x1401
#define GL_SHORT                    0x1402
#define GL_UNSIGNED_SHORT           0x1403
#define GL_INT                      0x1404
#define GL_UNSIGNED_INT             0x1405
#define GL_FLOAT                    0x1406

// String queries
#define GL_VENDOR                   0x1F00
#define GL_RENDERER                 0x1F01
#define GL_VERSION                  0x1F02
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C

// Polygon mode
#define GL_POINT                    0x1B00
#define GL_LINE                     0x1B01
#define GL_FILL                     0x1B02

// Texture
#define GL_TEXTURE_2D               0x0DE1
#define GL_TEXTURE0                 0x84C0
#define GL_TEXTURE_MIN_FILTER       0x2801
#define GL_TEXTURE_MAG_FILTER       0x2800
#define GL_TEXTURE_WRAP_S           0x2802
#define GL_TEXTURE_WRAP_T           0x2803
#define GL_LINEAR                   0x2601
#define GL_NEAREST                  0x2600
#define GL_NEAREST_MIPMAP_NEAREST   0x2700
#define GL_LINEAR_MIPMAP_NEAREST    0x2701
#define GL_NEAREST_MIPMAP_LINEAR    0x2702
#define GL_LINEAR_MIPMAP_LINEAR     0x2703
#define GL_CLAMP_TO_EDGE            0x812F
#define GL_REPEAT                   0x2901
#define GL_MIRRORED_REPEAT          0x8370
#define GL_RGB                      0x1907
#define GL_RGBA                     0x1908
#define GL_RGB8                     0x8051
#define GL_RGBA8                    0x8058

// Framebuffer (future use)
#define GL_FRAMEBUFFER              0x8D40
#define GL_RENDERBUFFER             0x8D41
#define GL_COLOR_ATTACHMENT0        0x8CE0
#define GL_DEPTH_ATTACHMENT         0x8D00
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define GL_FRAMEBUFFER_COMPLETE     0x8CD5
#define GL_DEPTH_COMPONENT24        0x81A6
#define GL_DEPTH24_STENCIL8         0x88F0
#define GL_DEPTH_STENCIL            0x84F9
#define GL_UNSIGNED_INT_24_8        0x84FA
#define GL_READ_FRAMEBUFFER         0x8CA8
#define GL_DRAW_FRAMEBUFFER         0x8CA9

// ============================================================================
// GL 1.1 Functions  (linked from opengl32.lib — no runtime loading needed)
// ============================================================================
extern "C" {
    GV_GL_IMPORT void     APIENTRY glViewport(GLint x, GLint y, GLsizei w, GLsizei h);
    GV_GL_IMPORT void     APIENTRY glClear(GLbitfield mask);
    GV_GL_IMPORT void     APIENTRY glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a);
    GV_GL_IMPORT void     APIENTRY glEnable(GLenum cap);
    GV_GL_IMPORT void     APIENTRY glDisable(GLenum cap);
    GV_GL_IMPORT void     APIENTRY glBlendFunc(GLenum sfactor, GLenum dfactor);
    GV_GL_IMPORT void     APIENTRY glDepthFunc(GLenum func);
    GV_GL_IMPORT void     APIENTRY glDepthMask(GLboolean flag);
    GV_GL_IMPORT void     APIENTRY glCullFace(GLenum mode);
    GV_GL_IMPORT void     APIENTRY glFrontFace(GLenum mode);
    GV_GL_IMPORT void     APIENTRY glDrawArrays(GLenum mode, GLint first, GLsizei count);
    GV_GL_IMPORT void     APIENTRY glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices);
    GV_GL_IMPORT GLenum   APIENTRY glGetError(void);
    GV_GL_IMPORT const GLubyte* APIENTRY glGetString(GLenum name);
    GV_GL_IMPORT void     APIENTRY glGetIntegerv(GLenum pname, GLint* params);
    GV_GL_IMPORT void     APIENTRY glPolygonMode(GLenum face, GLenum mode);
    GV_GL_IMPORT void     APIENTRY glLineWidth(GLfloat width);
    GV_GL_IMPORT void     APIENTRY glPointSize(GLfloat size);
    GV_GL_IMPORT void     APIENTRY glPixelStorei(GLenum pname, GLint param);
    GV_GL_IMPORT void     APIENTRY glGenTextures(GLsizei n, GLuint* textures);
    GV_GL_IMPORT void     APIENTRY glDeleteTextures(GLsizei n, const GLuint* textures);
    GV_GL_IMPORT void     APIENTRY glBindTexture(GLenum target, GLuint texture);
    GV_GL_IMPORT void     APIENTRY glTexImage2D(GLenum target, GLint level, GLint internalformat,
                                                 GLsizei width, GLsizei height, GLint border,
                                                 GLenum format, GLenum type, const void* pixels);
    GV_GL_IMPORT void     APIENTRY glTexParameteri(GLenum target, GLenum pname, GLint param);
    GV_GL_IMPORT void     APIENTRY glScissor(GLint x, GLint y, GLsizei w, GLsizei h);
    GV_GL_IMPORT void     APIENTRY glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a);
    GV_GL_IMPORT void     APIENTRY glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h,
                                                 GLenum format, GLenum type, void* pixels);
    GV_GL_IMPORT void     APIENTRY glFinish(void);
    GV_GL_IMPORT void     APIENTRY glFlush(void);
    GV_GL_IMPORT GLboolean APIENTRY glIsEnabled(GLenum cap);
}

// ============================================================================
// GL 2.0+ / 3.3 Core Functions  (loaded at runtime via gvLoadGL)
// ============================================================================

// ── Function-pointer typedefs ──────────────────────────────────────────────

// Shaders
typedef GLuint (APIENTRY *PFN_glCreateShader)(GLenum type);
typedef void   (APIENTRY *PFN_glDeleteShader)(GLuint shader);
typedef void   (APIENTRY *PFN_glShaderSource)(GLuint shader, GLsizei count,
                                               const GLchar** string, const GLint* length);
typedef void   (APIENTRY *PFN_glCompileShader)(GLuint shader);
typedef void   (APIENTRY *PFN_glGetShaderiv)(GLuint shader, GLenum pname, GLint* params);
typedef void   (APIENTRY *PFN_glGetShaderInfoLog)(GLuint shader, GLsizei bufSize,
                                                    GLsizei* length, GLchar* infoLog);
// Programs
typedef GLuint (APIENTRY *PFN_glCreateProgram)(void);
typedef void   (APIENTRY *PFN_glDeleteProgram)(GLuint program);
typedef void   (APIENTRY *PFN_glAttachShader)(GLuint program, GLuint shader);
typedef void   (APIENTRY *PFN_glDetachShader)(GLuint program, GLuint shader);
typedef void   (APIENTRY *PFN_glLinkProgram)(GLuint program);
typedef void   (APIENTRY *PFN_glUseProgram)(GLuint program);
typedef void   (APIENTRY *PFN_glGetProgramiv)(GLuint program, GLenum pname, GLint* params);
typedef void   (APIENTRY *PFN_glGetProgramInfoLog)(GLuint program, GLsizei bufSize,
                                                     GLsizei* length, GLchar* infoLog);
// Uniforms
typedef GLint  (APIENTRY *PFN_glGetUniformLocation)(GLuint program, const GLchar* name);
typedef void   (APIENTRY *PFN_glUniform1i)(GLint location, GLint v0);
typedef void   (APIENTRY *PFN_glUniform1f)(GLint location, GLfloat v0);
typedef void   (APIENTRY *PFN_glUniform2f)(GLint location, GLfloat v0, GLfloat v1);
typedef void   (APIENTRY *PFN_glUniform3f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void   (APIENTRY *PFN_glUniform4f)(GLint location, GLfloat v0, GLfloat v1,
                                            GLfloat v2, GLfloat v3);
typedef void   (APIENTRY *PFN_glUniform3fv)(GLint location, GLsizei count, const GLfloat* v);
typedef void   (APIENTRY *PFN_glUniform4fv)(GLint location, GLsizei count, const GLfloat* v);
typedef void   (APIENTRY *PFN_glUniformMatrix4fv)(GLint location, GLsizei count,
                                                    GLboolean transpose, const GLfloat* value);

// Vertex Array Objects (GL 3.0)
typedef void   (APIENTRY *PFN_glGenVertexArrays)(GLsizei n, GLuint* arrays);
typedef void   (APIENTRY *PFN_glDeleteVertexArrays)(GLsizei n, const GLuint* arrays);
typedef void   (APIENTRY *PFN_glBindVertexArray)(GLuint array);

// Vertex Buffer Objects (GL 1.5 — but loaded dynamically on Windows)
typedef void   (APIENTRY *PFN_glGenBuffers)(GLsizei n, GLuint* buffers);
typedef void   (APIENTRY *PFN_glDeleteBuffers)(GLsizei n, const GLuint* buffers);
typedef void   (APIENTRY *PFN_glBindBuffer)(GLenum target, GLuint buffer);
typedef void   (APIENTRY *PFN_glBufferData)(GLenum target, GLsizeiptr size,
                                             const void* data, GLenum usage);
typedef void   (APIENTRY *PFN_glBufferSubData)(GLenum target, GLintptr offset,
                                                GLsizeiptr size, const void* data);

// Vertex attributes
typedef void   (APIENTRY *PFN_glVertexAttribPointer)(GLuint index, GLint size, GLenum type,
                                                       GLboolean normalized, GLsizei stride,
                                                       const void* pointer);
typedef void   (APIENTRY *PFN_glEnableVertexAttribArray)(GLuint index);
typedef void   (APIENTRY *PFN_glDisableVertexAttribArray)(GLuint index);

// Texture (GL 1.3+)
typedef void   (APIENTRY *PFN_glActiveTexture)(GLenum texture);
typedef void   (APIENTRY *PFN_glGenerateMipmap)(GLenum target);

// Framebuffer objects (GL 3.0)
typedef void   (APIENTRY *PFN_glGenFramebuffers)(GLsizei n, GLuint* ids);
typedef void   (APIENTRY *PFN_glDeleteFramebuffers)(GLsizei n, const GLuint* ids);
typedef void   (APIENTRY *PFN_glBindFramebuffer)(GLenum target, GLuint framebuffer);
typedef void   (APIENTRY *PFN_glFramebufferTexture2D)(GLenum target, GLenum attachment,
                                                       GLenum textarget, GLuint texture, GLint level);
typedef GLenum (APIENTRY *PFN_glCheckFramebufferStatus)(GLenum target);
typedef void   (APIENTRY *PFN_glGenRenderbuffers)(GLsizei n, GLuint* renderbuffers);
typedef void   (APIENTRY *PFN_glDeleteRenderbuffers)(GLsizei n, const GLuint* renderbuffers);
typedef void   (APIENTRY *PFN_glBindRenderbuffer)(GLenum target, GLuint renderbuffer);
typedef void   (APIENTRY *PFN_glRenderbufferStorage)(GLenum target, GLenum internalformat,
                                                      GLsizei width, GLsizei height);
typedef void   (APIENTRY *PFN_glFramebufferRenderbuffer)(GLenum target, GLenum attachment,
                                                          GLenum renderbuffertarget, GLuint renderbuffer);
typedef void   (APIENTRY *PFN_glBlitFramebuffer)(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                                                  GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                                                  GLbitfield mask, GLenum filter);

// ── Extern function pointers ───────────────────────────────────────────────

// Shaders
extern PFN_glCreateShader           glCreateShader;
extern PFN_glDeleteShader           glDeleteShader;
extern PFN_glShaderSource           glShaderSource;
extern PFN_glCompileShader          glCompileShader;
extern PFN_glGetShaderiv            glGetShaderiv;
extern PFN_glGetShaderInfoLog       glGetShaderInfoLog;

// Programs
extern PFN_glCreateProgram          glCreateProgram;
extern PFN_glDeleteProgram          glDeleteProgram;
extern PFN_glAttachShader           glAttachShader;
extern PFN_glDetachShader           glDetachShader;
extern PFN_glLinkProgram            glLinkProgram;
extern PFN_glUseProgram             glUseProgram;
extern PFN_glGetProgramiv           glGetProgramiv;
extern PFN_glGetProgramInfoLog      glGetProgramInfoLog;

// Uniforms
extern PFN_glGetUniformLocation     glGetUniformLocation;
extern PFN_glUniform1i              glUniform1i;
extern PFN_glUniform1f              glUniform1f;
extern PFN_glUniform2f              glUniform2f;
extern PFN_glUniform3f              glUniform3f;
extern PFN_glUniform4f              glUniform4f;
extern PFN_glUniform3fv             glUniform3fv;
extern PFN_glUniform4fv             glUniform4fv;
extern PFN_glUniformMatrix4fv       glUniformMatrix4fv;

// VAO
extern PFN_glGenVertexArrays        glGenVertexArrays;
extern PFN_glDeleteVertexArrays     glDeleteVertexArrays;
extern PFN_glBindVertexArray        glBindVertexArray;

// VBO
extern PFN_glGenBuffers             glGenBuffers;
extern PFN_glDeleteBuffers          glDeleteBuffers;
extern PFN_glBindBuffer             glBindBuffer;
extern PFN_glBufferData             glBufferData;
extern PFN_glBufferSubData          glBufferSubData;

// Vertex attributes
extern PFN_glVertexAttribPointer    glVertexAttribPointer;
extern PFN_glEnableVertexAttribArray  glEnableVertexAttribArray;
extern PFN_glDisableVertexAttribArray glDisableVertexAttribArray;

// Texture
extern PFN_glActiveTexture          glActiveTexture;
extern PFN_glGenerateMipmap         glGenerateMipmap;

// Framebuffer
extern PFN_glGenFramebuffers          glGenFramebuffers;
extern PFN_glDeleteFramebuffers       glDeleteFramebuffers;
extern PFN_glBindFramebuffer          glBindFramebuffer;
extern PFN_glFramebufferTexture2D     glFramebufferTexture2D;
extern PFN_glCheckFramebufferStatus   glCheckFramebufferStatus;
extern PFN_glGenRenderbuffers         glGenRenderbuffers;
extern PFN_glDeleteRenderbuffers      glDeleteRenderbuffers;
extern PFN_glBindRenderbuffer         glBindRenderbuffer;
extern PFN_glRenderbufferStorage      glRenderbufferStorage;
extern PFN_glFramebufferRenderbuffer  glFramebufferRenderbuffer;
extern PFN_glBlitFramebuffer          glBlitFramebuffer;

// ── Loader ─────────────────────────────────────────────────────────────────
/// Load all GL 2.0+ / 3.3 function pointers.
/// Must be called AFTER a valid OpenGL context is made current
/// (i.e. after glfwMakeContextCurrent).
/// Returns true if all critical functions were resolved.
bool gvLoadGL();

#endif // GV_HAS_GLFW
