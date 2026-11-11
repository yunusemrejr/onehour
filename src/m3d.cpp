// One Hour - the model kit's builder and renderer (see m3d.h)
#include "m3d.h"

namespace m3d {

static const float DEG = 0.01745329252f;

// ------------------------------------------------------------ builder
static void mul3(const float a[9], const float b[9], float o[9]) {
    for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) o[r * 3 + c] = a[r * 3] * b[c] + a[r * 3 + 1] * b[3 + c] + a[r * 3 + 2] * b[6 + c];
}
// local = R * (p - c). Turning the solid by a rotation Q (model space, about its centre) gives local = R * Q^T * (p - c).
static void turn(Prim& p, int axis, float deg) {
    float c = std::cos(deg * DEG), s = std::sin(deg * DEG);
    float qt[9];   // Q transposed
    if (axis == 2) { float q[9] = { c, s, 0, -s, c, 0, 0, 0, 1 }; memcpy(qt, q, sizeof q); }
    else if (axis == 1) { float q[9] = { c, 0, -s, 0, 1, 0, s, 0, c }; memcpy(qt, q, sizeof q); }
    else { float q[9] = { 1, 0, 0, 0, c, s, 0, -s, c }; memcpy(qt, q, sizeof q); }
    float o[9]; mul3(p.R, qt, o); memcpy(p.R, o, sizeof o);
}
Prim& Prim::rz(float deg) { turn(*this, 2, deg); return *this; }
Prim& Prim::ry(float deg) { turn(*this, 1, deg); return *this; }
Prim& Prim::rx(float deg) { turn(*this, 0, deg); return *this; }

