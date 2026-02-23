// ============================================================================
// GameVoid Engine — Terrain Utilities
// ============================================================================
// Extra terrain tools beyond the base TerrainComponent:
//   - Heightmap import/export (raw 16-bit, PNG grayscale)
//   - Thermal erosion simulation
//   - Hydraulic erosion (simplified)
//   - Terrain LOD settings
//   - Splat-map auto-painting based on slope/height
// ============================================================================
#pragma once

#include "core/Types.h"
#include "core/Math.h"
#include "terrain/Terrain.h"
#include <vector>
#include <string>

namespace gv {

class TerrainComponent;  // forward

// ============================================================================
// TerrainUtils — static utility functions for terrain manipulation
// ============================================================================
class TerrainUtils {
public:
    // ── Heightmap I/O ──────────────────────────────────────────────────────

    /// Export heightmap as a raw 16-bit grayscale file.
    static bool ExportHeightmapRAW16(const TerrainComponent& terrain,
                                      const std::string& path);

    /// Import a raw 16-bit grayscale file into terrain.
    /// Resolution must match (res+1)^2 16-bit samples.
    static bool ImportHeightmapRAW16(TerrainComponent& terrain,
                                      const std::string& path,
                                      f32 heightScale = 1.0f);

    // ── Erosion ────────────────────────────────────────────────────────────

    /// Apply simple thermal erosion for the given number of iterations.
    /// talus = max slope angle (radians) before material slides downhill.
    static void ThermalErosion(TerrainComponent& terrain,
                                i32 iterations = 30,
                                f32 talus = 0.05f);

    /// Simplified hydraulic erosion: drops rain particles that erode and deposit.
    static void HydraulicErosion(TerrainComponent& terrain,
                                  i32 droplets = 5000,
                                  f32 erosionRate = 0.01f,
                                  f32 depositionRate = 0.005f,
                                  i32 lifetime = 60);

    // ── Auto-paint ─────────────────────────────────────────────────────────

    /// Automatically paint splat-map layers based on height and slope:
    ///  - Layer 0 (grass): low slope, medium height
    ///  - Layer 1 (rock):  steep slopes
    ///  - Layer 2 (sand):  low altitude near water-line
    ///  - Layer 3 (snow):  high altitude
    static void AutoPaintSplatmap(TerrainComponent& terrain,
                                   f32 waterLine = 0.1f,
                                   f32 snowLine  = 0.75f,
                                   f32 slopeThreshold = 0.6f);

    // ── Flatten / smooth tools ─────────────────────────────────────────────

    /// Set entire heightmap to a flat height.
    static void Flatten(TerrainComponent& terrain, f32 height = 0.0f);

    /// Apply a box-blur smoothing pass over the entire heightmap.
    static void SmoothAll(TerrainComponent& terrain, i32 passes = 1);

    /// Add random noise on top of existing heightmap.
    static void AddNoise(TerrainComponent& terrain, f32 amplitude = 0.5f, u32 seed = 12345);
};

} // namespace gv
