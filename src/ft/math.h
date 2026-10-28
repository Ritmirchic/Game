// FRACTURE — vector / matrix math
#pragma once
#include "common.h"

namespace ft {

struct Vec2 {
    f32 x = 0, y = 0;
    Vec2() = default;
    Vec2(f32 x_, f32 y_) : x(x_), y(y_) {}
    Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(f32 s) const { return {x * s, y * s}; }
};

struct Vec3 {
    f32 x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(f32 x_, f32 y_, f32 z_) : x(x_), y(y_), z(z_) {}
    Vec3 operator+(Vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(Vec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
    Vec3 operator*(f32 s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(f32 s) const { return {x / s, y / s, z / s}; }
    Vec3& operator+=(Vec3 o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(Vec3 o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(f32 s) { x *= s; y *= s; z *= s; return *this; }
    Vec3 mul(Vec3 o) const { return {x * o.x, y * o.y, z * o.z}; }
    f32 operator[](int i) const { return (&x)[i]; }
    f32& operator[](int i) { return (&x)[i]; }
};
inline f32 dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline f32 lenSq(Vec3 v) { return dot(v, v); }
inline f32 len(Vec3 v) { return std::sqrt(dot(v, v)); }
inline Vec3 norm(Vec3 v) { f32 l = len(v); return l > 1e-8f ? v / l : Vec3(0, 0, 0); }
inline Vec3 lerp(Vec3 a, Vec3 b, f32 t) { return a + (b - a) * t; }
inline Vec3 vmin(Vec3 a, Vec3 b) { return {tmin(a.x, b.x), tmin(a.y, b.y), tmin(a.z, b.z)}; }
inline Vec3 vmax(Vec3 a, Vec3 b) { return {tmax(a.x, b.x), tmax(a.y, b.y), tmax(a.z, b.z)}; }

struct Vec4 {
    f32 x = 0, y = 0, z = 0, w = 0;
    Vec4() = default;
    Vec4(f32 a, f32 b, f32 c, f32 d) : x(a), y(b), z(c), w(d) {}
    Vec4(Vec3 v, f32 w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
};

// column-major4x4 (OpenGL convention)
struct Mat4 {
    f32 m[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    f32& at(int r, int c) { return m[c * 4 + r]; }
    f32 at(int r, int c) const { return m[c * 4 + r]; }
    static Mat4 identity() { return Mat4(); }
    static Mat4 translate(Vec3 t) { Mat4 r; r.m[12]=t.x; r.m[13]=t.y; r.m[14]=t.z; return r; }
    static Mat4 scale(Vec3 s) { Mat4 r; r.m[0]=s.x; r.m[5]=s.y; r.m[10]=s.z; return r; }
    static Mat4 rotateY(f32 a) { Mat4 r; f32 c=std::cos(a), s=std::sin(a); r.m[0]=c; r.m[2]=-s; r.m[8]=s; r.m[10]=c; return r; }
    static Mat4 rotateX(f32 a) { Mat4 r; f32 c=std::cos(a), s=std::sin(a); r.m[5]=c; r.m[6]=s; r.m[9]=-s; r.m[10]=c; return r; }
    static Mat4 rotateZ(f32 a) { Mat4 r; f32 c=std::cos(a), s=std::sin(a); r.m[0]=c; r.m[1]=s; r.m[4]=-s; r.m[5]=c; return r; }
    Mat4 operator*(const Mat4& o) const {
        Mat4 r;
        for (int c = 0; c < 4; c++)
            for (int row = 0; row < 4; row++) {
                f32 s = 0;
                for (int k = 0; k < 4; k++) s += at(row, k) * o.at(k, c);
                r.at(row, c) = s;
            }
        return r;
    }
    Vec3 mulPoint(Vec3 p) const {
        return {m[0]*p.x + m[4]*p.y + m[8]*p.z + m[12],
                m[1]*p.x + m[5]*p.y + m[9]*p.z + m[13],
                m[2]*p.x + m[6]*p.y + m[10]*p.z + m[14]};
    }
    Vec3 mulDir(Vec3 p) const {
        return {m[0]*p.x + m[4]*p.y + m[8]*p.z,
                m[1]*p.x + m[5]*p.y + m[9]*p.z,
                m[2]*p.x + m[6]*p.y + m[10]*p.z};
    }
    Vec4 mul(Vec4 v) const {
        return {m[0]*v.x + m[4]*v.y + m[8]*v.z + m[12]*v.w,
                m[1]*v.x + m[5]*v.y + m[9]*v.z + m[13]*v.w,
                m[2]*v.x + m[6]*v.y + m[10]*v.z + m[14]*v.w,
                m[3]*v.x + m[7]*v.y + m[11]*v.z + m[15]*v.w};
    }
    static Mat4 perspective(f32 fovYRad, f32 aspect, f32 zn, f32 zf) {
        Mat4 r; std::memset(r.m, 0, sizeof(r.m));
        f32 t = std::tan(fovYRad * 0.5f);
        r.m[0] = 1.f / (aspect * t);
        r.m[5] = 1.f / t;
        r.m[10] = -(zf + zn) / (zf - zn);
        r.m[11] = -1.f;
        r.m[14] = -(2.f * zf * zn) / (zf - zn);
        return r;
    }
    static Mat4 ortho(f32 l, f32 r_, f32 b, f32 t, f32 zn, f32 zf) {
        Mat4 r; std::memset(r.m, 0, sizeof(r.m));
        r.m[0] = 2.f / (r_ - l);
        r.m[5] = 2.f / (t - b);
        r.m[10] = -2.f / (zf - zn);
        r.m[12] = -(r_ + l) / (r_ - l);
        r.m[13] = -(t + b) / (t - b);
        r.m[14] = -(zf + zn) / (zf - zn);
        r.m[15] = 1.f;
        return r;
    }
    // look-at (right handed, -Z forward)
    static Mat4 lookDir(Vec3 eye, Vec3 fwd, Vec3 up) {
        Vec3 f = norm(fwd);
        Vec3 s = norm(cross(f, up));
        Vec3 u = cross(s, f);
        Mat4 r;
        r.m[0]=s.x; r.m[4]=s.y; r.m[8]=s.z;
        r.m[1]=u.x; r.m[5]=u.y; r.m[9]=u.z;
        r.m[2]=-f.x; r.m[6]=-f.y; r.m[10]=-f.z;
        r.m[12]=-dot(s,eye); r.m[13]=-dot(u,eye); r.m[14]=dot(f,eye);
        return r;
    }
    Mat4 inverse() const {
        const f32* a = m;
        f32 inv[16];
        inv[0]  = a[5]*a[10]*a[15]-a[5]*a[11]*a[14]-a[9]*a[6]*a[15]+a[9]*a[7]*a[14]+a[13]*a[6]*a[11]-a[13]*a[7]*a[10];
        inv[4]  = -a[4]*a[10]*a[15]+a[4]*a[11]*a[14]+a[8]*a[6]*a[15]-a[8]*a[7]*a[14]-a[12]*a[6]*a[11]+a[12]*a[7]*a[10];
        inv[8]  = a[4]*a[9]*a[15]-a[4]*a[11]*a[13]-a[8]*a[5]*a[15]+a[8]*a[7]*a[13]+a[12]*a[5]*a[11]-a[12]*a[7]*a[9];
        inv[12] = -a[4]*a[9]*a[14]+a[4]*a[10]*a[13]+a[8]*a[5]*a[14]-a[8]*a[6]*a[13]-a[12]*a[5]*a[10]+a[12]*a[6]*a[9];
        inv[1]  = -a[1]*a[10]*a[15]+a[1]*a[11]*a[14]+a[9]*a[2]*a[15]-a[9]*a[3]*a[14]-a[13]*a[2]*a[11]+a[13]*a[3]*a[10];
        inv[5]  = a[0]*a[10]*a[15]-a[0]*a[11]*a[14]-a[8]*a[2]*a[15]+a[8]*a[3]*a[14]+a[12]*a[2]*a[11]-a[12]*a[3]*a[10];
        inv[9]  = -a[0]*a[9]*a[15]+a[0]*a[11]*a[13]+a[8]*a[1]*a[15]-a[8]*a[3]*a[13]-a[12]*a[1]*a[11]+a[12]*a[3]*a[9];
        inv[13] = a[0]*a[9]*a[14]-a[0]*a[10]*a[13]-a[8]*a[1]*a[14]+a[8]*a[2]*a[13]+a[12]*a[1]*a[10]-a[12]*a[2]*a[9];
        inv[2]  = a[1]*a[6]*a[15]-a[1]*a[7]*a[14]-a[5]*a[2]*a[15]+a[5]*a[3]*a[14]+a[13]*a[2]*a[7]-a[13]*a[3]*a[6];
        inv[6]  = -a[0]*a[6]*a[15]+a[0]*a[7]*a[14]+a[4]*a[2]*a[15]-a[4]*a[3]*a[14]-a[12]*a[2]*a[7]+a[12]*a[3]*a[6];
        inv[10] = a[0]*a[5]*a[15]-a[0]*a[7]*a[13]-a[4]*a[1]*a[15]+a[4]*a[3]*a[13]+a[12]*a[1]*a[7]-a[12]*a[3]*a[5];
        inv[14] = -a[0]*a[5]*a[14]+a[0]*a[6]*a[13]+a[4]*a[1]*a[14]-a[4]*a[2]*a[13]-a[12]*a[1]*a[6]+a[12]*a[2]*a[5];
        inv[3]  = -a[1]*a[6]*a[11]+a[1]*a[7]*a[10]+a[5]*a[2]*a[11]-a[5]*a[3]*a[10]-a[9]*a[2]*a[7]+a[9]*a[3]*a[6];
        inv[7]  = a[0]*a[6]*a[11]-a[0]*a[7]*a[10]-a[4]*a[2]*a[11]+a[4]*a[3]*a[10]+a[8]*a[2]*a[7]-a[8]*a[3]*a[6];
        inv[11] = -a[0]*a[5]*a[11]+a[0]*a[7]*a[9]+a[4]*a[1]*a[11]-a[4]*a[3]*a[9]-a[8]*a[1]*a[7]+a[8]*a[3]*a[5];
        inv[15] = a[0]*a[5]*a[10]-a[0]*a[6]*a[9]-a[4]*a[1]*a[10]+a[4]*a[2]*a[9]+a[8]*a[1]*a[6]-a[8]*a[2]*a[5];
        f32 det = a[0]*inv[0] + a[1]*inv[4] + a[2]*inv[8] + a[3]*inv[12];
        Mat4 r;
        if (std::fabs(det) < 1e-12f) return r;
        f32 id = 1.f / det;
        for (int i = 0; i < 16; i++) r.m[i] = inv[i] * id;
        return r;
    }
};

struct Quat {
    f32 x = 0, y = 0, z = 0, w = 1;
    Quat() = default;
    Quat(f32 x_, f32 y_, f32 z_, f32 w_) : x(x_), y(y_), z(z_), w(w_) {}
    static Quat axisAngle(Vec3 axis, f32 a) {
        Vec3 n = norm(axis); f32 s = std::sin(a * 0.5f);
        return {n.x * s, n.y * s, n.z * s, std::cos(a * 0.5f)};
    }
    Quat operator*(const Quat& o) const {
        return {w*o.x + x*o.w + y*o.z - z*o.y,
                w*o.y - x*o.z + y*o.w + z*o.x,
                w*o.z + x*o.y - y*o.x + z*o.w,
                w*o.w - x*o.x - y*o.y - z*o.z};
    }
    Vec3 rotate(Vec3 v) const {
        Vec3 q(x, y, z);
        Vec3 t = cross(q, v) * 2.f;
        return v + t * w + cross(q, t);
    }
    Quat conj() const { return {-x, -y, -z, w}; }
    Mat4 toMat() const {
        Mat4 r;
        f32 xx=x*x, yy=y*y, zz=z*z, xy=x*y, xz=x*z, yz=y*z, wx=w*x, wy=w*y, wz=w*z;
        r.m[0]=1-2*(yy+zz); r.m[4]=2*(xy-wz);   r.m[8]=2*(xz+wy);
        r.m[1]=2*(xy+wz);   r.m[5]=1-2*(xx+zz); r.m[9]=2*(yz-wx);
        r.m[2]=2*(xz-wy);   r.m[6]=2*(yz+wx);   r.m[10]=1-2*(xx+yy);
        return r;
    }
    static Quat mulAxis(Quat q, Vec3 axis, f32 ang) { return q * axisAngle(axis, ang); }
};
inline Quat slerpCheap(Quat a, Quat b, f32 t) {
    f32 d = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
    if (d < 0) b = {-b.x, -b.y, -b.z, -b.w};
    Quat r(lerpf(a.x, b.x, t), lerpf(a.y, b.y, t), lerpf(a.z, b.z, t), lerpf(a.w, b.w, t));
    f32 l = std::sqrt(r.x*r.x + r.y*r.y + r.z*r.z + r.w*r.w);
    if (l > 1e-6f) { r.x/=l; r.y/=l; r.z/=l; r.w/=l; }
    return r;
}

struct AABB {
    Vec3 lo, hi;
    AABB() = default;
    AABB(Vec3 a, Vec3 b) : lo(vmin(a, b)), hi(vmax(a, b)) {}
    static AABB center(Vec3 c, Vec3 half) { return {c - half, c + half}; }
    Vec3 center() const { return (lo + hi) * 0.5f; }
    Vec3 half() const { return (hi - lo) * 0.5f; }
    Vec3 size() const { return hi - lo; }
    bool overlaps(const AABB& o) const {
        return lo.x <= o.hi.x && hi.x >= o.lo.x && lo.y <= o.hi.y && hi.y >= o.lo.y && lo.z <= o.hi.z && hi.z >= o.lo.z;
    }
    AABB expand(f32 m) const { return AABB{{lo.x-m, lo.y-m, lo.z-m}, {hi.x+m, hi.y+m, hi.z+m}}; }
    AABB merge(const AABB& o) const { return {vmin(lo, o.lo), vmax(hi, o.hi)}; }
    bool contains(Vec3 p) const {
        return p.x >= lo.x && p.x <= hi.x && p.y >= lo.y && p.y <= hi.y && p.z >= lo.z && p.z <= hi.z;
    }
};

struct RayHit {
    bool hit = false;
    f32 t = 1e30f;
    Vec3 pos, nrm;
    int vx = 0, vy = 0, vz = 0;   // voxel hit
    u8 mat = 0;
};

// segment vs sphere
inline bool raySphere(Vec3 o, Vec3 d, Vec3 c, f32 r, f32& tOut) {
    Vec3 oc = o - c;
    f32 b = dot(oc, d);
    f32 cc = dot(oc, oc) - r * r;
    f32 disc = b * b - cc;
    if (disc < 0) return false;
    f32 t = -b - std::sqrt(disc);
    if (t < 0) t = -b + std::sqrt(disc);
    if (t < 0) return false;
    tOut = t;
    return true;
}

// intersect OBB (pos/orientation, half extents) with ray — slab test in local space
inline bool rayOBB(Vec3 o, Vec3 d, Vec3 c, Vec3 half, const Quat& rot, f32& tOut) {
    Vec3 lo = rot.rotate(o - c);
    Vec3 ld = rot.rotate(d);
    f32 nearT = -1e30f, farT = 1e30f;
    for (int i = 0; i < 3; i++) {
        f32 od = (&ld.x)[i], oo = (&lo.x)[i], h = (&half.x)[i];
        if (std::fabs(od) < 1e-8f) { if (std::fabs(oo) > h) return false; }
        else {
            f32 t1 = (-h - oo) / od, t2 = (h - oo) / od;
            if (t1 > t2) std::swap(t1, t2);
            nearT = tmax(nearT, t1); farT = tmin(farT, t2);
            if (nearT > farT) return false;
        }
    }
    if (farT < 0) return false;
    tOut = nearT >= 0 ? nearT : farT;
    return true;
}

} // namespace ft
