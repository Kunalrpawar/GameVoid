// ============================================================================
// GameVoid Engine — Terrain Utilities Implementation
// ============================================================================
#include "terrain/TerrainUtils.h"
#include "terrain/Terrain.h"
#include <cmath>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <random>

namespace gv {

// ── Heightmap export (raw 16-bit little-endian) ────────────────────────────
bool TerrainUtils::ExportHeightmapRAW16(const TerrainComponent& terrain,
                                         const std::string& path) {
    const auto& hm = terrain.GetHeightmap();
    if (hm.empty()) return false;

    std::ofstream ofs(path, std::ios::binary);
    if (!ofs.is_open()) return false;

    f32 maxH = terrain.GetMaxHeight();
    if (maxH < 0.001f) maxH = 1.0f;

    for (f32 h : hm) {
        f32 norm = h / maxH;
        if (norm < 0.0f) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;
        u16 v = static_cast<u16>(norm * 65535.0f);
        ofs.write(reinterpret_cast<const char*>(&v), 2);
    }
    return true;
}

// ── Heightmap import (raw 16-bit little-endian) ────────────────────────────
bool TerrainUtils::ImportHeightmapRAW16(TerrainComponent& terrain,
                                         const std::string& path,
                                         f32 heightScale) {
    auto& hm = const_cast<std::vector<f32>&>(terrain.GetHeightmap());
    if (hm.empty()) return false;

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) return false;

    for (size_t i = 0; i < hm.size(); ++i) {
        u16 v = 0;
        ifs.read(reinterpret_cast<char*>(&v), 2);
        if (!ifs) break;
        hm[i] = (static_cast<f32>(v) / 65535.0f) * terrain.GetMaxHeight() * heightScale;
    }
    terrain.RebuildMesh();
    return true;
}

// ── Thermal erosion ────────────────────────────────────────────────────────
void TerrainUtils::ThermalErosion(TerrainComponent& terrain,
                                   i32 iterations, f32 talus) {
    auto& hm = const_cast<std::vector<f32>&>(terrain.GetHeightmap());
    u32 res = terrain.GetResolution();
    u32 sz  = res + 1;
    f32 cellSize = terrain.GetWorldSize() / static_cast<f32>(res);

    // Neighbor offsets (4-connected)
    const i32 dx[] = { -1, 1, 0, 0 };
    const i32 dy[] = { 0, 0, -1, 1 };

    for (i32 iter = 0; iter < iterations; ++iter) {
        for (u32 y = 1; y < sz - 1; ++y) {
            for (u32 x = 1; x < sz - 1; ++x) {
                f32 h = hm[y * sz + x];
                f32 maxDiff = 0.0f;
                f32 totalDiff = 0.0f;
                i32 count = 0;

                for (int n = 0; n < 4; ++n) {
                    u32 nx = x + dx[n];
                    u32 ny = y + dy[n];
                    f32 diff = h - hm[ny * sz + nx];
                    if (diff > talus * cellSize) {
                        totalDiff += diff;
                        if (diff > maxDiff) maxDiff = diff;
                        count++;
                    }
                }

                if (count > 0) {
                    f32 transfer = maxDiff * 0.5f / static_cast<f32>(count);
                    for (int n = 0; n < 4; ++n) {
                        u32 nx = x + dx[n];
                        u32 ny = y + dy[n];
                        f32 diff = h - hm[ny * sz + nx];
                        if (diff > talus * cellSize) {
                            hm[ny * sz + nx] += transfer;
                            hm[y * sz + x]   -= transfer;
                        }
                    }
                }
            }
        }
    }
    terrain.RebuildMesh();
}

// ── Hydraulic erosion (simplified droplet-based) ───────────────────────────
void TerrainUtils::HydraulicErosion(TerrainComponent& terrain,
                                     i32 droplets,
                                     f32 erosionRate,
                                     f32 depositionRate,
                                     i32 lifetime) {
    auto& hm = const_cast<std::vector<f32>&>(terrain.GetHeightmap());
    u32 res = terrain.GetResolution();
    u32 sz  = res + 1;
    f32 cellSize = terrain.GetWorldSize() / static_cast<f32>(res);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> distX(1.0f, static_cast<float>(sz - 2));
    std::uniform_real_distribution<float> distY(1.0f, static_cast<float>(sz - 2));

    for (i32 d = 0; d < droplets; ++d) {
        f32 px = distX(rng);
        f32 py = distY(rng);
        f32 sediment = 0.0f;
        f32 water = 1.0f;
        f32 speed = 0.0f;

        for (i32 step = 0; step < lifetime; ++step) {
            i32 ix = static_cast<i32>(px);
            i32 iy = static_cast<i32>(py);
            if (ix < 1 || iy < 1 || ix >= static_cast<i32>(sz) - 1 || iy >= static_cast<i32>(sz) - 1)
                break;

            // Compute gradient
            f32 h   = hm[iy * sz + ix];
            f32 hR  = hm[iy * sz + ix + 1];
            f32 hU  = hm[(iy + 1) * sz + ix];
            f32 gx  = h - hR;
            f32 gy  = h - hU;

            // Move
            f32 len = std::sqrt(gx * gx + gy * gy);
            if (len < 0.0001f) break;
            gx /= len; gy /= len;

            speed = std::sqrt(speed * speed + len);
            px += gx;
            py += gy;

            // Check bounds
            i32 nx = static_cast<i32>(px);
            i32 ny = static_cast<i32>(py);
            if (nx < 1 || ny < 1 || nx >= static_cast<i32>(sz) - 1 || ny >= static_cast<i32>(sz) - 1)
                break;

            f32 newH = hm[ny * sz + nx];
            f32 diff = h - newH;

            if (diff > 0) {
                // Going downhill → erode
                f32 erode = std::min(diff, erosionRate * speed * water);
                hm[iy * sz + ix] -= erode;
                sediment += erode;
            } else {
                // Going uphill → deposit
                f32 deposit = std::min(sediment, -diff);
                deposit = std::min(deposit, depositionRate);
                hm[ny * sz + nx] += deposit;
                sediment -= deposit;
            }

            water *= 0.99f;  // evaporation
            if (water < 0.01f) break;
        }
    }
    terrain.RebuildMesh();
}

