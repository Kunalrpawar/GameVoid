#include "procedural/NoiseGen.h"
#include <cmath>
#include <vector>
#include <algorithm>

// ─── Permutation table (classic Perlin) ──────────────────────────────────────

static int perm[512];
static bool permInit = false;

static void InitPerm() {
    if (permInit) return;
    int p[256];
    for (int i = 0; i < 256; i++) p[i] = i;
    for (int i = 255; i > 0; i--) {
        int j = (i * 1103515245 + 12345) & 0xFF;
        std::swap(p[i], p[j]);
    }
    for (int i = 0; i < 512; i++) perm[i] = p[i & 255];
    permInit = true;
}

static float fade(float t)  { return t*t*t*(t*(t*6-15)+10); }
static float lerp(float a, float b, float t) { return a + t*(b-a); }
static float grad(int h, float x, float y, float z) {
    h &= 15;
    float u = h<8 ? x:y, v = h<4 ? y : (h==12||h==14 ? x:z);
    return ((h&1)?-u:u) + ((h&2)?-v:v);
}

float Noise::Perlin3D(float x, float y, float z) {
    InitPerm();
    int X=(int)floor(x)&255, Y=(int)floor(y)&255, Z=(int)floor(z)&255;
    x-=floor(x); y-=floor(y); z-=floor(z);
    float u=fade(x), v=fade(y), w=fade(z);
    int A=perm[X]+Y, AA=perm[A]+Z, AB=perm[A+1]+Z;
    int B=perm[X+1]+Y, BA=perm[B]+Z, BB=perm[B+1]+Z;
    return lerp(
        lerp(lerp(grad(perm[AA],x,y,z),grad(perm[BA],x-1,y,z),u),
             lerp(grad(perm[AB],x,y-1,z),grad(perm[BB],x-1,y-1,z),u),v),
        lerp(lerp(grad(perm[AA+1],x,y,z-1),grad(perm[BA+1],x-1,y,z-1),u),
             lerp(grad(perm[AB+1],x,y-1,z-1),grad(perm[BB+1],x-1,y-1,z-1),u),v),w);
}

float Noise::Perlin2D(float x, float y) { return Perlin3D(x, y, 0); }

float Noise::Simplex2D(float x, float y) { return Perlin2D(x, y) * 0.9f; }  // simplified alias
float Noise::Simplex3D(float x, float y, float z) { return Perlin3D(x, y, z) * 0.9f; }

float Noise::FBM2D(float x, float y, int oct, float lac, float pers) {
    float val=0, amp=1, freq=1, max=0;
    for (int i=0;i<oct;i++) { max+=amp; val+=Perlin2D(x*freq,y*freq)*amp; freq*=lac; amp*=pers; }
    return val / max;
}

float Noise::FBM3D(float x, float y, float z, int oct, float lac, float pers) {
    float val=0, amp=1, freq=1, max=0;
    for (int i=0;i<oct;i++) { max+=amp; val+=Perlin3D(x*freq,y*freq,z*freq)*amp; freq*=lac; amp*=pers; }
    return val / max;
}

float Noise::Worley2D(float x, float y, int seed) {
    int ix=(int)floor(x), iy=(int)floor(y); float minD=1e9f;
    for (int dy=-1;dy<=1;dy++) for (int dx=-1;dx<=1;dx++) {
        int cx=ix+dx, cy=iy+dy;
        float hx = (float)((cx*seed*1103515245+cy*22695477+12345)&0xFFFF)/65535.0f;
        float hy = (float)((cy*seed*1103515245+cx*22695477+54321)&0xFFFF)/65535.0f;
        float px=cx+hx-x, py=cy+hy-y; float d=px*px+py*py; if(d<minD) minD=d;
    }
    return std::sqrt(minD);
}

float Noise::Worley3D(float x, float y, float z, int seed) { return Worley2D(x+z*0.7f, y+z*0.3f, seed); }

float Noise::RidgedMF2D(float x, float y, int oct) {
    float val=0, amp=0.5f, freq=1;
    for (int i=0;i<oct;i++) { float n=1.0f-std::abs(Perlin2D(x*freq,y*freq)); val+=n*n*amp; freq*=2; amp*=0.5f; }
    return val;
}

float Noise::DomainWarp2D(float x, float y, float str, int oct) {
    float wx = FBM2D(x, y, oct), wy = FBM2D(x+5.2f, y+1.3f, oct);
    return FBM2D(x+str*wx, y+str*wy, oct);
}

// ─── HeightmapGen ─────────────────────────────────────────────────────────────

HeightmapGen::HeightmapGen(const HeightmapConfig& c) : m_Cfg(c) {}

