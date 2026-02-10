// ============================================================================
// GameVoid Engine — Window (GLFW Wrapper)
// ============================================================================
// Manages the application window, OpenGL context, and low-level input state.
// When compiled without GV_HAS_GLFW every method is a harmless stub so the
// CLI-only build still compiles.
// ============================================================================
#pragma once

#include "core/Types.h"
#include "core/Math.h"
#include <string>

// ── Key codes  (values match GLFW — use these to avoid including GLFW) ─────
namespace gv { namespace GVKey {
    const i32 Space      = 32;
    const i32 Apostrophe = 39;
    const i32 Comma      = 44;
    const i32 Minus      = 45;
    const i32 Period     = 46;
    const i32 Slash      = 47;
    const i32 Key0 = 48; const i32 Key1 = 49; const i32 Key2 = 50;
    const i32 Key3 = 51; const i32 Key4 = 52; const i32 Key5 = 53;
    const i32 Key6 = 54; const i32 Key7 = 55; const i32 Key8 = 56;
    const i32 Key9 = 57;
    const i32 A = 65; const i32 B = 66; const i32 C = 67; const i32 D = 68;
    const i32 E = 69; const i32 F = 70; const i32 G = 71; const i32 H = 72;
    const i32 I = 73; const i32 J = 74; const i32 K = 75; const i32 L = 76;
    const i32 M = 77; const i32 N = 78; const i32 O = 79; const i32 P = 80;
    const i32 Q = 81; const i32 R = 82; const i32 S = 83; const i32 T = 84;
    const i32 U = 85; const i32 V = 86; const i32 W = 87; const i32 X = 88;
    const i32 Y = 89; const i32 Z = 90;
    const i32 Escape    = 256;
    const i32 Enter     = 257;
    const i32 Tab       = 258;
    const i32 Backspace = 259;
    const i32 Insert    = 260;
    const i32 Delete    = 261;
    const i32 Right     = 262;
    const i32 Left      = 263;
    const i32 Down      = 264;
    const i32 Up        = 265;
    const i32 F1 = 290; const i32 F2 = 291; const i32 F3 = 292;
    const i32 F4 = 293; const i32 F5 = 294; const i32 F6 = 295;
    const i32 F7 = 296; const i32 F8 = 297; const i32 F9 = 298;
    const i32 F10 = 299; const i32 F11 = 300; const i32 F12 = 301;
    const i32 LeftShift   = 340;
    const i32 LeftControl = 341;
    const i32 LeftAlt     = 342;
}}

// ── Mouse button codes ─────────────────────────────────────────────────────
namespace gv { namespace GVMouse {
    const i32 Left   = 0;
    const i32 Right  = 1;
    const i32 Middle = 2;
}}

// Forward-declare the opaque GLFW type so we don't leak the header
#ifdef GV_HAS_GLFW
struct GLFWwindow;
#endif

namespace gv {

class Window {
public:
    Window() = default;
    ~Window();

    // ── Lifecycle ──────────────────────────────────────────────────────────
    bool Init(u32 width, u32 height, const std::string& title);
    void Shutdown();

    // ── Frame ──────────────────────────────────────────────────────────────
    bool ShouldClose() const;
    void SetShouldClose(bool close);
    void PollEvents();
    void SwapBuffers();
    /// Call at the start of each frame to snapshot previous-frame input state.
    void BeginFrame();

    // ── Input queries ──────────────────────────────────────────────────────
    bool IsKeyDown(i32 key) const;
    bool IsKeyPressed(i32 key) const;   // true on the frame the key goes down
    bool IsKeyReleased(i32 key) const;  // true on the frame the key goes up

    bool IsMouseButtonDown(i32 button) const;
    Vec2 GetMousePosition() const;
    Vec2 GetMouseDelta() const;
    f32  GetScrollDelta() const;

    // ── Accessors ──────────────────────────────────────────────────────────
    u32  GetWidth()  const { return m_Width; }
    u32  GetHeight() const { return m_Height; }
    bool IsInitialised() const;
    void SetTitle(const std::string& title);

#ifdef GV_HAS_GLFW
    GLFWwindow* GetNativeWindow() const { return m_Window; }
#endif

private:
#ifdef GV_HAS_GLFW
    GLFWwindow* m_Window = nullptr;

    static const int MAX_KEYS    = 512;
    static const int MAX_BUTTONS = 8;

    bool m_Keys[MAX_KEYS]           = {};
    bool m_KeysPrev[MAX_KEYS]       = {};
    bool m_MouseButtons[MAX_BUTTONS]= {};

    f64  m_MouseX = 0.0, m_MouseY = 0.0;
    f64  m_LastMouseX = 0.0, m_LastMouseY = 0.0;
    f32  m_ScrollDelta = 0.0f;

    // Static GLFW callbacks (retrieve Window* via glfwGetWindowUserPointer)
    static void KeyCallback(GLFWwindow* w, int key, int scancode, int action, int mods);
    static void MouseButtonCallback(GLFWwindow* w, int button, int action, int mods);
    static void CursorPosCallback(GLFWwindow* w, double xpos, double ypos);
    static void ScrollCallback(GLFWwindow* w, double xoffset, double yoffset);
    static void FramebufferSizeCallback(GLFWwindow* w, int width, int height);
#endif

    u32  m_Width  = 0;
    u32  m_Height = 0;
};

} // namespace gv
