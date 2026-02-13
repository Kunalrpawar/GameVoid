// ============================================================================
// GameVoid Engine — Skeletal Animation System Implementation
// ============================================================================
#include "animation/SkeletalAnimation.h"
#include "core/GameObject.h"
#include <algorithm>
#include <cmath>

namespace gv {

// ============================================================================
// Skeleton
// ============================================================================
i32 Skeleton::AddBone(const std::string& name, i32 parentIndex, const Mat4& offsetMatrix) {
    i32 index = static_cast<i32>(m_Bones.size());
    Bone bone;
    bone.name = name;
    bone.id = index;
    bone.parentIndex = parentIndex;
    bone.offsetMatrix = offsetMatrix;
    bone.localTransform = Mat4::Identity();
    bone.globalTransform = Mat4::Identity();
    m_Bones.push_back(bone);
    m_BoneNameMap[name] = index;

    // Add as child to parent
    if (parentIndex >= 0 && parentIndex < static_cast<i32>(m_Bones.size())) {
        m_Bones[parentIndex].childIndices.push_back(index);
    }

    return index;
}

Bone* Skeleton::FindBone(const std::string& name) {
    auto it = m_BoneNameMap.find(name);
    if (it != m_BoneNameMap.end()) return &m_Bones[it->second];
    return nullptr;
}

const Bone* Skeleton::FindBone(const std::string& name) const {
    auto it = m_BoneNameMap.find(name);
    if (it != m_BoneNameMap.end()) return &m_Bones[it->second];
    return nullptr;
}

i32 Skeleton::FindBoneIndex(const std::string& name) const {
    auto it = m_BoneNameMap.find(name);
    return (it != m_BoneNameMap.end()) ? it->second : -1;
}

void Skeleton::ComputeGlobalTransforms() {
    for (size_t i = 0; i < m_Bones.size(); ++i) {
        Bone& bone = m_Bones[i];

        // Build local transform from components
        bone.localTransform = Mat4::Translate(bone.localPosition)
                            * bone.localRotation.ToMat4()
                            * Mat4::Scale(bone.localScale);

        if (bone.parentIndex >= 0 && bone.parentIndex < static_cast<i32>(m_Bones.size())) {
            bone.globalTransform = m_Bones[bone.parentIndex].globalTransform * bone.localTransform;
        } else {
            bone.globalTransform = bone.localTransform;
        }
    }
}

std::vector<Mat4> Skeleton::GetSkinningMatrices() const {
    std::vector<Mat4> matrices(m_Bones.size());
    for (size_t i = 0; i < m_Bones.size(); ++i) {
        matrices[i] = m_GlobalInverseTransform * m_Bones[i].globalTransform * m_Bones[i].offsetMatrix;
    }
    return matrices;
}

// ============================================================================
// BoneChannel
// ============================================================================
static Quaternion SlerpQuatSkel(Quaternion a, Quaternion b, f32 t) {
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

BoneKeyframe BoneChannel::Sample(f32 t, bool loop, f32 duration) const {
    if (keyframes.empty()) return {};
    if (keyframes.size() == 1) return keyframes[0];

    if (duration <= 0) duration = 0.001f;
    if (loop && t > duration) t = std::fmod(t, duration);
    if (t <= keyframes.front().time) return keyframes.front();
    if (t >= keyframes.back().time) return keyframes.back();

    for (size_t i = 0; i + 1 < keyframes.size(); ++i) {
        const BoneKeyframe& k0 = keyframes[i];
        const BoneKeyframe& k1 = keyframes[i + 1];
        if (t >= k0.time && t <= k1.time) {
            f32 seg = k1.time - k0.time;
            f32 frac = (seg > 0.0001f) ? (t - k0.time) / seg : 0.0f;

            BoneKeyframe result;
            result.time = t;
            result.position = LerpVec3(k0.position, k1.position, frac);
            result.rotation = SlerpQuatSkel(k0.rotation, k1.rotation, frac);
            result.scale = LerpVec3(k0.scale, k1.scale, frac);
            return result;
        }
    }
    return keyframes.back();
}

// ============================================================================
// SkeletalAnimClip
// ============================================================================
void SkeletalAnimClip::AddChannel(const BoneChannel& channel) {
    m_Channels.push_back(channel);
    // Auto-extend duration
    for (auto& kf : channel.keyframes) {
        if (kf.time > m_Duration) m_Duration = kf.time;
    }
}

BoneChannel* SkeletalAnimClip::GetChannel(const std::string& boneName) {
    for (auto& ch : m_Channels) {
        if (ch.boneName == boneName) return &ch;
    }
    return nullptr;
}

void SkeletalAnimClip::Apply(Skeleton& skeleton, f32 time) const {
    for (auto& channel : m_Channels) {
        i32 boneIdx = (channel.boneIndex >= 0) ? channel.boneIndex
                                                : skeleton.FindBoneIndex(channel.boneName);
        if (boneIdx < 0) continue;

        BoneKeyframe kf = channel.Sample(time, m_Loop, m_Duration);
        Bone& bone = skeleton.GetBone(boneIdx);
        bone.localPosition = kf.position;
        bone.localRotation = kf.rotation;
        bone.localScale = kf.scale;
    }
    skeleton.ComputeGlobalTransforms();
}

// ============================================================================
// VertexWeight
// ============================================================================
void VertexWeight::AddBoneInfluence(i32 boneID, f32 weight) {
    // Find empty slot or replace smallest weight
    for (int i = 0; i < 4; ++i) {
        if (boneIDs[i] < 0) {
            boneIDs[i] = boneID;
            weights[i] = weight;
            return;
        }
    }
    // All slots full — replace the smallest
    int minIdx = 0;
    for (int i = 1; i < 4; ++i) {
        if (weights[i] < weights[minIdx]) minIdx = i;
    }
    if (weight > weights[minIdx]) {
        boneIDs[minIdx] = boneID;
        weights[minIdx] = weight;
    }
}

void VertexWeight::Normalize() {
    f32 total = 0;
    for (int i = 0; i < 4; ++i) total += weights[i];
    if (total > 0.0001f) {
        for (int i = 0; i < 4; ++i) weights[i] /= total;
    }
}

// ============================================================================
// SkeletalAnimator
// ============================================================================
void SkeletalAnimator::AddClip(const SkeletalAnimClip& clip) {
    m_Clips[clip.GetName()] = clip;
}

void SkeletalAnimator::RemoveClip(const std::string& name) {
    m_Clips.erase(name);
    if (m_CurrentClip == name) Stop();
}

SkeletalAnimClip* SkeletalAnimator::GetClip(const std::string& name) {
    auto it = m_Clips.find(name);
    return (it != m_Clips.end()) ? &it->second : nullptr;
}

void SkeletalAnimator::Play(const std::string& clipName) {
    auto it = m_Clips.find(clipName);
    if (it == m_Clips.end()) return;
    m_CurrentClip = clipName;
    m_Time = 0;
    m_Playing = true;
    m_Paused = false;
    m_Blending = false;
}

void SkeletalAnimator::Stop() {
    m_Playing = false;
    m_Paused = false;
    m_Time = 0;
    m_Blending = false;
}

void SkeletalAnimator::Pause() { m_Paused = true; }
void SkeletalAnimator::Resume() { m_Paused = false; }

void SkeletalAnimator::CrossFade(const std::string& toClip, f32 transitionTime) {
    auto it = m_Clips.find(toClip);
    if (it == m_Clips.end()) return;
    m_BlendFromClip = m_CurrentClip;
    m_BlendToClip = toClip;
    m_BlendDuration = transitionTime;
    m_BlendProgress = 0.0f;
    m_Blending = true;
}

void SkeletalAnimator::OnUpdate(f32 dt) {
    if (!m_Playing || m_Paused || !m_Skeleton) return;

    m_Time += dt * m_Speed;

    if (m_Blending) {
        m_BlendProgress += dt / m_BlendDuration;
        if (m_BlendProgress >= 1.0f) {
            m_BlendProgress = 1.0f;
            m_CurrentClip = m_BlendToClip;
            m_Blending = false;
        }

        // Apply both clips and blend
        auto* clipA = GetClip(m_BlendFromClip);
        auto* clipB = GetClip(m_BlendToClip);
        if (clipA && clipB) {
            // Apply clip A
            clipA->Apply(*m_Skeleton, m_Time);
            // Save bone A transforms
            std::vector<Vec3> posA(m_Skeleton->GetBoneCount());
            std::vector<Quaternion> rotA(m_Skeleton->GetBoneCount());
            std::vector<Vec3> sclA(m_Skeleton->GetBoneCount());
            for (u32 i = 0; i < m_Skeleton->GetBoneCount(); ++i) {
                posA[i] = m_Skeleton->GetBone(i).localPosition;
                rotA[i] = m_Skeleton->GetBone(i).localRotation;
                sclA[i] = m_Skeleton->GetBone(i).localScale;
            }
            // Apply clip B
            clipB->Apply(*m_Skeleton, m_Time);
            // Blend
            f32 t = m_BlendProgress;
            for (u32 i = 0; i < m_Skeleton->GetBoneCount(); ++i) {
                Bone& bone = m_Skeleton->GetBone(i);
                bone.localPosition = LerpVec3(posA[i], bone.localPosition, t);
                bone.localRotation = SlerpQuatSkel(rotA[i], bone.localRotation, t);
                bone.localScale = LerpVec3(sclA[i], bone.localScale, t);
            }
            m_Skeleton->ComputeGlobalTransforms();
        }
    } else {
        auto* clip = GetClip(m_CurrentClip);
        if (clip) {
            clip->Apply(*m_Skeleton, m_Time);
        }
    }

    // Compute final bone matrices
    m_BoneMatrices = m_Skeleton->GetSkinningMatrices();
}

// ============================================================================
// Demo Helpers
// ============================================================================
Shared<Skeleton> CreateDemoSkeleton() {
    auto skel = MakeShared<Skeleton>("DemoSkeleton");
    skel->SetGlobalInverseTransform(Mat4::Identity());

    // Simple humanoid skeleton:
    // 0: Root (hips)
    // 1: Spine
    // 2: Head
    // 3: LeftUpperArm
    // 4: LeftLowerArm
    // 5: RightUpperArm
    // 6: RightLowerArm
    // 7: LeftUpperLeg
    // 8: LeftLowerLeg
    // 9: RightUpperLeg
    // 10: RightLowerLeg

    skel->AddBone("Hips",          -1, Mat4::Identity());        // 0
    skel->AddBone("Spine",          0, Mat4::Identity());        // 1
    skel->AddBone("Head",           1, Mat4::Identity());        // 2
    skel->AddBone("LeftUpperArm",   1, Mat4::Identity());        // 3
    skel->AddBone("LeftLowerArm",   3, Mat4::Identity());        // 4
    skel->AddBone("RightUpperArm",  1, Mat4::Identity());        // 5
    skel->AddBone("RightLowerArm",  5, Mat4::Identity());        // 6
    skel->AddBone("LeftUpperLeg",   0, Mat4::Identity());        // 7
    skel->AddBone("LeftLowerLeg",   7, Mat4::Identity());        // 8
    skel->AddBone("RightUpperLeg",  0, Mat4::Identity());        // 9
    skel->AddBone("RightLowerLeg",  9, Mat4::Identity());        // 10

    // Set default bind pose positions
    skel->GetBone(0).localPosition = { 0, 1.0f, 0 };       // Hips
    skel->GetBone(1).localPosition = { 0, 0.4f, 0 };       // Spine
    skel->GetBone(2).localPosition = { 0, 0.5f, 0 };       // Head
    skel->GetBone(3).localPosition = { -0.3f, 0.3f, 0 };   // LeftUpperArm
    skel->GetBone(4).localPosition = { -0.3f, 0, 0 };      // LeftLowerArm
    skel->GetBone(5).localPosition = { 0.3f, 0.3f, 0 };    // RightUpperArm
    skel->GetBone(6).localPosition = { 0.3f, 0, 0 };       // RightLowerArm
    skel->GetBone(7).localPosition = { -0.15f, 0, 0 };     // LeftUpperLeg
    skel->GetBone(8).localPosition = { 0, -0.5f, 0 };      // LeftLowerLeg
    skel->GetBone(9).localPosition = { 0.15f, 0, 0 };      // RightUpperLeg
    skel->GetBone(10).localPosition = { 0, -0.5f, 0 };     // RightLowerLeg

    skel->ComputeGlobalTransforms();
    GV_LOG_INFO("Created demo skeleton with " + std::to_string(skel->GetBoneCount()) + " bones.");
    return skel;
}

SkeletalAnimClip CreateDemoWalkAnimation() {
    SkeletalAnimClip clip("Walk");
    clip.SetDuration(1.0f);
    clip.SetLooping(true);

    const f32 PI = 3.14159265358979f;

    // Hips — subtle bob
    {
        BoneChannel ch;
        ch.boneName = "Hips";
        ch.keyframes.push_back({ 0.0f,  { 0, 1.0f, 0 }, {}, { 1, 1, 1 } });
        ch.keyframes.push_back({ 0.25f, { 0, 1.02f, 0 }, {}, { 1, 1, 1 } });
        ch.keyframes.push_back({ 0.5f,  { 0, 1.0f, 0 }, {}, { 1, 1, 1 } });
        ch.keyframes.push_back({ 0.75f, { 0, 1.02f, 0 }, {}, { 1, 1, 1 } });
        ch.keyframes.push_back({ 1.0f,  { 0, 1.0f, 0 }, {}, { 1, 1, 1 } });
        clip.AddChannel(ch);
    }

    // Left leg swing
    {
        BoneChannel ch;
        ch.boneName = "LeftUpperLeg";
        ch.keyframes.push_back({ 0.0f,  { -0.15f, 0, 0 }, Quaternion::FromAxisAngle({ 1,0,0 }, -0.3f), { 1,1,1 } });
        ch.keyframes.push_back({ 0.5f,  { -0.15f, 0, 0 }, Quaternion::FromAxisAngle({ 1,0,0 },  0.3f), { 1,1,1 } });
        ch.keyframes.push_back({ 1.0f,  { -0.15f, 0, 0 }, Quaternion::FromAxisAngle({ 1,0,0 }, -0.3f), { 1,1,1 } });
        clip.AddChannel(ch);
    }

    // Right leg swing (opposite phase)
    {
        BoneChannel ch;
        ch.boneName = "RightUpperLeg";
        ch.keyframes.push_back({ 0.0f,  { 0.15f, 0, 0 }, Quaternion::FromAxisAngle({ 1,0,0 },  0.3f), { 1,1,1 } });
        ch.keyframes.push_back({ 0.5f,  { 0.15f, 0, 0 }, Quaternion::FromAxisAngle({ 1,0,0 }, -0.3f), { 1,1,1 } });
        ch.keyframes.push_back({ 1.0f,  { 0.15f, 0, 0 }, Quaternion::FromAxisAngle({ 1,0,0 },  0.3f), { 1,1,1 } });
        clip.AddChannel(ch);
    }

    // Left arm swing (opposite to left leg)
    {
        BoneChannel ch;
        ch.boneName = "LeftUpperArm";
        ch.keyframes.push_back({ 0.0f,  { -0.3f, 0.3f, 0 }, Quaternion::FromAxisAngle({ 1,0,0 },  0.2f), { 1,1,1 } });
        ch.keyframes.push_back({ 0.5f,  { -0.3f, 0.3f, 0 }, Quaternion::FromAxisAngle({ 1,0,0 }, -0.2f), { 1,1,1 } });
        ch.keyframes.push_back({ 1.0f,  { -0.3f, 0.3f, 0 }, Quaternion::FromAxisAngle({ 1,0,0 },  0.2f), { 1,1,1 } });
        clip.AddChannel(ch);
    }

    // Right arm swing
    {
        BoneChannel ch;
        ch.boneName = "RightUpperArm";
        ch.keyframes.push_back({ 0.0f,  { 0.3f, 0.3f, 0 }, Quaternion::FromAxisAngle({ 1,0,0 }, -0.2f), { 1,1,1 } });
        ch.keyframes.push_back({ 0.5f,  { 0.3f, 0.3f, 0 }, Quaternion::FromAxisAngle({ 1,0,0 },  0.2f), { 1,1,1 } });
        ch.keyframes.push_back({ 1.0f,  { 0.3f, 0.3f, 0 }, Quaternion::FromAxisAngle({ 1,0,0 }, -0.2f), { 1,1,1 } });
        clip.AddChannel(ch);
    }

    (void)PI;
    GV_LOG_INFO("Created demo walk animation (1.0s, 5 channels).");
    return clip;
}

} // namespace gv
