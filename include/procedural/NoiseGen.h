#pragma once
#include "core/Math.h"
#include <vector>
#include <string>
#include <functional>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
//  NoiseGen.h  —  2D / 3D noise functions for procedural generation
// ─────────────────────────────────────────────────────────────────────────────

namespace Noise {

    // Classic Perlin noise,  range approx [-1, +1]
    float Perlin2D(float x, float y);
    float Perlin3D(float x, float y, float z);

    // Simplex noise (faster than Perlin, fewer directional artefacts)
    float Simplex2D(float x, float y);
    float Simplex3D(float x, float y, float z);

    // Fractional Brownian Motion: sum of N octaves of noise
    //   x, y        — input coordinates
    //   octaves     — number of layers
    //   lacunarity  — frequency multiplier per octave (typically 2.0)
    //   persistence — amplitude multiplier per octave (typically 0.5)
    float FBM2D(float x, float y, int octaves = 6,
                float lacunarity = 2.0f, float persistence = 0.5f);

    float FBM3D(float x, float y, float z, int octaves = 6,
                float lacunarity = 2.0f, float persistence = 0.5f);

    // Voronoi / Worley noise (cellular), returns distance to nearest seed
    float Worley2D(float x, float y, int seed = 42);
    float Worley3D(float x, float y, float z, int seed = 42);

    // Ridged multi-fractal (sharp mountain-ridge terrain)
    float RidgedMF2D(float x, float y, int octaves = 6);

    // Domain-warped noise (Inigo Quilez style)
    float DomainWarp2D(float x, float y, float strength = 1.0f, int octaves = 4);

} // namespace Noise

// ─────────────────────────────────────────────────────────────────────────────
//  HeightmapGen  —  Generate terrain heightmaps using noise combinations
// ─────────────────────────────────────────────────────────────────────────────

struct HeightmapConfig {
    int   width      = 256;
    int   depth      = 256;
    float scale      = 0.01f;  // noise coordinate scale (lower = zoomed out)
    float amplitude  = 50.0f;  // max terrain height in world units
    int   octaves    = 6;
    float lacunarity = 2.0f;
    float persistence= 0.5f;
    int   seed       = 0;
    enum class Mode { Perlin, FBM, Ridged, DomainWarp } mode = Mode::FBM;
};

class HeightmapGen {
public:
    explicit HeightmapGen(const HeightmapConfig& cfg);

    // Generate a flat array [width * depth] of height values [0..1]
    std::vector<float> Generate() const;

    // Apply erosion passes to the raw heights in-place
    static void ApplyHydraulicErosion(std::vector<float>& heights,
                                       int width, int depth, int iterations = 50);

    // Sample at fractional grid coordinates using bilinear interpolation
    static float Sample(const std::vector<float>& heights,
                         int width, int depth, float u, float v);

private:
    HeightmapConfig m_Cfg;
};

// ─────────────────────────────────────────────────────────────────────────────
//  LSystem.h  —  Deterministic Lindenmayer systems for organic shapes
// ─────────────────────────────────────────────────────────────────────────────

struct LRule { char symbol; std::string replacement; };

class LSystem {
public:
    std::string axiom;
    std::vector<LRule> rules;
    int   iterations = 4;
    float angle      = 25.0f;  // degrees per + / - turn

    // Expand axiom applying rules for N iterations
    std::string Expand() const;

    // Interpret the string as a 3D turtle and produce line segments
    struct Segment { Vec3 from; Vec3 to; float thickness; };
    std::vector<Segment> Interpret(const std::string& str,
                                    Vec3 startPos = Vec3(0,0,0),
                                    float startLen = 1.0f) const;

    // Pre-defined plant templates
    static LSystem SimplePlant();
    static LSystem FractalTree();
    static LSystem Bush();
    static LSystem Koch();      // Koch snowflake curve
};
