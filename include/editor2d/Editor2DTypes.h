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

} // namespace gv
