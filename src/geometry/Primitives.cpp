#include "geometry/Primitives.h"
#include <cmath>
#include <algorithm>

static const float kPi = 3.14159265358979323846f;

// ── Helper utilities ─────────────────────────────────────────────────────────

void MeshData::RecalcFlatNormals() {
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        auto& v0 = vertices[indices[i]];
        auto& v1 = vertices[indices[i+1]];
        auto& v2 = vertices[indices[i+2]];
        Vec3 e0 = Vec3(v1.position.x-v0.position.x, v1.position.y-v0.position.y, v1.position.z-v0.position.z);
        Vec3 e1 = Vec3(v2.position.x-v0.position.x, v2.position.y-v0.position.y, v2.position.z-v0.position.z);
        Vec3 n; // cross product
        n.x = e0.y*e1.z - e0.z*e1.y;
        n.y = e0.z*e1.x - e0.x*e1.z;
        n.z = e0.x*e1.y - e0.y*e1.x;
        float len = std::sqrt(n.x*n.x + n.y*n.y + n.z*n.z);
        if (len > 1e-8f) { n.x /= len; n.y /= len; n.z /= len; }
        v0.normal = v1.normal = v2.normal = n;
    }
}

void MeshData::RecalcSmoothNormals() {
    for (auto& v : vertices) v.normal = Vec3(0,0,0);
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        uint32_t i0 = indices[i], i1 = indices[i+1], i2 = indices[i+2];
        Vec3 p0 = vertices[i0].position, p1 = vertices[i1].position, p2 = vertices[i2].position;
        Vec3 e0{p1.x-p0.x, p1.y-p0.y, p1.z-p0.z};
        Vec3 e1{p2.x-p0.x, p2.y-p0.y, p2.z-p0.z};
        Vec3 n{e0.y*e1.z-e0.z*e1.y, e0.z*e1.x-e0.x*e1.z, e0.x*e1.y-e0.y*e1.x};
        for (auto idx : {i0, i1, i2}) {
            vertices[idx].normal.x += n.x;
            vertices[idx].normal.y += n.y;
            vertices[idx].normal.z += n.z;
        }
    }
    for (auto& v : vertices) {
        float len = std::sqrt(v.normal.x*v.normal.x + v.normal.y*v.normal.y + v.normal.z*v.normal.z);
        if (len > 1e-8f) { v.normal.x /= len; v.normal.y /= len; v.normal.z /= len; }
    }
}

void MeshData::FlipNormals() {
    for (auto& v : vertices) { v.normal.x = -v.normal.x; v.normal.y = -v.normal.y; v.normal.z = -v.normal.z; }
    for (size_t i = 0; i + 2 < indices.size(); i += 3) std::swap(indices[i+1], indices[i+2]);
}

void MeshData::ComputeAABB(Vec3& outMin, Vec3& outMax) const {
    outMin = outMax = vertices.empty() ? Vec3(0,0,0) : vertices[0].position;
    for (auto& v : vertices) {
        outMin.x = std::min(outMin.x, v.position.x); outMin.y = std::min(outMin.y, v.position.y); outMin.z = std::min(outMin.z, v.position.z);
        outMax.x = std::max(outMax.x, v.position.x); outMax.y = std::max(outMax.y, v.position.y); outMax.z = std::max(outMax.z, v.position.z);
    }
}

// ── Box ───────────────────────────────────────────────────────────────────────

