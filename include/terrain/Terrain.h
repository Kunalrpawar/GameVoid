// ============================================================================
// GameVoid Engine — Procedural Terrain System
// ============================================================================
// Heightmap-based terrain with Perlin noise, brush editing, multi-texture
// painting, and collision support.
// ============================================================================
#pragma once

#include "core/Types.h"
#include "core/Math.h"
#include "core/Component.h"
#include <vector>
#include <string>

namespace gv {

// ── Perlin Noise ───────────────────────────────────────────────────────────
/// Simple 2D Perlin-style noise used for procedural terrain generation.
class PerlinNoise {
public:
    PerlinNoise(u32 seed = 42);

    /// Returns value in roughly [-1, 1] for continuous (x,y).
    f32 Noise2D(f32 x, f32 y) const;

    /// Fractal Brownian Motion — multiple octaves layered on top.
    f32 FBM(f32 x, f32 y, i32 octaves = 6, f32 lacunarity = 2.0f,
            f32 persistence = 0.5f) const;

private:
    u8 m_Perm[512];
    static f32 Fade(f32 t);
    static f32 Lerp(f32 a, f32 b, f32 t);
    static f32 Grad(i32 hash, f32 x, f32 y);
};

// ── Terrain Brush ──────────────────────────────────────────────────────────
enum class TerrainBrushMode { Raise, Lower, Smooth, Flatten, Paint };

struct TerrainBrush {
    TerrainBrushMode mode     = TerrainBrushMode::Raise;
    f32              radius   = 3.0f;
    f32              strength = 0.4f;
    i32              paintLayer = 0;   // 0=grass, 1=rock, 2=sand, 3=snow
};

// ── Terrain Vertex ─────────────────────────────────────────────────────────
struct TerrainVertex {
    f32 px, py, pz;     // position
    f32 nx, ny, nz;     // normal
    f32 u, v;           // texture coord
    f32 w0, w1, w2, w3; // texture weights (grass, rock, sand, snow)
};

// ── Terrain Component ──────────────────────────────────────────────────────
/// Attach to a GameObject to give it terrain behaviour.
/// Contains the heightmap, generates the mesh, supports brush editing.
class TerrainComponent : public Component {
public:
    TerrainComponent() = default;
    ~TerrainComponent() override = default;

    std::string GetTypeName() const override { return "Terrain"; }

    // ── Generation ─────────────────────────────────────────────────────────
    /// Generate a new terrain with the given resolution and noise parameters.
    void Generate(u32 resolution = 64, f32 worldSize = 40.0f,
                  f32 maxHeight = 6.0f, u32 seed = 42, i32 octaves = 6);

    /// Regenerate mesh from current heightmap (after brush edits).
    void RebuildMesh();

    // ── Brush editing ──────────────────────────────────────────────────────
    /// Apply a brush stroke at world-space (wx, wz).
    void ApplyBrush(f32 wx, f32 wz, const TerrainBrush& brush, f32 dt);

    // ── Collision ──────────────────────────────────────────────────────────
    /// Get interpolated height at world (wx, wz). Returns 0 if outside bounds.
    f32 GetHeightAt(f32 wx, f32 wz) const;

    /// Get surface normal at world (wx, wz).
    Vec3 GetNormalAt(f32 wx, f32 wz) const;

    // ── GPU resources ──────────────────────────────────────────────────────
    u32 GetVAO() const { return m_VAO; }
    u32 GetIndexCount() const { return m_IndexCount; }
    bool HasMesh() const { return m_VAO != 0; }

    // ── Accessors ──────────────────────────────────────────────────────────
    u32 GetResolution() const { return m_Resolution; }
    f32 GetWorldSize()  const { return m_WorldSize; }
    f32 GetMaxHeight()  const { return m_MaxHeight; }
    const std::vector<f32>& GetHeightmap() const { return m_Heightmap; }
    const std::vector<f32>& GetSplatmap()  const { return m_Splatmap; }

private:
    void UploadMesh();
    void ComputeNormals();

    u32 m_Resolution = 0;     // heightmap size (res+1 x res+1 vertices)
    f32 m_WorldSize  = 40.0f;
    f32 m_MaxHeight  = 6.0f;

    std::vector<f32> m_Heightmap;  // (res+1)*(res+1) heights
    std::vector<f32> m_Splatmap;   // (res+1)*(res+1)*4 paint weights (RGBA = grass/rock/sand/snow)
    std::vector<TerrainVertex> m_Vertices;
    std::vector<u32> m_Indices;

    // GPU handles
    u32 m_VAO = 0, m_VBO = 0, m_EBO = 0;
    u32 m_IndexCount = 0;
};

} // namespace gv
