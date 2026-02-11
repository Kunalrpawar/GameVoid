// ============================================================================
// GameVoid Engine — Terrain System Implementation
// ============================================================================
#include "terrain/Terrain.h"
#include <cmath>
#include <cstring>
#include <algorithm>

#ifdef GV_HAS_GLFW
#include "core/GLDefs.h"
#endif

namespace gv {

// ============================================================================
// Perlin Noise
// ============================================================================
PerlinNoise::PerlinNoise(u32 seed) {
    // Fisher-Yates shuffle of 0..255
    for (u32 i = 0; i < 256; ++i) m_Perm[i] = static_cast<u8>(i);
    u32 s = seed;
    for (u32 i = 255; i > 0; --i) {
        s = s * 1664525u + 1013904223u; // LCG
        u32 j = s % (i + 1);
        u8 tmp = m_Perm[i];
        m_Perm[i] = m_Perm[j];
        m_Perm[j] = tmp;
    }
    for (u32 i = 0; i < 256; ++i) m_Perm[256 + i] = m_Perm[i];
}

f32 PerlinNoise::Fade(f32 t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

f32 PerlinNoise::Lerp(f32 a, f32 b, f32 t) {
    return a + t * (b - a);
}

f32 PerlinNoise::Grad(i32 hash, f32 x, f32 y) {
    i32 h = hash & 7;
    f32 u = (h < 4) ? x : y;
    f32 v = (h < 4) ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

f32 PerlinNoise::Noise2D(f32 x, f32 y) const {
    i32 xi = static_cast<i32>(std::floor(x)) & 255;
    i32 yi = static_cast<i32>(std::floor(y)) & 255;
    f32 xf = x - std::floor(x);
    f32 yf = y - std::floor(y);
    f32 u = Fade(xf);
    f32 v = Fade(yf);

    i32 aa = m_Perm[m_Perm[xi] + yi];
    i32 ab = m_Perm[m_Perm[xi] + yi + 1];
    i32 ba = m_Perm[m_Perm[xi + 1] + yi];
    i32 bb = m_Perm[m_Perm[xi + 1] + yi + 1];

    f32 x1 = Lerp(Grad(aa, xf, yf), Grad(ba, xf - 1, yf), u);
    f32 x2 = Lerp(Grad(ab, xf, yf - 1), Grad(bb, xf - 1, yf - 1), u);
    return Lerp(x1, x2, v);
}

f32 PerlinNoise::FBM(f32 x, f32 y, i32 octaves, f32 lacunarity, f32 persistence) const {
    f32 total = 0, amp = 1, freq = 1, maxAmp = 0;
    for (i32 i = 0; i < octaves; ++i) {
        total += Noise2D(x * freq, y * freq) * amp;
        maxAmp += amp;
        amp *= persistence;
        freq *= lacunarity;
    }
    return total / maxAmp;
}

// ============================================================================
// TerrainComponent
// ============================================================================
void TerrainComponent::Generate(u32 resolution, f32 worldSize, f32 maxHeight,
                                 u32 seed, i32 octaves) {
    m_Resolution = resolution;
    m_WorldSize  = worldSize;
    m_MaxHeight  = maxHeight;

    u32 verts = (resolution + 1);
    u32 vertCount = verts * verts;
    m_Heightmap.resize(vertCount, 0.0f);
    m_Splatmap.resize(vertCount * 4, 0.0f);

    PerlinNoise noise(seed);
    f32 scale = 4.0f; // noise frequency scale

    for (u32 z = 0; z <= resolution; ++z) {
        for (u32 x = 0; x <= resolution; ++x) {
            f32 nx = static_cast<f32>(x) / static_cast<f32>(resolution) * scale;
            f32 nz = static_cast<f32>(z) / static_cast<f32>(resolution) * scale;
            f32 h = noise.FBM(nx, nz, octaves) * 0.5f + 0.5f; // remap to 0..1
            m_Heightmap[z * verts + x] = h * maxHeight;

            // Automatic splatmap: grass below 30%, rock 30-60%, sand/snow above
            u32 idx = (z * verts + x) * 4;
            if (h < 0.3f) {
                m_Splatmap[idx + 0] = 1.0f; // grass
            } else if (h < 0.6f) {
                f32 t = (h - 0.3f) / 0.3f;
                m_Splatmap[idx + 0] = 1.0f - t;
                m_Splatmap[idx + 1] = t;    // rock
            } else if (h < 0.85f) {
                f32 t = (h - 0.6f) / 0.25f;
                m_Splatmap[idx + 1] = 1.0f - t;
                m_Splatmap[idx + 2] = t;    // sand
            } else {
                m_Splatmap[idx + 3] = 1.0f; // snow
            }
        }
    }

    RebuildMesh();
}

void TerrainComponent::RebuildMesh() {
    if (m_Resolution == 0) return;

    u32 verts = m_Resolution + 1;
    m_Vertices.resize(verts * verts);
    m_Indices.clear();
    m_Indices.reserve(m_Resolution * m_Resolution * 6);

    f32 cellSize = m_WorldSize / static_cast<f32>(m_Resolution);
    f32 halfSize = m_WorldSize * 0.5f;

    // Build vertices
    for (u32 z = 0; z < verts; ++z) {
        for (u32 x = 0; x < verts; ++x) {
            u32 i = z * verts + x;
            TerrainVertex& tv = m_Vertices[i];
            tv.px = static_cast<f32>(x) * cellSize - halfSize;
            tv.py = m_Heightmap[i];
            tv.pz = static_cast<f32>(z) * cellSize - halfSize;
            tv.u  = static_cast<f32>(x) / static_cast<f32>(m_Resolution);
            tv.v  = static_cast<f32>(z) / static_cast<f32>(m_Resolution);

            // splatmap weights
            u32 si = i * 4;
            tv.w0 = m_Splatmap[si + 0];
            tv.w1 = m_Splatmap[si + 1];
            tv.w2 = m_Splatmap[si + 2];
            tv.w3 = m_Splatmap[si + 3];
        }
    }

    // Compute normals
    ComputeNormals();

    // Build indices (two triangles per cell)
    for (u32 z = 0; z < m_Resolution; ++z) {
        for (u32 x = 0; x < m_Resolution; ++x) {
            u32 tl = z * verts + x;
            u32 tr = tl + 1;
            u32 bl = (z + 1) * verts + x;
            u32 br = bl + 1;
            m_Indices.push_back(tl);
            m_Indices.push_back(bl);
            m_Indices.push_back(tr);
            m_Indices.push_back(tr);
            m_Indices.push_back(bl);
            m_Indices.push_back(br);
        }
    }

    m_IndexCount = static_cast<u32>(m_Indices.size());
    UploadMesh();
}

void TerrainComponent::ComputeNormals() {
    u32 verts = m_Resolution + 1;
    f32 cellSize = m_WorldSize / static_cast<f32>(m_Resolution);
    for (u32 z = 0; z < verts; ++z) {
        for (u32 x = 0; x < verts; ++x) {
            f32 hL = (x > 0)              ? m_Heightmap[z * verts + x - 1] : m_Heightmap[z * verts + x];
            f32 hR = (x < m_Resolution)   ? m_Heightmap[z * verts + x + 1] : m_Heightmap[z * verts + x];
            f32 hD = (z > 0)              ? m_Heightmap[(z - 1) * verts + x] : m_Heightmap[z * verts + x];
            f32 hU = (z < m_Resolution)   ? m_Heightmap[(z + 1) * verts + x] : m_Heightmap[z * verts + x];

            Vec3 n;
            n.x = hL - hR;
            n.y = 2.0f * cellSize;
            n.z = hD - hU;
            f32 len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
            if (len > 0.0001f) { n.x /= len; n.y /= len; n.z /= len; }

            TerrainVertex& tv = m_Vertices[z * verts + x];
            tv.nx = n.x; tv.ny = n.y; tv.nz = n.z;
        }
    }
}

void TerrainComponent::UploadMesh() {
#ifdef GV_HAS_GLFW
    if (m_VAO == 0) {
        glGenVertexArrays(1, &m_VAO);
        glGenBuffers(1, &m_VBO);
        glGenBuffers(1, &m_EBO);
    }

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<long>(m_Vertices.size() * sizeof(TerrainVertex)),
                 m_Vertices.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<long>(m_Indices.size() * sizeof(u32)),
                 m_Indices.data(), GL_STATIC_DRAW);

    // Position (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex),
                          reinterpret_cast<void*>(0));
    // Normal (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex),
                          reinterpret_cast<void*>(3 * sizeof(f32)));
    // UV (location 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex),
                          reinterpret_cast<void*>(6 * sizeof(f32)));
    // Splat weights (location 3)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex),
                          reinterpret_cast<void*>(8 * sizeof(f32)));

    glBindVertexArray(0);