MeshData Primitives::Box(float hx, float hy, float hz, int /*subdivisions*/) {
    MeshData m; m.name = "Box";
    // 6 faces × 4 verts = 24 verts, 6 faces × 2 tris = 36 indices
    const Vec3 norms[6] = {{0,0,1},{0,0,-1},{-1,0,0},{1,0,0},{0,1,0},{0,-1,0}};
    const Vec3 tangs[6] = {{1,0,0},{-1,0,0},{0,0,-1},{0,0,1},{1,0,0},{1,0,0}};
    // Positions for each face (CCW winding from outside)
    float px[6][4][3] = {
        {{-hx,-hy, hz},{ hx,-hy, hz},{ hx, hy, hz},{-hx, hy, hz}}, // +Z
        {{ hx,-hy,-hz},{-hx,-hy,-hz},{-hx, hy,-hz},{ hx, hy,-hz}}, // -Z
        {{-hx,-hy,-hz},{-hx,-hy, hz},{-hx, hy, hz},{-hx, hy,-hz}}, // -X
        {{ hx,-hy, hz},{ hx,-hy,-hz},{ hx, hy,-hz},{ hx, hy, hz}}, // +X
        {{-hx, hy, hz},{ hx, hy, hz},{ hx, hy,-hz},{-hx, hy,-hz}}, // +Y
        {{-hx,-hy,-hz},{ hx,-hy,-hz},{ hx,-hy, hz},{-hx,-hy, hz}}  // -Y
    };
    float uvs[4][2] = {{0,0},{1,0},{1,1},{0,1}};
    for (int f = 0; f < 6; f++) {
        uint32_t base = (uint32_t)m.vertices.size();
        for (int v = 0; v < 4; v++) {
            Vertex3D vert;
            vert.position = Vec3(px[f][v][0], px[f][v][1], px[f][v][2]);
            vert.normal   = norms[f];
            vert.uv       = Vec2(uvs[v][0], uvs[v][1]);
            m.vertices.push_back(vert);
        }
        m.indices.insert(m.indices.end(), {base,base+1,base+2, base,base+2,base+3});
    }
    return m;
}

// ── Sphere ────────────────────────────────────────────────────────────────────

MeshData Primitives::Sphere(float radius, int latSegs, int lonSegs) {
    MeshData m; m.name = "Sphere";
    for (int lat = 0; lat <= latSegs; lat++) {
        float theta = kPi * lat / (float)latSegs;
        float sinT = std::sin(theta), cosT = std::cos(theta);
        for (int lon = 0; lon <= lonSegs; lon++) {
            float phi = 2.0f * kPi * lon / (float)lonSegs;
            Vertex3D v;
            v.normal   = Vec3(sinT*std::cos(phi), cosT, sinT*std::sin(phi));
            v.position = Vec3(v.normal.x*radius, v.normal.y*radius, v.normal.z*radius);
            v.uv       = Vec2((float)lon/lonSegs, (float)lat/latSegs);
            m.vertices.push_back(v);
        }
    }
    for (int lat = 0; lat < latSegs; lat++) {
        for (int lon = 0; lon < lonSegs; lon++) {
            uint32_t cur  = lat*(lonSegs+1) + lon;
            uint32_t next = cur + lonSegs + 1;
            m.indices.insert(m.indices.end(), {cur, next, cur+1, cur+1, next, next+1});
        }
    }
    return m;
}

// ── Cylinder ──────────────────────────────────────────────────────────────────

MeshData Primitives::Cylinder(float rTop, float rBot, float height, int segs, bool caps) {
    MeshData m; m.name = "Cylinder";
    float halfH = height * 0.5f;
    for (int i = 0; i <= segs; i++) {
        float a = 2.0f*kPi*i/segs;
        float ca = std::cos(a), sa = std::sin(a);
        Vertex3D vb, vt;
        vb.position = Vec3(rBot*ca, -halfH, rBot*sa);
        vb.normal   = Vec3(ca, 0, sa);
        vb.uv       = Vec2((float)i/segs, 0);
        vt.position = Vec3(rTop*ca,  halfH, rTop*sa);
        vt.normal   = Vec3(ca, 0, sa);
        vt.uv       = Vec2((float)i/segs, 1);
        m.vertices.push_back(vb); m.vertices.push_back(vt);
    }
    for (int i = 0; i < segs; i++) {
        uint32_t b0=2*i, t0=2*i+1, b1=2*i+2, t1=2*i+3;
        m.indices.insert(m.indices.end(), {b0,b1,t0, t0,b1,t1});
    }
    if (caps) {
        // Bottom cap
        auto addCap = [&](float y, float r, bool flip) {
            uint32_t ctr = (uint32_t)m.vertices.size();
            Vertex3D c; c.position = Vec3(0,y,0); c.normal = Vec3(0,flip?-1.f:1.f,0); c.uv = Vec2(0.5f,0.5f);
            m.vertices.push_back(c);
            uint32_t first = (uint32_t)m.vertices.size();
            for (int i = 0; i < segs; i++) {
                float a = 2*kPi*i/segs;
                Vertex3D v; v.position = Vec3(r*std::cos(a),y,r*std::sin(a));
                v.normal = Vec3(0,flip?-1.f:1.f,0); v.uv = Vec2(0.5f+0.5f*std::cos(a), 0.5f+0.5f*std::sin(a));
                m.vertices.push_back(v);
            }
            for (int i = 0; i < segs; i++) {
                uint32_t a = first+i, b = first+(i+1)%segs;
                if (flip) m.indices.insert(m.indices.end(), {ctr,b,a});
                else      m.indices.insert(m.indices.end(), {ctr,a,b});
            }
        };
        addCap(-halfH, rBot, true);
        addCap( halfH, rTop, false);
    }
    return m;
}

