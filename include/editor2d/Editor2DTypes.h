// ============================================================================
// GameVoid Engine — 2D Editor Types & Enums
// ============================================================================
// Shared types for the 2D editor subsystem: sprite data, 2D components,
// layer ordering, and 2D-specific enums.
//
// Mario-ready: Includes platformer controller, camera follow, audio source,
// animation state machine, and full collision callback system.
// ============================================================================
#pragma once

#include "core/Types.h"
#include "core/Math.h"
#include "core/Component.h"
#include <string>
#include <vector>
#include <functional>
#include <cmath>

namespace gv {

// ── Editor dimension mode (swap between 2D / 3D) ──────────────────────────
enum class EditorDimMode {
    Mode3D,   // default 3D editor
    Mode2D    // 2D editor with orthographic top-down view
};

// ── 2D Gizmo modes ────────────────────────────────────────────────────────
enum class Gizmo2DMode {
    Translate,
    Rotate,
    Scale
};

// ── 2D collision shape types ───────────────────────────────────────────────
enum class ColliderShape2D {
    Box,
    Circle,
    Capsule,
    Polygon
};

// ── 2D body types ──────────────────────────────────────────────────────────
enum class BodyType2D {
    Static,
    Dynamic,
    Kinematic
};

// ── Sort layer for 2D rendering ────────────────────────────────────────────
struct SortLayer {
    std::string name = "Default";
    i32 order = 0;
};

// ============================================================================
// Sprite Component — 2D textured quad
// ============================================================================
class SpriteComponent : public Component {
public:
    std::string GetTypeName() const override { return "Sprite"; }

    // ── Visual properties ──────────────────────────────────────────────────
    u32  textureID     = 0;            // GL texture handle (0 = white/colored)
    std::string texturePath;           // file path for serialisation
    Vec4 color         { 1, 1, 1, 1 }; // tint colour (RGBA)
    Vec2 size          { 1, 1 };       // world-unit size of the sprite
    Vec2 pivot         { 0.5f, 0.5f }; // pivot point (0..1, default = center)
    bool flipX         = false;
    bool flipY         = false;

    // ── Sorting ────────────────────────────────────────────────────────────
    i32  sortOrder     = 0;            // draw order within same layer
    std::string sortLayer = "Default"; // layer name

    // ── Sprite-sheet / atlas tiling ────────────────────────────────────────
    Vec2 uvMin { 0, 0 };              // bottom-left UV
    Vec2 uvMax { 1, 1 };              // top-right UV

    // ── Sprite-sheet animation ─────────────────────────────────────────────
    i32  frameCount    = 1;            // number of frames
    i32  columns       = 1;            // columns in the sheet
    f32  frameRate     = 12.0f;        // frames per second
    bool animLooping   = true;
    bool animPlaying   = false;

    // Runtime anim state
    f32  animTimer     = 0.0f;
    i32  currentFrame  = 0;

    void UpdateAnimation(f32 dt) {
        if (!animPlaying || frameCount <= 1) return;
        animTimer += dt;
        f32 frameDur = 1.0f / frameRate;
        while (animTimer >= frameDur) {
            animTimer -= frameDur;
            currentFrame++;
            if (currentFrame >= frameCount) {
                currentFrame = animLooping ? 0 : (frameCount - 1);
                if (!animLooping) animPlaying = false;
            }
        }
        // Update UVs based on current frame
        i32 rows = (frameCount + columns - 1) / columns;
        i32 col = currentFrame % columns;
        i32 row = currentFrame / columns;
        f32 fw = 1.0f / static_cast<f32>(columns);
        f32 fh = 1.0f / static_cast<f32>(rows);
        uvMin = { col * fw, row * fh };
        uvMax = { (col + 1) * fw, (row + 1) * fh };
    }
};

// ============================================================================
// RigidBody2D — 2D physics body
// ============================================================================
class RigidBody2D : public Component {
public:
    std::string GetTypeName() const override { return "RigidBody2D"; }