Prim& Model::add(Prim& p) {
    if (fz != 0) {   // the assembly frame: turn the solid and its centre about z
        float c = std::cos(fz * DEG), s = std::sin(fz * DEG);
        V3 q = { p.c.x * c - p.c.y * s, p.c.x * s + p.c.y * c, p.c.z };
        p.c = q; turn(p, 2, fz);
    }
    p.c = p.c + org;
    prims.push_back(p);
    return prims.back();
}
Prim& Model::convex(const std::vector<Plane>& pl, V3 centre, float rad, Color col) {
    Prim p; p.kind = 0; p.col = col; p.c = centre; p.rad = rad;
    p.p0 = (int)planes.size(); p.pn = (int)pl.size();
    for (const Plane& q : pl) planes.push_back(q);
    return add(p);
}
Prim& Model::box(float cx, float cy, float cz, float hx, float hy, float hz, Color col) {
    std::vector<Plane> pl = { { 1, 0, 0, hx }, { -1, 0, 0, hx }, { 0, 1, 0, hy }, { 0, -1, 0, hy }, { 0, 0, 1, hz }, { 0, 0, -1, hz } };
    Prim& p = convex(pl, { cx, cy, cz }, std::sqrt(hx * hx + hy * hy + hz * hz), col);
    p.h = { hx, hy, hz };
    return p;
}
static Plane planeThrough(V3 a, V3 b, V3 c, V3 inside) {   // plane through three points, facing away from 'inside'
    V3 u = b - a, v = c - a;
    V3 n = norm(V3{ u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x });
    float d = dot(n, a);
    if (dot(n, inside) > d) { n = n * -1; d = -d; }
    return { n.x, n.y, n.z, d };
}
Prim& Model::taper(float cx, float cy, float z0, float hx, float hy, float height, float tx, float ty, Color col, float sx, float sy) {
    const float hz = height * 0.5f;
    // corners relative to the centre (cx, cy, z0 + hz)
    V3 b[4] = { { -hx, -hy, -hz }, { hx, -hy, -hz }, { hx, hy, -hz }, { -hx, hy, -hz } };
    V3 t[4] = { { sx - tx, sy - ty, hz }, { sx + tx, sy - ty, hz }, { sx + tx, sy + ty, hz }, { sx - tx, sy + ty, hz } };
    V3 in = { sx * 0.5f, sy * 0.5f, 0 };
    std::vector<Plane> pl = { { 0, 0, 1, hz }, { 0, 0, -1, hz } };
    for (int i = 0; i < 4; i++) { int j = (i + 1) & 3; pl.push_back(planeThrough(b[i], b[j], t[i], in)); }
    float r = 0; for (int i = 0; i < 4; i++) { r = std::max(r, std::sqrt(dot(b[i], b[i]))); r = std::max(r, std::sqrt(dot(t[i], t[i]))); }
    Prim& p = convex(pl, { cx, cy, z0 + hz }, r, col);
    p.h = { std::max(hx, tx), std::max(hy, ty), hz };
    return p;
}
// convex polygon (a, b) -> edge planes; returns the centroid and fills the radius
static void polyPlanes(const std::vector<P2>& v, int ia, int ib, float e0, float e1, int ie, std::vector<Plane>& pl, V3& centre, float& rad) {
    float ca = 0, cb = 0; for (const P2& p : v) { ca += p.a; cb += p.b; } ca /= v.size(); cb /= v.size();
    float area = 0; for (size_t i = 0; i < v.size(); i++) { const P2& p = v[i]; const P2& q = v[(i + 1) % v.size()]; area += p.a * q.b - q.a * p.b; }
    float sg = area > 0 ? 1.0f : -1.0f, r2 = 0;
    float cc[3] = { 0, 0, 0 }; cc[ia] = ca; cc[ib] = cb; cc[ie] = (e0 + e1) * 0.5f;
    centre = { cc[0], cc[1], cc[2] };
    for (size_t i = 0; i < v.size(); i++) {
        const P2& p = v[i]; const P2& q = v[(i + 1) % v.size()];
        float ea = q.a - p.a, eb = q.b - p.b, l = std::sqrt(ea * ea + eb * eb);
        if (l < 1e-5f) continue;
        float na = sg * eb / l, nb = -sg * ea / l;
        float n[3] = { 0, 0, 0 }; n[ia] = na; n[ib] = nb;
        pl.push_back({ n[0], n[1], n[2], na * (p.a - ca) + nb * (p.b - cb) });
        r2 = std::max(r2, (p.a - ca) * (p.a - ca) + (p.b - cb) * (p.b - cb));
    }
    float he = std::abs(e1 - e0) * 0.5f, n[3] = { 0, 0, 0 };
    n[ie] = 1; pl.push_back({ n[0], n[1], n[2], he });
    n[ie] = -1; pl.push_back({ n[0], n[1], n[2], he });
    rad = std::sqrt(r2 + he * he);
}
Prim& Model::profileY(const std::vector<P2>& xz, float y0, float y1, Color col) {
    std::vector<Plane> pl; V3 c; float r; polyPlanes(xz, 0, 2, y0, y1, 1, pl, c, r);
    Prim& p = convex(pl, c, r, col); p.h = { r, std::abs(y1 - y0) * 0.5f, r }; return p;
}
Prim& Model::profileX(const std::vector<P2>& yz, float x0, float x1, Color col) {
    std::vector<Plane> pl; V3 c; float r; polyPlanes(yz, 1, 2, x0, x1, 0, pl, c, r);
    Prim& p = convex(pl, c, r, col); p.h = { std::abs(x1 - x0) * 0.5f, r, r }; return p;
}
Prim& Model::plan(const std::vector<P2>& xy, float z0, float z1, Color col) {
    std::vector<Plane> pl; V3 c; float r; polyPlanes(xy, 0, 1, z0, z1, 2, pl, c, r);
    Prim& p = convex(pl, c, r, col); p.h = { r, r, std::abs(z1 - z0) * 0.5f }; return p;
}
Prim& Model::cyl(char axis, float cx, float cy, float cz, float r, float halfLen, Color col, float top) {
    Prim p; p.kind = 1; p.col = col; p.c = { cx, cy, cz }; p.h = { r, r, halfLen }; p.top = top;
    p.rad = std::sqrt(r * r * std::max(1.0f, top * top) + halfLen * halfLen);
    if (axis == 'x') { float R[9] = { 0, 0, -1, 0, 1, 0, 1, 0, 0 }; memcpy(p.R, R, sizeof R); }        // local z = model x
    else if (axis == 'y') { float R[9] = { 1, 0, 0, 0, 0, -1, 0, 1, 0 }; memcpy(p.R, R, sizeof R); }   // local z = model y
    return add(p);
}
Prim& Model::rod(V3 a, V3 b, float r, Color col, float top) {
    V3 ax = b - a; float len = std::sqrt(dot(ax, ax));
    Prim p; p.kind = 1; p.col = col; p.c = (a + b) * 0.5f; p.h = { r, r, len * 0.5f }; p.top = top;
    p.rad = std::sqrt(r * r * std::max(1.0f, top * top) + len * len * 0.25f);
    V3 w = norm(ax), up = std::abs(w.z) < 0.9f ? V3{ 0, 0, 1 } : V3{ 1, 0, 0 };
    V3 u = norm(V3{ up.y * w.z - up.z * w.y, up.z * w.x - up.x * w.z, up.x * w.y - up.y * w.x });
    V3 v = { w.y * u.z - w.z * u.y, w.z * u.x - w.x * u.z, w.x * u.y - w.y * u.x };
    float R[9] = { u.x, u.y, u.z, v.x, v.y, v.z, w.x, w.y, w.z }; memcpy(p.R, R, sizeof R);
    return add(p);
}
Prim& Model::ball(float cx, float cy, float cz, float rx, float ry, float rz, Color col) {
    Prim p; p.kind = 2; p.col = col; p.c = { cx, cy, cz }; p.h = { rx, ry, rz }; p.rad = std::max(rx, std::max(ry, rz));
    return add(p);
}
Prim& Model::dome(float cx, float cy, float z0, float rx, float ry, float rz, Color col) {
    Prim p; p.kind = 3; p.col = col; p.c = { cx, cy, z0 }; p.h = { rx, ry, rz }; p.rad = std::max(rx, std::max(ry, rz));
    return add(p);
}

void Model::scale(float k) {
    for (Prim& p : prims) {
        p.c = p.c * k; p.h = p.h * k; p.rad *= k; p.bevel *= k; p.texS *= k;
        if (p.kind == 0) for (int i = 0; i < p.pn; i++) planes[p.p0 + i].d *= k;
    }
    hub *= k;
}