#endif
}

void TerrainComponent::ApplyBrush(f32 wx, f32 wz, const TerrainBrush& brush, f32 dt) {
    if (m_Resolution == 0) return;

    u32 verts = m_Resolution + 1;
    f32 halfSize = m_WorldSize * 0.5f;
    f32 cellSize = m_WorldSize / static_cast<f32>(m_Resolution);

    // Convert world pos to grid coords
    f32 gx = (wx + halfSize) / cellSize;
    f32 gz = (wz + halfSize) / cellSize;

    f32 gridRadius = brush.radius / cellSize;
    i32 minX = std::max(0, static_cast<i32>(gx - gridRadius));
    i32 maxX = std::min(static_cast<i32>(m_Resolution), static_cast<i32>(gx + gridRadius));
    i32 minZ = std::max(0, static_cast<i32>(gz - gridRadius));
    i32 maxZ = std::min(static_cast<i32>(m_Resolution), static_cast<i32>(gz + gridRadius));

    for (i32 z = minZ; z <= maxZ; ++z) {
        for (i32 x = minX; x <= maxX; ++x) {
            f32 dx = static_cast<f32>(x) - gx;
            f32 dz = static_cast<f32>(z) - gz;
            f32 dist = std::sqrt(dx * dx + dz * dz);
            if (dist > gridRadius) continue;

            f32 falloff = 1.0f - (dist / gridRadius);
            falloff = falloff * falloff; // quadratic falloff
            f32 effect = brush.strength * falloff * dt;

            u32 idx = static_cast<u32>(z) * verts + static_cast<u32>(x);

            switch (brush.mode) {
            case TerrainBrushMode::Raise:
                m_Heightmap[idx] += effect;
                break;
            case TerrainBrushMode::Lower:
                m_Heightmap[idx] -= effect;
                break;
            case TerrainBrushMode::Flatten: {
                f32 target = m_Heightmap[static_cast<u32>(gz) * verts + static_cast<u32>(gx)];
                f32 diff = target - m_Heightmap[idx];
                m_Heightmap[idx] += diff * effect * 0.5f;
                break;
            }
            case TerrainBrushMode::Smooth: {
                f32 avg = 0; i32 count = 0;
                for (i32 sz = z - 1; sz <= z + 1; ++sz) {
                    for (i32 sx = x - 1; sx <= x + 1; ++sx) {
                        if (sx >= 0 && sx <= static_cast<i32>(m_Resolution) &&
                            sz >= 0 && sz <= static_cast<i32>(m_Resolution)) {
                            avg += m_Heightmap[static_cast<u32>(sz) * verts + static_cast<u32>(sx)];
                            count++;
                        }
                    }
                }
                if (count > 0) {
                    avg /= static_cast<f32>(count);
                    m_Heightmap[idx] += (avg - m_Heightmap[idx]) * effect;
                }
                break;
            }
            case TerrainBrushMode::Paint: {
                u32 si = idx * 4;
                // Reduce all layers, boost selected
                for (i32 l = 0; l < 4; ++l) {
                    if (l == brush.paintLayer)
                        m_Splatmap[si + l] = std::min(1.0f, m_Splatmap[si + l] + effect);
                    else
                        m_Splatmap[si + l] = std::max(0.0f, m_Splatmap[si + l] - effect * 0.3f);
                }
                // Normalize
                f32 total = m_Splatmap[si] + m_Splatmap[si+1] + m_Splatmap[si+2] + m_Splatmap[si+3];
                if (total > 0.001f) {
                    for (i32 l = 0; l < 4; ++l) m_Splatmap[si + l] /= total;
                }
                break;
            }
            }
        }
    }

    RebuildMesh();
}