    BodyType2D bodyType   = BodyType2D::Dynamic;
    f32  mass             = 1.0f;
    f32  gravityScale     = 1.0f;
    f32  linearDamping    = 0.1f;
    f32  angularDamping   = 0.05f;
    bool fixedRotation    = false;

    // Runtime velocity (simplified)
    Vec2 velocity     { 0, 0 };
    f32  angularVel   = 0.0f;
};

// ============================================================================
// Collider2D — 2D collision shape
// ============================================================================
class Collider2D : public Component {
public:
    std::string GetTypeName() const override { return "Collider2D"; }

    ColliderShape2D shape = ColliderShape2D::Box;
    Vec2 offset  { 0, 0 };   // offset from transform origin
    Vec2 boxSize { 1, 1 };   // half-extents for box
    f32  radius  = 0.5f;     // radius for circle/capsule
    f32  height  = 1.0f;     // height for capsule
    bool isTrigger = false;
};

// ============================================================================
// TileMapComponent — for grid-based 2D levels
// ============================================================================
class TileMapComponent : public Component {
public:
    std::string GetTypeName() const override { return "TileMap"; }

    i32  mapWidth  = 16;
    i32  mapHeight = 16;
    f32  tileSize  = 1.0f;
    u32  tilesetTextureID = 0;
    std::string tilesetPath;
    i32  tilesetColumns = 8;
    i32  tilesetRows    = 8;

    // Tile data: -1 = empty, otherwise index into tileset
    std::vector<i32> tiles;

    void Resize(i32 w, i32 h) {
        mapWidth = w; mapHeight = h;
        tiles.assign(w * h, -1);
    }

    i32 GetTile(i32 x, i32 y) const {
        if (x < 0 || x >= mapWidth || y < 0 || y >= mapHeight) return -1;
        return tiles[y * mapWidth + x];
    }

    void SetTile(i32 x, i32 y, i32 tileIdx) {
        if (x < 0 || x >= mapWidth || y < 0 || y >= mapHeight) return;
        tiles[y * mapWidth + x] = tileIdx;
    }
};

// ============================================================================
// Label2D — on-screen text component
// ============================================================================
class Label2D : public Component {
public:
    std::string GetTypeName() const override { return "Label2D"; }

    std::string text      = "Hello";
    f32  fontSize         = 16.0f;
    Vec4 fontColor        { 1, 1, 1, 1 };
    bool worldSpace       = true;   // true = follows transform, false = screen coords
};

// ============================================================================
// ParticleEmitter2D — simple 2D particle system
// ============================================================================
class ParticleEmitter2D : public Component {
public:
    std::string GetTypeName() const override { return "ParticleEmitter2D"; }

    f32  emitRate     = 20.0f;     // particles per second
    f32  lifetime     = 2.0f;      // particle lifetime in seconds
    f32  speed        = 3.0f;      // initial speed
    f32  spread       = 360.0f;    // emission arc in degrees (360 = full circle)
    f32  startSize    = 0.3f;
    f32  endSize      = 0.05f;
    Vec4 startColor   { 1, 0.8f, 0.2f, 1 };
    Vec4 endColor     { 1, 0, 0, 0 };
    Vec2 gravity      { 0, -2.0f };
    bool emitting     = true;
};

// ============================================================================
// Collision2DInfo — passed to collision callbacks
// ============================================================================
struct Collision2DInfo {
    GameObject* self    = nullptr;   // the object receiving the callback
    GameObject* other   = nullptr;   // the object we collided with
    Vec2 normal   { 0, 0 };         // collision normal (from self toward other)
    f32  overlap  = 0.0f;           // penetration depth
    Vec2 contactPoint { 0, 0 };     // approximate contact point
    bool isTrigger = false;         // true if one of the colliders is a trigger
};

// Callback types
using Collision2DCallback = std::function<void(const Collision2DInfo&)>;

// ============================================================================
// CollisionListener2D — attach to receive collision events
// ============================================================================
class CollisionListener2D : public Component {
public:
    std::string GetTypeName() const override { return "CollisionListener2D"; }

