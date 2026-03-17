#include "mesh/MeshOps.h"
#include <cmath>
#include <algorithm>
#include <unordered_map>

namespace MeshOps {

MeshData Combine(const std::vector<MeshData>& meshes) {
    MeshData out;
    for (auto& m : meshes) {
        uint32_t off = (uint32_t)out.vertices.size();
        for (auto& v : m.vertices) out.vertices.push_back(v);
        for (auto idx : m.indices) out.indices.push_back(idx + off);
    }
    return out;
}

MeshData CombineTransformed(const std::vector<std::pair<MeshData,Mat4>>& pairs) {
    MeshData out;
    for (auto& [mesh, mat] : pairs) {
        uint32_t off = (uint32_t)out.vertices.size();
        for (auto v : mesh.vertices) {
            // Simple mat4 × vec3 (ignores rotation for now, just scale+translate)
            v.position.x = mat[0][0]*v.position.x + mat[3][0];
            v.position.y = mat[1][1]*v.position.y + mat[3][1];
            v.position.z = mat[2][2]*v.position.z + mat[3][2];
            out.vertices.push_back(v);
        }
        for (auto idx : mesh.indices) out.indices.push_back(idx + off);
    }
    return out;
}

MeshData Weld(const MeshData& mesh, float tol) {
    MeshData out;
    std::vector<uint32_t> remap(mesh.vertices.size());
    for (size_t i = 0; i < mesh.vertices.size(); i++) {
        uint32_t found = (uint32_t)out.vertices.size();
        for (uint32_t j = 0; j < (uint32_t)out.vertices.size(); j++) {
            Vec3 d{mesh.vertices[i].position.x-out.vertices[j].position.x,
                   mesh.vertices[i].position.y-out.vertices[j].position.y,
                   mesh.vertices[i].position.z-out.vertices[j].position.z};
            if (d.x*d.x+d.y*d.y+d.z*d.z < tol*tol) { found=j; break; }
        }
        if (found == (uint32_t)out.vertices.size()) out.vertices.push_back(mesh.vertices[i]);
        remap[i] = found;
    }
    for (auto idx : mesh.indices) out.indices.push_back(remap[idx]);
    return out;
}

MeshData SubdivideMidpoint(const MeshData& mesh, int passes) {
    MeshData cur = mesh;
    for (int p = 0; p < passes; p++) {
        MeshData next;
        next.vertices = cur.vertices;
        for (size_t i = 0; i + 2 < cur.indices.size(); i += 3) {
            uint32_t i0=cur.indices[i], i1=cur.indices[i+1], i2=cur.indices[i+2];
            Vertex3D m01, m12, m20;
            auto midV = [](const Vertex3D& a, const Vertex3D& b) -> Vertex3D {
                Vertex3D v;
                v.position = {(a.position.x+b.position.x)*0.5f,(a.position.y+b.position.y)*0.5f,(a.position.z+b.position.z)*0.5f};
                v.normal   = {(a.normal.x+b.normal.x)*0.5f,(a.normal.y+b.normal.y)*0.5f,(a.normal.z+b.normal.z)*0.5f};
                v.uv       = {(a.uv.x+b.uv.x)*0.5f,(a.uv.y+b.uv.y)*0.5f};
                return v;
            };
            m01 = midV(cur.vertices[i0], cur.vertices[i1]);
            m12 = midV(cur.vertices[i1], cur.vertices[i2]);
            m20 = midV(cur.vertices[i2], cur.vertices[i0]);
            uint32_t a=(uint32_t)next.vertices.size(); next.vertices.push_back(m01);
            uint32_t b=(uint32_t)next.vertices.size(); next.vertices.push_back(m12);
            uint32_t c=(uint32_t)next.vertices.size(); next.vertices.push_back(m20);
            next.indices.insert(next.indices.end(), {i0,a,c, a,i1,b, c,b,i2, a,b,c});
        }
        cur = std::move(next);
    }
    cur.RecalcSmoothNormals();
    return cur;
}

MeshData SubdivideLoop(const MeshData& mesh, int passes) { return SubdivideMidpoint(mesh, passes); }
MeshData SubdivideCatmullClark(const MeshData& mesh, int passes) { return SubdivideMidpoint(mesh, passes); }

MeshData LaplacianSmooth(const MeshData& mesh, float strength, int iters, bool /*pin*/) {
    MeshData out = mesh;
    for (int it = 0; it < iters; it++) {
        std::vector<Vec3> avg(out.vertices.size(), Vec3(0,0,0));
        std::vector<int>  cnt(out.vertices.size(), 0);
        for (size_t i = 0; i + 2 < out.indices.size(); i += 3) {
            uint32_t i0=out.indices[i], i1=out.indices[i+1], i2=out.indices[i+2];
            for (auto [from, to] : std::initializer_list<std::pair<uint32_t,uint32_t>>{{i0,i1},{i0,i2},{i1,i0},{i1,i2},{i2,i0},{i2,i1}}) {
                avg[from].x += out.vertices[to].position.x;
                avg[from].y += out.vertices[to].position.y;
                avg[from].z += out.vertices[to].position.z;
                cnt[from]++;
            }
        }
        for (size_t i = 0; i < out.vertices.size(); i++) {
            if (cnt[i] == 0) continue;
            float n = (float)cnt[i];
            Vec3 a{avg[i].x/n, avg[i].y/n, avg[i].z/n};
            out.vertices[i].position.x = out.vertices[i].position.x*(1-strength) + a.x*strength;
            out.vertices[i].position.y = out.vertices[i].position.y*(1-strength) + a.y*strength;
            out.vertices[i].position.z = out.vertices[i].position.z*(1-strength) + a.z*strength;
        }
    }
    out.RecalcSmoothNormals();
    return out;
}

MeshData Decimate(const MeshData& mesh, int /*target*/) { return mesh; }  // stub

MeshData ExtrudeNormals(const MeshData& mesh, float amount) {
    MeshData out = mesh;
    for (auto& v : out.vertices) {
        v.position.x += v.normal.x * amount;
        v.position.y += v.normal.y * amount;
        v.position.z += v.normal.z * amount;
    }
    return out;
}

MeshData SweepProfile(const std::vector<Vec2>& /*profile*/, const std::vector<PathPoint>& /*path*/, bool) { return {}; }

MeshData ClipByPlane(const MeshData& mesh, Vec3 n, Vec3 p) {
    MeshData out;
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        uint32_t i0=mesh.indices[i], i1=mesh.indices[i+1], i2=mesh.indices[i+2];
        auto& v0=mesh.vertices[i0]; auto& v1=mesh.vertices[i1]; auto& v2=mesh.vertices[i2];
        float d0=(v0.position.x-p.x)*n.x+(v0.position.y-p.y)*n.y+(v0.position.z-p.z)*n.z;
        float d1=(v1.position.x-p.x)*n.x+(v1.position.y-p.y)*n.y+(v1.position.z-p.z)*n.z;
        float d2=(v2.position.x-p.x)*n.x+(v2.position.y-p.y)*n.y+(v2.position.z-p.z)*n.z;
        if (d0 >= 0 && d1 >= 0 && d2 >= 0) {
            uint32_t base = (uint32_t)out.vertices.size();
            out.vertices.insert(out.vertices.end(), {v0,v1,v2});
            out.indices.insert(out.indices.end(), {base,base+1,base+2});
        }
    }
    return out;
}

