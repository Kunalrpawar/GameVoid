#pragma once
#include "core/Component.h"
#include "core/Math.h"
#include "core/GameObject.h"
#include "physics/Physics.h"
#include <cmath>
#include <string>

namespace gv {

// CarController3D can run on any dynamic body.
class CarController3D : public ::gv::Component {
public:
    // Tuning
    float maxSpeed       = 20.0f;   // m/s forward top speed
    float reverseSpeed   =  8.0f;   // m/s reverse top speed
    float acceleration   = 15.0f;   // m/s^2 throttle ramp
    float brakingPower   = 25.0f;   // m/s^2 brake/reverse deceleration
    float turnSpeed      = 90.0f;   // deg/s yaw rate at full steering
    float drag           =  2.5f;   // natural speed decay (no input)
    float angularDamp    =  0.95f;  // applied each frame to RB angular velocity

    // Per-frame input (set by editor or gameplay code)
    float inputForward = 0.0f;   // -1 = reverse,  0 = neutral,  +1 = accelerate
    float inputTurn    = 0.0f;   // -1 = left,      0 = straight, +1 = right

    // Runtime state
    float currentSpeed = 0.0f;   // signed: positive = forward
    float heading      = 0.0f;   // yaw in degrees (world Y axis)

    std::string GetTypeName() const override { return "CarController3D"; }

    // Called every physics step with the car's single RigidBody.
    void UpdateController(float dt, ::gv::RigidBody* rb) {
        if (!rb) return;

        // Acceleration / braking
        if (inputForward > 0.01f) {
            currentSpeed += acceleration * inputForward * dt;
        } else if (inputForward < -0.01f) {
            // Braking or reversing
            if (currentSpeed > 0.1f)
                currentSpeed -= brakingPower * dt;          // braking forward motion
            else
                currentSpeed += acceleration * inputForward * dt; // reversing
        } else {
            // Drag
            float dir = (currentSpeed >= 0.0f) ? 1.0f : -1.0f;
            currentSpeed -= dir * drag * dt;
            if (dir * currentSpeed < 0.0f) currentSpeed = 0.0f;
        }

        // Clamp speed
        float maxFwd = maxSpeed;
        float maxRev = -reverseSpeed;
        if (currentSpeed > maxFwd) currentSpeed = maxFwd;
        if (currentSpeed < maxRev) currentSpeed = maxRev;

        // Steering (only effective when moving)
        float speedRatio = std::abs(currentSpeed) / std::max(maxSpeed, 1.0f);
        float effectiveTurn = turnSpeed * inputTurn * speedRatio;
        heading += effectiveTurn * dt;

        // Drive heading direction
        float rad = heading * (3.14159265f / 180.0f);
        ::gv::Vec3 fwd(std::sin(rad), 0.0f, std::cos(rad));
        ::gv::Vec3 newVel = fwd * currentSpeed;
        newVel.y = rb->velocity.y;  // preserve gravity / fall velocity
        rb->velocity = newVel;

        // Suppress any spin torques accumulated from physics collisions
        rb->angularVelocity = rb->angularVelocity * angularDamp;

        // Sync transform yaw so the mesh faces the right direction
        if (GetOwner()) {
            auto& t = GetOwner()->GetTransform();
            ::gv::Vec3 euler = t.GetEulerDeg();
            euler.y = heading;
            t.SetEulerDeg(euler.x, euler.y, euler.z);
        }
    }
};

} // namespace gv
