// ============================================================================
// GameVoid Engine — Future-Ready Placeholders
// ============================================================================
// Stub classes for systems that will be fleshed out later:
//   • AnimationSystem     — skeletal & keyframe animation
//   • ShaderLibrary       — shader hot-reload & cache
//   • PostProcessing      — fullscreen effects pipeline
//   • AudioEngine         — sound playback & spatial audio
//   • NetworkManager      — multiplayer / replication
//   • InputManager        — keyboard, mouse, gamepad abstraction
//   • ParticleSystem      — GPU particle emitters
//   • UISystem            — in-game UI / HUD
// ============================================================================
#pragma once

#include "core/Types.h"
#include "core/Math.h"
#include "core/Component.h"
#include <string>
#include <vector>

namespace gv {

// ============================================================================
// Animation System (placeholder)
// ============================================================================
/// Represents a single animation clip (e.g. "walk", "idle", "attack").
struct AnimationClip {
    std::string name;
    f32 duration   = 1.0f;     // seconds
    bool looping   = true;
    // In production: keyframe data, bone indices, etc.
};

/// Component that plays AnimationClips on the owning GameObject's mesh.
class Animator : public Component {
public:
    std::string GetTypeName() const override { return "Animator"; }

    void Play(const std::string& clipName);
    void Stop();
    void Pause();
    void SetSpeed(f32 speed);

    void AddClip(const AnimationClip& clip) { m_Clips.push_back(clip); }

    void OnUpdate(f32 dt) override {
        // TODO: advance playback time, interpolate keyframes
        (void)dt;
    }

private:
    std::vector<AnimationClip> m_Clips;
    i32  m_CurrentClip = -1;
    f32  m_PlaybackTime = 0.0f;
    f32  m_Speed = 1.0f;
    bool m_Playing = false;
};

// ============================================================================
// Shader Library (placeholder)
// ============================================================================
class ShaderLibrary {
public:
    /// Load a shader from files and cache it under the given name.
    bool Load(const std::string& name, const std::string& vertPath, const std::string& fragPath);

    /// Retrieve a cached shader.
    // Shader* Get(const std::string& name);

    /// Reload all shaders from disk (hot-reload support).
    void ReloadAll();

private:
    // std::unordered_map<std::string, Unique<Shader>> m_Shaders;
};

// ============================================================================
// Post-Processing Pipeline (placeholder)
// ============================================================================
class PostProcessing {
public:
    void Init(u32 width, u32 height);
    void Resize(u32 width, u32 height);

    /// Add a named post-process effect (bloom, SSAO, tone-mapping, …).
    void AddEffect(const std::string& effectName);
    void RemoveEffect(const std::string& effectName);

    /// Run the full chain on the current framebuffer.
    void Apply();

private:
    std::vector<std::string> m_Effects;
    u32 m_FBO = 0;      // framebuffer object
    u32 m_ColorTex = 0;  // colour attachment
    u32 m_DepthRBO = 0; // depth renderbuffer
};

// ============================================================================
// Audio Engine (placeholder)
// ============================================================================
class AudioEngine {
public:
    bool Init();
    void Shutdown();

    /// Play a 2D sound effect (fire-and-forget).
    void PlaySound(const std::string& path, f32 volume = 1.0f);

    /// Play spatial 3D audio at a world position.
    void PlaySound3D(const std::string& path, const Vec3& position, f32 volume = 1.0f);

    /// Set the listener position (usually the camera).
    void SetListenerPosition(const Vec3& position, const Vec3& forward, const Vec3& up);

    void SetMasterVolume(f32 volume);
};

/// Component for attaching a looping audio source to a GameObject.
class AudioSource : public Component {
public:
    std::string clipPath;
    f32 volume    = 1.0f;
    f32 pitch     = 1.0f;
    f32 minDist   = 1.0f;
    f32 maxDist   = 50.0f;
    bool loop     = false;
    bool playOnStart = false;
    bool spatial   = true;

    std::string GetTypeName() const override { return "AudioSource"; }
    void OnStart() override  { /* if (playOnStart) play(); */ }
    void OnUpdate(f32) override { /* update spatial position */ }
};

// ============================================================================
// Network Manager (placeholder)
// ============================================================================
class NetworkManager {
public:
    bool StartServer(u16 port);
    bool ConnectToServer(const std::string& address, u16 port);
    void Disconnect();
    void SendMessage(const std::string& channel, const std::string& data);
    void Poll();     // process incoming packets
    bool IsConnected() const { return m_Connected; }

private:
    bool m_Connected = false;
};

// ============================================================================
// Input Manager (placeholder)
// ============================================================================
class InputManager {
public:
    void Init();
    void Update();   // poll events each frame

    bool IsKeyDown(i32 keyCode) const;
    bool IsKeyPressed(i32 keyCode) const;   // single-frame press
    bool IsKeyReleased(i32 keyCode) const;

    bool IsMouseButtonDown(i32 button) const;
    Vec2 GetMousePosition() const;
    Vec2 GetMouseDelta() const;
    f32  GetMouseScrollDelta() const;

    // Gamepad
    bool IsGamepadConnected(i32 index) const;
    f32  GetGamepadAxis(i32 index, i32 axis) const;
    bool IsGamepadButtonDown(i32 index, i32 button) const;

private:
    // key state arrays, mouse state, etc.
};

// ============================================================================
// Particle System (placeholder)
// ============================================================================
struct ParticleProperties {
    Vec3 position{ 0, 0, 0 };
    Vec3 velocity{ 0, 1, 0 };
    Vec4 colorBegin{ 1, 1, 1, 1 };
    Vec4 colorEnd{ 1, 1, 1, 0 };
    f32  sizeBegin  = 0.5f;
    f32  sizeEnd    = 0.0f;
    f32  lifetime   = 1.0f;
};

class ParticleEmitter : public Component {
public:
    ParticleProperties properties;
    u32 maxParticles = 1000;
    f32 emissionRate = 100.0f;   // particles per second

    std::string GetTypeName() const override { return "ParticleEmitter"; }

    void OnUpdate(f32 dt) override {
        // TODO: emit new particles, update alive ones, remove dead
        (void)dt;
    }

    void OnRender() override {
        // TODO: draw particles as billboarded quads or point sprites
    }
};

// ============================================================================
// UI System (placeholder)
// ============================================================================
class UISystem {
public:
    void Init(u32 screenWidth, u32 screenHeight);
    void Shutdown();

    /// Begin a new UI frame (call before any widget functions).
    void BeginFrame();
    void EndFrame();

    // Immediate-mode widgets (like Dear ImGui)
    void Label(const std::string& text, f32 x, f32 y);
    void Button(const std::string& label, f32 x, f32 y, f32 w, f32 h);
    void ProgressBar(f32 fraction, f32 x, f32 y, f32 w, f32 h);

private:
    u32 m_ScreenWidth = 0, m_ScreenHeight = 0;
};

} // namespace gv
