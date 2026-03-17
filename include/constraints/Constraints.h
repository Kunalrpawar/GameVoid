#pragma once
#include "core/Component.h"
#include "core/Math.h"
#include <vector>

using gv::Component;
using gv::Vec3;

// ─────────────────────────────────────────────────────────────────────────────
//  Physics Constraints  —  Joint types for multi-body dynamics
//
//  Each constraint links two GameObjects (A and B) via their RigidBodies.
//  Call Solve() from the physics step after integration to enforce limits.
// ─────────────────────────────────────────────────────────────────────────────

class RigidBody;
class GameObject;

// ─── Hinge Joint ─────────────────────────────────────────────────────────────
// Allows rotation about one shared axis; can limit angle range and add a motor.
class HingeConstraint : public Component {
public:
    GameObject* bodyA   = nullptr;
    GameObject* bodyB   = nullptr;
    Vec3  axisWorld     = Vec3(0,1,0);  // hinge axis in world space
    Vec3  anchorA;                       // attachment point on bodyA (local)
    Vec3  anchorB;                       // attachment point on bodyB (local)

    bool  useLimits     = false;
    float lowerAngleDeg = -90.0f;
    float upperAngleDeg =  90.0f;

    bool  useMotor      = false;
    float motorTargetRPM= 60.0f;
    float motorMaxTorque= 100.0f;

    void Solve(float dt);
    float CurrentAngleDeg() const;
};

// ─── Spring Joint ─────────────────────────────────────────────────────────────
// Linear spring connecting two anchor points; obeys Hooke's law.
class SpringConstraint : public Component {
public:
    GameObject* bodyA         = nullptr;
    GameObject* bodyB         = nullptr;
    Vec3  anchorA;
    Vec3  anchorB;

    float restLength          = 1.0f;   // natural spring length (m)
    float stiffness           = 200.0f; // k  N/m
    float damping             = 5.0f;   // c  Ns/m
    float minLength           = 0.0f;   // compression clamp (0 = no clamp)
    float maxLength           = 99.0f;  // extension clamp

    void Solve(float dt);
    float CurrentLength() const;
    float CurrentExtension() const;  // signed: + = stretched, - = compressed
};

// ─── Slider Joint ─────────────────────────────────────────────────────────────
// Allows translation along one axis; no rotation between the two bodies.
class SliderConstraint : public Component {
public:
    GameObject* bodyA     = nullptr;
    GameObject* bodyB     = nullptr;
    Vec3  slideAxisWorld  = Vec3(1,0,0);

    bool  useLimits       = false;
    float lowerDistM      = -2.0f;
    float upperDistM      =  2.0f;

    bool  useMotor        = false;
    float motorTargetMPS  = 1.0f;   // m/s
    float motorMaxForce   = 500.0f; // N

    void  Solve(float dt);
    float CurrentOffset() const;  // distance along slide axis
};

// ─── Fixed Joint ─────────────────────────────────────────────────────────────
// Welds two bodies together; useful for breakable joints (set breakForce).
class FixedConstraint : public Component {
public:
    GameObject* bodyA        = nullptr;
    GameObject* bodyB        = nullptr;
    float       breakForce   = 1e9f;   // force (N) that snaps this joint
    float       breakTorque  = 1e9f;   // torque (Nm)
    bool        isBroken     = false;

    void Solve(float dt);
};

// ─── Ball-Socket Joint ────────────────────────────────────────────────────────
// Free rotation in all directions about a shared pivot (no translation).
class BallSocketConstraint : public Component {
public:
    GameObject* bodyA   = nullptr;
    GameObject* bodyB   = nullptr;
    Vec3  anchorWorld;              // shared pivot in world space
    float swingCone     = 60.0f;   // max swing angle from rest (degrees)
    float twistLimit    = 180.0f;  // twist freedom (degrees)

    void Solve(float dt);
};