std::vector<float> HeightmapGen::Generate() const {
    std::vector<float> h(m_Cfg.width * m_Cfg.depth);
    for (int z=0;z<m_Cfg.depth;z++) for (int x=0;x<m_Cfg.width;x++) {
        float fx = (x + m_Cfg.seed) * m_Cfg.scale;
        float fz = (z + m_Cfg.seed) * m_Cfg.scale;
        float v = Noise::FBM2D(fx, fz, m_Cfg.octaves, m_Cfg.lacunarity, m_Cfg.persistence);
        h[z*m_Cfg.width+x] = (v * 0.5f + 0.5f);
    }
    return h;
}

void HeightmapGen::ApplyHydraulicErosion(std::vector<float>& h, int W, int D, int iters) {
    for (int iter=0; iter<iters; iter++) {
        for (int z=1;z<D-1;z++) for (int x=1;x<W-1;x++) {
            float cur = h[z*W+x];
            float nbrs[4] = {h[(z-1)*W+x], h[(z+1)*W+x], h[z*W+x-1], h[z*W+x+1]};
            float lowest = *std::min_element(nbrs, nbrs+4);
            if (lowest < cur) h[z*W+x] -= (cur - lowest) * 0.02f;
        }
    }
}

float HeightmapGen::Sample(const std::vector<float>& h, int W, int D, float u, float v) {
    float fx = u*(W-1), fz = v*(D-1);
    int x0=(int)fx, z0=(int)fz; float tx=fx-x0, tz=fz-z0;
    x0 = std::max(0, std::min(x0, W-2)); z0 = std::max(0, std::min(z0, D-2));
    float a=h[z0*W+x0], b=h[z0*W+x0+1], c=h[(z0+1)*W+x0], d2=h[(z0+1)*W+x0+1];
    return a*(1-tx)*(1-tz) + b*tx*(1-tz) + c*(1-tx)*tz + d2*tx*tz;
}

// ─── LSystem ──────────────────────────────────────────────────────────────────

std::string LSystem::Expand() const {
    std::string s = axiom;
    for (int i=0;i<iterations;i++) {
        std::string next;
        for (char c : s) {
            bool found=false;
            for (auto& r : rules) if (r.symbol==c) { next += r.replacement; found=true; break; }
            if (!found) next += c;
        }
        s = next;
    }
    return s;
}

std::vector<LSystem::Segment> LSystem::Interpret(const std::string& str, Vec3 pos, float len) const {
    std::vector<Segment> segs;
    // Turtle state stack
    struct State { Vec3 pos; float yaw, pitch, thickness; };
    std::vector<State> stack;
    float yaw = 0, pitch = 90.0f, thickness = 0.05f;
    static const float kPiL = 3.14159265f;
    auto forward = [&]() -> Vec3 {
        float r = pitch*kPiL/180.0f, y = yaw*kPiL/180.0f;
        return {std::cos(r)*std::sin(y), std::sin(r), std::cos(r)*std::cos(y)};
    };
    for (char c : str) {
        if (c=='F'||c=='G') {
            Vec3 dir = forward(); Vec3 next = {pos.x+dir.x*len, pos.y+dir.y*len, pos.z+dir.z*len};
            segs.push_back({pos, next, thickness}); pos = next;
        } else if (c=='+') { yaw += angle; } else if (c=='-') { yaw -= angle; }
        else if (c=='&') { pitch -= angle; } else if (c=='^') { pitch += angle; }
        else if (c=='[') { stack.push_back({pos, yaw, pitch, thickness}); }
        else if (c==']' && !stack.empty()) { auto& st=stack.back(); pos=st.pos; yaw=st.yaw; pitch=st.pitch; thickness=st.thickness; stack.pop_back(); }
    }
    return segs;
}

LSystem LSystem::SimplePlant() {
    LSystem l; l.axiom="X"; l.iterations=5; l.angle=25.0f;
    l.rules = {{'X',"F+[[X]-X]-F[-FX]+X"},{'F',"FF"}};
    return l;
}
LSystem LSystem::FractalTree() {
    LSystem l; l.axiom="F"; l.iterations=5; l.angle=25.0f;
    l.rules = {{'F',"FF+[+F-F-F]-[-F+F+F]"}};
    return l;
}
LSystem LSystem::Bush() {
    LSystem l; l.axiom="Y"; l.iterations=4; l.angle=22.5f;
    l.rules = {{'X',"X[-FFF][+FFF]FX"},{'Y',"YFX[+Y][-Y]"}};
    return l;
}
LSystem LSystem::Koch() {
    LSystem l; l.axiom="F"; l.iterations=4; l.angle=60.0f;
    l.rules = {{'F',"F+F--F+F"}};
    return l;
}
