// ============================================================================
// GameVoid Engine — Animation System Implementation
// ============================================================================
#include "animation/Animation.h"
#include "core/GameObject.h"
#include "core/Transform.h"
#include <algorithm>
#include <cmath>

namespace gv {

// ============================================================================
// AnimationClip
// ============================================================================
void AnimationClip::AddKeyframe(const Keyframe& kf) {
    m_Keyframes.push_back(kf);
    SortKeyframes();
    // Auto-extend duration
    if (kf.time > m_Duration) m_Duration = kf.time;
}

void AnimationClip::RemoveKeyframe(u32 index) {
    if (index < m_Keyframes.size()) {
        m_Keyframes.erase(m_Keyframes.begin() + static_cast<long>(index));
    }
}

void AnimationClip::SortKeyframes() {
    std::sort(m_Keyframes.begin(), m_Keyframes.end(),
              [](const Keyframe& a, const Keyframe& b) { return a.time < b.time; });
}

static Vec3 LerpVec3(Vec3 a, Vec3 b, f32 t) {
    return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
}

static Quaternion SlerpQuat(Quaternion a, Quaternion b, f32 t) {
    f32 dot = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
    if (dot < 0) { b.w = -b.w; b.x = -b.x; b.y = -b.y; b.z = -b.z; dot = -dot; }
    if (dot > 0.9995f) {
        return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                 a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t };
    }
    f32 theta = std::acos(dot);
    f32 sinTheta = std::sin(theta);
    f32 wa = std::sin((1 - t) * theta) / sinTheta;
    f32 wb = std::sin(t * theta) / sinTheta;
    return { a.x * wa + b.x * wb, a.y * wa + b.y * wb,
             a.z * wa + b.z * wb, a.w * wa + b.w * wb };
}

Keyframe AnimationClip::Sample(f32 t) const {
    if (m_Keyframes.empty()) return {};
    if (m_Keyframes.size() == 1) return m_Keyframes[0];

    // Wrap time
    f32 dur = m_Duration;
    if (dur <= 0) dur = 0.001f;
    if (m_Loop && t > dur) t = std::fmod(t, dur);
    if (t <= m_Keyframes.front().time) return m_Keyframes.front();
    if (t >= m_Keyframes.back().time) return m_Keyframes.back();

    // Find surrounding keyframes
    for (size_t i = 0; i + 1 < m_Keyframes.size(); ++i) {
        const Keyframe& k0 = m_Keyframes[i];
        const Keyframe& k1 = m_Keyframes[i + 1];
        if (t >= k0.time && t <= k1.time) {
            f32 seg = k1.time - k0.time;
            f32 frac = (seg > 0.0001f) ? (t - k0.time) / seg : 0.0f;

            Keyframe result;
            result.time = t;
            switch (m_Interp) {
            case InterpMode::Step:
                result.position = k0.position;
                result.rotation = k0.rotation;
                result.scale    = k0.scale;
                break;
            case InterpMode::CubicSmooth: {
                // Smoothstep
                f32 s = frac * frac * (3 - 2 * frac);
                result.position = LerpVec3(k0.position, k1.position, s);
                result.rotation = SlerpQuat(k0.rotation, k1.rotation, s);
                result.scale    = LerpVec3(k0.scale, k1.scale, s);
                break;
            }
            case InterpMode::Linear:
            default:
                result.position = LerpVec3(k0.position, k1.position, frac);
                result.rotation = SlerpQuat(k0.rotation, k1.rotation, frac);
                result.scale    = LerpVec3(k0.scale, k1.scale, frac);
                break;
            }
            return result;
        }
    }

    return m_Keyframes.back();
}

// ============================================================================
// Animator
// ============================================================================
void Animator::AddClip(const AnimationClip& clip) {
    m_Clips[clip.GetName()] = clip;
}

void Animator::RemoveClip(const std::string& name) {
    m_Clips.erase(name);
    if (m_CurrentClip == name) Stop();
}

AnimationClip* Animator::GetClip(const std::string& name) {
    auto it = m_Clips.find(name);
    return (it != m_Clips.end()) ? &it->second : nullptr;
}

void Animator::Play(const std::string& clipName) {
    auto it = m_Clips.find(clipName);
    if (it == m_Clips.end()) return;
    m_CurrentClip = clipName;
    m_Time = 0;
    m_Playing = true;
    m_Paused = false;
    m_Blending = false;
}

void Animator::Stop() {
    m_Playing = false;
    m_Paused  = false;
    m_Time    = 0;
    m_Blending = false;
}

void Animator::Pause()  { m_Paused = true; }
void Animator::Resume() { m_Paused = false; }

void Animator::CrossFade(const std::string& toClip, f32 transitionTime) {
    auto it = m_Clips.find(toClip);
    if (it == m_Clips.end()) return;
    m_Blend.fromClip = m_CurrentClip;
    m_Blend.toClip = toClip;
    m_Blend.transitionTime = transitionTime;
    m_Blend.blendProgress = 0;
    m_Blending = true;
}

void Animator::OnUpdate(f32 dt) {
    if (!m_Playing || m_Paused || !Owner) return;

    m_Time += dt * m_Speed;

    Transform* tr = Owner->GetTransform();
    if (!tr) return;

    if (m_Blending) {
        m_Blend.blendProgress += dt / m_Blend.transitionTime;
        if (m_Blend.blendProgress >= 1.0f) {
            m_Blend.blendProgress = 1.0f;
            m_CurrentClip = m_Blend.toClip;
            m_Blending = false;
        }
        auto* clipA = GetClip(m_Blend.fromClip);
        auto* clipB = GetClip(m_Blend.toClip);
        if (clipA && clipB) {
            Keyframe kA = clipA->Sample(m_Time);
            Keyframe kB = clipB->Sample(m_Time);
            f32 t = m_Blend.blendProgress;
            tr->position = LerpVec3(kA.position, kB.position, t);
            tr->rotation = SlerpQuat(kA.rotation, kB.rotation, t);
            tr->scale    = LerpVec3(kA.scale, kB.scale, t);
        }
    } else {
        auto* clip = GetClip(m_CurrentClip);
        if (clip) {
            Keyframe k = clip->Sample(m_Time);
            tr->position = k.position;
            tr->rotation = k.rotation;
            tr->scale    = k.scale;
        }
    }
}

// ============================================================================
// AnimationLibrary
// ============================================================================
AnimationClip* AnimationLibrary::CreateClip(const std::string& name) {
    m_Clips[name] = AnimationClip(name);
    return &m_Clips[name];
}

AnimationClip* AnimationLibrary::GetClip(const std::string& name) {
    auto it = m_Clips.find(name);
    return (it != m_Clips.end()) ? &it->second : nullptr;
}

} // namespace gv