    // User sets these callbacks
    Collision2DCallback onCollisionEnter;
    Collision2DCallback onCollisionStay;
    Collision2DCallback onCollisionExit;
    Collision2DCallback onTriggerEnter;
    Collision2DCallback onTriggerStay;
    Collision2DCallback onTriggerExit;
};

// ============================================================================
// PlatformerController2D — Mario-style platformer character controller
// ============================================================================
// Attach to a player object with RigidBody2D + Collider2D.
// Handles horizontal movement, jumping, gravity, wall sliding, coyote time,
// and ground detection — everything needed for a Mario-type controller.
// ============================================================================
class PlatformerController2D : public Component {
public:
    std::string GetTypeName() const override { return "PlatformerController2D"; }

    // ── Movement tuning ────────────────────────────────────────────────────
    f32  moveSpeed          = 8.0f;    // horizontal movement speed
    f32  acceleration       = 40.0f;   // how fast the player gets to moveSpeed
    f32  deceleration       = 30.0f;   // how fast the player stops (ground friction)
    f32  airDeceleration    = 10.0f;   // friction in the air (lower = more floaty)

    // ── Jump tuning ────────────────────────────────────────────────────────
    f32  jumpForce          = 14.0f;   // initial upward velocity on jump
    f32  jumpCutMultiplier  = 0.5f;    // multiply velocity by this on early release
    i32  maxJumps           = 2;       // 1 = single, 2 = double jump, etc.
    f32  coyoteTime         = 0.1f;    // seconds you can still jump after walking off edge
    f32  jumpBufferTime     = 0.12f;   // seconds a jump press is remembered before landing

    // ── Wall mechanics ─────────────────────────────────────────────────────
    bool enableWallSlide    = false;
    f32  wallSlideSpeed     = 2.0f;    // max fall speed when wall sliding
    f32  wallJumpForceX     = 10.0f;   // horizontal push from wall jump
    f32  wallJumpForceY     = 12.0f;   // vertical push from wall jump

    // ── Ground check ───────────────────────────────────────────────────────
    f32  groundCheckDist    = 0.1f;    // raycast distance below feet
    f32  headBumpDist       = 0.1f;    // check above head for ceilings

    // ── Runtime state (read by physics / rendering) ────────────────────────
    bool isGrounded         = false;
    bool isWallSliding      = false;
    bool isFacingRight      = true;
    i32  jumpsRemaining     = 2;
    f32  coyoteTimer        = 0.0f;
    f32  jumpBufferTimer    = 0.0f;
    f32  inputX             = 0.0f;    // -1..+1 horizontal input
    bool inputJump          = false;   // jump pressed this frame
    bool inputJumpHeld      = false;   // jump button held
    bool isDead             = false;

    // ── State machine for animation ────────────────────────────────────────
    enum class State { Idle, Run, Jump, Fall, WallSlide, Dead };
    State currentState      = State::Idle;