MeshData ProjectUVsXZ(const MeshData& mesh, float tU, float tV) {
    MeshData out = mesh;
    for (auto& v : out.vertices) v.uv = Vec2(v.position.x*tU, v.position.z*tV);
    return out;
}

MeshData ProjectUVsBox(const MeshData& mesh, float s) {
    MeshData out = mesh;
    for (auto& v : out.vertices) {
        float ax=std::abs(v.normal.x), ay=std::abs(v.normal.y), az=std::abs(v.normal.z);
        if (ax>=ay && ax>=az) v.uv = Vec2(v.position.z*s, v.position.y*s);
        else if (ay>=ax && ay>=az) v.uv = Vec2(v.position.x*s, v.position.z*s);
        else v.uv = Vec2(v.position.x*s, v.position.y*s);
    }
    return out;
}

MeshData TransformVertices(const MeshData& mesh, const Mat4& m) {
    MeshData out = mesh;
    for (auto& v : out.vertices) {
        v.position.x = m[0][0]*v.position.x + m[3][0];
        v.position.y = m[1][1]*v.position.y + m[3][1];
        v.position.z = m[2][2]*v.position.z + m[3][2];
    }
    return out;
}

MeshData ApplyNoise(const MeshData& mesh, std::function<float(Vec3)> fn, float s) {
    MeshData out = mesh;
    for (auto& v : out.vertices) {
        float n = fn(v.position);
        v.position.x += v.normal.x * n * s;
        v.position.y += v.normal.y * n * s;
        v.position.z += v.normal.z * n * s;
    }
    out.RecalcSmoothNormals();
    return out;
}

} // namespace MeshOps