f32 TerrainComponent::GetHeightAt(f32 wx, f32 wz) const {
    if (m_Resolution == 0) return 0;
    u32 verts = m_Resolution + 1;
    f32 halfSize = m_WorldSize * 0.5f;
    f32 cellSize = m_WorldSize / static_cast<f32>(m_Resolution);

    f32 gx = (wx + halfSize) / cellSize;
    f32 gz = (wz + halfSize) / cellSize;
    if (gx < 0 || gz < 0 || gx >= static_cast<f32>(m_Resolution) || gz >= static_cast<f32>(m_Resolution))
        return 0;

    u32 x0 = static_cast<u32>(gx);
    u32 z0 = static_cast<u32>(gz);
    u32 x1 = std::min(x0 + 1, m_Resolution);
    u32 z1 = std::min(z0 + 1, m_Resolution);

    f32 fx = gx - static_cast<f32>(x0);
    f32 fz = gz - static_cast<f32>(z0);

    f32 h00 = m_Heightmap[z0 * verts + x0];
    f32 h10 = m_Heightmap[z0 * verts + x1];
    f32 h01 = m_Heightmap[z1 * verts + x0];
    f32 h11 = m_Heightmap[z1 * verts + x1];

    f32 h0 = h00 + (h10 - h00) * fx;
    f32 h1 = h01 + (h11 - h01) * fx;
    return h0 + (h1 - h0) * fz;
}

Vec3 TerrainComponent::GetNormalAt(f32 wx, f32 wz) const {
    f32 eps = m_WorldSize / static_cast<f32>(m_Resolution) * 0.5f;
    f32 hL = GetHeightAt(wx - eps, wz);
    f32 hR = GetHeightAt(wx + eps, wz);
    f32 hD = GetHeightAt(wx, wz - eps);
    f32 hU = GetHeightAt(wx, wz + eps);

    Vec3 n;
    n.x = hL - hR;
    n.y = 2.0f * eps;
    n.z = hD - hU;
    return n.Normalized();
}

} // namespace gv