// ------------------------------------------------------------ renderer
namespace {

struct VPrim {          // a solid placed for one view
    const Prim* p;
    float R[9];         // world -> local
    V3 c;
    float rad;
};
struct Hit { float t; V3 n; V3 pl; V3 nl; float edge; };   // distance, world normal, local point, local normal, 0..1 nearness to an edge of the solid

inline V3 toLocal(const VPrim& v, V3 w) { return { v.R[0] * w.x + v.R[1] * w.y + v.R[2] * w.z, v.R[3] * w.x + v.R[4] * w.y + v.R[5] * w.z, v.R[6] * w.x + v.R[7] * w.y + v.R[8] * w.z }; }
inline V3 toWorld(const VPrim& v, V3 l) { return { v.R[0] * l.x + v.R[3] * l.y + v.R[6] * l.z, v.R[1] * l.x + v.R[4] * l.y + v.R[7] * l.z, v.R[2] * l.x + v.R[5] * l.y + v.R[8] * l.z }; }

// nearest intersection of the ray o + t d (t in (tmin, tmax)) with a solid. 'full' also fills the shading data.
bool intersect(const Model& m, const VPrim& v, V3 o, V3 d, float tmin, float tmax, Hit& h, bool full) {
    const Prim& p = *v.p;
    // bounding sphere
    V3 oc = o - v.c;
    float b = dot(oc, d), cq = dot(oc, oc) - v.rad * v.rad;
    if (cq > 0 && (b > 0 || b * b < cq)) return false;
    V3 lo = toLocal(v, oc), ld = toLocal(v, d);
    if (p.kind == 0) {
        float t0 = tmin, t1 = tmax; int hitPl = -1;
        const Plane* pl = &m.planes[p.p0];
        for (int i = 0; i < p.pn; i++) {
            float dn = pl[i].nx * ld.x + pl[i].ny * ld.y + pl[i].nz * ld.z;
            float num = pl[i].d - (pl[i].nx * lo.x + pl[i].ny * lo.y + pl[i].nz * lo.z);
            if (std::abs(dn) < 1e-7f) { if (num < 0) return false; continue; }
            float t = num / dn;
            if (dn < 0) { if (t > t0) { t0 = t; hitPl = i; } }
            else if (t < t1) t1 = t;
            if (t0 > t1) return false;
        }
        if (hitPl < 0) return false;   // (the ray starts inside: no entry face)
        h.t = t0;
        if (!full) return true;
        V3 q = lo + ld * t0;
        V3 n = { pl[hitPl].nx, pl[hitPl].ny, pl[hitPl].nz };
        // the chamfer: near another face the normal leans toward it, so every edge catches the light like a real bevel
        float edge = 0;
        if (p.bevel > 0) {
            V3 acc = n;
            for (int i = 0; i < p.pn; i++) {
                if (i == hitPl) continue;
                float dist = pl[i].d - (pl[i].nx * q.x + pl[i].ny * q.y + pl[i].nz * q.z);
                if (dist < p.bevel) {
                    float w = 1.0f - std::max(0.0f, dist) / p.bevel; w *= w;
                    acc = acc + V3{ pl[i].nx, pl[i].ny, pl[i].nz } * (w * 0.85f);
                    edge = std::max(edge, w);
                }
            }
            n = norm(acc);
        }
        h.pl = q; h.nl = { pl[hitPl].nx, pl[hitPl].ny, pl[hitPl].nz }; h.n = toWorld(v, n); h.edge = edge;
        return true;
    }
    if (p.kind == 1) {   // frustum: unit space x/hx, y/hy, z/hz; radius a + b z
        V3 so = { lo.x / p.h.x, lo.y / p.h.y, lo.z / p.h.z }, sd = { ld.x / p.h.x, ld.y / p.h.y, ld.z / p.h.z };
        const float ra = (1 + p.top) * 0.5f, rb = (p.top - 1) * 0.5f;
        float best = tmax; int part = -1;
        if (std::abs(so.z) < 1 && so.x * so.x + so.y * so.y < (ra + rb * so.z) * (ra + rb * so.z)) return false;   // (starts inside)
        float A = sd.x * sd.x + sd.y * sd.y - rb * rb * sd.z * sd.z;
        float k0 = ra + rb * so.z;
        float B = so.x * sd.x + so.y * sd.y - rb * sd.z * k0;
        float Cc = so.x * so.x + so.y * so.y - k0 * k0;
        if (std::abs(A) > 1e-9f) {
            float disc = B * B - A * Cc;
            if (disc >= 0) {
                float sq = std::sqrt(disc);
                for (int s = -1; s <= 1; s += 2) {
                    float t = (-B + s * sq) / A;
                    if (t <= tmin || t >= best) continue;
                    float z = so.z + sd.z * t;
                    if (z < -1 || z > 1 || ra + rb * z < 0) continue;
                    best = t; part = 0;
                }
            }
        }
        if (std::abs(sd.z) > 1e-9f) for (int s = -1; s <= 1; s += 2) {   // caps
            float t = ((float)s - so.z) / sd.z;
            if (t <= tmin || t >= best) continue;
            float x = so.x + sd.x * t, y = so.y + sd.y * t, r = s < 0 ? 1.0f : p.top;
            if (x * x + y * y > r * r) continue;
            best = t; part = s < 0 ? 1 : 2;
        }
        if (part < 0) return false;
        // (only the entry point counts: a ray leaving through a face is inside the solid)
        h.t = best;
        if (!full) return true;
        V3 q = { so.x + sd.x * best, so.y + sd.y * best, so.z + sd.z * best };
        V3 n; float edge = 0;
        const float rr = std::sqrt(q.x * q.x + q.y * q.y);
        if (part == 0) {
            n = { q.x / p.h.x, q.y / p.h.y, -rb * (ra + rb * q.z) / p.h.z };
            float de = (1 - std::abs(q.z)) * p.h.z;
            if (p.bevel > 0 && de < p.bevel) { float w = 1 - de / p.bevel; w *= w; n = norm(n) + V3{ 0, 0, q.z > 0 ? 1.0f : -1.0f } * (w * 0.85f); edge = w; }
        } else {
            n = { 0, 0, part == 1 ? -1.0f : 1.0f };
            float rim = part == 1 ? 1.0f : p.top, de = (rim - rr) * std::min(p.h.x, p.h.y);
            if (p.bevel > 0 && de < p.bevel && rr > 1e-4f) { float w = 1 - std::max(0.0f, de) / p.bevel; w *= w; n = n + V3{ q.x / rr, q.y / rr, 0 } * (w * 0.85f); edge = w; }
        }
        h.pl = { q.x * p.h.x, q.y * p.h.y, q.z * p.h.z };
        V3 nn = norm(n);
        h.nl = part == 0 ? V3{ nn.x, nn.y, 0 } : V3{ 0, 0, nn.z };
        h.n = toWorld(v, nn); h.edge = edge;
        return true;
    }
    // ellipsoid (2) and dome (3: only z >= 0, closed by a flat base)
    V3 so = { lo.x / p.h.x, lo.y / p.h.y, lo.z / p.h.z }, sd = { ld.x / p.h.x, ld.y / p.h.y, ld.z / p.h.z };
    float A = dot(sd, sd), B = dot(so, sd), Cc = dot(so, so) - 1;
    float disc = B * B - A * Cc;
    if (disc < 0) return false;
    float sq = std::sqrt(disc), t = (-B - sq) / A;
    bool flat = false;
    if (p.kind == 3) {
        float z = so.z + sd.z * t;
        if (t <= tmin || z < 0) {   // through the base instead
            if (std::abs(sd.z) < 1e-9f) return false;
            float tb = -so.z / sd.z; float x = so.x + sd.x * tb, y = so.y + sd.y * tb;
            if (tb <= tmin || x * x + y * y > 1 || sd.z < 0) {
                // the far side of the shell seen from below or inside: not drawn
                return false;
            }
            t = tb; flat = true;
        }
    }
    if (t <= tmin || t >= tmax) return false;
    h.t = t;
    if (!full) return true;
    V3 q = { so.x + sd.x * t, so.y + sd.y * t, so.z + sd.z * t };
    V3 n = flat ? V3{ 0, 0, -1 } : V3{ q.x / p.h.x, q.y / p.h.y, q.z / p.h.z };
    h.pl = { q.x * p.h.x, q.y * p.h.y, q.z * p.h.z };
    V3 nn = norm(n);
    h.nl = nn; h.n = toWorld(v, nn); h.edge = 0;
    return true;
}

inline u32 hash3(int x, int y, int z) { u32 h = (u32)x * 73856093u ^ (u32)y * 19349663u ^ (u32)z * 83492791u; h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15; return h; }
inline float rnd3(int x, int y, int z) { return (hash3(x, y, z) & 0xFFFF) / 65535.0f; }
float vnoise(V3 p) {   // value noise 0..1
    int ix = (int)std::floor(p.x), iy = (int)std::floor(p.y), iz = (int)std::floor(p.z);
    float fx = p.x - ix, fy = p.y - iy, fz = p.z - iz;
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy); fz = fz * fz * (3 - 2 * fz);
    float v = 0;
    for (int k = 0; k < 2; k++) for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++)
        v += rnd3(ix + i, iy + j, iz + k) * (i ? fx : 1 - fx) * (j ? fy : 1 - fy) * (k ? fz : 1 - fz);
    return v;
}
inline float fract(float v) { return v - std::floor(v); }

