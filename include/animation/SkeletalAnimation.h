// ============================================================================
// GameVoid Engine — Skeletal Animation System
// ============================================================================
// Provides bone hierarchy, skinned mesh support, and skeletal animation.
// Works alongside the existing keyframe animation system.
// ============================================================================
#pragma once

#include "core/Types.h"
#include "core/Math.h"
#include "core/Component.h"
#include "animation/Animation.h"
#include <string>
#include <vector>
#include <map>

namespace gv {

// Forward declarations
class GameObject;

// ── Bone ───────────────────────────────────────────────────────────────────
/// Represents a single bone in a skeletal hierarchy.
struct Bone {
    std::string name;
    i32 id            = -1;       // bone index in the skeleton
    i32 parentIndex   = -1;       // -1 for root bones
    Mat4 offsetMatrix;            // transforms from mesh space to bone space (bind pose inverse)
    Mat4 localTransform;          // local transform relative to parent
    Mat4 globalTransform;         // computed world-space transform

    // Local transform components (for animation blending)
    Vec3 localPosition { 0, 0, 0 };
    Quaternion localRotation;
    Vec3 localScale { 1, 1, 1 };

    std::vector<i32> childIndices;
};

// ── Skeleton ───────────────────────────────────────────────────────────────
/// A hierarchy of bones that defines the rig of a character/model.
class Skeleton {
public:
    Skeleton() = default;
    explicit Skeleton(const std::string& name) : m_Name(name) {}

    /// Add a bone to the skeleton. Returns the bone index.
    i32 AddBone(const std::string& name, i32 parentIndex, const Mat4& offsetMatrix);

    /// Find a bone by name. Returns nullptr if not found.
    Bone* FindBone(const std::string& name);
    const Bone* FindBone(const std::string& name) const;

    /// Find a bone index by name. Returns -1 if not found.
    i32 FindBoneIndex(const std::string& name) const;

    /// Get a bone by index.
    Bone& GetBone(i32 index) { return m_Bones[index]; }
    const Bone& GetBone(i32 index) const { return m_Bones[index]; }

    /// Get all bones.
    const std::vector<Bone>& GetBones() const { return m_Bones; }
    std::vector<Bone>& GetBonesMut() { return m_Bones; }

    /// Number of bones in the skeleton.
    u32 GetBoneCount() const { return static_cast<u32>(m_Bones.size()); }

    /// Get the global inverse transform of the skeleton root.
    const Mat4& GetGlobalInverseTransform() const { return m_GlobalInverseTransform; }
    void SetGlobalInverseTransform(const Mat4& m) { m_GlobalInverseTransform = m; }

    /// Recompute global transforms for all bones based on local transforms.
    void ComputeGlobalTransforms();

    /// Get the final bone matrices for skinning (bone_count matrices).
    /// Each matrix = globalInverse * bone.globalTransform * bone.offsetMatrix
    std::vector<Mat4> GetSkinningMatrices() const;

    const std::string& GetName() const { return m_Name; }

    /// Maximum bones supported in the shader.
    static constexpr u32 MAX_BONES = 128;

private:
    std::string m_Name;
    std::vector<Bone> m_Bones;
    std::map<std::string, i32> m_BoneNameMap;
    Mat4 m_GlobalInverseTransform;
};

// ── Bone Keyframe ──────────────────────────────────────────────────────────
/// A keyframe for a single bone's local transform.
struct BoneKeyframe {
    f32 time = 0.0f;
    Vec3 position { 0, 0, 0 };
    Quaternion rotation;
    Vec3 scale { 1, 1, 1 };
};

// ── Bone Animation Channel ────────────────────────────────────────────────
/// Animation data for a single bone within a skeletal animation clip.
struct BoneChannel {
    std::string boneName;
    i32 boneIndex = -1;
    std::vector<BoneKeyframe> keyframes;

    /// Sample the channel at time t.
    BoneKeyframe Sample(f32 t, bool loop, f32 duration) const;
};

// ── Skeletal Animation Clip ────────────────────────────────────────────────
/// A named skeletal animation containing channels for multiple bones.
class SkeletalAnimClip {
public:
    SkeletalAnimClip(const std::string& name = "SkeletalClip") : m_Name(name) {}

