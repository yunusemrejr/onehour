// One Hour - 3D models: the wild - trees, bushes, boulders and the ground props (see m3d.h / m3d_reg.h).
// Rendered once at start-up at one model unit per world pixel: the same oblique view, the same low western sun as the units and buildings,
// so a forest stands in the same light as the armies that fight through it.
#include "m3d_parts.h"

using namespace m3d;

namespace {
// woods: bark, and four leaf palettes (deep shade, body, lit crown, and a dry edge tone)
const Color bark = C(0x6A4C33), barkDark = C(0x3B2A1E), barkLight = C(0x8A6746);
const Color leafDeep = C(0x1E3F22), leafMid = C(0x35662C), leafLit = C(0x538A34), leafDry = C(0x6E8A38);
const Color coniferDeep = C(0x17331F), coniferMid = C(0x27502B), coniferLit = C(0x3C6B33);
const Color autumnDeep = C(0x7A3416), autumnMid = C(0xB45C1C), autumnLit = C(0xD98C33);
const Color bushDeep = C(0x27461F), bushMid = C(0x446B26), bushLit = C(0x6A9130);
const Color stone = C(0x8A867E), stoneDark = C(0x5C5952), stonePale = C(0xAFA99C), moss = C(0x5C7A3A);

// A leaf clump: a wide, shallow ellipsoid, mottled with its own darker tone (offsets are from the trunk in model x/y)
void clump(Model& m, float x, float y, float z, float rx, float ry, float rz, Color col, Color shade) {
    m.ball(x, y, z, rx, ry, rz, col).m(FOLIAGE).t(T_NOISE, 3.0f, shade).bev(0.4f);
}
}  // namespace

// ---------------------------------------------------------------- broadleaf (oak / beech): a stout trunk, three boughs, a heavy crown of leaf clumps
static void treeBroad(Model& m, int variant) {
    Rng r(0xA11CEu + variant * 7919u);
    const float H = 19.0f + r.f(0, 4.0f);          // trunk to the first boughs
    const float lean = r.f(-0.6f, 0.6f);
    m.dirt = 0.15f;
    // trunk, flaring at the root
    m.cyl('z', 0, 0, H * 0.5f, 2.1f, H * 0.5f, bark, 0.72f).m(MATTE).t(T_NOISE, 2.6f, barkDark).bev(0.35f);
    m.cyl('z', 0, 0, 0.8f, 2.8f, 0.9f, barkDark, 0.7f).m(MATTE).bev(0.3f);
    // boughs reaching out and up
    const int nb = 3 + (variant & 1);
    for (int i = 0; i < nb; i++) {
        const float an = i * 6.2832f / nb + r.f(-0.35f, 0.35f), reach = r.f(6.0f, 10.0f);
        const V3 a{ lean * 0.3f, 0, H - r.f(0.5f, 2.0f) };
        const V3 b{ std::cos(an) * reach, std::sin(an) * reach, H + r.f(3.0f, 7.0f) };
        m.rod(a, b, 1.15f, bark, 0.55f).m(MATTE).t(T_NOISE, 2.2f, barkDark).bev(0.25f);
    }
    // the crown: a low, wide dome of overlapping flattened clumps, lightest on the west, deepening to the east and the skirt
    const int n = 9 + (int)r.f(0, 3);
    for (int i = 0; i < n; i++) {
        const float an = i * 6.2832f / n + r.f(-0.35f, 0.35f);
        const float west = -std::cos(an);                        // 1 on the sunlit side
        const float d = r.f(5.0f, 10.5f), zz = H + r.f(0.0f, 5.0f) + west * 2.2f;
        const float rr = r.f(5.6f, 8.2f);
        const Color body = west > 0.35f ? leafLit : (west < -0.45f ? leafDeep : leafMid);
        clump(m, lean * 0.4f + std::cos(an) * d, std::sin(an) * d, zz, rr * 1.18f, rr * 1.18f, rr * r.f(0.46f, 0.62f), body, west > 0.35f ? leafMid : leafDeep);
    }
    // a second, smaller tier riding on top: the crown's crown
    for (int i = 0; i < 4; i++) {
        const float an = -2.3f + i * 1.5f + r.f(-0.25f, 0.25f), d = r.f(1.5f, 5.5f), zz = H + r.f(5.5f, 8.0f);
        const float rr = r.f(3.6f, 5.2f);
        clump(m, -1.6f + std::cos(an) * d, std::sin(an) * d, zz, rr, rr, rr * 0.52f, i < 2 ? leafDry : leafLit, leafMid);
    }
    // a deep skirt of shade under the crown
    for (int i = 0; i < 3; i++) {
        const float an = 0.6f + i * 2.1f + r.f(-0.3f, 0.3f), d = r.f(3.0f, 7.5f);
        const float rr = r.f(4.4f, 6.0f);
        clump(m, lean * 0.5f + std::cos(an) * d, std::sin(an) * d, H - r.f(1.0f, 3.2f), rr * 1.1f, rr * 1.1f, rr * 0.4f, leafDeep, leafDeep);
    }
}
M3D_PROP("tree.broad", treeBroad)