struct F3 { float r, g, b; };
inline F3 lin(Color c) { auto f = [](u8 v) { float x = v / 255.0f; return x * x; }; return { f(c.r), f(c.g), f(c.b) }; }   // (gamma 2: cheap and close enough)
inline F3 operator*(F3 a, float s) { return { a.r * s, a.g * s, a.b * s }; }
inline F3 operator+(F3 a, F3 b) { return { a.r + b.r, a.g + b.g, a.b + b.b }; }
inline F3 operator*(F3 a, F3 b) { return { a.r * b.r, a.g * b.g, a.b * b.b }; }
inline F3 mixf(F3 a, F3 b, float t) { return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t }; }

// the surface colour of a solid at a point: its paint, pattern, seams and dirt. 'rough' and 'nudge' (a normal tilt along local axes) come back too.
F3 surface(const Prim& p, const Hit& h, float zWorld, float dirt, u8& mat, float& gloss) {
    F3 a = lin(p.col), b = lin(p.col2);
    mat = p.mat; gloss = 1;
    bool second = false;   // the pattern chose col2 for this point
    const V3 q = h.pl; const V3 n = h.nl;
    // the two axes that run along the face
    const float ax = std::abs(n.x), ay = std::abs(n.y), az = std::abs(n.z);
    float u, v; bool upright;
    if (az >= ax && az >= ay) { u = q.x; v = q.y; upright = false; }
    else if (ax >= ay) { u = q.y; v = q.z; upright = true; }
    else { u = q.x; v = q.z; upright = true; }
    const float s = std::max(0.2f, p.texS);
    switch (p.tex) {
    case T_PANEL: {
        float fu = fract(u / s + 0.5f), fv = fract(v / s + 0.5f), w = 0.22f / s;
        if (fu < w || fv < w) a = a * 0.62f;
        else if (fu < w * 2.2f || fv < w * 2.2f) a = a * 1.10f;
        float tone = rnd3((int)std::floor(u / s + 0.5f), (int)std::floor(v / s + 0.5f), p.p0 + 7);
        a = a * (0.94f + 0.12f * tone);
        break; }
    case T_TREAD: {
        float f = fract(q.x / s);
        a = a * (f < 0.45f ? 0.55f : 1.15f);
        break; }
    case T_HAZARD: { if (fract((u + v) / s) < 0.5f) { a = b; second = true; } break; }
    case T_WINDOWS: {
        if (upright) {
            float fu = fract(u / s), fv = fract(v / (s * 1.25f) + 0.15f);
            if (fu > 0.22f && fu < 0.78f && fv > 0.25f && fv < 0.75f) {
                a = b; mat = GLASS;
                if (rnd3((int)std::floor(u / s), (int)std::floor(v / (s * 1.25f) + 0.15f), p.p0) > 0.72f) { a = mixf(b, F3{ 1.0f, 0.86f, 0.55f }, 0.75f); mat = EMIT; gloss = 0.55f; }
            }
        }
        break; }
    case T_RIBS: { float f = fract((upright ? u : q.x) / s); a = a * (f < 0.5f ? 0.80f : 1.08f); break; }
    case T_BRICK: {
        if (upright) {
            float row = std::floor(v / (s * 0.5f)), fu = fract(u / s + (((int)row) & 1) * 0.5f), fv = fract(v / (s * 0.5f));
            if (fu < 0.10f || fv < 0.16f) a = a * 0.66f;
            else a = a * (0.88f + 0.24f * rnd3((int)std::floor(u / s + (((int)row) & 1) * 0.5f), (int)row, p.p0 + 3));
        } else a = a * (0.9f + 0.2f * vnoise(V3{ q.x * 0.6f, q.y * 0.6f, 1.5f }));
        break; }
    case T_PLANKS: {
        float fv = fract((upright ? v : q.y) / s);
        if (fv < 0.12f) a = a * 0.6f; else a = a * (0.86f + 0.26f * rnd3((int)std::floor((upright ? v : q.y) / s), 5, p.p0));
        break; }
    case T_CAMO: {
        float f = vnoise(V3{ q.x / s + 13.7f, q.y / s + 5.1f, q.z / s + 9.3f }) * 0.65f + vnoise(V3{ q.x / s * 2.3f, q.y / s * 2.3f, q.z / s * 2.3f }) * 0.35f;
        if (f > 0.52f) a = b; else if (f > 0.46f) a = mixf(a, b, 0.5f);
        break; }
    case T_TILES: {
        float row = std::floor(q.z / (s * 0.42f) + (upright ? 0 : (q.y / s))), fu = fract(q.x / s + (((int)row) & 1) * 0.5f);
        float fr = fract(q.z / (s * 0.42f) + (upright ? 0 : (q.y / s)));
        a = a * (fr < 0.22f ? 0.70f : (0.92f + 0.2f * rnd3((int)std::floor(q.x / s + (((int)row) & 1) * 0.5f), (int)row, p.p0)));
        if (fu < 0.08f) a = a * 0.85f;
        break; }
    case T_GRILLE: { float f = fract((upright ? v : q.x) / s); if (f < 0.5f) { a = a * 0.25f; mat = MATTE; } break; }
    case T_STRIPE: { if (std::abs(q.y) < s * 0.5f) { a = b; second = true; } break; }
    case T_BAND: { if (std::abs(q.x) < s * 0.5f) { a = b; second = true; } break; }
    case T_NOISE: {
        float f = vnoise(V3{ q.x / s, q.y / s, q.z / s }) * 0.6f + vnoise(V3{ q.x / s * 2.7f + 31, q.y / s * 2.7f, q.z / s * 2.7f }) * 0.4f;
        a = mixf(a, b, clampf((f - 0.3f) * 2.4f, 0, 1));
        break; }
    default: break;
    }
    if ((second ? p.col2.a : p.col.a) == 254 && mat != GLASS && mat != EMIT) mat = TEAM;
    if (mat == PAINT || mat == METAL || mat == MATTE || mat == TEAM) {
        // a fine mottle, worn edges, and dust low down
        float g = vnoise(V3{ q.x * 1.7f + p.p0, q.y * 1.7f, q.z * 1.7f });
        a = a * (0.93f + 0.14f * g);
        if (mat != MATTE && h.edge > 0.25f) { float w = (h.edge - 0.25f) * 0.5f; a = mixf(a, F3{ 0.80f, 0.80f, 0.78f }, w * (mat == METAL ? 0.5f : 0.38f)); }
        if (dirt > 0 && mat != TEAM) {
            float d = clampf(1.0f - zWorld / 7.0f, 0, 1) * dirt * (0.55f + 0.9f * vnoise(V3{ q.x * 0.9f + 3, q.y * 0.9f, q.z * 0.9f + p.p0 }));
            a = mixf(a, F3{ 0.13f, 0.105f, 0.075f }, clampf(d, 0, 0.75f));
            gloss *= 1.0f - clampf(d, 0, 0.8f);
        }
    }
    return a;
}

