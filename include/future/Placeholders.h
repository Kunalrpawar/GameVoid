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
#include <unordered_map>

namespace gv {

// Animation System — see animation/Animation.h for full implementation

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
// Audio Engine (miniaudio-backed)
// ============================================================================
class AudioEngine {
public:
    bool Init();
    void Shutdown();

    /// Play a 2D sound effect (fire-and-forget).
    void PlaySound(const std::string& path, f32 volume = 1.0f);

    /// Play spatial 3D audio at a world position.
    void PlaySound3D(const std::string& path, const Vec3& position, f32 volume = 1.0f);

    /// Play a looping background music track. Stops previous music.
    void PlayMusic(const std::string& path, f32 volume = 1.0f);

    /// Stop the currently playing music.
    void StopMusic();

    /// Set the listener position (usually the camera).
    void SetListenerPosition(const Vec3& position, const Vec3& forward, const Vec3& up);

    void SetMasterVolume(f32 volume);
    f32  GetMasterVolume() const { return m_MasterVolume; }
    bool IsInitialised() const { return m_Initialised; }

    /// Get the raw miniaudio engine handle (ma_engine*) for AudioSource use.
    void* GetEngineHandle() const { return m_Engine; }

    /// Stop all currently playing sounds.
    void StopAll();

    /// Update audio sources (call once per frame to clean up finished sounds).
    void Update();

private:
    bool m_Initialised = false;
    f32  m_MasterVolume = 1.0f;
    // In production: ma_engine* m_Engine — see Placeholders.cpp for real impl
    void* m_Engine = nullptr;
    void* m_MusicSound = nullptr;   // pointer to ma_sound for music track
};

/// Component for attaching an audio source to a GameObject.
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
    bool isPlaying = false;

    std::string GetTypeName() const override { return "AudioSource"; }
    void OnStart() override;
    void OnUpdate(f32 dt) override;
    void OnDetach() override;

    void Play();
    void Stop();

    /// Set the AudioEngine so this component can init sounds.
    void SetAudioEngine(AudioEngine* engine) { m_AudioEngine = engine; }
    AudioEngine* GetAudioEngine() const      { return m_AudioEngine; }

private:
    void* m_Sound = nullptr;   // pointer to ma_sound
    AudioEngine* m_AudioEngine = nullptr;
};

// ============================================================================
// Network Manager (Winsock TCP client-server)
// ============================================================================
class NetworkManager {
public:
    bool StartServer(u16 port);
    bool ConnectToServer(const std::string& address, u16 port);
    void Disconnect();

    /// Send a message on a named channel.
    void SendMessage(const std::string& channel, const std::string& data);

    /// Process incoming packets (call once per frame).
    void Poll();

    /// Broadcast a message to all connected clients (server only).
    void Broadcast(const std::string& channel, const std::string& data);

    bool IsConnected() const { return m_Connected; }
    bool IsServer()    const { return m_IsServer; }

    /// Get received messages since last Poll().
    struct NetMessage {
        std::string channel;
        std::string data;
        u32 senderID = 0;
    };
    const std::vector<NetMessage>& GetMessages() const { return m_Messages; }

    /// Get number of connected clients (server only).
    u32 GetClientCount() const { return static_cast<u32>(m_ClientSockets.size()); }

    void Shutdown();

private:
    bool m_Connected = false;
    bool m_IsServer  = false;
    u64  m_ServerSocket = 0;
    u64  m_ClientSocket = 0;
    std::vector<u64> m_ClientSockets;   // connected clients (server)
    std::vector<NetMessage> m_Messages; // incoming messages
    bool m_WinsockInit = false;
};

// ============================================================================
// Input Manager — keyboard, mouse, gamepad abstraction with action mapping
// ============================================================================

// Forward declaration
class Window;

