// ============================================================================
// GameVoid Engine — Physics Scripting Examples
// ============================================================================
// C++ examples for using the physics scripting system.
// Copy and modify these examples to create custom physics behaviors.
// ============================================================================

/*

EXAMPLE 1: Simple Player Controller with Force-Based Movement
==============================================================

class PlayerController : public PhysicsScript {
public:
    f32 moveForce = 50.0f;
    f32 jumpForce = 100.0f;

    void OnCreate() override {
        // Register input bindings
        BindKeyToForce("W", "forceY", jumpForce, true);      // Jump
        BindKeyToForce("A", "forceX", -moveForce, false);    // Move left
        BindKeyToForce("D", "forceX", moveForce, false);     // Move right
    }

    void OnPhysicsUpdate(f32 dt) override {
        // Check input
        if (IsKeyPressed("W")) {
            ApplyForce(Vec3(0, jumpForce, 0), true);  // Impulse
        }
        if (IsKeyPressed("A")) {
            ApplyForce(Vec3(-moveForce, 0, 0), false);  // Continuous force
        }
        if (IsKeyPressed("D")) {
            ApplyForce(Vec3(moveForce, 0, 0), false);
        }
    }
};


EXAMPLE 2: Vehicle Physics with Gravity Control
=================================================

class VehicleController : public PhysicsScript {
public:
    f32 engineForce = 100.0f;
    f32 steeringTorque = 50.0f;

    void OnCreate() override {
        // Override gravity for this vehicle
        gravityScale = Vec3(1.0f, 0.8f, 1.0f);  // Less gravity in Y
    }

    void OnPhysicsUpdate(f32 dt) override {
        // Engine: forward/backward
        if (IsKeyPressed("W")) {
            ApplyForce(Vec3(0, 0, engineForce), false);
        }
        if (IsKeyPressed("S")) {
            ApplyForce(Vec3(0, 0, -engineForce), false);
        }

        // Steering: left/right rotation
        if (IsKeyPressed("A")) {
            ApplyMoment(Vec3(0, steeringTorque, 0));
        }
        if (IsKeyPressed("D")) {
            ApplyMoment(Vec3(0, -steeringTorque, 0));
        }
    }
};


EXAMPLE 3: Using ForceController with Input Bindings
======================================================

// In your initialization code:
auto* obj = scene->FindByName("Player");
auto* forceCtrl = obj->AddComponent<ForceController>();

// Bind keys to forces
forceCtrl->BindKeyToForce("W", ForceDirection::Up, 100.0f);
forceCtrl->BindKeyToForce("A", ForceDirection::Left, 80.0f);
forceCtrl->BindKeyToForce("D", ForceDirection::Right, 80.0f);

// Bind keys to moments (rotation)
forceCtrl->BindKeyToMoment("Q", MomentDirection::Roll, 45.0f);
forceCtrl->BindKeyToMoment("E", MomentDirection::Roll, -45.0f);

// Configure limits
forceCtrl->SetMaxVelocity(50.0f);
forceCtrl->SetMaxAngularVelocity(180.0f);

// Apply custom gravity
forceCtrl->SetGravityScale(0.5f);  // Half gravity


EXAMPLE 4: Behavior Analysis - Monitor Object Physics
======================================================

// Attach analyzer to object
auto* obj = scene->FindByName("Enemy");
auto* analyzer = obj->AddComponent<BehaviorAnalyzer>();
analyzer->enableDebugLog = true;

// Query behavior state
if (analyzer->GetBehaviorState() == PhysicsBehaviorState::Falling) {
    // Object is falling
}

if (analyzer->GetVelocityMagnitude() > 10.0f) {
    // Object is moving fast
}

// Get summary report
std::string summary = analyzer->GetBehaviorSummary();
std::cout << summary;


EXAMPLE 5: Gravity Override and Custom Physics
==============================================

class ZeroGravityZone : public PhysicsScript {
public:
    void OnCreate() override {
        disableGravity = true;
        customGravity = -2.0f;  // Weak gravity
    }

    void OnPhysicsUpdate(f32 dt) override {
        // Free-floating physics behavior
        if (IsKeyPressed("W")) {
            ApplyForce(Vec3(0, 20.0f, 0), false);
        }
        if (IsKeyPressed("S")) {
            ApplyForce(Vec3(0, -20.0f, 0), false);
        }
    }
};


EXAMPLE 6: Ball Physics (2D)
=============================

class BallController : public PhysicsScript {
public:
    f32 bounce = 0.8f;
    f32 friction = 0.95f;

    void OnPhysicsUpdate(f32 dt) override {
        auto* obj = GetGameObject();
        auto* rb2d = obj->GetComponent<RigidBody2D>();
        if (!rb2d) return;

        // Damping to simulate friction
        rb2d->velocity *= friction;

        // Apply gravity (if not disabled)
        if (!disableGravity) {
            rb2d->velocity.y -= 9.81f * dt;
        }

        // Boost: Space key applies upward impulse
        if (IsKeyPressed("Space")) {
            ApplyForce(Vec3(0, 50.0f, 0), true);
        }
    }
};


EXAMPLE 7: Racing Game Steering
=================================

class RaceCarController : public PhysicsScript {
public:
    f32 maxSpeed = 80.0f;
    f32 acceleration = 40.0f;
    f32 steering = 120.0f;  // degrees per second

    void OnPhysicsUpdate(f32 dt) override {
        auto* rb = GetGameObject()->GetComponent<RigidBody>();
        if (!rb) return;

        // Throttle
        if (IsKeyPressed("W")) {
            rb->velocity.z = std::min(rb->velocity.z + acceleration * dt, maxSpeed);
        }
        if (IsKeyPressed("S")) {
            rb->velocity.z = std::max(rb->velocity.z - acceleration * dt, -maxSpeed * 0.5f);
        }

        // Steering (rotate around Y)
        if (IsKeyPressed("A")) {
            ApplyMoment(Vec3(0, steering, 0));
        }
        if (IsKeyPressed("D")) {
            ApplyMoment(Vec3(0, -steering, 0));
        }
    }
};


KEY CONCEPTS
============

1. Force vs. Impulse:
   - Force: continuous acceleration (applied over time)
   - Impulse: instantaneous velocity change (applied once)

2. Gravity Control:
   - disableGravity: turn off gravity entirely
   - gravityScale: multiply gravity by this factor
   - customGravity: override world gravity with custom value

3. Moment/Torque:
   - Angular acceleration (rotation)
   - Applied around X (pitch), Y (yaw), Z (roll) axes

4. Behavior Analysis:
   - Track velocity, acceleration, energy, momentum
   - Monitor collision and falling states
   - Useful for AI and debugging

5. Input Binding:
   - Pre-bind keys during OnCreate()
   - OR check IsKeyPressed() manually in OnPhysicsUpdate()
   - Supports WASD, arrows, Space, Q, E, Shift, Ctrl

*/