inline F3 skyAt(V3 r) {   // what a mirror facing r sees: pale horizon haze up to a deep zenith, warm earth below
    if (r.z >= 0) { float t = std::pow(clampf(r.z, 0, 1), 0.55f); return mixf(F3{ 0.80f, 0.86f, 0.92f }, F3{ 0.20f, 0.42f, 0.78f }, t); }
    return mixf(F3{ 0.80f, 0.86f, 0.92f }, F3{ 0.16f, 0.13f, 0.09f }, clampf(-r.z * 3.0f, 0, 1));
}
inline float tone(float v) { v = std::max(0.0f, v); float x = v / (1.0f + 0.22f * v); return std::sqrt(std::min(1.0f, x * 1.13f)); }   // soft shoulder, then back to display gamma

}  // namespace

void render(const Model& m, const RenderOpt& o, Rendered& out) {
    const int n = (int)m.prims.size();
    const float ch = std::cos(o.heading), sh = std::sin(o.heading);
    std::vector<VPrim> vp(n);
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f, zTop = 1;
    const V3 L = norm(V3{ SUN[0], SUN[1], SUN[2] });
    const float SQ = std::sqrt(1 + OBL_K * OBL_K);
    for (int i = 0; i < n; i++) {
        const Prim& p = m.prims[i];
        VPrim& v = vp[i]; v.p = &p; v.rad = p.rad;
        v.c = { p.c.x * ch - p.c.y * sh, p.c.x * sh + p.c.y * ch, p.c.z };
        // world -> local = R * Rz(-heading)
        const float rzT[9] = { ch, sh, 0, -sh, ch, 0, 0, 0, 1 };
        mul3(p.R, rzT, v.R);
        if (!(p.flags & F_SHADOWONLY)) {
            x0 = std::min(x0, v.c.x - v.rad); x1 = std::max(x1, v.c.x + v.rad);
            y0 = std::min(y0, v.c.y - OBL_K * v.c.z - v.rad * SQ); y1 = std::max(y1, v.c.y - OBL_K * v.c.z + v.rad * SQ);
        }
        zTop = std::max(zTop, v.c.z + v.rad);
        if (m.ground && !(p.flags & F_NOSHADOW)) {   // its shadow on the ground
            float gx = v.c.x - L.x / L.z * v.c.z, gy = v.c.y - L.y / L.z * v.c.z, gr = v.rad * 1.6f + 1;
            x0 = std::min(x0, gx - gr); x1 = std::max(x1, gx + gr); y0 = std::min(y0, gy - gr); y1 = std::max(y1, gy + gr);
        }
    }
    if (n == 0) { out.color = Canvas(1, 1); out.mask = Canvas(1, 1); out.hasMask = false; out.ox = out.oy = 0; return; }
    const float S = o.scale; const int ss = std::max(1, o.ss);
    const int pad = 2;
    const int px0 = (int)std::floor(x0 * S) - pad, py0 = (int)std::floor(y0 * S) - pad;
    const int W = std::min(1024, (int)std::ceil(x1 * S) + pad - px0), H = std::min(1024, (int)std::ceil(y1 * S) + pad - py0);
    const int SW = W * ss, SH = H * ss;
    const float inv = 1.0f / (S * ss);
    const V3 D = norm(V3{ 0, -OBL_K, -1 });
    const float zT = zTop + 1;
    // --- visibility: nearest solid per sample ---
    std::vector<float> depth((size_t)SW * SH, 1e9f);
    std::vector<short> owner((size_t)SW * SH, -1);
    auto rayO = [&](int sx, int sy) { float wx = (px0 * ss + sx + 0.5f) * inv, wy = (py0 * ss + sy + 0.5f) * inv; return V3{ wx, wy + OBL_K * zT, zT }; };
    Hit h;
    for (int i = 0; i < n; i++) {
        const VPrim& v = vp[i];
        if (v.p->flags & F_SHADOWONLY) continue;
        const float cy = v.c.y - OBL_K * v.c.z;
        int ax = std::max(0, (int)std::floor((v.c.x - v.rad) / inv) - px0 * ss), bx = std::min(SW - 1, (int)std::ceil((v.c.x + v.rad) / inv) - px0 * ss);
        int ay = std::max(0, (int)std::floor((cy - v.rad * SQ) / inv) - py0 * ss), by = std::min(SH - 1, (int)std::ceil((cy + v.rad * SQ) / inv) - py0 * ss);
        for (int y = ay; y <= by; y++) for (int x = ax; x <= bx; x++) {
            size_t k = (size_t)y * SW + x;
            if (intersect(m, v, rayO(x, y), D, 0.0f, depth[k], h, false)) { depth[k] = h.t; owner[k] = (short)i; }
        }
    }
    // --- shading ---
    struct Acc { float r, g, b, a, mr, mg, mb, ma, er, eg, eb; };
    std::vector<Acc> acc((size_t)W * H, Acc{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 });
    const F3 sunCol = { 1.00f, 0.955f, 0.86f }, skyCol = { 0.60f, 0.72f, 0.95f }, bounce = { 0.50f, 0.44f, 0.30f };
    const float KEY = 1.28f, AMB = 0.52f;
    // a small fixed set of hemisphere directions for the occlusion rays, rotated per sample
    static const float AOD[6][3] = { { 0.75f, 0.10f, 0.65f }, { -0.45f, 0.62f, 0.64f }, { -0.30f, -0.72f, 0.62f }, { 0.10f, 0.05f, 0.99f }, { 0.55f, -0.55f, 0.63f }, { -0.72f, -0.05f, 0.69f } };
    const float AO_REACH = 9.0f;
    auto occluded = [&](V3 from, V3 dir, float tmax, int self) {
        Hit hh;
        for (int i = 0; i < n; i++) {
            if (vp[i].p->flags & F_NOSHADOW) continue;
            if (intersect(m, vp[i], from, dir, i == self ? 0.05f : 0.0f, tmax, hh, false)) return true;
        }
        return false;
    };
    for (int sy = 0; sy < SH; sy++) for (int sx = 0; sx < SW; sx++) {
        const size_t k = (size_t)sy * SW + sx;
        const int i = owner[k];
        Acc& A = acc[(size_t)(sy / ss) * W + sx / ss];
        const u32 hsh = hash3(sx, sy, 17);
        // jittered sun (a soft-edged shadow) and a turned occlusion set
        const float j1 = ((hsh & 255) / 255.0f - 0.5f), j2 = ((hsh >> 8 & 255) / 255.0f - 0.5f);
        const V3 Lj = norm(V3{ L.x + j1 * 0.16f, L.y + j2 * 0.16f, L.z });
        const float rot = (hsh >> 16 & 255) / 255.0f * 6.2832f, cr = std::cos(rot), sr = std::sin(rot);
        if (i < 0) {
            if (!m.ground) continue;
            // the ground: is this spot in the model's shadow, or tucked in under it?
            V3 o0 = rayO(sx, sy);
            V3 gp = { o0.x, o0.y - OBL_K * zT, 0.0f };
            float sha = occluded(gp, Lj, 1e9f, -1) ? 1.0f : 0.0f;
            int occ = 0;
            for (int q = 0; q < 3; q++) {
                const float* a = AOD[(q * 2 + (hsh >> 24 & 1)) % 6];
                V3 dir = { a[0] * cr - a[1] * sr, a[0] * sr + a[1] * cr, a[2] };
                if (occluded(gp, dir, AO_REACH * 0.6f, -1)) occ++;
            }
            float al = 1.0f - (1.0f - sha * o.shadow) * (1.0f - occ / 3.0f * 0.42f);
            if (al > 0) { A.r += 0.012f * al; A.g += 0.016f * al; A.b += 0.030f * al; A.a += al; }
            continue;
        }
        const VPrim& v = vp[i];
        intersect(m, v, rayO(sx, sy), D, 0.0f, 1e9f, h, true);
        V3 P = rayO(sx, sy) + D * h.t;
        V3 N = h.n;
        u8 mat; float gloss;
        F3 alb = surface(*v.p, h, P.z, m.dirt, mat, gloss);
        if (mat == TEAM) alb = { 0.78f, 0.78f, 0.78f };
        F3 col;
        if (mat == EMIT) {
            col = alb * 1.9f * gloss + F3{ 0.10f, 0.10f, 0.10f };
            A.er += alb.r; A.eg += alb.g; A.eb += alb.b;
        } else {
            V3 Ps = P + N * 0.06f;
            float ndl = dot(N, Lj);
            if (mat == FOLIAGE) ndl = ndl * 0.6f + 0.4f;   // light wraps round and through leaves
            float lit = ndl > 0 && !occluded(Ps, Lj, 1e9f, v.p->kind == 0 ? i : -1) ? ndl : 0.0f;
            int occ = 0;
            // (tangent frame for the hemisphere)
            V3 up = std::abs(N.z) < 0.95f ? V3{ 0, 0, 1 } : V3{ 1, 0, 0 };
            V3 T = norm(V3{ up.y * N.z - up.z * N.y, up.z * N.x - up.x * N.z, up.x * N.y - up.y * N.x });
            V3 Bv = { N.y * T.z - N.z * T.y, N.z * T.x - N.x * T.z, N.x * T.y - N.y * T.x };
            const int NAO = 4;
            for (int q = 0; q < NAO; q++) {
                const float* a = AOD[(q + (hsh >> 24 & 3)) % 6];
                float lx = a[0] * cr - a[1] * sr, ly = a[0] * sr + a[1] * cr;
                V3 dir = T * lx + Bv * ly + N * a[2];
                if (m.ground && dir.z < -0.05f) { float tg = -Ps.z / dir.z; if (tg < AO_REACH) { occ++; continue; } }   // the ground itself closes the view
                if (occluded(Ps, dir, AO_REACH, v.p->kind == 0 ? i : -1)) occ++;
            }
            float ao = 1.0f - occ / (float)NAO * 0.78f;
            float hemi = 0.5f + 0.5f * N.z;
            F3 amb = mixf(bounce, skyCol, hemi) * (AMB * ao);
            F3 light = sunCol * (KEY * lit) + amb;
            col = alb * light;
            // sheen
            V3 Vv = D * -1.0f;
            V3 Hh = norm(Lj + Vv);
            float ndh = std::max(0.0f, dot(N, Hh));
            V3 Rr = N * (2 * dot(N, Vv)) - Vv;
            float fres = std::pow(1.0f - clampf(dot(N, Vv), 0, 1), 3.0f);
            if (mat == METAL) {
                col = col * 0.55f + alb * skyAt(Rr) * (0.55f * ao) + alb * sunCol * (std::pow(ndh, 42.0f) * 2.2f * (lit > 0 ? 1.0f : 0.0f) * gloss);
            } else if (mat == GLASS) {
                col = col * 0.45f + skyAt(Rr) * ((0.30f + 0.6f * fres) * ao) + sunCol * (std::pow(ndh, 90.0f) * 3.0f * (lit > 0 ? 1.0f : 0.0f));
            } else if (mat == PAINT || mat == TEAM) {
                col = col + skyAt(Rr) * ((0.035f + 0.16f * fres) * ao * gloss) + sunCol * (std::pow(ndh, 26.0f) * 0.42f * (lit > 0 ? 1.0f : 0.0f) * gloss);
            } else if (mat == RUBBER) {
                col = col + sunCol * (std::pow(ndh, 10.0f) * 0.07f * (lit > 0 ? 1.0f : 0.0f));
            }
        }
        if (mat == TEAM) { A.mr += col.r; A.mg += col.g; A.mb += col.b; A.ma += 1; }
        A.r += col.r; A.g += col.g; A.b += col.b; A.a += 1;
    }
    // --- resolve ---
    out.color = Canvas(W, H); out.mask = Canvas(W, H); out.hasMask = false;
    out.ox = (float)-px0; out.oy = (float)-py0;
    const float nS = 1.0f / (ss * ss);
    std::vector<float> emitA((size_t)W * H, 0.0f); std::vector<F3> emitC((size_t)W * H, F3{ 0, 0, 0 });
    bool anyEmit = false;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        const Acc& A = acc[(size_t)y * W + x];
        if (A.a <= 0) continue;
        float al = std::min(1.0f, A.a * nS), k = 1.0f / A.a;
        // (a ground-shadow sample carries its own alpha in A.a: solid samples count 1 each)
        out.color.px[(size_t)y * W + x] = Canvas::pack(Color{ (u8)(tone(A.r * k) * 255), (u8)(tone(A.g * k) * 255), (u8)(tone(A.b * k) * 255), (u8)(al * 255) });
        if (A.ma > 0) {
            float km = 1.0f / A.ma;
            out.mask.px[(size_t)y * W + x] = Canvas::pack(Color{ (u8)(tone(A.mr * km) * 255), (u8)(tone(A.mg * km) * 255), (u8)(tone(A.mb * km) * 255), (u8)(std::min(1.0f, A.ma * nS) * 255) });
            out.hasMask = true;
        }
        if (A.er + A.eg + A.eb > 0) { emitA[(size_t)y * W + x] = std::min(1.0f, (A.er + A.eg + A.eb) * nS); float m3 = std::max(A.er, std::max(A.eg, A.eb)); emitC[(size_t)y * W + x] = { A.er / m3, A.eg / m3, A.eb / m3 }; anyEmit = true; }
    }
    if (o.outline) {   // the contact line: silhouette pixels next to open ground lean dark, and a faint ink sits just outside them
        std::vector<u32> src = out.color.px;
        auto solid = [&](int x, int y) { return x >= 0 && y >= 0 && x < W && y < H && (src[(size_t)y * W + x] >> 24) >= 200 && acc[(size_t)y * W + x].a * nS > 0.75f && acc[(size_t)y * W + x].r + acc[(size_t)y * W + x].g + acc[(size_t)y * W + x].b > 0.2f * acc[(size_t)y * W + x].a; };
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
            if (solid(x, y)) continue;
            bool near = solid(x - 1, y) || solid(x + 1, y) || solid(x, y - 1) || solid(x, y + 1);
            if (!near) continue;
            Color c = Canvas::unpack(src[(size_t)y * W + x]);
            float a = c.a / 255.0f, na = std::max(a, 0.62f);
            // ink over whatever is there (a soft edge pixel or shadow)
            Color ink{ 14, 18, 30, 255 };
            float t = a > 0 ? 0.55f : 1.0f;
            out.color.px[(size_t)y * W + x] = Canvas::pack(Color{ (u8)(c.r + (ink.r - c.r) * t), (u8)(c.g + (ink.g - c.g) * t), (u8)(c.b + (ink.b - c.b) * t), (u8)(na * 255) });
        }
    }
    if (anyEmit) {   // lights bleed a little into the pixels round them
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
            float g = 0; F3 gc = { 0, 0, 0 };
            for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) {
                int nx = x + dx, ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= W || ny >= H || (dx == 0 && dy == 0)) continue;
                float e = emitA[(size_t)ny * W + nx]; if (e <= 0) continue;
                float w = e / (1.0f + (float)(dx * dx + dy * dy) * 1.6f);
                g += w; gc = gc + emitC[(size_t)ny * W + nx] * w;
            }
            if (g <= 0.02f) continue;
            gc = gc * (1.0f / g); g = std::min(0.55f, g * 0.55f);
            Color c = Canvas::unpack(out.color.px[(size_t)y * W + x]);
            float a = c.a / 255.0f;
            if (a <= 0.02f) { out.color.px[(size_t)y * W + x] = Canvas::pack(Color{ (u8)(gc.r * 255), (u8)(gc.g * 255), (u8)(gc.b * 255), (u8)(g * 150) }); continue; }
            out.color.px[(size_t)y * W + x] = Canvas::pack(Color{ (u8)std::min(255.0f, c.r + gc.r * 255 * g * 0.7f), (u8)std::min(255.0f, c.g + gc.g * 255 * g * 0.7f), (u8)std::min(255.0f, c.b + gc.b * 255 * g * 0.7f), c.a });
        }
    }
}

Canvas composite(const Rendered& r, Color team) {
    Canvas c = r.color;
    if (!r.hasMask) return c;
    for (size_t i = 0; i < c.px.size(); i++) {
        Color mk = Canvas::unpack(r.mask.px[i]);
        if (mk.a == 0) continue;
        Color b = Canvas::unpack(c.px[i]);
        float a = mk.a / 255.0f * (b.a > 0 ? 1.0f : 0.0f);
        float tr = team.r * mk.r / 255.0f, tg = team.g * mk.g / 255.0f, tb = team.b * mk.b / 255.0f;
        // (the neutral shade was averaged into the base pixel with the rest: take it out again in proportion)
        c.px[i] = Canvas::pack(Color{ (u8)clampf(b.r + (tr - mk.r) * a, 0, 255), (u8)clampf(b.g + (tg - mk.g) * a, 0, 255), (u8)clampf(b.b + (tb - mk.b) * a, 0, 255), b.a });
    }
    return c;
}

}  // namespace m3d