// ---------------------------------------------------------------- conifer (spruce / fir): a narrow spire of ragged tiers
static void treeConifer(Model& m, int variant) {
    Rng r(0xC0FEu + variant * 6151u);
    const float H = 40.0f + r.f(0, 7.0f);
    m.dirt = 0.12f;
    m.cyl('z', 0, 0, H * 0.3f, 1.9f, H * 0.3f, bark, 0.6f).m(MATTE).t(T_NOISE, 2.4f, barkDark).bev(0.25f);
    // seven tiers of needles: each a shallow cone with a few smaller cones round its rim, so the outline is ragged rather than a smooth stack
    const int tiers = 7;
    for (int i = 0; i < tiers; i++) {
        const float t = i / (float)(tiers - 1);                       // 0 at the bottom
        const float z0 = 2.0f + t * (H - 6.0f), rad = (12.5f - 9.2f * t) * r.f(0.93f, 1.04f);
        const float hgt = 7.0f - 3.4f * t;
        const Color body = t < 0.25f ? coniferDeep : (t > 0.72f ? coniferLit : coniferMid);
        m.cyl('z', 0, 0, z0 + hgt * 0.5f, rad, hgt * 0.5f, body, 0.12f).m(FOLIAGE).t(T_NOISE, 2.6f, coniferDeep).bev(0.3f);
        for (int k = 0; k < 4; k++) {                                 // the rim's ragged points, the west ones lit
            const float an = k * 1.5708f + i * 0.7f + r.f(-0.25f, 0.25f), dd = rad * r.f(0.55f, 0.8f), rr2 = rad * r.f(0.3f, 0.42f);
            m.cyl('z', std::cos(an) * dd, std::sin(an) * dd, z0 + hgt * 0.3f, rr2, hgt * 0.3f,
                  std::cos(an) < -0.3f ? coniferLit : body, 0.1f).m(FOLIAGE).t(T_NOISE, 2.2f, coniferDeep).bev(0.2f);
        }
    }
    m.cyl('z', 0.4f, 0.2f, H - 2.0f, 2.2f, 2.6f, coniferLit, 0.0f).m(FOLIAGE).t(T_NOISE, 2.0f, coniferMid).bev(0.2f);
}
M3D_PROP("tree.conifer", treeConifer)

// ---------------------------------------------------------------- autumn tree: a broadleaf crown turned, a little thinner
static void treeAutumn(Model& m, int variant) {
    Rng r(0xA070u + variant * 4271u);
    const float H = 17.0f + r.f(0, 3.5f);
    m.dirt = 0.15f;
    m.cyl('z', 0, 0, H * 0.5f, 1.9f, H * 0.5f, bark, 0.7f).m(MATTE).t(T_NOISE, 2.6f, barkDark).bev(0.3f);
    const int nb = 3;
    for (int i = 0; i < nb; i++) {
        const float an = i * 6.2832f / nb + r.f(-0.3f, 0.3f), reach = r.f(5.0f, 9.0f);
        m.rod({ 0, 0, H - 1.5f }, { std::cos(an) * reach, std::sin(an) * reach, H + r.f(2.5f, 5.5f) }, 1.2f, bark, 0.5f)
            .m(MATTE).t(T_NOISE, 2.2f, barkDark).bev(0.25f);
    }
    const int n = 9 + (int)r.f(0, 3);
    for (int i = 0; i < n; i++) {
        const float an = i * 6.2832f / n + r.f(-0.35f, 0.35f);
        const float west = -std::cos(an);
        const float d = r.f(4.5f, 10.0f), zz = H + r.f(0.0f, 4.6f) + west * 2.0f;
        const float rr = r.f(5.0f, 7.4f);
        const Color body = west > 0.35f ? autumnLit : (west < -0.45f ? autumnDeep : autumnMid);
        clump(m, std::cos(an) * d, std::sin(an) * d, zz, rr * 1.18f, rr * 1.18f, rr * r.f(0.46f, 0.62f), body, west > 0.35f ? autumnMid : autumnDeep);
    }
    for (int i = 0; i < 4; i++) {
        const float an = -2.3f + i * 1.5f + r.f(-0.25f, 0.25f), d = r.f(1.2f, 4.8f), zz = H + r.f(5.0f, 7.6f), rr = r.f(3.2f, 4.6f);
        clump(m, -1.4f + std::cos(an) * d, std::sin(an) * d, zz, rr, rr, rr * 0.52f, i < 2 ? C(0xE6A848) : autumnLit, autumnMid);
    }
}
M3D_PROP("tree.autumn", treeAutumn)

