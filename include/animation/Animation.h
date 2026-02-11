// ============================================================================
// GameVoid Engine — Animation System
// ============================================================================
// Keyframe-based animation with clips, blend states, and a timeline.
// Supports transform animation (position, rotation, scale) on GameObjects.
// ============================================================================
#pragma once

#include "core/Types.h"
#include "core/Math.h"
#include "core/Component.h"
#include <vector>
#include <string>
#include <map>

namespace gv {

// ── Keyframe ───────────────────────────────────────────────────────────────
/// A single keyframe storing a transform snapshot at a given time.
struct Keyframe {
    f32  time = 0.0f;          // seconds from clip start
    Vec3 position { 0, 0, 0 };
    Quaternion rotation;
    Vec3 scale { 1, 1, 1 };
};

// ── Interpolation Modes ────────────────────────────────────────────────────
enum class InterpMode { Linear, Step, CubicSmooth };

// ── Animation Clip ─────────────────────────────────────────────────────────
/// A named sequence of keyframes that can be played on a GameObject.
class AnimationClip {
public:
    AnimationClip(const std::string& name = "Clip") : m_Name(name) {}

    void AddKeyframe(const Keyframe& kf);
    void RemoveKeyframe(u32 index);
    void SortKeyframes();

    /// Sample the clip at time t (loops if wrap).
    Keyframe Sample(f32 t) const;

    // ── Accessors ──────────────────────────────────────────────────────────
    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& n) { m_Name = n; }
    f32 GetDuration() const { return m_Duration; }
    void SetDuration(f32 d) { m_Duration = d; }
    bool IsLooping() const { return m_Loop; }
    void SetLooping(bool l) { m_Loop = l; }
    InterpMode GetInterpMode() const { return m_Interp; }
    void SetInterpMode(InterpMode m) { m_Interp = m; }

    const std::vector<Keyframe>& GetKeyframes() const { return m_Keyframes; }
    std::vector<Keyframe>& GetKeyframesMut() { return m_Keyframes; }

private:
    std::string m_Name;
    std::vector<Keyframe> m_Keyframes;
    f32 m_Duration = 2.0f;
    bool m_Loop = true;
    InterpMode m_Interp = InterpMode::Linear;
};

// ── Blend State ────────────────────────────────────────────────────────────
/// Describes how two clips blend during a transition.
struct BlendState {
    std::string fromClip;
    std::string toClip;
    f32 transitionTime = 0.3f;    // seconds to cross-fade
    f32 blendProgress  = 0.0f;    // 0 = fully from, 1 = fully to
};

// ── Animator Component ─────────────────────────────────────────────────────
/// Attach to a GameObject to animate its Transform using clips.
class Animator : public Component {
public:
    Animator() = default;
    ~Animator() override = default;

    std::string GetTypeName() const override { return "Animator"; }

    void OnUpdate(f32 dt) override;

    // ── Clip management ────────────────────────────────────────────────────
    void AddClip(const AnimationClip& clip);
    void RemoveClip(const std::string& name);
    AnimationClip* GetClip(const std::string& name);
    const std::map<std::string, AnimationClip>& GetClips() const { return m_Clips; }

    // ── Playback ───────────────────────────────────────────────────────────
    void Play(const std::string& clipName);
    void Stop();
    void Pause();
    void Resume();
    void CrossFade(const std::string& toClip, f32 transitionTime = 0.3f);

    bool IsPlaying() const { return m_Playing; }
    f32  GetTime() const { return m_Time; }
    void SetTime(f32 t) { m_Time = t; }
    f32  GetSpeed() const { return m_Speed; }
    void SetSpeed(f32 s) { m_Speed = s; }

    const std::string& GetCurrentClipName() const { return m_CurrentClip; }

private:
    std::map<std::string, AnimationClip> m_Clips;
    std::string m_CurrentClip;
    f32 m_Time     = 0.0f;
    f32 m_Speed    = 1.0f;
    bool m_Playing = false;
    bool m_Paused  = false;

    // Blending
    BlendState m_Blend;
    bool m_Blending = false;
};

// ── Animation Library ──────────────────────────────────────────────────────
class AnimationLibrary {
public:
    AnimationClip* CreateClip(const std::string& name);
    AnimationClip* GetClip(const std::string& name);
    const std::map<std::string, AnimationClip>& GetAll() const { return m_Clips; }

private:
    std::map<std::string, AnimationClip> m_Clips;
};

} // namespace gv