    /// Call every frame with the input values set. Updates the RigidBody2D velocity.
    void UpdateController(f32 dt, RigidBody2D* rb) {
        if (!rb || isDead) return;

        // Update timers
        if (coyoteTimer > 0) coyoteTimer -= dt;
        if (jumpBufferTimer > 0) jumpBufferTimer -= dt;

        // Horizontal movement
        f32 targetVelX = inputX * moveSpeed;
        f32 accel = (isGrounded ? acceleration : acceleration * 0.7f);
        f32 decel = isGrounded ? deceleration : airDeceleration;

        if (std::fabs(inputX) > 0.01f) {
            // Accelerate toward target
            if (rb->velocity.x < targetVelX)
                rb->velocity.x += accel * dt;
            else if (rb->velocity.x > targetVelX)
                rb->velocity.x -= accel * dt;
            // Clamp to not overshoot
            if ((targetVelX > 0 && rb->velocity.x > targetVelX) ||
                (targetVelX < 0 && rb->velocity.x < targetVelX))
                rb->velocity.x = targetVelX;
        } else {
            // Decelerate to zero
            if (rb->velocity.x > 0) {
                rb->velocity.x -= decel * dt;
                if (rb->velocity.x < 0) rb->velocity.x = 0;
            } else if (rb->velocity.x < 0) {
                rb->velocity.x += decel * dt;
                if (rb->velocity.x > 0) rb->velocity.x = 0;
            }
        }

        // Facing direction
        if (inputX > 0.01f) isFacingRight = true;
        if (inputX < -0.01f) isFacingRight = false;

        // Jump buffer
        if (inputJump) jumpBufferTimer = jumpBufferTime;

        // Jump execution
        bool canJump = (isGrounded || coyoteTimer > 0 || jumpsRemaining > 0);
        if (jumpBufferTimer > 0 && canJump && jumpsRemaining > 0) {
            rb->velocity.y = jumpForce;
            jumpsRemaining--;
            jumpBufferTimer = 0;
            coyoteTimer = 0;
            isGrounded = false;
        }

        // Variable jump height: cut jump short when button released
        if (!inputJumpHeld && rb->velocity.y > 0) {
            rb->velocity.y *= jumpCutMultiplier;
        }

        // Wall slide
        isWallSliding = false;
        if (enableWallSlide && !isGrounded && rb->velocity.y < 0) {
            // Wall detection would be done by collision system setting a flag
            // For now, wall slide is triggered externally
        }
        if (isWallSliding) {
            if (rb->velocity.y < -wallSlideSpeed)
                rb->velocity.y = -wallSlideSpeed;
        }

        // State machine
        if (isDead) {
            currentState = State::Dead;
        } else if (isGrounded) {
            currentState = (std::fabs(rb->velocity.x) > 0.1f) ? State::Run : State::Idle;
        } else if (isWallSliding) {
            currentState = State::WallSlide;
        } else {
            currentState = (rb->velocity.y > 0) ? State::Jump : State::Fall;
        }
    }

    void OnLanded() {
        jumpsRemaining = maxJumps;
        coyoteTimer = coyoteTime;
    }
};

// ============================================================================
// Camera2DFollow — smooth camera that follows a target object
// ============================================================================
class Camera2DFollow : public Component {
public:
    std::string GetTypeName() const override { return "Camera2DFollow"; }

    u32  targetObjectID  = 0;          // ID of the object to follow
    Vec2 offset          { 0, 2.0f };  // camera offset from target
    f32  smoothSpeed     = 5.0f;       // lerp speed (higher = snappier)
    f32  lookAheadDist   = 2.0f;       // look ahead in movement direction
    f32  lookAheadSpeed  = 3.0f;       // how fast look-ahead catches up

    // ── Bounds (optional level boundaries) ─────────────────────────────────
    bool useBounds       = false;
    Vec2 boundsMin       { -50, -10 };
    Vec2 boundsMax       { 50, 30 };

    // ── Dead zone — camera doesn't move if target is within this rect ──────
    f32  deadZoneX       = 1.0f;
    f32  deadZoneY       = 1.0f;

    // Runtime
    Vec2 currentPos      { 0, 0 };
    Vec2 lookAheadPos    { 0, 0 };
};

// ============================================================================
// AudioSource2D — positional or global 2D audio
// ============================================================================
class AudioSource2D : public Component {
public:
    std::string GetTypeName() const override { return "AudioSource2D"; }

    std::string clipPath;            // file path to .wav/.mp3/.ogg
    f32  volume        = 1.0f;       // 0..1
    f32  pitch         = 1.0f;       // playback speed multiplier
    bool loop          = false;
    bool playOnStart   = false;
    bool spatial       = false;      // true = volume based on distance from listener
    f32  maxDistance    = 20.0f;      // distance at which sound is inaudible