/// Gamepad axis constants (match GLFW gamepad axis indices)
namespace GVGamepad {
    const i32 LeftStickX  = 0;
    const i32 LeftStickY  = 1;
    const i32 RightStickX = 2;
    const i32 RightStickY = 3;
    const i32 LeftTrigger = 4;
    const i32 RightTrigger= 5;

    const i32 ButtonA     = 0;
    const i32 ButtonB     = 1;
    const i32 ButtonX     = 2;
    const i32 ButtonY     = 3;
    const i32 BumperLeft  = 4;
    const i32 BumperRight = 5;
    const i32 Back        = 6;
    const i32 Start       = 7;
    const i32 Guide       = 8;
    const i32 StickLeft   = 9;
    const i32 StickRight  = 10;
    const i32 DPadUp      = 11;
    const i32 DPadRight   = 12;
    const i32 DPadDown    = 13;
    const i32 DPadLeft    = 14;
}

class InputManager {
public:
    void Init(Window* window = nullptr);
    void Update();   // poll events each frame

    /// Set the window to delegate input queries to. 
    void SetWindow(Window* window) { m_Window = window; }

    // ── Keyboard ───────────────────────────────────────────────────────────
    bool IsKeyDown(i32 keyCode) const;
    bool IsKeyPressed(i32 keyCode) const;   // single-frame press
    bool IsKeyReleased(i32 keyCode) const;

    // ── Mouse ──────────────────────────────────────────────────────────────
    bool IsMouseButtonDown(i32 button) const;
    Vec2 GetMousePosition() const;
    Vec2 GetMouseDelta() const;
    f32  GetMouseScrollDelta() const;

    // ── Gamepad ────────────────────────────────────────────────────────────
    bool IsGamepadConnected(i32 index = 0) const;
    f32  GetGamepadAxis(i32 index, i32 axis) const;
    bool IsGamepadButtonDown(i32 index, i32 button) const;
    bool IsGamepadButtonPressed(i32 index, i32 button) const;
    void SetDeadzone(f32 deadzone) { m_Deadzone = deadzone; }
    f32  GetDeadzone() const       { return m_Deadzone; }

    // ── Action Mapping ─────────────────────────────────────────────────────
    /// Bind a key to a named action ("jump", "fire", "move_forward", etc.)
    void BindAction(const std::string& action, i32 keyCode);
    /// Bind a gamepad button to a named action.
    void BindGamepadAction(const std::string& action, i32 button);
    /// Query an action by name (checks all bound keys and gamepad buttons).
    bool IsActionDown(const std::string& action) const;
    bool IsActionPressed(const std::string& action) const;

    /// Bind a key or axis to a named axis mapping ("move_x", "move_y", etc.)
    /// For keys: negative key adds -1, positive key adds +1.
    void BindAxis(const std::string& axisName, i32 negativeKey, i32 positiveKey);
    /// Bind a gamepad axis to a named axis mapping.
    void BindGamepadAxis(const std::string& axisName, i32 gpIndex, i32 gpAxis);
    /// Query the current value of a named axis (returns -1..+1).
    f32  GetAxis(const std::string& axisName) const;

private:
    Window* m_Window = nullptr;
    f32 m_Deadzone = 0.15f;

    // Gamepad previous-frame state for press detection
    static const int MAX_GP_BUTTONS = 16;
    bool m_GPButtons[MAX_GP_BUTTONS] = {};
    bool m_GPButtonsPrev[MAX_GP_BUTTONS] = {};

    // Action bindings
    struct ActionBinding {
        std::vector<i32> keys;        // keyboard keys
        std::vector<i32> gpButtons;   // gamepad buttons
    };
    std::unordered_map<std::string, ActionBinding> m_Actions;

    // Axis bindings
    struct AxisBinding {
        i32 negKey = -1, posKey = -1;     // keyboard axis (neg/pos pair)
        i32 gpIndex = -1, gpAxis = -1;    // gamepad axis
    };
    std::unordered_map<std::string, AxisBinding> m_Axes;
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
