// ============================================================================
// GameVoid Engine — Window Implementation
// ============================================================================
#include "core/Window.h"
#include "core/Types.h"
#include <string>
#include <vector>

#ifdef GV_HAS_GLFW

// Prevent GLFW from including any GL header (we use our own GLDefs.h)
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "core/GLDefs.h"   // for glViewport in resize callback

namespace gv {

// ── Lifecycle ──────────────────────────────────────────────────────────────

Window::~Window() { Shutdown(); }

bool Window::Init(u32 width, u32 height, const std::string& title) {
    if (m_Window) return true;  // already initialised

    if (!glfwInit()) {
        GV_LOG_FATAL("GLFW: glfwInit() failed.");
        return false;
    }

    // Request an OpenGL 3.3 Core context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 0);   // No MSAA on default FB (scene renders to its own FBO)

    m_Window = glfwCreateWindow(static_cast<int>(width),
                                static_cast<int>(height),
                                title.c_str(), nullptr, nullptr);
    if (!m_Window) {
        GV_LOG_FATAL("GLFW: failed to create window.");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_Window);
    glfwSwapInterval(1);   // v-sync on

    m_Width  = width;
    m_Height = height;

    // Store 'this' so static callbacks can access the Window instance
    glfwSetWindowUserPointer(m_Window, this);

    // Register callbacks
    glfwSetKeyCallback(m_Window, KeyCallback);
    glfwSetMouseButtonCallback(m_Window, MouseButtonCallback);
    glfwSetCursorPosCallback(m_Window, CursorPosCallback);
    glfwSetScrollCallback(m_Window, ScrollCallback);
    glfwSetFramebufferSizeCallback(m_Window, FramebufferSizeCallback);
    glfwSetDropCallback(m_Window, DropCallback);

    // Get initial mouse position
    glfwGetCursorPos(m_Window, &m_MouseX, &m_MouseY);
    m_LastMouseX = m_MouseX;
    m_LastMouseY = m_MouseY;

    GV_LOG_INFO("Window created (" + std::to_string(width) + "x"
                + std::to_string(height) + ")  —  " + title);
    return true;
}

void Window::Shutdown() {
    if (m_Window) {
        glfwDestroyWindow(m_Window);
        m_Window = nullptr;
        glfwTerminate();
        GV_LOG_INFO("Window destroyed.");
    }
}

// ── Frame ──────────────────────────────────────────────────────────────────

bool Window::ShouldClose() const {
    return m_Window ? glfwWindowShouldClose(m_Window) != 0 : true;
}

void Window::SetShouldClose(bool close) {
    if (m_Window) glfwSetWindowShouldClose(m_Window, close ? GLFW_TRUE : GLFW_FALSE);
}

void Window::PollEvents() {
    glfwPollEvents();
}

void Window::SwapBuffers() {
    if (m_Window) glfwSwapBuffers(m_Window);
}

void Window::BeginFrame() {
    // Snapshot previous-frame keys so IsKeyPressed / IsKeyReleased work
    for (int i = 0; i < MAX_KEYS; ++i)
        m_KeysPrev[i] = m_Keys[i];

    // Reset per-frame accumulators
    m_ScrollDelta = 0.0f;

    // Compute mouse delta from last frame
    m_LastMouseX = m_MouseX;
    m_LastMouseY = m_MouseY;
}

// ── Input queries ──────────────────────────────────────────────────────────

bool Window::IsKeyDown(i32 key) const {
    return (key >= 0 && key < MAX_KEYS) ? m_Keys[key] : false;
}

bool Window::IsKeyPressed(i32 key) const {
    return (key >= 0 && key < MAX_KEYS) ? (m_Keys[key] && !m_KeysPrev[key]) : false;
}

bool Window::IsKeyReleased(i32 key) const {
    return (key >= 0 && key < MAX_KEYS) ? (!m_Keys[key] && m_KeysPrev[key]) : false;
}

bool Window::IsMouseButtonDown(i32 button) const {
    return (button >= 0 && button < MAX_BUTTONS) ? m_MouseButtons[button] : false;
}

Vec2 Window::GetMousePosition() const {
    return { static_cast<f32>(m_MouseX), static_cast<f32>(m_MouseY) };
}

Vec2 Window::GetMouseDelta() const {
    return { static_cast<f32>(m_MouseX - m_LastMouseX),
             static_cast<f32>(m_MouseY - m_LastMouseY) };
}

f32 Window::GetScrollDelta() const { return m_ScrollDelta; }

// ── Accessors ──────────────────────────────────────────────────────────────

bool Window::IsInitialised() const { return m_Window != nullptr; }

void Window::SetTitle(const std::string& title) {
    if (m_Window) glfwSetWindowTitle(m_Window, title.c_str());
}

void Window::SetCursorCaptured(bool captured) {
    m_CursorCaptured = captured;
    if (m_Window) {
        if (captured) {
            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            // Reset deltas so the first frame doesn't have a huge jump
            glfwGetCursorPos(m_Window, &m_MouseX, &m_MouseY);
            m_LastMouseX = m_MouseX;
            m_LastMouseY = m_MouseY;
        } else {
            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
}

// ── GLFW Callbacks (static) ────────────────────────────────────────────────

void Window::KeyCallback(GLFWwindow* w, int key, int /*scancode*/, int action, int /*mods*/) {
    Window* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
    if (!self || key < 0 || key >= MAX_KEYS) return;
    if (action == GLFW_PRESS || action == GLFW_REPEAT)
        self->m_Keys[key] = true;
    else if (action == GLFW_RELEASE)
        self->m_Keys[key] = false;
}

void Window::MouseButtonCallback(GLFWwindow* w, int button, int action, int /*mods*/) {
    Window* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
    if (!self || button < 0 || button >= MAX_BUTTONS) return;
    self->m_MouseButtons[button] = (action == GLFW_PRESS);
}

void Window::CursorPosCallback(GLFWwindow* w, double xpos, double ypos) {
    Window* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
    if (!self) return;
    self->m_MouseX = xpos;
    self->m_MouseY = ypos;
}

void Window::ScrollCallback(GLFWwindow* w, double /*xoffset*/, double yoffset) {
    Window* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
    if (!self) return;
    self->m_ScrollDelta += static_cast<f32>(yoffset);
}

void Window::FramebufferSizeCallback(GLFWwindow* w, int width, int height) {
    Window* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
    // Ignore zero-size framebuffer (e.g. on minimize) to prevent division by zero
    if (width <= 0 || height <= 0) return;
    if (self) {
        self->m_Width  = static_cast<u32>(width);
        self->m_Height = static_cast<u32>(height);
    }
    glViewport(0, 0, width, height);
}

void Window::DropCallback(GLFWwindow* w, int count, const char** paths) {
    Window* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
    if (!self) return;
    for (int i = 0; i < count; ++i) {
        if (paths[i]) {
            self->m_DroppedFiles.emplace_back(paths[i]);
        }
    }
}

std::vector<std::string> Window::PollDroppedFiles() {
    std::vector<std::string> result;
    result.swap(m_DroppedFiles);
    return result;
}

} // namespace gv

// ============================================================================
#else  // !GV_HAS_GLFW — stub implementation for CLI-only builds
// ============================================================================

namespace gv {

Window::~Window() {}
bool Window::Init(u32, u32, const std::string&) {
    GV_LOG_WARN("Window: compiled without GLFW (no GV_HAS_GLFW). Window unavailable.");
    return false;
}
void Window::Shutdown()                     {}
bool Window::ShouldClose() const            { return true; }
void Window::SetShouldClose(bool)           {}
void Window::PollEvents()                   {}
void Window::SwapBuffers()                  {}
void Window::BeginFrame()                   {}
bool Window::IsKeyDown(i32) const           { return false; }
bool Window::IsKeyPressed(i32) const        { return false; }
bool Window::IsKeyReleased(i32) const       { return false; }
bool Window::IsMouseButtonDown(i32) const   { return false; }
Vec2 Window::GetMousePosition() const       { return {}; }
Vec2 Window::GetMouseDelta() const          { return {}; }
f32  Window::GetScrollDelta() const         { return 0; }
bool Window::IsInitialised() const          { return false; }
void Window::SetTitle(const std::string&)   {}
void Window::SetCursorCaptured(bool)         {}
std::vector<std::string> Window::PollDroppedFiles() { return {}; }

} // namespace gv

#endif // GV_HAS_GLFW