// ---------------------------------------------------------------- bush: a low, wide knot of leaf clumps
static void bush(Model& m, int variant) {
    Rng r(0xB05Fu + variant * 3571u);
    m.dirt = 0.2f;
    const int n = 4 + (int)r.f(0, 2);
    for (int i = 0; i < n; i++) {
        const float an = i * 6.2832f / n + r.f(-0.4f, 0.4f), d = r.f(2.0f, 6.0f), rr = r.f(4.4f, 6.2f);
        clump(m, std::cos(an) * d, std::sin(an) * d, rr * 0.5f + r.f(0, 1.0f), rr * 1.15f, rr * 1.15f, rr * 0.56f,
              std::cos(an) < -0.3f ? bushLit : bushMid, bushDeep);
    }
    for (int i = 0; i < 2; i++) {
        const float an = -2.2f + i * 1.4f, d = r.f(0.5f, 3.0f), rr = r.f(3.0f, 4.2f);
        clump(m, -1.0f + std::cos(an) * d, std::sin(an) * d, rr + r.f(1.2f, 2.2f), rr, rr, rr * 0.6f, bushLit, bushMid);
    }
    if (variant & 1) {   // berries
        for (int i = 0; i < 5; i++) {
            const float an = r.f(0, 6.2832f), d = r.f(0.5f, 4.0f);
            m.ball(std::cos(an) * d, std::sin(an) * d, r.f(3.0f, 6.0f), 0.7f, 0.7f, 0.7f, C(0xB0362E)).m(MATTE).f(F_NOSHADOW);
        }
    }
}
M3D_PROP("bush", bush)

// ---------------------------------------------------------------- boulder: a weathered block, faceted, moss on the north face
static void boulder(Model& m, int variant) {
    Rng r(0xB01Du + variant * 2731u);
    m.dirt = 0.35f;
    const float sc = r.f(0.85f, 1.15f);
    const Color base = shade(stone, r.f(0.88f, 1.1f));
    // the main mass: a block turned a little, heavily bevelled, then a shoulder and a foot stone
    m.box(0, 0, 3.0f * sc, 4.6f * sc, 3.9f * sc, 2.9f * sc, base).m(MATTE).t(T_NOISE, 3.2f, stoneDark).bev(1.1f).rz(r.f(-24, 24)).ry(r.f(-9, 9));
    m.box(-0.6f * sc, 1.6f * sc, 4.6f * sc, 2.8f * sc, 2.0f * sc, 1.6f * sc, stonePale).m(MATTE).t(T_NOISE, 2.4f, base).bev(0.9f).rz(r.f(-30, 30));
    m.box(3.4f * sc, 1.2f * sc, 1.2f * sc, 2.0f * sc, 1.8f * sc, 1.2f * sc, stoneDark).m(MATTE).t(T_NOISE, 2.2f, base).bev(0.7f).rz(r.f(-30, 30));
    // moss on the shaded, damp north face (and a lick over the top)
    m.ball(-1.0f * sc, -3.4f * sc, 3.2f * sc, 2.6f * sc, 1.2f * sc, 1.6f * sc, moss).m(MATTE).t(T_NOISE, 2.0f, C(0x3E5626)).bev(0.5f);
    m.ball(-1.5f * sc, 0.5f * sc, 5.9f * sc, 1.8f * sc, 1.6f * sc, 0.6f * sc, moss).m(MATTE).t(T_NOISE, 1.8f, C(0x465E2A)).bev(0.4f);
}
M3D_PROP("boulder", boulder)