    /// Add a channel for a bone.
    void AddChannel(const BoneChannel& channel);

    /// Get a channel by bone name.
    BoneChannel* GetChannel(const std::string& boneName);

    /// Apply this clip at time t to a skeleton (updates bone local transforms).
    void Apply(Skeleton& skeleton, f32 time) const;

    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& n) { m_Name = n; }
    f32 GetDuration() const { return m_Duration; }
    void SetDuration(f32 d) { m_Duration = d; }
    bool IsLooping() const { return m_Loop; }
    void SetLooping(bool l) { m_Loop = l; }

    const std::vector<BoneChannel>& GetChannels() const { return m_Channels; }

private:
    std::string m_Name;
    std::vector<BoneChannel> m_Channels;
    f32 m_Duration = 1.0f;
    bool m_Loop = true;
};

// ── Vertex Weight ──────────────────────────────────────────────────────────
/// Per-vertex bone influence data.
struct VertexWeight {
    i32 boneIDs[4]    = { -1, -1, -1, -1 };   // up to 4 bones per vertex
    f32 weights[4]    = { 0, 0, 0, 0 };

    /// Add a bone influence to this vertex (up to 4).
    void AddBoneInfluence(i32 boneID, f32 weight);

    /// Normalize weights so they sum to 1.
    void Normalize();
};

// ── Skinned Mesh Renderer ──────────────────────────────────────────────────
/// Component that renders a mesh deformed by skeletal animation.
class SkinnedMeshRenderer : public Component {
public:
    SkinnedMeshRenderer() = default;
    ~SkinnedMeshRenderer() override = default;

    std::string GetTypeName() const override { return "SkinnedMeshRenderer"; }

    /// The skeleton used for skinning.
    Shared<Skeleton> skeleton;

    /// Per-vertex bone weights (same count as mesh vertices).
    std::vector<VertexWeight> vertexWeights;

    /// Mesh path (loaded model).
    std::string meshPath;

    /// Color tint.
    Vec4 color { 1, 1, 1, 1 };

    void OnRender() override {}
};

// ── Skeletal Animator Component ────────────────────────────────────────────
/// Attach to a GameObject with SkinnedMeshRenderer to play skeletal animations.
class SkeletalAnimator : public Component {
public:
    SkeletalAnimator() = default;
    ~SkeletalAnimator() override = default;

    std::string GetTypeName() const override { return "SkeletalAnimator"; }

    void OnUpdate(f32 dt) override;

    // ── Clip management ────────────────────────────────────────────────────
    void AddClip(const SkeletalAnimClip& clip);
    void RemoveClip(const std::string& name);
    SkeletalAnimClip* GetClip(const std::string& name);
    const std::map<std::string, SkeletalAnimClip>& GetClips() const { return m_Clips; }

    // ── Playback ───────────────────────────────────────────────────────────
    void Play(const std::string& clipName);
    void Stop();
    void Pause();
    void Resume();
    void CrossFade(const std::string& toClip, f32 transitionTime = 0.3f);

    bool IsPlaying() const { return m_Playing; }
    f32 GetTime() const { return m_Time; }
    const std::string& GetCurrentClipName() const { return m_CurrentClip; }

    /// Get the computed bone matrices for the current frame (for shader upload).
    const std::vector<Mat4>& GetBoneMatrices() const { return m_BoneMatrices; }

    /// Set the skeleton reference.
    void SetSkeleton(Shared<Skeleton> skel) { m_Skeleton = skel; }

private:
    std::map<std::string, SkeletalAnimClip> m_Clips;
    std::string m_CurrentClip;
    Shared<Skeleton> m_Skeleton;
    std::vector<Mat4> m_BoneMatrices;
    f32 m_Time = 0.0f;
    f32 m_Speed = 1.0f;
    bool m_Playing = false;
    bool m_Paused = false;

    // Blending
    std::string m_BlendFromClip;
    std::string m_BlendToClip;
    f32 m_BlendProgress = 0.0f;
    f32 m_BlendDuration = 0.3f;
    bool m_Blending = false;
};

/// Helper to create a simple demo skeleton for testing.
Shared<Skeleton> CreateDemoSkeleton();

/// Helper to create a simple walk animation for the demo skeleton.
SkeletalAnimClip CreateDemoWalkAnimation();

} // namespace gv