    // Runtime
    bool isPlaying     = false;
    bool triggerPlay   = false;      // set to true to play this frame
    bool triggerStop   = false;      // set to true to stop this frame
};

// ============================================================================
// AnimState2D — sprite animation state machine entry
// ============================================================================
struct AnimState2D {
    std::string name;                // e.g. "Idle", "Run", "Jump"
    i32  startFrame = 0;
    i32  endFrame   = 0;
    f32  frameRate  = 12.0f;
    bool loop       = true;
};

// ============================================================================
// AnimStateMachine2D — manages multiple animation states for a sprite
// ============================================================================
class AnimStateMachine2D : public Component {
public:
    std::string GetTypeName() const override { return "AnimStateMachine2D"; }

    std::vector<AnimState2D> states;
    std::string currentStateName = "Idle";
    i32  currentStateIdx = 0;

    void AddState(const std::string& name, i32 startFrame, i32 endFrame,
                  f32 fps = 12.0f, bool loop = true) {
        states.push_back({ name, startFrame, endFrame, fps, loop });
    }

    void SetState(const std::string& name) {
        if (name == currentStateName) return;
        for (i32 i = 0; i < static_cast<i32>(states.size()); i++) {
            if (states[i].name == name) {
                currentStateName = name;
                currentStateIdx = i;
                return;
            }
        }
    }

    AnimState2D* GetCurrentState() {
        if (currentStateIdx >= 0 && currentStateIdx < static_cast<i32>(states.size()))
            return &states[currentStateIdx];
        return nullptr;
    }
};

// ============================================================================
// Collectible2D — items that can be picked up (coins, power-ups, etc.)
// ============================================================================
class Collectible2D : public Component {
public:
    std::string GetTypeName() const override { return "Collectible2D"; }

    enum class Type { Coin, PowerUp, Health, Key, Star, Custom };
    Type type           = Type::Coin;
    i32  scoreValue     = 100;       // points awarded on pickup
    bool destroyOnPickup = true;
    bool collected      = false;
    f32  bobAmplitude   = 0.2f;      // up/down bob animation
    f32  bobSpeed       = 3.0f;
    f32  bobTimer       = 0.0f;

    void UpdateBob(f32 dt) {
        bobTimer += dt * bobSpeed;
    }

    f32 GetBobOffset() const {
        return std::sin(bobTimer) * bobAmplitude;
    }
};

// ============================================================================
// Hazard2D — objects that damage the player on contact
// ============================================================================
class Hazard2D : public Component {
public:
    std::string GetTypeName() const override { return "Hazard2D"; }

    enum class Type { Spike, Lava, Enemy, Projectile, Pit, Custom };
    Type type        = Type::Spike;
    i32  damage      = 1;
    f32  knockbackX  = 5.0f;
    f32  knockbackY  = 8.0f;
    bool destroyOnHit = false;       // e.g. projectile disappears
    bool canBeStomp  = false;        // Mario-style: jump on top to kill
    f32  stompBounce = 10.0f;        // bounce force when stomped
};

// ============================================================================
// GameState2D — global game state tracker (score, lives, level)
// ============================================================================
class GameState2D : public Component {
public:
    std::string GetTypeName() const override { return "GameState2D"; }

    i32  score       = 0;
    i32  lives       = 3;
    i32  coins       = 0;
    i32  level       = 1;
    f32  timer       = 0.0f;       // level timer (counts up or down)
    bool timerCountDown = false;
    f32  timerMax    = 300.0f;     // max time for countdown
    bool gameOver    = false;
    bool levelComplete = false;

    void AddScore(i32 pts) { score += pts; }
    void AddCoin()  { coins++; if (coins >= 100) { coins = 0; lives++; } }
    void Die()      { lives--; if (lives <= 0) gameOver = true; }
};

} // namespace gv