// ── Cone ──────────────────────────────────────────────────────────────────────

MeshData Primitives::Cone(float radius, float height, int segs) {
    return Cylinder(0.0f, radius, height, segs, true);
}

// ── Plane ─────────────────────────────────────────────────────────────────────

MeshData Primitives::Plane(float w, float d, int sx, int sz) {
    MeshData m; m.name = "Plane";
    for (int iz = 0; iz <= sz; iz++) {
        for (int ix = 0; ix <= sx; ix++) {
            Vertex3D v;
            v.position = Vec3((ix/(float)sx - 0.5f)*w, 0, (iz/(float)sz - 0.5f)*d);
            v.normal   = Vec3(0,1,0);
            v.uv       = Vec2((float)ix/sx, (float)iz/sz);
            m.vertices.push_back(v);
        }
    }
    for (int iz = 0; iz < sz; iz++) {
        for (int ix = 0; ix < sx; ix++) {
            uint32_t a=(iz*(sx+1)+ix), b=a+1, c=a+(sx+1), d2=c+1;
            m.indices.insert(m.indices.end(), {a,b,c, b,d2,c});
        }
    }
    return m;
}

// ── Torus ─────────────────────────────────────────────────────────────────────

MeshData Primitives::Torus(float R, float r, int maj, int min_) {
    MeshData m; m.name = "Torus";
    for (int i = 0; i <= maj; i++) {
        float u = 2*kPi*i/maj;
        for (int j = 0; j <= min_; j++) {
            float v = 2*kPi*j/min_;
            Vertex3D vert;
            vert.position = Vec3((R+r*std::cos(v))*std::cos(u), r*std::sin(v), (R+r*std::cos(v))*std::sin(u));
            Vec3 center(R*std::cos(u), 0, R*std::sin(u));
            vert.normal = Vec3(vert.position.x-center.x, vert.position.y-center.y, vert.position.z-center.z);
            float len = std::sqrt(vert.normal.x*vert.normal.x + vert.normal.y*vert.normal.y + vert.normal.z*vert.normal.z);
            if (len > 1e-8f) { vert.normal.x/=len; vert.normal.y/=len; vert.normal.z/=len; }
            vert.uv = Vec2((float)i/maj, (float)j/min_);
            m.vertices.push_back(vert);
        }
    }
    for (int i = 0; i < maj; i++) {
        for (int j = 0; j < min_; j++) {
            uint32_t a=(i*(min_+1)+j), b=a+1, c=((i+1)*(min_+1)+j), d2=c+1;
            m.indices.insert(m.indices.end(), {a,c,b, b,c,d2});
        }
    }
    return m;
}

// ── Capsule (simplified: cylinder + 2 hemispheres) ────────────────────────────

MeshData Primitives::Capsule(float radius, float height, int segs) {
    // Cylinder body
    MeshData m = Cylinder(radius, radius, height, segs, false);
    m.name = "Capsule";
    // Top hemisphere
    MeshData topHemi = Sphere(radius, segs/2, segs);
    for (auto& v : topHemi.vertices) v.position.y += height * 0.5f;
    uint32_t offset = (uint32_t)m.vertices.size();
    for (auto& v : topHemi.vertices) m.vertices.push_back(v);
    for (auto idx : topHemi.indices) m.indices.push_back(idx + offset);
    // Bottom hemisphere
    MeshData botHemi = Sphere(radius, segs/2, segs);
    for (auto& v : botHemi.vertices) { v.position.y = -v.position.y - height*0.5f; v.normal.y = -v.normal.y; }
    offset = (uint32_t)m.vertices.size();
    for (auto& v : botHemi.vertices) m.vertices.push_back(v);
    for (auto idx : botHemi.indices) m.indices.push_back(idx + offset);
    return m;
}

// ── Pyramid ───────────────────────────────────────────────────────────────────

