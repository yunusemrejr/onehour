// One Hour - 3D models: the twelve fixed-wing aircraft of both armies (see m3d.h / m3d_parts.h).
// Cyber: pale ceramic plate with deep cobalt trim, cyan nozzle glow, cranked and faceted planforms.
// Clanker: olive and grey camouflage over gunmetal, hazard paint and rust, amber nozzle glow, riveted hardware.
// All are aircraft: body.ground = false, the belly rides at z = 1..3 (the game adds the flight height), the turret stays empty.
// Plan coordinates are the old painters' canvas pixels minus 32, so every type keeps its old length and wingspan.
#include "m3d_parts.h"

using namespace m3d;

namespace {

// ---------------------------------------------------------------- the palette of one airframe
struct Pal {
    Color hull, hull2, dark, trim, glow, glass, steel, store, storeTip, haz, rivet;
    Tex tex; float texS;
};
static Pal cyberPal() {
    Pal p;
    p.hull = pal::cHull; p.hull2 = pal::cHull2; p.dark = pal::cDark; p.trim = pal::cTrim;
    p.glow = pal::cGlow; p.glass = pal::cGlass; p.steel = pal::cSteel; p.rivet = pal::cSteel;
    p.store = C(0xBDC8D6); p.storeTip = pal::cDark; p.haz = C(0x93A6C4);
    p.tex = T_PANEL; p.texS = 6.5f;
    return p;
}
static Pal clankerPal(Color hull, Color hull2) {
    Pal p;
    p.hull = hull; p.hull2 = hull2; p.dark = pal::kDark; p.trim = pal::kHaz;
    p.glow = pal::kGlow; p.glass = C(0x4A6E7E); p.steel = pal::kSteel; p.rivet = pal::kBrass;
    p.store = C(0xAAA890); p.storeTip = pal::kDark; p.haz = pal::kHaz;
    p.tex = T_CAMO; p.texS = 8.0f;
    return p;
}
static const Pal CY = cyberPal();

// ---------------------------------------------------------------- fuselage: 2-4 tapered tube sections, nose to tail
struct Sec { float x0, x1, r0, r1, cz; };   // radius r0 at x0, r1 at x1, the axis at height cz
static void fuse(Model& m, std::initializer_list<Sec> L, const Pal& p) {
    for (const Sec& s : L) {
        float len = s.x1 - s.x0;
        m.cyl('x', s.x0 + len * 0.5f, 0, s.cz, s.r0, len * 0.5f, p.hull, s.r1 / s.r0)
         .m(PAINT).t(p.tex, p.texS, p.hull2).bev(0.7f);
    }
}
static void tipCone(Model& m, float x0, float x1, float r, float z, Color col) {
    m.cyl('x', (x0 + x1) * 0.5f, 0, z, r, (x1 - x0) * 0.5f, col, 0.0f).m(PAINT).bev(0.4f);
}

// ---------------------------------------------------------------- lifting surfaces
// a convex plan outline given for the starboard half (y > 0), built twice: once as given, once mirrored to port.
// shrink > 0 adds a second copy of the outline pulled toward its own centroid, standing just proud of the surface: the owner's panel.
static void panel(Model& m, std::initializer_list<P2> up, float z0, float z1, Color col, const Pal& p, float bevel, float shrink) {
    float cx = 0, cy = 0;
    for (const P2& q : up) { cx += q.a; cy += q.b; }
    cx /= (float)up.size(); cy /= (float)up.size();
    for (int s = -1; s <= 1; s += 2) {
        std::vector<P2> v, w;
        for (const P2& q : up) { v.push_back({ q.a, s * q.b }); w.push_back({ q.a + (cx - q.a) * shrink, s * (q.b + (cy - q.b) * shrink) }); }
        m.plan(v, z0, z1, col).m(PAINT).t(p.tex, p.texS, p.hull2).bev(bevel);
        if (shrink > 0) m.plan(w, z1 - 0.02f, z1 + 0.2f, OWNER).bev(0.1f);
    }
}
// a plain owner-coloured plate lying on a wing (both sides): the owner's panel
static void panelOwn(Model& m, std::initializer_list<P2> up, float z0, float z1) {
    for (int s = -1; s <= 1; s += 2) {
        std::vector<P2> v;
        for (const P2& q : up) v.push_back({ q.a, s * q.b });
        m.plan(v, z0, z1, OWNER).bev(0.1f);
    }
}
// a swept wing or tailplane as one convex quad a side
struct Wing { float leR, leT, teT, teR, yR, yT, z0, z1; };      // x of the leading / trailing edge at the root and at the tip
static float leAt(const Wing& w, float y) { return w.leR + (w.leT - w.leR) * (y - w.yR) / (w.yT - w.yR); }
static float teAt(const Wing& w, float y) { return w.teR + (w.teT - w.teR) * (y - w.yR) / (w.yT - w.yR); }
static void wing(Model& m, const Wing& w, const Pal& p, Color col, int owner) {
    for (int s = -1; s <= 1; s += 2) {
        m.plan({ { w.leR, s * w.yR }, { w.leT, s * w.yT }, { w.teT, s * w.yT }, { w.teR, s * w.yR } },
               w.z0, w.z1, col).m(PAINT).t(p.tex, p.texS, p.hull2).bev(0.3f);
        if (!owner) continue;
        float in = 0.8f;                                          // the owner-coloured inlay, inset from every edge
        float y0 = owner == 1 ? (w.yR + w.yT) * 0.5f - (w.yT - w.yR) * 0.17f : w.yR + (w.yT - w.yR) * 0.74f - in;
        float y1 = owner == 1 ? (w.yR + w.yT) * 0.5f + (w.yT - w.yR) * 0.17f : w.yT - in;
        m.plan({ { leAt(w, y0) - in, s * y0 }, { leAt(w, y1) - in, s * y1 }, { teAt(w, y1) + in, s * y1 }, { teAt(w, y0) + in, s * y0 } },
               w.z1 - 0.02f, w.z1 + 0.2f, OWNER).bev(0.1f);
    }
}
// a leading-edge root extension: a narrow strake hugging the fuselage
static void strake(Model& m, const Wing& w, const Pal& p, Color col) {
    for (int s = -1; s <= 1; s += 2)
        m.plan({ { w.leR, s * w.yR }, { w.leT, s * w.yT }, { w.teT, s * w.yT }, { w.teR, s * w.yR } },
               w.z0, w.z1, col).m(PAINT).t(p.tex, p.texS, p.hull2).bev(0.3f);
}
// a thin slab lying along the surface from (xa,ya) to (xb,yb): stripes, tip bands, roundel bars
static void plate(Model& m, float xa, float ya, float xb, float yb, float w, float z0, float z1, Color col) {
    float dx = xb - xa, dy = yb - ya, L = std::sqrt(dx * dx + dy * dy);
    m.box((xa + xb) * 0.5f, (ya + yb) * 0.5f, (z0 + z1) * 0.5f, L * 0.5f, w * 0.5f, (z1 - z0) * 0.5f, col)
     .m(PAINT).bev(0.1f).rz(std::atan2(dy, dx) * 57.29578f);
}

// ---------------------------------------------------------------- tails
// a thin swept fin slab, one a side, leaning outward by cant degrees; bandW > 0 puts an owner-coloured band on it
static void fin(Model& m, float xr0, float xr1, float xt0, float xt1, float z0, float z1,
                float yoff, float th, float cant, const Pal& p, Color col, float bandW) {
    for (int s = -1; s <= 1; s += 2) {
        float y0 = s > 0 ? yoff : -yoff - th;
        Prim& q = m.profileY({ { xr0, z0 }, { xr1, z0 }, { xt1, z1 }, { xt0, z1 } }, y0, y0 + th, col)
                  .m(PAINT).bev(0.25f).rx(s * cant);
        if (bandW > 0) q.t(T_BAND, bandW, OWNER);
        else q.t(p.tex, p.texS * 0.8f, p.hull2);
    }
}
// a single centreline fin
static void finC(Model& m, float xr0, float xr1, float xt0, float xt1, float z0, float z1,
                 float th, const Pal& p, Color col, float bandW) {
    Prim& q = m.profileY({ { xr0, z0 }, { xr1, z0 }, { xt1, z1 }, { xt0, z1 } }, -th * 0.5f, th * 0.5f, col)
              .m(PAINT).bev(0.3f);
    if (bandW > 0) q.t(T_BAND, bandW, OWNER);
    else q.t(p.tex, p.texS * 0.8f, p.hull2);
}

// ---------------------------------------------------------------- engines, cockpit, stores
static void nozzle(Model& m, float x, float y, float z, float r, const Pal& p) {
    m.cyl('x', x, y, z, r, 1.2f, p.dark).m(METAL).bev(0.3f);
    m.cyl('x', x - r * 0.85f, y, z, r * 0.58f, 0.35f, p.glow).m(EMIT).bev(0);
}
// a blended intake pod on each flank with its dark lip
static void intake(Model& m, float x, float y, float z, float len, float w, float h, const Pal& p, Color col) {
    for (int s = -1; s <= 1; s += 2) {
        m.box(x, s * y, z, len * 0.5f, w * 0.5f, h * 0.5f, col).m(PAINT).t(p.tex, p.texS, p.hull2).bev(0.5f);
        m.box(x + len * 0.5f, s * y, z, 0.5f, w * 0.4f, h * 0.4f, p.dark).m(PAINT).t(T_GRILLE, 1.3f, p.dark).bev(0.15f);
    }
}
// a bubble canopy of glass with its windscreen frame
static void canopy(Model& m, float x, float len, float w, float zc, float h, const Pal& p, Color frame) {
    m.ball(x, 0, zc, len, w, h, p.glass).m(GLASS);
    m.box(x + len * 0.62f, 0, zc + h * 0.12f, 0.45f, w * 0.8f, h * 0.7f, frame).m(PAINT).bev(0.15f);
}
// one hung store, mirrored to both sides: kind 0 missile, 1 bomb, 2 rocket pod, 3 the compact nuclear weapon
static void slung(Model& m, float xa, float xb, float y, float z, float r, const Pal& p, Color body, Color tip, int kind) {
    float mid = (xa + xb) * 0.5f, hl = (xb - xa) * 0.5f - r * 0.8f;
    m.cyl('x', mid, y, z, r, hl, body).m(PAINT).bev(0.35f);
    if (kind == 2) {
        m.cyl('x', xb - r * 0.5f, y, z, r * 0.88f, r * 0.55f, p.haz).m(PAINT).bev(0.2f);
        m.ball(xa + r * 0.5f, y, z, r * 0.5f, r * 0.5f, r * 0.5f, p.dark).m(METAL);
    } else {
        m.ball(xb - r * 0.6f, y, z, r * 0.82f, r * 0.82f, r * 0.82f, tip).m(PAINT);
        if (kind == 0) {
            m.box(xa + r * 1.0f, y, z, r * 0.8f, r * 1.9f, 0.2f, body).m(PAINT).bev(0.1f);
            m.box(xa + r * 1.0f, y, z, r * 0.2f, r * 0.8f, r * 1.8f, body).m(PAINT).bev(0.1f);
        } else if (kind == 1) {
            m.box(xa + r * 0.9f, y, z + r * 0.45f, r * 0.95f, r * 0.24f, r * 0.6f, p.dark).m(PAINT).bev(0.1f);
        } else {
            m.box(mid + hl * 0.35f, y, z, r * 0.3f, r * 1.05f, 0.22f, p.trim).m(PAINT).bev(0.1f);
            m.box(xa + r * 0.7f, y, z, r * 0.95f, r * 0.26f, r * 0.75f, p.dark).m(PAINT).bev(0.1f);
        }
    }
    m.box(mid + hl * 0.2f, y, z + r + 0.45f, r * 0.45f, 0.3f, 0.95f, p.dark).m(METAL).bev(0.1f);   // the pylon
}
static void store(Model& m, float xa, float xb, float y, float z, float r, const Pal& p, Color body, Color tip, int kind) {
    slung(m, xa, xb, -y, z, r, p, body, tip, kind);
    slung(m, xa, xb, y, z, r, p, body, tip, kind);
}
static void navLights(Model& m, float x, float y, float z) {
    m.ball(x, -y, z, 0.5f, 0.5f, 0.5f, C(0xFF5046)).m(EMIT);
    m.ball(x, y, z, 0.5f, 0.5f, 0.5f, C(0x50FF78)).m(EMIT);
}
// a thin dorsal spine fairing along the centreline
static void spine(Model& m, float xa, float xb, float z0, float z1, float w, const Pal& p, Color col) {
    float mid = (xa + xb) * 0.5f, hl = (xb - xa) * 0.5f;
    m.taper(mid, 0, z0, hl, w, z1 - z0, hl * 0.72f, w * 0.6f, col).m(PAINT).t(p.tex, p.texS, p.hull2).bev(0.3f);
}
static void hazard(Model& m, float x, float y, float z, float lx, float ly, const Pal& p) {
    for (int s = -1; s <= 1; s += 2)
        m.box(x, s * y, z, lx, ly, 0.18f, p.haz).m(PAINT).t(T_HAZARD, 1.5f, p.dark).bev(0);
}
// an owner-colour disc lying flat on each wing
static void roundel(Model& m, float x, float y, float z, float r) {
    for (int s = -1; s <= 1; s += 2) m.ball(x, s * y, z, r, r, 0.2f, OWNER).bev(0.1f);
}
// the owner's band across the tail
static void tailBand(Model& m, float x, float y, float z, const Pal& p) {
    m.box(x, 0, z, 1.3f, y, 0.4f, p.dark).m(PAINT).t(T_STRIPE, 1.3f, OWNER).bev(0.1f);
}
// ================================================================ 1 / 2 - the supersonic jets (Specter / Talon): 57 long, 46 across
// Specter (Cyber): a cropped delta with a chined nose, a bubble canopy, canted twin fins, twin cyan nozzles,
// two bombs a side on the pylons and a missile on each wingtip rail.
static void cJet(Model& b, Model&, int) {
    b.ground = false; b.dirt = 0.06f;
    const Pal& p = CY;
    fuse(b, { { -24, -6, 3.2f, 3.5f, 5.4f }, { -6, 12, 3.5f, 3.5f, 5.5f }, { 12, 20, 3.5f, 2.7f, 5.4f } }, p);
    tipCone(b, 19, 30, 2.7f, 5.3f, p.hull2);
    spine(b, -6, 17, 6.7f, 7.7f, 1.4f, p, p.hull2);                                   // the chined deck
    panel(b, { { 12, 3.8f }, { -12, 23.0f }, { -14.5f, 20.4f }, { -17.5f, 3.5f } }, 4.3f, 5.3f, p.hull, p, 0.3f, 0);
    panelOwn(b, { { -1.0f, 15.6f }, { -9.0f, 21.0f }, { -13.5f, 20.6f }, { -12.5f, 15.0f } }, 5.3f, 5.55f);   // the owner's outer panel
    strake(b, { 12, 8.5f, 3.0f, 1.0f, 3.4f, 6.4f, 4.7f, 5.6f }, p, p.hull2);          // leading-edge root extension
    wing(b, { -12, -20.5f, -19.4f, -15, 2.0f, 5.5f, 5.0f, 5.8f }, p, p.hull, 0);      // the stabilators
    fin(b, -11, -15.5f, -21.0f, -17.0f, 6.2f, 10.4f, 2.3f, 0.8f, 15, p, p.hull, 2.3f);
    nozzle(b, -25, -3.8f, 5.3f, 1.8f, p); nozzle(b, -25, 3.8f, 5.3f, 1.8f, p);
    intake(b, 2.5f, 3.4f, 4.2f, 4.2f, 1.9f, 2.3f, p, p.hull2);
    canopy(b, 15.2f, 4.4f, 2.1f, 7.5f, 1.7f, p, p.dark);
    store(b, -11, 1.5f, 13.5f, 3.2f, 1.5f, p, C(0x9AA6B4), p.dark, 1);
    store(b, -13.5f, -3.0f, 6.5f, 3.3f, 1.5f, p, C(0x9AA6B4), p.dark, 1);
    store(b, -16.2f, -4.5f, 21.8f, 3.9f, 0.85f, p, p.store, p.storeTip, 0);           // the wingtip missiles
    plate(b, -13.5f, 21.0f, -14.5f, 22.6f, 1.0f, 5.35f, 5.55f, OWNER);                 // the owner's tip band
    navLights(b, -14.0f, 22.4f, 5.6f);
    tailBand(b, -22.5f, 2.6f, 6.9f, p);
    b.rod({ -19, 0, 8.2f }, { -12.5f, 0, 9.2f }, 0.22f, p.dark).f(F_NOSHADOW);
    for (int s = -1; s <= 1; s += 2) b.box(1.0f, s * 2.9f, 2.5f, 2.4f, 0.8f, 0.4f, p.hull2).m(PAINT).bev(0.15f);
    for (int s = -1; s <= 1; s += 2) b.box(9.0f, s * 2.6f, 7.9f, 1.2f, 0.5f, 0.4f, p.trim).m(PAINT).bev(0.1f);   // cobalt deck trim
}
M3D_UNIT(U_C_JET, cJet)

// Talon (Clanker): a broad flat-sided attack jet - a blunt duck-bill nose, a wide side-by-side cockpit,
// three rocket pods a side, canted fins with owner-coloured tips, hazard-striped tips and amber nozzles.
static void kJet(Model& b, Model&, int) {
    b.ground = false; b.dirt = 0.1f;
    const Pal p = clankerPal(C(0x5E6B42), C(0x8A8058));
    fuse(b, { { -24.5f, -6, 3.5f, 3.8f, 5.5f }, { -6, 14, 3.8f, 3.8f, 5.6f }, { 14, 22, 3.8f, 3.3f, 5.5f } }, p);
    b.box(25.5f, 0, 5.5f, 3.4f, 3.1f, 1.7f, p.hull).m(PAINT).t(T_PANEL, 5, p.hull2).bev(0.8f);      // the duck-bill nose
    b.profileY({ { 21, 3.9f }, { 29.2f, 4.6f }, { 29.2f, 6.8f }, { 21, 7.2f } }, -2.9f, 2.9f, p.hull2)
     .m(PAINT).t(T_CAMO, 6, p.hull).bev(0.4f);                                                       // the flat nose sides
    b.box(28.4f, 0, 7.0f, 1.2f, 1.7f, 0.35f, p.steel).m(METAL).bev(0.15f);                          // pitot boom
    wing(b, { 10, -13, -16.5f, -8, 4.0f, 23.0f, 4.3f, 5.4f }, p, p.hull, 0);
    panelOwn(b, { { -1.0f, 8.0f }, { -8.0f, 17.0f }, { -13.0f, 16.5f }, { -7.0f, 8.0f } }, 5.4f, 5.62f);   // the owner's panel
    wing(b, { -12, -22, -24.4f, -16.2f, 1.8f, 6.8f, 5.1f, 5.9f }, p, p.hull, 0);
    fin(b, -12.5f, -17.5f, -23.5f, -19.5f, 6.4f, 10.8f, 2.6f, 0.9f, 13, p, p.hull, 2.6f);
    nozzle(b, -26.2f, -3.4f, 5.4f, 2.1f, p); nozzle(b, -26.2f, 3.4f, 5.4f, 2.1f, p);
    intake(b, 4.0f, 3.3f, 4.2f, 4.8f, 2.1f, 2.5f, p, p.hull2);
    canopy(b, 18.5f, 4.6f, 3.0f, 7.5f, 1.5f, p, p.dark);                                             // the wide cockpit
    store(b, -15.5f, -5.5f, 20.0f, 3.5f, 1.3f, p, p.store, p.storeTip, 2);
    store(b, -14.5f, -3.5f, 15.0f, 3.4f, 1.3f, p, p.store, p.storeTip, 2);
    store(b, -13.0f, -2.0f, 10.0f, 3.4f, 1.3f, p, p.store, p.storeTip, 2);
    hazard(b, -10.5f, 21.5f, 5.7f, 2.2f, 1.6f, p);
    navLights(b, -19.0f, 22.6f, 6.0f);
    tailBand(b, -22.5f, 3.0f, 7.0f, p);
    for (int s = -1; s <= 1; s += 2) b.box(16.0f, s * 3.4f, 2.3f, 2.2f, 0.6f, 0.45f, p.steel).m(METAL).bev(0.15f);
    b.box(-2.0f, 0, 7.5f, 4.0f, 1.6f, 0.4f, p.hull2).m(PAINT).t(T_NOISE, 5, pal::kRust).bev(0.2f);   // a rusted spine hatch
    for (int i = 0; i < 3; i++) b.box(-14 + i * 4.0f, 0, 7.9f, 0.5f, 0.5f, 0.28f, p.rivet).m(METAL).bev(0);
}
M3D_UNIT(U_K_JET, kJet)

// ================================================================ 3 / 4 - the air-strike fighters (light grey): 51 long, 38 across
// Both: broad trapezoid wings with a leading-edge root extension, twin canted fins with an owner's flash, twin nozzles,
// two heavy bombs and a missile on each wing. Cyber's fins splay wide, Clanker's are shorter and steeper and carry canards.
static void strikeJet(Model& b, const Pal& p, bool cy) {
    b.ground = false; b.dirt = cy ? 0.05f : 0.12f;
    fuse(b, { { -21, -4, 3.2f, 3.5f, 5.3f }, { -4, 13, 3.5f, 3.5f, 5.4f }, { 13, 19, 3.5f, 2.9f, 5.3f } }, p);
    tipCone(b, 18, 29.5f, 2.8f, 5.2f, p.hull2);
    panel(b, { { 12, 2.6f }, { -6.5f, 19.1f }, { -11.2f, 17.4f }, { 0, 1.4f } }, 4.2f, 5.3f, p.hull, p, 0.3f, 0);
    panelOwn(b, { { -4.5f, 14.0f }, { -6.8f, 18.0f }, { -9.0f, 17.6f }, { -7.4f, 13.6f } }, 5.3f, 5.5f);   // the owner's wingtips
    strake(b, { 17, 3.0f, 0.5f, 4.6f, 2.4f, 4.4f, 4.6f, 5.5f }, p, p.hull2);                          // the LEX
    wing(b, { -10, -19.5f, -20.8f, -13.4f, 1.4f, 5.6f, 4.9f, 5.7f }, p, p.hull, 0);
    if (cy) fin(b, -12.6f, -17.0f, -19.2f, -22.0f, 6.1f, 10.6f, 3.0f, 0.85f, 16, p, p.hull, 2.6f);
    else    fin(b, -12.6f, -16.5f, -15.5f, -18.0f, 6.1f, 10.0f, 3.0f, 0.9f, 10, p, p.hull, 2.4f);
    nozzle(b, -22.4f, -2.8f, 5.2f, 2.0f, p); nozzle(b, -22.4f, 2.8f, 5.2f, 2.0f, p);
    intake(b, 4.4f, 2.4f, 4.1f, 4.4f, 1.7f, 2.1f, p, p.hull2);
    canopy(b, 8.5f, 4.8f, 2.2f, 7.4f, 1.7f, p, p.dark);
    if (!cy) panel(b, { { 14, 3.4f }, { 7.0f, 6.8f }, { 5.4f, 6.8f }, { 9.4f, 3.2f } }, 4.9f, 5.5f, p.hull, p, 0.2f, 0);   // canards
    store(b, -7.2f, 3.0f, 12.5f, 3.2f, 1.3f, p, p.store, C(0xECD89A), 1);
    store(b, -3.4f, 7.2f, 7.4f, 3.3f, 1.3f, p, p.store, C(0xECD89A), 1);
    store(b, -11.2f, -4.0f, 16.4f, 3.6f, 0.9f, p, C(0xE2E4E6), p.dark, 0);
    plate(b, -7.0f, 17.5f, -9.5f, 18.6f, 1.0f, 5.35f, 5.55f, OWNER);
    navLights(b, -8.0f, 18.6f, 5.5f);
    tailBand(b, -19.5f, 2.6f, 6.8f, p);
    if (cy) { b.rod({ -17, 0, 8.4f }, { -11, 0, 9.4f }, 0.22f, p.dark).f(F_NOSHADOW);
        for (int s = -1; s <= 1; s += 2) b.box(6.0f, s * 2.6f, 7.7f, 1.2f, 0.5f, 0.4f, p.trim).m(PAINT).bev(0.1f);
    } else {
        hazard(b, -3.0f, 14.0f, 5.5f, 2.2f, 2.0f, p);
        for (int i = 0; i < 3; i++) b.box(-4 + i * 5.0f, 0, 7.5f, 1.2f, 1.4f, 0.3f, p.rivet).m(METAL).bev(0.1f);
    }
}
static void cStrike(Model& b, Model&, int) { strikeJet(b, CY, true); }
M3D_UNIT(U_C_STRIKEJET, cStrike)
static void kStrike(Model& b, Model&, int) { strikeJet(b, clankerPal(C(0x77808C), C(0x4E5763)), false); }
M3D_UNIT(U_K_STRIKEJET, kStrike)
// ================================================================ 5 / 6 - the plain fighters: Raptor (Cyber) 55 long, 40 across; Saber (Clanker) 56 long, 42 across
// Raptor: a diamond wing with a cut-back tip, a bubble canopy over a slim body, canted twin fins, two missiles a side.
static void cFighter(Model& b, Model&, int) {
    b.ground = false; b.dirt = 0.04f;
    const Pal& p = CY;
    fuse(b, { { -26, -6, 2.9f, 3.4f, 5.2f }, { -6, 18, 3.4f, 3.4f, 5.4f } }, p);
    tipCone(b, 18, 28, 3.4f, 5.4f, p.hull2);                                                   // the pointed nose
    wing(b, { 8.5f, -12.5f, -16.8f, -10.2f, 2.5f, 20.4f, 4.0f, 5.0f }, p, p.hull, 2);         // diamond wing, owner's colour on the outer panels
    panel(b, { { 4.5f, 4.6f }, { -3.5f, 12.2f }, { -7.0f, 9.8f }, { -0.5f, 4.4f } }, 5.0f, 5.25f, p.dark, p, 0.2f, 0);   // radar-absorbing panel
    panel(b, { { -9.5f, 3.8f }, { -18.2f, 8.6f }, { -21.0f, 6.8f }, { -15.8f, 1.8f } }, 4.6f, 5.2f, p.hull2, p, 0.6f, 0);  // tailplanes
    fin(b, -14.0f, -17.0f, -19.0f, -24.0f, 5.8f, 10.4f, 3.0f, 0.8f, 14, p, p.hull, 2.2f);
    nozzle(b, -27.0f, -2.8f, 5.4f, 1.9f, p); nozzle(b, -27.0f, 2.8f, 5.4f, 1.9f, p);
    intake(b, 4.0f, 2.6f, 3.8f, 3.6f, 1.6f, 1.9f, p, p.hull2);
    canopy(b, 12.4f, 5.4f, 2.6f, 7.3f, 1.8f, p, p.dark);
    store(b, -14.6f, -5.0f, 14.4f, 3.0f, 0.9f, p, p.store, p.storeTip, 0);
    store(b, -11.8f, -0.4f, 8.6f, 3.2f, 0.9f, p, p.store, p.storeTip, 0);
    navLights(b, -15.6f, 19.2f, 5.6f);
}
M3D_UNIT(U_C_FIGHTER, cFighter)

// Saber: a broad swept wing with a leading-edge root extension, fat engine nacelles hugging the rear fuselage with their nozzles behind,
// big canted twin fins with owner-coloured tips, a sand patch on the wing and two missiles a side.
static void kFighter(Model& b, Model&, int) {
    b.ground = false; b.dirt = 0.1f;
    const Pal p = clankerPal(C(0x546242), C(0x323C28));
    fuse(b, { { -23, -6, 2.8f, 3.4f, 5.0f }, { -6, 18, 3.4f, 3.4f, 5.4f } }, p);
    tipCone(b, 18, 28.5f, 3.3f, 5.4f, p.hull);
    wing(b, { 6.5f, -12.4f, -15.6f, -9.4f, 3.0f, 21.0f, 4.0f, 5.0f }, p, p.hull, 0);
    strake(b, { 18.5f, 4.0f, 6.0f, 14.0f, 2.4f, 10.2f, 4.5f, 5.4f }, p, p.hull2);            // leading-edge root extension
    panel(b, { { -8.0f, 12.5f }, { -12.5f, 15.5f }, { -13.5f, 14.2f }, { -9.5f, 11.5f } }, 5.0f, 5.2f, C(0xBAAA7C), p, 0.4f, 0);   // sand patch
    panelOwn(b, { { -10.0f, 15.0f }, { -11.6f, 20.2f }, { -14.2f, 20.0f }, { -13.2f, 14.6f } }, 5.0f, 5.2f);                       // the owner's wingtips
    panel(b, { { -11.5f, 2.0f }, { -22.0f, 9.6f }, { -24.4f, 8.4f }, { -16.2f, 0.2f } }, 4.6f, 5.2f, C(0x3F4A30), p, 0.6f, 0);     // tailplanes
    for (int s = -1; s <= 1; s += 2) {
        b.cyl('x', -10.0f, s * 6.2f, 3.2f, 2.6f, 13.0f, p.hull2, 0.85f).m(PAINT).t(p.tex, p.texS, p.hull).bev(0.6f);   // engine nacelle
        b.cyl('x', 3.0f, s * 6.2f, 3.2f, 2.2f, 0.4f, p.dark, 1.0f).m(METAL).bev(0.2f);                                 // intake lip
        nozzle(b, -23.5f, s * 6.2f, 3.2f, 1.9f, p);
    }
    fin(b, -14.5f, -19.5f, -21.0f, -24.0f, 5.6f, 11.4f, 3.2f, 0.9f, 18, p, p.hull, 2.4f);   // the owner's band on each fin
    canopy(b, 12.4f, 4.6f, 2.3f, 7.4f, 1.8f, p, p.dark);
    store(b, -12.5f, -2.5f, 11.2f, 3.2f, 1.0f, p, C(0xD6D8D2), C(0x3C3E42), 0);
    navLights(b, -13.7f, 19.4f, 5.6f);
}
M3D_UNIT(U_K_FIGHTER, kFighter)

// ================================================================ 7 / 8 - the supersonic bombers: Aurora (Cyber) 58 long, 31 across; Backfire (Clanker) 58 long, 46 across
// Aurora: a slender double delta, a long chine running out to a sharp nose, twin canted fins with owner tips, two slotted exhausts with
// cyan glow and one compact silver bomb under the inboard wing.
static void cSBomber(Model& b, Model&, int) {
    b.ground = false; b.dirt = 0.02f;
    const Pal& p = CY;
    fuse(b, { { -26, -4, 2.4f, 3.0f, 5.4f }, { -4, 18, 3.0f, 3.1f, 5.4f } }, p);
    tipCone(b, 18, 29.6f, 3.1f, 5.4f, p.hull2);                                                  // the sharp nose
    panel(b, { { 10.0f, 3.5f }, { -16.5f, 15.4f }, { -21.5f, 13.2f }, { -9.0f, 3.4f } }, 3.8f, 4.8f, p.dark, p, 0.5f, 0);   // the swept wing
    panel(b, { { 28.2f, 0.2f }, { 19.0f, 3.6f }, { 6.5f, 7.6f }, { -2.0f, 0.2f } }, 4.6f, 5.0f, p.hull2, p, 0.4f, 0);         // the chine
    fin(b, -18.0f, -24.0f, -22.0f, -26.0f, 5.6f, 11.2f, 2.8f, 0.8f, 20, p, p.hull, 2.0f);
    nozzle(b, -27.4f, -3.8f, 5.0f, 1.8f, p); nozzle(b, -27.4f, 3.8f, 5.0f, 1.8f, p);
    canopy(b, 14.6f, 4.0f, 1.6f, 7.6f, 1.5f, p, p.dark);
    spine(b, -20.0f, 16.0f, 7.8f, 8.9f, 1.0f, p, p.hull2);
    intake(b, 3.0f, 4.6f, 4.0f, 4.0f, 1.8f, 1.6f, p, p.dark);
    b.rod({ 29.2f, 0, 5.4f }, { 31.2f, 0, 5.4f }, 0.2f, p.steel).m(METAL).bev(0);                 // nose probe
    slung(b, -10.5f, 1.0f, 5.6f, 2.9f, 1.6f, p, C(0xC8D0DA), p.trim, 3);                          // the silver bomb, south side only
    navLights(b, -15.8f, 13.6f, 5.2f);
}
M3D_UNIT(U_C_SBOMBER, cSBomber)

// Backfire: a swept-wing bomber in grey over olive, intakes on the flanks beside the cockpit, a tall centreline fin with the owner's band,
// twin nozzles in the tail and one compact silver bomb under the inboard wing.
static void kSBomber(Model& b, Model&, int) {
    b.ground = false; b.dirt = 0.1f;
    const Pal p = clankerPal(C(0x787C6A), C(0x585C48));
    fuse(b, { { -27, -6, 2.7f, 3.5f, 5.6f }, { -6, 14, 3.5f, 3.5f, 5.6f } }, p);
    tipCone(b, 14, 30, 3.5f, 5.6f, p.hull2);
    wing(b, { 8.0f, -14.8f, -18.6f, -8.0f, 3.0f, 23.4f, 3.8f, 4.9f }, p, p.hull, 0);
    panel(b, { { -2.0f, 9.0f }, { -7.0f, 14.0f }, { -10.5f, 13.0f }, { -5.5f, 8.4f } }, 4.9f, 5.1f, C(0x585C48), p, 0.3f, 0);   // olive patch
    panelOwn(b, { { -12.0f, 19.0f }, { -16.5f, 23.0f }, { -17.6f, 21.8f }, { -14.0f, 18.0f } }, 4.9f, 5.1f);                       // the owner's wingtips
    intake(b, 12.8f, 3.4f, 3.2f, 3.0f, 1.6f, 2.0f, p, p.hull2);
    canopy(b, 22.2f, 4.0f, 1.8f, 7.8f, 1.6f, p, p.dark);
    panel(b, { { -15.0f, 3.4f }, { -25.4f, 9.8f }, { -27.6f, 7.6f }, { -21.0f, 1.4f } }, 4.6f, 5.2f, p.hull2, p, 0.6f, 0);     // swept tailplanes
    finC(b, -20.0f, -27.4f, -22.5f, -27.4f, 5.4f, 13.0f, 1.4f, p, p.hull, 2.2f);
    nozzle(b, -27.6f, -3.4f, 5.6f, 2.0f, p); nozzle(b, -27.6f, 3.4f, 5.6f, 2.0f, p);
    slung(b, -5.4f, 5.0f, -6.4f, 2.6f, 1.6f, p, C(0xC8D0DA), C(0xECBE3E), 3);                     // the silver bomb under the inboard wing
}
M3D_UNIT(U_K_SBOMBER, kSBomber)

// ================================================================ 9 / 10 - the strategic bombers: Nightfall (Cyber) 42 long, 56 across; Thunderhead (Clanker) 57 long, 51 across
// Nightfall: a B-2 style flying wing, one smooth planform with a swept leading edge, a low canopy bump on the centreline, twin exhaust
// slots and an owner roundel on each outboard panel.
static void cNuke(Model& b, Model&, int) {
    b.ground = false; b.dirt = 0.0f;
    const Pal& p = CY;
    panel(b, { { 18.0f, 0.0f }, { 6.0f, 11.0f }, { -20.0f, 27.8f }, { -22.0f, 25.5f }, { -10.0f, 11.5f } }, 2.2f, 3.6f, p.dark, p, 0.6f, 0);   // outboard panels
    panel(b, { { 18.0f, 0.0f }, { -10.0f, 11.5f }, { -12.0f, 0.0f } }, 2.4f, 3.8f, p.hull, p, 0.5f, 0);                                    // centre section
    b.ball(9.0f, 0, 3.4f, 4.4f, 2.2f, 1.4f, p.hull2).m(PAINT).t(T_PANEL, 3, p.hull).bev(0.6f);                                              // dorsal bump
    canopy(b, 9.5f, 2.6f, 1.2f, 4.6f, 0.8f, p, p.dark);
    for (int s = -1; s <= 1; s += 2) {
        b.box(-9.0f, s * 4.8f, 3.2f, 1.6f, 0.9f, 0.5f, p.dark).m(METAL).bev(0.2f);     // exhaust slot
        b.ball(-10.8f, s * 4.8f, 3.2f, 0.5f, 0.5f, 0.5f, p.glow).m(EMIT);
    }
    roundel(b, -3.0f, 12.0f, 3.8f, 2.0f);                                                       // roundel draws both outboard panels
    panelOwn(b, { { -13.0f, 19.0f }, { -17.5f, 24.5f }, { -19.5f, 23.0f }, { -15.0f, 17.8f } }, 3.6f, 3.8f);   // the owner's wingtips
    for (int i = 0; i < 3; i++) {                                                              // sawtooth trailing edge: three teeth a side
        const float t0 = i / 3.0f, t1 = (i + 1) / 3.0f, tm = t0 + (t1 - t0) * 0.8f;
        const P2 a{ -10 - 12 * t0, 11.5f + 14 * t0 }, c{ -10 - 12 * t1, 11.5f + 14 * t1 };
        panel(b, { a, c, { -10 - 12 * tm - 1.2f, 11.5f + 14 * tm - 1.0f } }, 2.2f, 3.4f, p.dark, p, 0.2f, 0);
    }
    for (int s = -1; s <= 1; s += 2) {
        b.box(-16.0f, s * 18.5f, 3.4f, 2.6f, 0.5f, 0.2f, p.hull2).m(PAINT).bev(0.1f).rz(s * 130.6f);   // elevon on the trailing edge
        b.ball(6.0f, s * 3.2f, 3.4f, 1.6f, 1.2f, 0.8f, p.dark).m(METAL);                             // flank intake
        b.box(-3.0f, s * 9.0f, 3.62f, 5.0f, 0.12f, 0.05f, pal::cGlow).m(EMIT);                    // cyan panel seam
    }
    b.rod({ 3.0f, 0, 3.6f }, { 5.0f, 0, 6.8f }, 0.18f, p.dark).f(F_NOSHADOW);                     // sensor mast
    navLights(b, -20.2f, 26.4f, 3.4f);
}
M3D_UNIT(U_C_NUKEBOMBER, cNuke)

// Thunderhead: a swing-wing bomber at mid sweep, a long tubular fuselage with a glazed nose, two paired nacelles under the inner wing
// with their nozzles behind, a swept tailplane and a tall centreline fin with the owner's band.
static void kNuke(Model& b, Model&, int) {
    b.ground = false; b.dirt = 0.05f;
    const Pal p = clankerPal(C(0x8A8F80), C(0x5E6446));
    fuse(b, { { -25, -4, 3.2f, 3.7f, 5.8f }, { -4, 17, 3.7f, 3.7f, 5.8f } }, p);
    tipCone(b, 17, 29.4f, 3.7f, 5.8f, p.hull);
    b.ball(22.6f, 0, 6.2f, 3.8f, 2.0f, 1.6f, p.glass).m(GLASS);                                  // the glazed nose
    wing(b, { 6.0f, -13.5f, -17.2f, -5.0f, 3.0f, 25.6f, 3.6f, 4.6f }, p, p.hull, 0);
    panel(b, { { -16.0f, 3.6f }, { -25.5f, 9.6f }, { -27.5f, 7.8f }, { -22.0f, 1.6f } }, 4.4f, 5.0f, p.hull2, p, 0.6f, 0);   // tailplanes
    finC(b, -9.0f, -19.0f, -22.0f, -26.5f, 5.4f, 12.6f, 1.2f, p, p.hull, 1.6f);
    for (int s = -1; s <= 1; s += 2) {
        b.cyl('x', 1.0f, s * 6.4f, 2.6f, 2.2f, 6.5f, p.hull2, 0.9f).m(PAINT).t(p.tex, p.texS, p.hull).bev(0.5f);   // paired nacelle
        b.cyl('x', 8.6f, s * 6.4f, 2.6f, 1.6f, 0.25f, p.dark).m(METAL);                                            // intake
        nozzle(b, -5.8f, s * 5.0f, 2.6f, 1.4f, p);
        nozzle(b, -5.8f, s * 7.8f, 2.6f, 1.4f, p);
    }
    roundel(b, -7.8f, 15.5f, 4.6f, 1.9f);                                                       // roundel draws both wings
    spine(b, -20.0f, 4.0f, 9.3f, 10.0f, 1.0f, p, p.hull2);                                      // dorsal spine
    navLights(b, -13.0f, 25.2f, 4.2f);
}
M3D_UNIT(U_K_NUKEBOMBER, kNuke)

// ================================================================ 11 / 12 - the carrier jets: 44 long, 29 across (the old painter's 0.84 scale)
// Cyber: swept wings with a leading-edge root extension and wide canted twin fins.
static void cCarrierJet(Model& b, Model&, int) {
    b.ground = false; b.dirt = 0.02f;
    const Pal& p = CY;
    fuse(b, { { -18, 13, 2.6f, 2.7f, 4.2f } }, p);
    tipCone(b, 13, 22, 2.7f, 4.2f, p.hull2);                                                    // the pointed radome
    wing(b, { 7.5f, -7.5f, -9.6f, -2.5f, 2.2f, 14.0f, 3.4f, 4.4f }, p, p.hull, 0);
    panel(b, { { 12.5f, 2.2f }, { 6.6f, 3.7f }, { 0.0f, 2.2f } }, 4.1f, 4.6f, p.hull2, p, 0.5f, 0);   // root extension
    panelOwn(b, { { -4.8f, 10.6f }, { -7.2f, 13.0f }, { -8.4f, 12.2f }, { -6.0f, 10.2f } }, 4.4f, 4.6f);   // the owner's wingtips
    panel(b, { { -9.1f, 2.0f }, { -15.3f, 5.3f }, { -16.4f, 4.1f }, { -11.2f, 0.8f } }, 3.9f, 4.5f, p.hull2, p, 0.5f, 0);   // stabilators
    fin(b, -12.5f, -16.0f, -17.5f, -20.0f, 4.4f, 9.0f, 2.0f, 0.7f, 12, p, p.hull, 1.8f);
    nozzle(b, -19.2f, -3.2f, 4.2f, 1.7f, p); nozzle(b, -19.2f, 3.2f, 4.2f, 1.7f, p);
    canopy(b, 5.4f, 3.8f, 1.9f, 6.4f, 1.5f, p, p.dark);
    intake(b, 2.0f, 2.8f, 3.4f, 3.2f, 1.2f, 1.4f, p, p.hull2);                                 // side intakes
    spine(b, -14.0f, 8.0f, 6.9f, 7.6f, 0.8f, p, p.hull2);
    b.rod({ -18.5f, 0, 2.4f }, { -22.0f, 0, 1.2f }, 0.3f, p.steel).m(METAL).bev(0.1f);       // tail hook
    navLights(b, -9.6f, 13.0f, 4.6f);
}
M3D_UNIT(U_C_CARRIERJET, cCarrierJet)

// Clanker: the same planform in grey, with canards on the nose shoulders, upright fins with the owner's band and a tail hook.
static void kCarrierJet(Model& b, Model&, int) {
    b.ground = false; b.dirt = 0.1f;
    const Pal p = clankerPal(C(0x6F7A7E), C(0x4C5458));
    fuse(b, { { -18, 13, 2.6f, 2.7f, 4.2f } }, p);
    tipCone(b, 13, 22, 2.7f, 4.2f, p.hull2);
    wing(b, { 7.5f, -7.5f, -9.6f, -2.5f, 2.2f, 14.0f, 3.4f, 4.4f }, p, p.hull, 0);
    panel(b, { { 10.4f, 2.8f }, { 15.8f, 4.6f }, { 14.8f, 2.0f } }, 4.0f, 4.5f, p.hull2, p, 0.4f, 0);   // canards
    panel(b, { { -9.1f, 2.0f }, { -15.3f, 5.3f }, { -16.4f, 4.1f }, { -11.2f, 0.8f } }, 3.9f, 4.5f, p.hull2, p, 0.5f, 0);   // stabilators
    fin(b, -13.0f, -16.5f, -17.0f, -18.0f, 4.4f, 8.6f, 2.0f, 0.8f, 6, p, p.hull, 1.8f);
    nozzle(b, -19.2f, -3.2f, 4.2f, 1.7f, p); nozzle(b, -19.2f, 3.2f, 4.2f, 1.7f, p);
    canopy(b, 5.4f, 3.8f, 1.9f, 6.4f, 1.5f, p, p.dark);
    b.rod({ -18.5f, 0, 2.4f }, { -22.0f, 0, 1.2f }, 0.3f, p.steel).m(METAL).bev(0.1f);   // tail hook
    intake(b, 2.0f, 2.8f, 3.4f, 3.2f, 1.2f, 1.4f, p, p.hull2);                                 // side intakes
    spine(b, -14.0f, 8.0f, 6.9f, 7.6f, 0.8f, p, p.hull2);
    b.box(-2.0f, 0, 7.2f, 4.0f, 1.6f, 0.4f, p.hull2).m(PAINT).t(T_NOISE, 5, pal::kRust).bev(0.2f);   // a rusted spine hatch
    hazard(b, 13.0f, 3.2f, 4.5f, 1.4f, 0.6f, p);                                               // hazard tips on the canards
}
M3D_UNIT(U_K_CARRIERJET, kCarrierJet)
}  // namespace
