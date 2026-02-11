// ============================================================================
// GameVoid Engine — Future Placeholders Implementation
// ============================================================================
#include "future/Placeholders.h"
#include <algorithm>

namespace gv {

// Animator — removed; see animation/Animation.cpp for full implementation

// ── Shader Library ─────────────────────────────────────────────────────────
bool ShaderLibrary::Load(const std::string& name, const std::string& vertPath,
                         const std::string& fragPath) {
    (void)name; (void)vertPath; (void)fragPath;
    GV_LOG_INFO("ShaderLibrary — loaded '" + name + "' (placeholder).");
    return true;
}

void ShaderLibrary::ReloadAll() {
    GV_LOG_INFO("ShaderLibrary — reload all (placeholder).");
}

// ── Post-Processing ────────────────────────────────────────────────────────
void PostProcessing::Init(u32 width, u32 height) {
    (void)width; (void)height;
    GV_LOG_INFO("PostProcessing pipeline initialised (placeholder).");
}

void PostProcessing::Resize(u32 width, u32 height) { (void)width; (void)height; }

void PostProcessing::AddEffect(const std::string& effectName) {
    m_Effects.push_back(effectName);
    GV_LOG_INFO("PostProcessing — added effect: " + effectName);
}

void PostProcessing::RemoveEffect(const std::string& effectName) {
    m_Effects.erase(std::remove(m_Effects.begin(), m_Effects.end(), effectName), m_Effects.end());
}

void PostProcessing::Apply() {
    // Bind FBO, run each effect pass, blit to screen.
}

// ── Audio Engine ───────────────────────────────────────────────────────────
bool AudioEngine::Init() { GV_LOG_INFO("AudioEngine initialised (placeholder)."); return true; }
void AudioEngine::Shutdown() { GV_LOG_INFO("AudioEngine shut down."); }
void AudioEngine::PlaySound(const std::string& path, f32 volume) { (void)path; (void)volume; }
void AudioEngine::PlaySound3D(const std::string& path, const Vec3& pos, f32 volume) {
    (void)path; (void)pos; (void)volume;
}
void AudioEngine::SetListenerPosition(const Vec3& pos, const Vec3& fwd, const Vec3& up) {
    (void)pos; (void)fwd; (void)up;
}
void AudioEngine::SetMasterVolume(f32 volume) { (void)volume; }

// ── Network Manager ────────────────────────────────────────────────────────
bool NetworkManager::StartServer(u16 port)  { (void)port; return false; }
bool NetworkManager::ConnectToServer(const std::string& addr, u16 port) {
    (void)addr; (void)port; return false;
}
void NetworkManager::Disconnect() { m_Connected = false; }
void NetworkManager::SendMessage(const std::string& ch, const std::string& data) {
    (void)ch; (void)data;
}
void NetworkManager::Poll() {}

// ── Input Manager ──────────────────────────────────────────────────────────
void InputManager::Init()   {}
void InputManager::Update() {}
bool InputManager::IsKeyDown(i32 keyCode) const     { (void)keyCode; return false; }
bool InputManager::IsKeyPressed(i32 keyCode) const   { (void)keyCode; return false; }
bool InputManager::IsKeyReleased(i32 keyCode) const  { (void)keyCode; return false; }
bool InputManager::IsMouseButtonDown(i32 btn) const   { (void)btn; return false; }
Vec2 InputManager::GetMousePosition() const  { return {}; }
Vec2 InputManager::GetMouseDelta() const     { return {}; }
f32  InputManager::GetMouseScrollDelta() const { return 0; }
bool InputManager::IsGamepadConnected(i32 idx) const { (void)idx; return false; }
f32  InputManager::GetGamepadAxis(i32 idx, i32 axis) const { (void)idx; (void)axis; return 0; }
bool InputManager::IsGamepadButtonDown(i32 idx, i32 btn) const { (void)idx; (void)btn; return false; }

// ── UI System ──────────────────────────────────────────────────────────────
void UISystem::Init(u32 w, u32 h) { m_ScreenWidth = w; m_ScreenHeight = h; }
void UISystem::Shutdown() {}
void UISystem::BeginFrame() {}
void UISystem::EndFrame()   {}
void UISystem::Label(const std::string& text, f32 x, f32 y) { (void)text; (void)x; (void)y; }
void UISystem::Button(const std::string& label, f32 x, f32 y, f32 w, f32 h) {
    (void)label; (void)x; (void)y; (void)w; (void)h;
}
void UISystem::ProgressBar(f32 frac, f32 x, f32 y, f32 w, f32 h) {
    (void)frac; (void)x; (void)y; (void)w; (void)h;
}

} // namespace gv
