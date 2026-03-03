// ============================================================================
// GameVoid Engine — 2D Pathfinding Implementation
// ============================================================================
#include "editor2d/Pathfinding2D.h"
#include "editor2d/Scene2D.h"
#include "editor2d/Editor2DTypes.h"
#include "core/GameObject.h"
#include <queue>
#include <unordered_set>
#include <cmath>
#include <algorithm>

namespace gv {

// ════════════════════════════════════════════════════════════════════════════
// PathGrid2D
// ════════════════════════════════════════════════════════════════════════════

void PathGrid2D::Clear() {
    cells.assign(static_cast<size_t>(gridWidth) * gridHeight, true);
}

void PathGrid2D::SetWalkable(i32 cx, i32 cy, bool walkable) {
    if (cx < 0 || cx >= gridWidth || cy < 0 || cy >= gridHeight) return;
    cells[static_cast<size_t>(cy) * gridWidth + cx] = walkable;
}

bool PathGrid2D::IsWalkable(i32 cx, i32 cy) const {
    if (cx < 0 || cx >= gridWidth || cy < 0 || cy >= gridHeight) return false;
    if (cells.empty()) return true;
    return cells[static_cast<size_t>(cy) * gridWidth + cx];
}

void PathGrid2D::WorldToCell(f32 wx, f32 wy, i32& outCX, i32& outCY) const {
    outCX = static_cast<i32>(std::floor((wx - origin.x) / cellSize));
    outCY = static_cast<i32>(std::floor((wy - origin.y) / cellSize));
}

Vec2 PathGrid2D::CellToWorld(i32 cx, i32 cy) const {
    return {
        origin.x + (static_cast<f32>(cx) + 0.5f) * cellSize,
        origin.y + (static_cast<f32>(cy) + 0.5f) * cellSize
    };
}

void PathGrid2D::RebuildFromScene(Scene2D* scene) {
    Clear();
    if (!scene) return;

    for (auto& obj : scene->GetAllObjects()) {
        if (!obj->IsActive()) continue;

        // Mark cells occupied by static colliders as blocked
        auto* col = obj->GetComponent<Collider2D>();
        auto* rb  = obj->GetComponent<RigidBody2D>();
        bool isStatic = (!rb || rb->bodyType == BodyType2D::Static);

        if (col && isStatic) {
            auto& t = obj->GetTransform();
            f32 cx = t.position.x + col->offset.x;
            f32 cy = t.position.y + col->offset.y;
            f32 hw = col->boxSize.x * t.scale.x;
            f32 hh = col->boxSize.y * t.scale.y;

            // Determine the world-space AABB of the collider
            f32 minX = cx - hw, maxX = cx + hw;
            f32 minY = cy - hh, maxY = cy + hh;

            // Convert to cell range
            i32 cMinX, cMinY, cMaxX, cMaxY;
            WorldToCell(minX, minY, cMinX, cMinY);
            WorldToCell(maxX, maxY, cMaxX, cMaxY);

            for (i32 y = cMinY; y <= cMaxY; y++) {
                for (i32 x = cMinX; x <= cMaxX; x++) {
                    SetWalkable(x, y, false);
                }
            }
        }

        // Mark tiles from tilemap components as blocked (non-empty tiles)
        auto* tm = obj->GetComponent<TileMapComponent>();
        if (tm) {
            for (i32 ty = 0; ty < tm->mapHeight; ty++) {
                for (i32 tx = 0; tx < tm->mapWidth; tx++) {
                    if (tm->GetTile(tx, ty) >= 0) {
                        // Convert tilemap cell to world, then to grid cell
                        f32 wx = obj->GetTransform().position.x + (static_cast<f32>(tx) + 0.5f) * tm->tileSize;
                        f32 wy = obj->GetTransform().position.y + (static_cast<f32>(ty) + 0.5f) * tm->tileSize;
                        i32 gcx, gcy;
                        WorldToCell(wx, wy, gcx, gcy);
                        SetWalkable(gcx, gcy, false);
                    }
                }
            }
        }
    }
}

// ── A* Implementation ──────────────────────────────────────────────────────

namespace {

struct AStarNode {
    i32 x, y;
    f32 g;          // cost from start
    f32 f;          // g + heuristic
    i32 parentX, parentY;
};

struct NodeCompare {
    bool operator()(const AStarNode& a, const AStarNode& b) const {
        return a.f > b.f;  // min-heap
    }
};

inline f32 Heuristic(i32 ax, i32 ay, i32 bx, i32 by) {
    // Manhattan distance
    return static_cast<f32>(std::abs(ax - bx) + std::abs(ay - by));
}

inline u64 PackCoord(i32 x, i32 y) {
    return (static_cast<u64>(static_cast<u32>(x)) << 32) |
            static_cast<u64>(static_cast<u32>(y));
}

} // anonymous namespace

std::vector<Vec2> PathGrid2D::FindPath(const Vec2& startWorld, const Vec2& goalWorld) const {
    std::vector<Vec2> result;

    i32 sx, sy, gx, gy;
    WorldToCell(startWorld.x, startWorld.y, sx, sy);
    WorldToCell(goalWorld.x,  goalWorld.y,  gx, gy);

    // Bounds check
    if (sx < 0 || sx >= gridWidth || sy < 0 || sy >= gridHeight) return result;
    if (gx < 0 || gx >= gridWidth || gy < 0 || gy >= gridHeight) return result;
    if (!IsWalkable(gx, gy)) return result;

    // Already there
    if (sx == gx && sy == gy) {
        result.push_back(CellToWorld(gx, gy));
        return result;
    }

    // Open set (priority queue) and closed set
    std::priority_queue<AStarNode, std::vector<AStarNode>, NodeCompare> open;
    std::unordered_map<u64, AStarNode> allNodes;
    std::unordered_set<u64> closed;

    AStarNode start;
    start.x = sx; start.y = sy;
    start.g = 0.0f;
    start.f = Heuristic(sx, sy, gx, gy);
    start.parentX = -1; start.parentY = -1;

    open.push(start);
    allNodes[PackCoord(sx, sy)] = start;

    // 4-directional (or 8 if allowDiag)
    const i32 dx4[] = { 1, -1, 0,  0 };
    const i32 dy4[] = { 0,  0, 1, -1 };
    const i32 dx8[] = { 1, -1, 0,  0, 1, -1,  1, -1 };
    const i32 dy8[] = { 0,  0, 1, -1, 1,  1, -1, -1 };
    const i32 numDirs = allowDiag ? 8 : 4;
    const i32* dxArr  = allowDiag ? dx8 : dx4;
    const i32* dyArr  = allowDiag ? dy8 : dy4;

    const i32 kMaxIterations = gridWidth * gridHeight;  // safety cap
    i32 iterations = 0;

    while (!open.empty() && iterations < kMaxIterations) {
        iterations++;

        AStarNode current = open.top();
        open.pop();

        u64 curKey = PackCoord(current.x, current.y);
        if (closed.count(curKey)) continue;
        closed.insert(curKey);

        // Goal reached — reconstruct path
        if (current.x == gx && current.y == gy) {
            // Backtrack
            std::vector<Vec2> reversed;
            i32 cx = current.x, cy = current.y;
            while (cx != -1 && cy != -1) {
                reversed.push_back(CellToWorld(cx, cy));
                u64 key = PackCoord(cx, cy);
                auto it = allNodes.find(key);
                if (it == allNodes.end()) break;
                i32 px = it->second.parentX;
                i32 py = it->second.parentY;
                cx = px; cy = py;
            }
            // Reverse to get start→goal order
            result.reserve(reversed.size());
            for (auto it2 = reversed.rbegin(); it2 != reversed.rend(); ++it2)
                result.push_back(*it2);
            return result;
        }

        // Expand neighbours
        for (i32 d = 0; d < numDirs; d++) {
            i32 nx = current.x + dxArr[d];
            i32 ny = current.y + dyArr[d];

            if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
            if (!IsWalkable(nx, ny)) continue;

            u64 nKey = PackCoord(nx, ny);
            if (closed.count(nKey)) continue;

            f32 moveCost = (dxArr[d] != 0 && dyArr[d] != 0) ? 1.414f : 1.0f;
            f32 tentG = current.g + moveCost;

            auto it = allNodes.find(nKey);
            if (it != allNodes.end() && tentG >= it->second.g) continue;

            AStarNode neighbor;
            neighbor.x = nx; neighbor.y = ny;
            neighbor.g = tentG;
            neighbor.f = tentG + Heuristic(nx, ny, gx, gy);
            neighbor.parentX = current.x;
            neighbor.parentY = current.y;

            allNodes[nKey] = neighbor;
            open.push(neighbor);
        }
    }

    return result;  // no path found
}

// ════════════════════════════════════════════════════════════════════════════
// PathAgent2D
// ════════════════════════════════════════════════════════════════════════════

void PathAgent2D::RequestPath(const PathGrid2D& grid, const Vec2& currentPos) {
    path = grid.FindPath(currentPos, target);
    currentWaypoint = 0;
    hasPath = !path.empty();
    arrived = !hasPath;
}

void PathAgent2D::FollowPath(f32 dt, RigidBody2D* rb) {
    if (!hasPath || arrived || path.empty()) return;
    if (currentWaypoint >= static_cast<i32>(path.size())) {
        arrived = true;
        hasPath = false;
        if (rb) { rb->velocity = { 0, 0 }; }
        return;
    }

    GameObject* owner = GetOwner();
    if (!owner) return;

    Vec2 wp = path[currentWaypoint];
    Vec2 pos(owner->GetTransform().position.x, owner->GetTransform().position.y);

    f32 dx = wp.x - pos.x;
    f32 dy = wp.y - pos.y;
    f32 dist = std::sqrt(dx * dx + dy * dy);

    if (dist < stoppingDist) {
        // Reached waypoint — advance
        currentWaypoint++;
        if (currentWaypoint >= static_cast<i32>(path.size())) {
            arrived = true;
            hasPath = false;
            if (rb) { rb->velocity = { 0, 0 }; }
        }
        return;
    }

    // Move toward waypoint
    f32 invDist = 1.0f / dist;
    f32 dirX = dx * invDist;
    f32 dirY = dy * invDist;

    if (rb) {
        rb->velocity.x = dirX * moveSpeed;
        rb->velocity.y = dirY * moveSpeed;
    } else {
        owner->GetTransform().position.x += dirX * moveSpeed * dt;
        owner->GetTransform().position.y += dirY * moveSpeed * dt;
    }
}

void PathAgent2D::ClearPath() {
    path.clear();
    currentWaypoint = 0;
    hasPath = false;
    arrived = true;
}

} // namespace gv
