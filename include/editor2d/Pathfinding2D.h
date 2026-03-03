// ============================================================================
// GameVoid Engine — 2D Pathfinding (A* Grid)
// ============================================================================
// Grid-based A* pathfinding for 2D scenes. Use it for enemy AI, NPC
// navigation, tower-defence creeps, or any entity that needs to find a
// route across a tile grid while avoiding obstacles.
//
// Usage:
//   1. Attach a PathGrid2D component to a "manager" object.
//      Set the grid dimensions and call RebuildFromScene() to auto-mark
//      blocked cells from colliders / tilemap data.
//   2. Attach a PathAgent2D component to every entity that should navigate.
//      Set targetX / targetY (world coords) and call RequestPath().
//   3. Each frame, call FollowPath(dt) to move the agent along the path.
//
// The grid is a flat array of booleans (walkable / blocked).  A* with
// Manhattan heuristic is used.  Diagonal movement is optional.
// ============================================================================
#pragma once

#include "core/Types.h"
#include "core/Math.h"
#include "core/Component.h"
#include <vector>
#include <string>

namespace gv {

// Forward
class Scene2D;
class RigidBody2D;

// ════════════════════════════════════════════════════════════════════════════
// PathGrid2D — the navigation grid (attach to one "manager" object)
// ════════════════════════════════════════════════════════════════════════════
class PathGrid2D : public Component {
public:
    std::string GetTypeName() const override { return "PathGrid2D"; }

    // ── Grid settings ──────────────────────────────────────────────────────
    i32  gridWidth    = 32;          // cells in X
    i32  gridHeight   = 32;          // cells in Y
    f32  cellSize     = 1.0f;        // world-unit size of each cell
    Vec2 origin       { -16, -16 };  // world position of bottom-left corner
    bool allowDiag    = false;       // allow 8-directional movement

    // ── Grid data ──────────────────────────────────────────────────────────
    /// true = walkable, false = blocked
    std::vector<bool> cells;

    // ── Helpers ────────────────────────────────────────────────────────────

    /// Resize and clear the grid (all walkable).
    void Clear();

    /// Mark a single cell as blocked or walkable.
    void SetWalkable(i32 cellX, i32 cellY, bool walkable);

    /// Query walkability.
    bool IsWalkable(i32 cellX, i32 cellY) const;

    /// Convert world position to cell index.
    void WorldToCell(f32 worldX, f32 worldY, i32& outCX, i32& outCY) const;

    /// Convert cell index to world center position.
    Vec2 CellToWorld(i32 cellX, i32 cellY) const;

    /// Auto-fill blocked cells by scanning scene colliders and tilemap.
    void RebuildFromScene(Scene2D* scene);

    /// Run A* and return the path as a sequence of world positions.
    /// Returns an empty vector if no path exists.
    std::vector<Vec2> FindPath(const Vec2& startWorld, const Vec2& goalWorld) const;
};

// ════════════════════════════════════════════════════════════════════════════
// PathAgent2D — attach to entities that navigate (enemies, NPCs, etc.)
// ════════════════════════════════════════════════════════════════════════════
class PathAgent2D : public Component {
public:
    std::string GetTypeName() const override { return "PathAgent2D"; }

    // ── Settings ───────────────────────────────────────────────────────────
    f32  moveSpeed      = 4.0f;      // world units / second
    f32  stoppingDist   = 0.15f;     // how close before we consider "arrived"
    f32  repathInterval = 0.5f;      // seconds between automatic re-paths
    bool autoRepath     = true;      // automatically re-path on interval

    // ── Target ─────────────────────────────────────────────────────────────
    Vec2 target         { 0, 0 };    // destination in world coords

    // ── Runtime state ──────────────────────────────────────────────────────
    std::vector<Vec2> path;          // current path waypoints
    i32  currentWaypoint = 0;
    bool hasPath         = false;
    bool arrived         = true;
    f32  repathTimer     = 0.0f;

    /// Request a new path from a PathGrid2D.  Pass the owning object's
    /// current position.
    void RequestPath(const PathGrid2D& grid, const Vec2& currentPos);

    /// Move along the stored path.  Writes directly into the owner's
    /// transform (or into a RigidBody2D velocity if present).
    void FollowPath(f32 dt, RigidBody2D* rb = nullptr);

    /// Cancel the current path.
    void ClearPath();
};

} // namespace gv