// ── Auto-paint splatmap ────────────────────────────────────────────────────
void TerrainUtils::AutoPaintSplatmap(TerrainComponent& terrain,
                                      f32 waterLine, f32 snowLine,
                                      f32 slopeThreshold) {
    auto& splat = const_cast<std::vector<f32>&>(terrain.GetSplatmap());
    const auto& hm = terrain.GetHeightmap();
    u32 res = terrain.GetResolution();
    u32 sz  = res + 1;
    f32 maxH = terrain.GetMaxHeight();
    if (maxH < 0.001f) maxH = 1.0f;

    for (u32 y = 0; y < sz; ++y) {
        for (u32 x = 0; x < sz; ++x) {
            u32 idx = (y * sz + x) * 4;
            f32 h = hm[y * sz + x] / maxH;  // normalized 0..1

            // Compute slope from neighbors
            f32 slope = 0.0f;
            if (x > 0 && x < sz - 1 && y > 0 && y < sz - 1) {
                f32 dhdx = hm[y * sz + x + 1] - hm[y * sz + x - 1];
                f32 dhdy = hm[(y + 1) * sz + x] - hm[(y - 1) * sz + x];
                slope = std::sqrt(dhdx * dhdx + dhdy * dhdy);
            }

            f32 grass = 0, rock = 0, sand = 0, snow = 0;

            if (slope > slopeThreshold) {
                rock = 1.0f;   // steep = rock
            } else if (h < waterLine) {
                sand = 1.0f;   // low = sand
            } else if (h > snowLine) {
                snow = 1.0f;   // high = snow
            } else {
                grass = 1.0f;  // default = grass
            }

            // Soft blend at transitions
            f32 total = grass + rock + sand + snow;
            if (total > 0) { grass /= total; rock /= total; sand /= total; snow /= total; }

            if (idx + 3 < splat.size()) {
                splat[idx + 0] = grass;
                splat[idx + 1] = rock;
                splat[idx + 2] = sand;
                splat[idx + 3] = snow;
            }
        }
    }
    terrain.RebuildMesh();
}

// ── Flatten ────────────────────────────────────────────────────────────────
void TerrainUtils::Flatten(TerrainComponent& terrain, f32 height) {
    auto& hm = const_cast<std::vector<f32>&>(terrain.GetHeightmap());
    for (auto& h : hm) h = height;
    terrain.RebuildMesh();
}

// ── Smooth all ─────────────────────────────────────────────────────────────
void TerrainUtils::SmoothAll(TerrainComponent& terrain, i32 passes) {
    auto& hm = const_cast<std::vector<f32>&>(terrain.GetHeightmap());
    u32 res = terrain.GetResolution();
    u32 sz  = res + 1;
    std::vector<f32> tmp(hm.size());

    for (i32 pass = 0; pass < passes; ++pass) {
        for (u32 y = 0; y < sz; ++y) {
            for (u32 x = 0; x < sz; ++x) {
                f32 sum = 0;
                i32 count = 0;
                for (i32 dy = -1; dy <= 1; ++dy) {
                    for (i32 dx = -1; dx <= 1; ++dx) {
                        i32 nx = static_cast<i32>(x) + dx;
                        i32 ny = static_cast<i32>(y) + dy;
                        if (nx >= 0 && ny >= 0 && nx < static_cast<i32>(sz) && ny < static_cast<i32>(sz)) {
                            sum += hm[ny * sz + nx];
                            count++;
                        }
                    }
                }
                tmp[y * sz + x] = sum / static_cast<f32>(count);
            }
        }
        hm = tmp;
    }
    terrain.RebuildMesh();
}

// ── Add noise ──────────────────────────────────────────────────────────────
void TerrainUtils::AddNoise(TerrainComponent& terrain, f32 amplitude, u32 seed) {
    auto& hm = const_cast<std::vector<f32>&>(terrain.GetHeightmap());
    PerlinNoise noise(seed);
    u32 res = terrain.GetResolution();
    u32 sz  = res + 1;
    f32 scale = 4.0f / static_cast<f32>(sz);

    for (u32 y = 0; y < sz; ++y) {
        for (u32 x = 0; x < sz; ++x) {
            hm[y * sz + x] += noise.FBM(x * scale, y * scale, 4) * amplitude;
        }
    }
    terrain.RebuildMesh();
}

} // namespace gv