MeshData Primitives::Pyramid(float bw, float bd, float h) {
    MeshData m; m.name = "Pyramid";
    // Base + 4 sides
    Vec3 apex(0, h, 0);
    Vec3 corners[4] = {{-bw/2,0,-bd/2},{bw/2,0,-bd/2},{bw/2,0,bd/2},{-bw/2,0,bd/2}};
    // Base quad
    for (int i=0;i<4;i++) { Vertex3D v; v.position=corners[i]; v.normal=Vec3(0,-1,0); v.uv=Vec2(i%2?1.f:0.f,i>1?1.f:0.f); m.vertices.push_back(v); }
    m.indices.insert(m.indices.end(), {0,3,2, 0,2,1});
    // Sides
    for (int i=0;i<4;i++) {
        Vec3 a=corners[i], b=corners[(i+1)%4];
        Vec3 e1{b.x-a.x,b.y-a.y,b.z-a.z}, e2{apex.x-a.x,apex.y-a.y,apex.z-a.z};
        Vec3 n{e1.y*e2.z-e1.z*e2.y, e1.z*e2.x-e1.x*e2.z, e1.x*e2.y-e1.y*e2.x};
        float len=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z); if(len>1e-8f){n.x/=len;n.y/=len;n.z/=len;}
        uint32_t base=(uint32_t)m.vertices.size();
        Vertex3D va,vb,vc;
        va.position=a; va.normal=n; va.uv=Vec2(0,0);
        vb.position=b; vb.normal=n; vb.uv=Vec2(1,0);
        vc.position=apex; vc.normal=n; vc.uv=Vec2(0.5f,1);
        m.vertices.insert(m.vertices.end(),{va,vb,vc});
        m.indices.insert(m.indices.end(),{base,base+1,base+2});
    }
    return m;
}

// ── Disk ───────────────────────────────────────────────────────────────────────

MeshData Primitives::Disk(float outerR, float innerR, int segs) {
    MeshData m; m.name = "Disk";
    bool ring = (innerR > 0.01f);
    if (!ring) {
        uint32_t ctr=(uint32_t)m.vertices.size();
        Vertex3D c; c.position=Vec3(0,0,0); c.normal=Vec3(0,1,0); c.uv=Vec2(0.5f,0.5f);
        m.vertices.push_back(c);
        uint32_t first=(uint32_t)m.vertices.size();
        for (int i=0;i<segs;i++) {
            float a=2*kPi*i/segs;
            Vertex3D v; v.position=Vec3(outerR*std::cos(a),0,outerR*std::sin(a));
            v.normal=Vec3(0,1,0); v.uv=Vec2(0.5f+0.5f*std::cos(a),0.5f+0.5f*std::sin(a));
            m.vertices.push_back(v);
        }
        for (int i=0;i<segs;i++) m.indices.insert(m.indices.end(),{ctr,first+(uint32_t)i,first+(uint32_t)((i+1)%segs)});
    } else {
        for (int i=0;i<=segs;i++) {
            float a=2*kPi*i/segs;
            Vertex3D vo,vi;
            vo.position=Vec3(outerR*std::cos(a),0,outerR*std::sin(a)); vo.normal=Vec3(0,1,0); vo.uv=Vec2(std::cos(a),std::sin(a));
            vi.position=Vec3(innerR*std::cos(a),0,innerR*std::sin(a)); vi.normal=Vec3(0,1,0); vi.uv=Vec2(0.5f*std::cos(a),0.5f*std::sin(a));
            m.vertices.push_back(vo); m.vertices.push_back(vi);
        }
        for (int i=0;i<segs;i++) {
            uint32_t o0=2*i, i0=2*i+1, o1=2*i+2, i1=2*i+3;
            m.indices.insert(m.indices.end(),{o0,o1,i0, i0,o1,i1});
        }
    }
    return m;
}

// ── Arrow ─────────────────────────────────────────────────────────────────────

MeshData Primitives::Arrow(float shaftR, float headR, float length, float headFrac) {
    MeshData m; m.name = "Arrow";
    float shaftLen = length*(1.0f-headFrac);
    float headLen  = length*headFrac;
    MeshData shaft = Cylinder(shaftR, shaftR, shaftLen, 8, true);
    for (auto& v : shaft.vertices) v.position.y += shaftLen * 0.5f;
    m.vertices = shaft.vertices; m.indices = shaft.indices;
    MeshData head = Cone(headR, headLen, 12);
    uint32_t offset=(uint32_t)m.vertices.size();
    for (auto& v : head.vertices) v.position.y += shaftLen + headLen*0.5f;
    for (auto& v : head.vertices) m.vertices.push_back(v);
    for (auto idx : head.indices) m.indices.push_back(idx+offset);
    return m;
}
