#include "art2.h"
#include "paint.h"

using namespace paint;

// The special fighters of the Hotline Strikes, painted at 64x64 facing +x (nose at 62, exhausts at 2, wingspan about 52).
// Valkyrie (Cyber): a cranked stealth delta with sawtooth aft edges, dark radar-absorbing panels, cyan energy lines along the leading
// edges and the spine, canted twin fins and twin nozzles with a hot glow ring.
// Warhawk (Clanker): a broad-winged twin-engine fighter-bomber in olive over sand with a shark mouth, a gun muzzle and hazard-striped
// rocket pods. Both carry the team colour on their fins and a roundel.
// paint::Art polygons are convex, so every wing and fuselage part is a convex piece; the far side of the aircraft is drawn by mirroring
// the upper half about the fuselage axis (y = 32).
namespace {

const Color INK = rgb(8, 10, 16), WHT = rgb(236, 238, 240);

inline float far(float y) { return 64 - y; }   // the mirror of a design-space y about the fuselage axis

// a convex upper-half piece and its reflection
void symPoly(Art& A, std::vector<P> up, Color base, float bevel = 2.2f, float kk = 0.55f) {
    std::vector<P> lo;
    for (const P& p : up) lo.push_back({p.x, far(p.y)});
    A.poly(up, base, bevel, kk);
    A.poly(lo, base, bevel, kk);
}

// a stroke and its reflection
void lineS(Art& A, float ax, float ay, float bx, float by, float w, Color col) {
    A.line(ax, ay, bx, by, w, col);
    A.line(ax, far(ay), bx, far(by), w, col);
}

// a capsule and its reflection
void capS(Art& A, float ax, float ay, float bx, float by, float r, Color base, float bevel = 1.6f, float kk = 0.55f) {
    A.cap(ax, ay, bx, by, r, base, bevel, kk);
    A.cap(ax, far(ay), bx, far(by), r, base, bevel, kk);
}

// a flat-lit ellipse and its reflection (camouflage patches, glints)
void ellS(Art& A, float cx, float cy, float rx, float ry, Color col) {
    A.ell(cx, cy, rx, ry, col, 1, 0);
    A.ell(cx, far(cy), rx, ry, col, 1, 0);
}

// sawtooth trailing edge: triangular teeth stand off the straight edge a -> b, each leaning towards b. The normal (-dy, dx) points aft
// when the edge runs from the wingtip (y small) to the root, which is how the calls below use it.
void sawEdge(Art& A, P a, P b, int n, float d, Color base) {
    float dx = b.x - a.x, dy = b.y - a.y, L = hyp(dx, dy), nx = -dy / L, ny = dx / L;
    for (int i = 0; i < n; i++) {
        float t0 = (float)i / n, t1 = (float)(i + 1) / n, tm = t0 + (t1 - t0) * 0.8f;
        P p0{a.x + dx * t0, a.y + dy * t0}, p1{a.x + dx * t1, a.y + dy * t1}, pm{a.x + dx * tm + nx * d, a.y + dy * tm + ny * d};
        A.poly({p0, p1, pm}, base, 1.1f, 0.5f);
    }
}

// a rocket pod seen from above: a capsule from the motor (x0) to the warhead cone (x1), a coloured warhead and a dark motor bell.
// A soft shadow falls to the south-east, so the store reads as hanging under the wing.
void rocketPod(Art& A, float x0, float x1, float y, float r, Color body, Color warhead) {
    float ax = x0 + 1.0f, bx = x1 - r * 0.5f;
    A.soft((int)ax - 3, (int)(y - r - 4), (int)x1 + 5, (int)(y + r + 5), [=](float px, float py) { return sdCap(px, py, ax + 0.9f, y + 1.0f, bx + 0.9f, y + 1.0f, r); }, rgba(INK, 120), 1.8f);
    A.cap(ax, y, bx, y, r, body, 0.8f, 0.6f);
    A.circle(x1 - r * 0.1f, y, r * 0.78f, warhead, 0.7f, 0.4f);
    A.dot(x1 + 0.2f, y - r * 0.2f, r * 0.22f, rgba(WHT, 200));
    A.dot(x0 + 0.5f, y, r * 0.55f, INK);
}

// the player's mark: a jewel-like dot with a dark seat ring, lit from the north-west
void teamMark(Art& A, float cx, float cy, float r) {
    A.circle(cx, cy, r + 1.2f, rgba(INK, 235), 1.0f, 0.2f);
    A.circle(cx, cy, r, A.team, r, 0.7f);
    A.dot(cx - r * 0.32f, cy - r * 0.36f, r * 0.24f, rgba(mix(A.team, WHT, 0.7f), 235));
}

// a twin exhaust nozzle: a flared bell, six petals around the rim and a hot glow ring (the renderer adds the flame)
void nozzle(Art& A, float cx, float cy, float r, Color bell, Color glow) {
    A.circle(cx, cy, r, bell, 1.2f, 0.6f);
    for (int i = 0; i < 6; i++) {
        float t = i * 1.0471976f;
        A.dot(cx + std::cos(t) * r * 0.74f, cy + std::sin(t) * r * 0.74f, r * 0.2f, shade(bell, 1.6f));
    }
    A.ring(cx, cy, r * 0.56f, 0.6f, glow, 0.6f, 0);
    A.dot(cx, cy, r * 0.42f, INK);
    A.glow(cx, cy, r * 2.4f, rgba(glow, 110));
}

// enhance-style local contrast (an unsharp mask on the opaque pixels), as the other unit sprites get
void sharpen(Canvas& c, float amount) {
    std::vector<u32> src = c.px;
    auto at = [&](int x, int y) { return Canvas::unpack(src[clampi(y, 0, c.h - 1) * c.w + clampi(x, 0, c.w - 1)]); };
    for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) {
        Color p = at(x, y);
        if (p.a < 200) continue;
        float r = 0, g = 0, b = 0, w = 0;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
            Color q = at(x + dx, y + dy);
            if (q.a < 200) continue;
            float k = (dx == 0 && dy == 0) ? 2.0f : 1.0f;
            r += q.r * k; g += q.g * k; b += q.b * k; w += k;
        }
        r /= w; g /= w; b /= w;
        Color o{(u8)clampf(p.r + amount * (p.r - r), 0, 255), (u8)clampf(p.g + amount * (p.g - g), 0, 255), (u8)clampf(p.b + amount * (p.b - b), 0, 255), p.a};
        c.px[y * c.w + x] = Canvas::pack(o);
    }
}

// ---------------------------------------------------------------- Valkyrie (Cyber)

void valkyrie(Art& A) {
    const Color H = rgb(56, 68, 92), HL = rgb(92, 112, 144), CER = rgb(152, 170, 192), RAM = rgb(17, 21, 31), GL = rgb(110, 240, 255);
    const Color GLS = rgb(34, 92, 122), POD = rgb(118, 130, 150);
    // cranked delta wing with a sawtooth aft edge
    symPoly(A, {{51, 27.5f}, {36, 13.5f}, {22, 6.2f}, {17, 8.2f}, {14.5f, 22}, {14, 31.5f}}, H, 2.4f, 0.6f);
    sawEdge(A, {17, 8.2f}, {14, 31.5f}, 6, 2.8f, H);
    sawEdge(A, {14, far(31.5f)}, {17, far(8.2f)}, 6, 2.8f, H);
    // dark radar-absorbing panels with their seams, and a raised root blend
    symPoly(A, {{43, 27.6f}, {35, 19.2f}, {27.5f, 15.4f}, {31, 22.5f}}, RAM, 1.0f, 0.3f);
    symPoly(A, {{24, 22.4f}, {18.5f, 19.5f}, {20, 25.5f}}, RAM, 1.0f, 0.3f);
    symPoly(A, {{48, 28.2f}, {42, 27.8f}, {40, 30.5f}}, shade(H, 1.2f), 1.2f, 0.4f);
    lineS(A, 40, 24.6f, 24, 14.6f, 0.6f, rgba(INK, 170));
    lineS(A, 30.5f, 27.4f, 17, 26.2f, 0.6f, rgba(INK, 150));
    lineS(A, 25, 30.5f, 16.5, 28, 0.5f, rgba(GL, 90));
    // cyan energy lines along the leading edges, with a soft halo
    lineS(A, 50.4f, 27.4f, 36, 13.5f, 2.6f, rgba(GL, 60));
    lineS(A, 50.4f, 27.4f, 36, 13.5f, 0.8f, rgba(GL, 240));
    lineS(A, 36, 13.5f, 22.5f, 6.5f, 0.8f, rgba(GL, 230));
    A.glow(21, 6.8f, 3.5f, rgba(GL, 150)); A.glow(21, far(6.8f), 3.5f, rgba(GL, 150));
    // lifting body spine and fuselage facets
    A.cap(10, 32, 49, 32, 4.4f, shade(H, 1.1f), 2.4f, 0.6f);
    lineS(A, 14, 28.4f, 42, 28.4f, 0.6f, rgba(INK, 160));
    A.line(12, 32, 52, 32, 0.7f, rgba(GL, 200));
    // sharp radome nose
    symPoly(A, {{42, 27.6f}, {56, 29.4f}, {62.5f, 32}, {42, 32}}, CER, 1.8f, 0.6f);
    A.line(44, 32, 60, 32, 0.4f, rgba(WHT, 170));
    // canopy: blended dark glass, frame and two glints
    A.ell(41.5f, 32, 6.0f, 2.6f, GLS, 1.6f, 0.6f);
    A.line(36.2f, 32, 46.8f, 32, 0.4f, rgba(INK, 200));
    A.ell(40.5f, 30.8f, 3.0f, 0.7f, rgba(WHT, 210), 1, 0);
    A.dot(45.4f, 31.6f, 0.45f, rgba(WHT, 220));
    // canards and wing-root intakes with lips
    symPoly(A, {{45, 28.6f}, {54.5f, 24.5f}, {49.5f, 29.4f}}, shade(H, 1.15f), 1.2f, 0.5f);
    for (int m = 0; m < 2; m++) {
        float y = m ? far(28.4f) : 28.4f;
        A.box(31.5f, y, 3.8f, 1.2f, 0.5f, INK, 1, 0.3f);
        A.line(27.8f, y + (m ? 0.9f : -0.9f), 35.2f, y + (m ? 0.9f : -0.9f), 0.5f, rgba(HL, 200));
    }
    // canted twin fins: slim blades leaning outward, team colour on the tips
    capS(A, 17, 27, 9.5f, 17.5f, 1.45f, shade(CER, 0.9f), 1.0f, 0.5f);
    capS(A, 10.6f, 20.2f, 9.4f, 15.8f, 1.0f, A.team, 0.8f, 0.5f);
    A.dot(9.4f, 15.6f, 0.6f, rgba(WHT, 200)); A.dot(9.4f, far(15.6f), 0.6f, rgba(WHT, 200));
    // small horizontal stabilisers
    symPoly(A, {{19, 28.4f}, {9.4f, 25.6f}, {10.6f, 27.6f}, {15.5f, 30.6f}}, shade(H, 1.1f), 1.1f, 0.5f);
    // twin exhausts
    nozzle(A, 6.6f, 28.4f, 2.7f, shade(RAM, 2.2f), GL);
    nozzle(A, 6.6f, 35.6f, 2.7f, shade(RAM, 2.2f), GL);
    // underwing rocket pods (three a side) with cyan warheads
    const float vY[3] = {11.5f, 17.5f, 23.5f}, vX0[3] = {19, 22.5f, 25.8f}, vX1[3] = {30, 33.5f, 36.3f};
    for (int i = 0; i < 3; i++) {
        rocketPod(A, vX0[i], vX1[i], vY[i], 1.2f, POD, GL);
        rocketPod(A, vX0[i], vX1[i], far(vY[i]), 1.2f, POD, GL);
    }
    // access hatches, rivets and the team roundel on the wing
    for (int i = 0; i < 5; i++) lineS(A, 24 + i * 4.5f, 29.8f, 25 + i * 4.5f, 30.8f, 0.45f, rgba(RAM, 110));
    for (int i = 0; i < 6; i++) { A.dot(22 + i * 3.2f, 14.3f - i * 0.2f, 0.35f, rgba(HL, 200)); A.dot(22 + i * 3.2f, far(14.3f - i * 0.2f), 0.35f, rgba(HL, 200)); }
    A.box(38.5f, 29.6f, 1.6f, 0.6f, 0.2f, RAM, 1, 0.2f); A.box(38.5f, far(29.6f), 1.6f, 0.6f, 0.2f, RAM, 1, 0.2f);   // fuselage hatches
    teamMark(A, 25.5f, 22.8f, 1.9f); teamMark(A, 25.5f, far(22.8f), 1.9f);
}

// ---------------------------------------------------------------- Warhawk (Clanker)

void warhawk(Art& A) {
    const Color H = rgb(112, 120, 92), HD = rgb(84, 92, 68), SAND = rgb(178, 162, 118), DK = rgb(34, 36, 30), RUST = rgb(168, 94, 48);
    const Color YEL = rgb(236, 190, 62), STL = rgb(150, 152, 150), RED = rgb(206, 52, 40), GLS = rgb(74, 110, 126), FLAME = rgb(255, 150, 50);
    const Color POD = rgb(150, 152, 126);
    // broad wings with a straight trailing edge, olive with two-tone camouflage
    symPoly(A, {{46, 28.5f}, {33, 11.5f}, {24, 5.8f}, {18, 6.6f}, {12.5f, 31.5f}}, H, 2.6f, 0.6f);
    ellS(A, 27.5f, 15.2f, 5.4f, 2.0f, rgba(SAND, 200));
    ellS(A, 21.5f, 22.0f, 3.8f, 2.0f, rgba(HD, 190));
    ellS(A, 33.5f, 23.0f, 3.0f, 1.6f, rgba(SAND, 170));
    ellS(A, 35.5f, 10.0f, 2.8f, 1.3f, rgba(HD, 160));
    // flap and aileron seams, wingtip lights, weathering streaks
    lineS(A, 37, 27.4f, 17.6f, 30.2f, 0.7f, rgba(DK, 170));
    lineS(A, 31, 9.2f, 16.6f, 7.8f, 0.5f, rgba(DK, 130));
    lineS(A, 40, 22.5f, 27, 21.5f, 0.45f, rgba(DK, 120));
    lineS(A, 22.5f, 13.4f, 30.5f, 16.8f, 0.5f, rgba(RUST, 140));
    A.dot(21.2f, 6.8f, 0.85f, rgb(255, 80, 70)); A.dot(21.2f, far(6.8f), 0.85f, rgb(80, 255, 120));
    // big wing-root fairing and the fuselage with a bulging nose
    A.cap(9, 32, 44, 32, 5.0f, shade(H, 1.06f), 2.8f, 0.62f);
    symPoly(A, {{40, 27.2f}, {52, 27.6f}, {58.6f, 29.8f}, {61, 32}, {40, 32}}, shade(H, 1.12f), 2.0f, 0.6f);
    // shark mouth: a red jaw strip with white teeth, two eyes
    symPoly(A, {{61.2f, 32}, {56, 30.6f}, {52.6f, 31.2f}, {52.6f, 32}}, RED, 0.9f, 0.3f);
    for (int i = 0; i < 3; i++) {
        float x = 54.4f + i * 2.0f;
        A.poly({{x - 0.8f, 30.9f}, {x + 0.8f, 30.9f}, {x, 31.9f}}, WHT, 0.6f, 0.2f);
        A.poly({{x - 0.8f, far(30.9f)}, {x + 0.8f, far(30.9f)}, {x, far(31.9f)}}, WHT, 0.6f, 0.2f);
    }
    A.dot(51.2f, 28.6f, 1.0f, WHT); A.dot(51.2f, 28.6f, 0.45f, DK); A.dot(51.2f, far(28.6f), 1.0f, WHT); A.dot(51.2f, far(28.6f), 0.45f, DK);
    // exposed gun muzzle at the nose
    A.cap(57.5f, 32, 63.2f, 32, 0.95f, STL, 0.8f, 0.5f);
    A.box(62.2f, 32, 0.5f, 1.5f, 0.2f, DK, 1, 0.3f);
    // canopy: teardrop glass with a frame and a glint
    A.ell(36.5f, 32, 6.4f, 3.3f, GLS, 2.0f, 0.75f);
    A.ell(35.2f, 30.6f, 3.4f, 1.0f, rgba(rgb(210, 232, 246), 200), 1, 0);
    A.line(31, 32, 42, 32, 0.5f, rgba(DK, 190));
    // twin engines under the wing: intakes at the leading edge, nozzles with glow rings
    capS(A, 15, 25, 39, 25, 2.9f, shade(H, 0.9f), 2.0f, 0.6f);
    for (int m = 0; m < 2; m++) {
        float y = m ? far(25.0f) : 25.0f;
        A.circle(40.2f, y, 2.1f, DK, 1.2f, 0.4f);
        A.ring(40.2f, y, 2.1f, 0.4f, STL, 0.8f, 0);
        nozzle(A, 12.2f, y, 2.4f, shade(STL, 0.6f), FLAME);
    }
    // canted twin fins, broad, with team-colour tips
    capS(A, 16.5f, 26.4f, 7.6f, 13.8f, 1.9f, shade(H, 0.95f), 1.4f, 0.6f);
    capS(A, 8.4f, 18.0f, 7.4f, 14.6f, 1.5f, A.team, 1.0f, 0.5f);
    // broad horizontal stabilisers
    symPoly(A, {{15, 31.2f}, {5, 26.8f}, {6.4f, 28.6f}, {12.5f, 32}}, shade(H, 1.05f), 1.2f, 0.5f);
    // three big rocket pods a side, each with a hazard band at the pylon
    const float podY[3] = {9.0f, 14.6f, 20.0f}, podX0[3] = {17.5f, 20.3f, 21.5f}, podX1[3] = {28.5f, 31.3f, 32.5f};
    for (int i = 0; i < 3; i++) {
        rocketPod(A, podX0[i], podX1[i], podY[i], 1.8f, POD, RED);
        rocketPod(A, podX0[i], podX1[i], far(podY[i]), 1.8f, POD, RED);
        for (int m = 0; m < 2; m++) A.box(podX0[i] + 3.0f, m ? far(podY[i]) : podY[i], 0.5f, 1.8f, 0.2f, YEL, 1, 0.2f);
    }
    ellS(A, 22.0f, 29.0f, 4.2f, 1.5f, rgba(SAND, 170));   // fuselage camouflage
    ellS(A, 28.0f, 34.5f, 3.2f, 1.2f, rgba(HD, 170));
    for (int i = 0; i < 6; i++) { A.dot(25 - i * 2.4f, 13.2f, 0.32f, rgba(DK, 200)); A.dot(25 - i * 2.4f, far(13.2f), 0.32f, rgba(DK, 200)); }   // rivet rows
    A.line(19.2f, 6.9f, 22.6f, 6.4f, 1.0f, YEL); A.line(19.2f, far(6.9f), 22.6f, far(6.4f), 1.0f, YEL);   // wingtip hazard bands
    // the team roundel on the fuselage
    teamMark(A, 26.5f, 32, 2.2f);
    // rust streaks and scratches
    A.line(10, 29.6f, 26, 27.8f, 0.5f, rgba(RUST, 130));
    A.line(16, 36.2f, 27, 36.9f, 0.4f, rgba(rgb(220, 220, 200), 90));
}

// ---------------------------------------------------------------- Supersonic Bomber (Cyber)

// A compact silver nuclear bomb slung under a wing, seen from above: a capsule about 12 canvas px long with the round
// silver warhead nose of the Underground Nuke's warhead (same gold band), four tail fins, a dark rack strap and a soft
// shadow thrown to the south-east so it reads as hanging UNDER the aircraft, not resting on the wing. A dark seat ring
// keeps the silver pod separated from any wing beneath it even where the shadow is weak.
void nukePod(Art& A, float x0, float x1, float y, float r, Color accent) {
    const Color SIL = rgb(200, 206, 216), SILD = rgb(104, 112, 126);
    A.soft((int)(x0 - 4), (int)(y - r - 5), (int)(x1 + 6), (int)(y + r + 6),
           [=](float px, float py) { return sdCap(px, py, x0 + 1.1f, y + 1.7f, x1 + 0.6f, y + 1.7f, r); }, rgba(INK, 145), 1.7f);
    A.cap(x0 + r * 0.7f, y, x1 - r * 0.8f, y, r + 0.85f, rgba(INK, 210), 1.1f, 0.4f);   // seat ring: the black rim of the pod
    // the two tail blades first, so the body covers their roots
    A.poly({{x0 + 2.0f, y - r * 0.4f}, {x0 - 0.3f, y - r * 1.95f}, {x0 + 1.1f, y - r * 0.1f}}, SILD, 0.7f, 0.3f);
    A.poly({{x0 + 2.0f, y + r * 0.4f}, {x0 - 0.3f, y + r * 1.95f}, {x0 + 1.1f, y + r * 0.1f}}, SILD, 0.7f, 0.3f);
    A.cap(x0 + r * 0.8f, y, x1 - r * 0.9f, y, r, SIL, 1.2f, 0.55f);
    A.ell(x1 - r * 0.8f, y, r * 0.95f, r * 0.88f, shade(SIL, 1.12f), 1.0f, 0.5f);   // rounded warhead nose
    A.box(x0 + (x1 - x0) * 0.55f, y, r * 0.30f, r * 1.02f, 0.2f, rgb(240, 196, 60), 0.5f, 0.2f);   // gold warhead band
    A.box(x0 + (x1 - x0) * 0.33f, y, r * 0.22f, r * 1.02f, 0.2f, accent, 0.5f, 0.2f);             // owner's accent stripe
    A.box(x0 + (x1 - x0) * 0.15f, y, r * 0.26f, r * 1.06f, 0.2f, rgba(INK, 235), 0.4f, 0.2f);     // rack strap
    A.dot(x1 - r * 1.55f, y - r * 0.42f, r * 0.30f, rgba(WHT, 225));                              // glint
    A.dot(x1 - r * 0.35f, y, r * 0.22f, INK);                                                      // fuse
}

// A pylon-hung missile or bomb seen from above: a slim body with a darker seeker or fuse at the nose, a cross-bar for the tail fins and a
// soft shadow to the south-east, so the store reads as hanging under the wing.
void missile(Art& A, float x0, float x1, float y, float r, Color body, Color tip) {
    A.soft((int)x0 - 4, (int)(y - r - 5), (int)x1 + 5, (int)(y + r + 6), [=](float px, float py) { return sdCap(px, py, x0 + 1.0f, y + 1.2f, x1 - r + 0.9f, y + 1.2f, r); }, rgba(INK, 130), 1.6f);
    A.box(x0 + r * 1.5f, y, r * 0.55f, r * 2.2f, 0.15f, shade(body, 0.72f), 0.5f, 0.3f);
    A.cap(x0 + r * 1.2f, y, x1 - r, y, r, body, 0.7f, 0.55f);
    A.circle(x1 - r * 0.6f, y, r * 0.75f, tip, 0.6f, 0.4f);
}

// ---------------------------------------------------------------- Fighter (Cyber): Raptor

// F-22 style stealth fighter: a diamond wing with an aft-swept leading edge and a forward-swept trailing edge, chined nose, bubble canopy,
// trapezoid tailplanes, canted twin fins with team-coloured tips, twin nozzles, and AIM-120-style missiles on the wing pylons.
void raptorFighter(Art& A) {
    const Color H = rgb(54, 68, 98), HL = rgb(92, 110, 142), RAM = rgb(16, 20, 32), GL = rgb(110, 240, 255);
    const Color GLS = rgb(44, 102, 134), WPN = rgb(190, 198, 208);
    // diamond wing
    symPoly(A, {{40.5f, 27.8f}, {19.5f, 11.6f}, {15.2f, 13.6f}, {21.8f, 27.4f}}, H, 2.4f, 0.6f);
    // raised radar-absorbing panels and their seams
    symPoly(A, {{36.5f, 27.4f}, {28.5f, 19.8f}, {25.0f, 22.2f}, {31.5f, 27.6f}}, RAM, 1.0f, 0.3f);
    lineS(A, 31.5f, 27.6f, 25.0f, 22.2f, 0.45f, rgba(INK, 170));
    // cyan leading-edge line with a soft halo
    lineS(A, 40.2f, 27.9f, 19.8f, 11.9f, 2.6f, rgba(GL, 45));
    lineS(A, 40.2f, 27.9f, 19.8f, 11.9f, 0.8f, rgba(GL, 235));
    // tailplanes, trapezoid
    symPoly(A, {{22.5f, 28.2f}, {13.8f, 23.4f}, {11.0f, 25.2f}, {16.2f, 30.2f}}, shade(H, 1.08f), 1.1f, 0.5f);
    // fuselage with a chined nose cone and a faint cyan chine seam
    A.cap(6.5f, 32, 47, 32, 4.1f, shade(H, 1.08f), 2.6f, 0.6f);
    symPoly(A, {{46, 28.0f}, {53, 29.2f}, {58.4f, 30.8f}, {59.8f, 32}, {46, 32}}, shade(H, 1.22f), 1.6f, 0.55f);
    lineS(A, 52.5f, 29.9f, 40.0f, 28.0f, 0.5f, rgba(GL, 170));
    A.line(9, 32, 46, 32, 0.6f, rgba(GL, 120));
    // bubble canopy: dark glass, a frame line and a glint
    A.ell(44.2f, 32, 5.6f, 2.8f, GLS, 1.5f, 0.65f);
    A.ell(43.4f, 31.0f, 2.8f, 0.7f, rgba(WHT, 210), 1, 0);
    A.line(40, 32, 49, 32, 0.35f, rgba(INK, 190));
    // canted twin fins, team colour on the tips
    capS(A, 14.6f, 28.4f, 9.6f, 17.6f, 1.35f, shade(HL, 0.95f), 1.0f, 0.5f);
    capS(A, 10.4f, 21.0f, 9.5f, 17.4f, 1.05f, A.team, 0.8f, 0.5f);
    // twin nozzles, glowing rings
    nozzle(A, 5.4f, 28.8f, 2.3f, shade(RAM, 2.2f), GL);
    nozzle(A, 5.4f, 35.2f, 2.3f, shade(RAM, 2.2f), GL);
    // wing-root intake lips, just forward of the wing
    for (int m = 0; m < 2; m++) {
        float y = m ? far(27.8f) : 27.8f;
        A.box(37.6f, y, 2.4f, 0.9f, 0.4f, INK, 0.9f, 0.3f);
    }
    // AIM-120 style missiles on the wing pylons, one stacked pair a side
    missile(A, 17.8f, 27.0f, 17.6f, 1.0f, WPN, INK); missile(A, 17.8f, 27.0f, far(17.6f), 1.0f, WPN, INK);
    missile(A, 20.2f, 31.6f, 23.4f, 1.0f, WPN, INK); missile(A, 20.2f, 31.6f, far(23.4f), 1.0f, WPN, INK);
    // team roundel on the wing, nav lights at the tips
    teamMark(A, 19.6f, 14.4f, 1.5f); teamMark(A, 19.6f, far(14.4f), 1.5f);
    A.dot(16.4f, 12.8f, 0.5f, rgb(255, 80, 70)); A.dot(16.4f, far(12.8f), 0.5f, rgb(80, 255, 120));
}

// ---------------------------------------------------------------- Fighter (Clanker): Saber

// MiG-29 / Su-27 style fighter: a broad wing with a leading-edge root extension (LERX) running forward along the chine, fat engine
// nacelles hugging the rear fuselage (the wing joins their outer sides, the twin nozzles sit at the back), big canted twin fins,
// two-tone green and sand camouflage, and missiles on the wing pylons. The team mark sits on the wing where the national star would be.
void saberFighter(Art& A) {
    const Color G = rgb(84, 98, 66), GD = rgb(50, 60, 40), SAND = rgb(186, 170, 124), GLS = rgb(74, 110, 126), STL = rgb(128, 130, 126);
    const Color MIS = rgb(214, 216, 210), FLAME = rgb(255, 150, 50);
    // broad wing, olive-green with sand patches
    symPoly(A, {{38.5f, 27.8f}, {19.6f, 11.0f}, {16.4f, 13.2f}, {17.5f, 20.5f}, {22.6f, 27.8f}}, G, 2.5f, 0.6f);
    ellS(A, 21.6f, 16.6f, 2.9f, 1.4f, rgba(SAND, 215));
    ellS(A, 25.2f, 19.0f, 2.4f, 1.0f, rgba(SAND, 200));
    // tailplanes, then the fat engine nacelles hugging the rear fuselage: the wing joins their outer sides and the nozzles sit at the back
    symPoly(A, {{20.5f, 30.0f}, {10.0f, 22.4f}, {7.6f, 23.6f}, {15.8f, 31.8f}}, shade(GD, 1.4f), 1.1f, 0.5f);
    for (int m = 0; m < 2; m++) {
        float y = m ? far(25.8f) : 25.8f;
        A.cap(36.0f, y, 7.6f, y, 2.8f, shade(GD, 1.2f), 1.6f, 0.55f);
        nozzle(A, 5.4f, y, 2.2f, shade(STL, 0.55f), FLAME);
    }
    // leading-edge root extension along the chine, sand on the green
    symPoly(A, {{50.5f, 28.8f}, {36.0f, 21.8f}, {38.5f, 27.8f}, {46.0f, 29.6f}}, shade(G, 1.12f), 1.6f, 0.5f);
    lineS(A, 38.5f, 27.8f, 19.6f, 11.0f, 0.6f, rgba(GD, 190));
    lineS(A, 17.5f, 20.5f, 22.6f, 27.8f, 0.6f, rgba(GD, 160));
    // fuselage with a pointed nose and a bubble canopy in front of the LERX
    A.cap(8.5f, 32, 47, 32, 3.7f, shade(G, 1.06f), 2.4f, 0.6f);
    symPoly(A, {{46, 28.6f}, {53, 29.6f}, {58.6f, 30.8f}, {60.5f, 32}, {46, 32}}, shade(G, 1.14f), 1.6f, 0.55f);
    A.ell(44.4f, 32, 4.6f, 2.3f, GLS, 1.4f, 0.65f);
    A.ell(43.3f, 31.1f, 2.4f, 0.6f, rgba(rgb(210, 232, 246), 200), 1, 0);
    A.line(40, 32, 48, 32, 0.4f, rgba(GD, 200));
    // two big canted fins with team-coloured tips, horizontal stabilisers
    capS(A, 14.5f, 28.6f, 8.2f, 16.4f, 1.4f, shade(G, 0.92f), 1.0f, 0.5f);
    capS(A, 10.2f, 20.6f, 8.0f, 16.4f, 1.0f, A.team, 0.8f, 0.5f);
    // missiles on the pylons under the wing, outboard of the nacelles (R-27 / R-73 style)
    missile(A, 19.5f, 29.5f, 20.8f, 1.0f, MIS, rgb(60, 62, 66)); missile(A, 19.5f, 29.5f, far(20.8f), 1.0f, MIS, rgb(60, 62, 66));
    // team roundel on the wing, nav lights at the tips
    teamMark(A, 20.4f, 15.2f, 1.6f); teamMark(A, 20.4f, far(15.2f), 1.6f);
    A.dot(18.3f, 12.6f, 0.5f, rgb(255, 80, 70)); A.dot(18.3f, far(12.6f), 0.5f, rgb(80, 255, 120));
}

// ---------------------------------------------------------------- Nuclear bomber (Cyber): Nightfall

// B-2 style flying wing: one smooth planform with a swept leading edge, a sawtooth trailing edge on each outboard panel, a low
// canopy bump on the centreline and faint cyan panel seams. The dark exhaust slots sit on the centre trailing edge.
void nightfallBomber(Art& A) {
    const Color H = rgb(44, 52, 68), HL = rgb(74, 86, 110), RAM = rgb(18, 22, 32), GL = rgb(110, 240, 255), GLS = rgb(24, 60, 84);
    // outboard panel and centre section, each convex; the outboard trailing edge is the sawtooth
    symPoly(A, {{50, 32}, {38, 21}, {12, 4.2f}, {10, 6.5f}, {22, 20.5f}}, H, 2.6f, 0.6f);
    sawEdge(A, {10, 6.5f}, {22, 20.5f}, 4, 2.0f, H);
    sawEdge(A, {22, far(20.5f)}, {10, far(6.5f)}, 4, 2.0f, H);   // reversed: the normal must point aft on the far side too
    symPoly(A, {{22, 20.5f}, {50, 32}, {20, 32}}, shade(H, 0.95f), 2.2f, 0.5f);
    sawEdge(A, {22, 20.5f}, {20, 32}, 2, 1.4f, shade(H, 0.95f));
    sawEdge(A, {20, 32}, {22, far(20.5f)}, 2, 1.4f, shade(H, 0.95f));
    // faint cyan panel seams, the centre spine
    lineS(A, 44, 29.6f, 16, 8.2f, 0.4f, rgba(GL, 110));
    lineS(A, 36, 30.8f, 22, 22, 0.35f, rgba(GL, 90));
    A.line(18, 32, 47, 32, 0.45f, rgba(GL, 120));
    // raised dorsal bump and canopy (dark glass)
    A.ell(41.0f, 32, 4.4f, 2.2f, shade(HL, 0.95f), 1.6f, 0.5f);
    A.ell(41.0f, 32, 3.2f, 1.3f, GLS, 1.0f, 0.5f);
    A.ell(40.0f, 31.4f, 1.6f, 0.4f, rgba(WHT, 200), 1, 0);
    // centre exhaust slots and the sawtooth bay
    for (int m = 0; m < 2; m++) {
        float y = m ? far(27.2f) : 27.2f;
        A.box(23.0f, y, 1.6f, 0.9f, 0.3f, shade(RAM, 2.4f), 0.6f, 0.3f);
        A.dot(21.4f, y, 0.4f, rgba(GL, 210));
    }
    // team roundel on each panel, tip lights
    teamMark(A, 29.0f, 20.0f, 2.1f); teamMark(A, 29.0f, far(20.0f), 2.1f);
    A.dot(11.8f, 5.6f, 0.9f, rgb(255, 80, 70)); A.dot(11.8f, far(5.6f), 0.9f, rgb(80, 255, 120));
}

// ---------------------------------------------------------------- Nuclear bomber (Clanker): Thunderhead

// Tu-160 / B-1 style swing-wing bomber, set at mid sweep: a long white fuselage with a glazed nose and an olive camouflage panel,
// broad wings with olive patches, two paired nacelles (one a side, under the inner wing) with their nozzles at the trailing edge, and a
// swept tailplane with a tall centreline fin.
void thunderheadBomber(Art& A) {
    const Color W = rgb(204, 208, 206), GRY = rgb(158, 164, 164), OLV = rgb(104, 112, 76), OLVD = rgb(66, 72, 48), GLS = rgb(62, 94, 112);
    const Color FLAME = rgb(255, 150, 50), STL = rgb(132, 134, 132);
    // wing and olive camouflage patches
    symPoly(A, {{38, 26.6f}, {18.5f, 6.4f}, {14.8f, 8.4f}, {27, 27.6f}}, W, 2.4f, 0.6f);
    ellS(A, 27.0f, 19.4f, 3.0f, 1.2f, rgba(OLV, 230));
    ellS(A, 19.5f, 12.0f, 2.2f, 1.2f, rgba(OLV, 220));
    lineS(A, 38, 26.6f, 18.5f, 6.4f, 0.6f, rgba(GRY, 200));
    lineS(A, 27, 27.6f, 14.8f, 8.4f, 0.5f, rgba(GRY, 190));
    // two paired nacelles, one a side, hugging the fuselage under the inner wing: each houses two engines behind one intake just
    // forward of the wing root, and the two nozzles sit at the trailing edge, so they read as part of the airframe
    for (int m = 0; m < 2; m++) {
        float y = m ? far(25.6f) : 25.6f;
        A.cap(39.5f, y, 26.5f, y, 2.9f, shade(GRY, 0.95f), 1.8f, 0.55f);
        A.ell(40.2f, y, 1.0f, 2.2f, INK, 1.0f, 0.4f);
        nozzle(A, 25.0f, y - 1.4f, 1.25f, shade(STL, 0.5f), FLAME);
        nozzle(A, 25.0f, y + 1.4f, 1.25f, shade(STL, 0.5f), FLAME);
    }
    // fuselage: long and tubular, glazed nose, olive panel along the spine
    A.cap(7.0f, 32, 50, 32, 3.7f, shade(W, 1.04f), 2.6f, 0.6f);
    symPoly(A, {{48, 28.6f}, {56, 29.4f}, {60, 30.8f}, {61.4f, 32}, {48, 32}}, shade(W, 1.08f), 1.8f, 0.55f);
    ellS(A, 36.0f, 29.4f, 5.0f, 1.1f, rgba(OLV, 230));
    A.ell(54.6f, 32, 3.8f, 2.0f, GLS, 1.4f, 0.6f);
    A.ell(53.6f, 31.1f, 2.0f, 0.5f, rgba(rgb(210, 232, 246), 200), 1, 0);
    A.line(48, 32, 60, 32, 0.4f, rgba(GRY, 200));
    // tailplanes swept wide, the tall fin edge-on with a team-coloured tip
    symPoly(A, {{16.0f, 28.4f}, {6.5f, 22.4f}, {4.5f, 24.2f}, {10.0f, 30.4f}}, shade(W, 0.98f), 1.2f, 0.5f);
    A.cap(14.0f, 32, 5.5f, 32, 1.3f, shade(W, 0.86f), 1.0f, 0.45f);
    A.box(6.8f, 32, 1.4f, 0.9f, 0.2f, A.team, 0.7f, 0.35f);
    A.line(12.8f, 31.2f, 5.6f, 31.2f, 0.5f, rgba(rgb(255, 255, 255), 150));
    // team roundel on each wing, rivets and hatches
    teamMark(A, 24.2f, 16.5f, 1.9f); teamMark(A, 24.2f, far(16.5f), 1.9f);
    A.box(42.0f, 32, 1.4f, 0.8f, 0.2f, rgba(GRY, 220), 0.5f, 0.2f);
    for (int i = 0; i < 5; i++) { A.dot(44.5f - i * 4.6f, 34.2f, 0.28f, rgba(OLVD, 200)); A.dot(44.5f - i * 4.6f, far(34.2f), 0.28f, rgba(OLVD, 200)); }
}

// ---------------------------------------------------------------- Supersonic Bomber (Cyber): Aurora

// The Cyber bomber: a slender black-blue double-delta in the SR-71 manner - a long chine running forward from a very swept
// wing to a sharp nose, twin canted fins with team-coloured tips, and two slotted exhausts with cyan glow. Its single compact
// silver bomb hangs under the south inboard wing.
void auroraBomber(Art& A) {
    const Color H = rgb(34, 42, 66), RAM = rgb(14, 17, 28), GL = rgb(112, 240, 255), GLS = rgb(24, 56, 84);
    // very swept delta wing, then the chine (leading-edge extension) running out to the nose
    symPoly(A, {{42, 28.5f}, {15.5f, 16.6f}, {10.5f, 18.8f}, {23, 28.6f}}, H, 2.2f, 0.6f);
    symPoly(A, {{60.2f, 32}, {51, 28.4f}, {38.5f, 24.4f}, {30, 32}}, shade(H, 1.08f), 1.4f, 0.55f);
    lineS(A, 42, 28.5f, 15.5f, 16.6f, 2.4f, rgba(GL, 40));
    lineS(A, 42, 28.5f, 15.5f, 16.6f, 0.8f, rgba(GL, 230));
    lineS(A, 60.2f, 32, 51, 28.4f, 0.8f, rgba(GL, 235));
    lineS(A, 51, 28.4f, 38.5f, 24.4f, 0.8f, rgba(GL, 210));
    // fuselage: slim, pointed nose with a dark glazed cockpit
    A.cap(7.0f, 32, 50, 32, 3.3f, shade(H, 1.08f), 2.2f, 0.6f);
    symPoly(A, {{50, 29.6f}, {57.5f, 30.5f}, {60.8f, 31.6f}, {61.6f, 32}, {50, 32}}, shade(H, 1.25f), 1.2f, 0.5f);
    A.ell(46.6f, 32, 4.0f, 1.7f, GLS, 1.3f, 0.6f);
    A.ell(45.6f, 31.2f, 2.0f, 0.5f, rgba(WHT, 200), 1, 0);
    A.line(9, 32, 47, 32, 0.5f, rgba(GL, 130));
    // twin canted fins with team-coloured tips
    capS(A, 14.0f, 27.6f, 8.8f, 17.4f, 1.2f, shade(H, 1.15f), 0.9f, 0.45f);
    capS(A, 10.8f, 21.4f, 8.6f, 17.4f, 0.95f, A.team, 0.7f, 0.4f);
    // two slotted exhausts with cyan glow
    for (int m = 0; m < 2; m++) {
        float y = m ? far(28.4f) : 28.4f;
        A.box(5.6f, y, 2.2f, 1.3f, 0.5f, shade(RAM, 2.4f), 0.9f, 0.45f);
        A.glow(3.6f, y, 3.6f, rgba(GL, 140));
    }
    // team roundel on the wing, wingtip lights
    teamMark(A, 21.6f, 22.6f, 1.6f); teamMark(A, 21.6f, far(22.6f), 1.6f);
    A.dot(16.2f, 18.4f, 0.5f, rgb(255, 80, 70)); A.dot(16.2f, far(18.4f), 0.5f, rgb(80, 255, 120));
    // the one compact silver bomb under the south inboard wing
    nukePod(A, 21.5f, 33.0f, 37.6f, 1.9f, GL);
}

// ---------------------------------------------------------------- Supersonic Bomber (Clanker): Backfire

// The Clanker bomber: a Tu-22M 'Backfire'-like swept-wing bomber in grey over olive - a long tapered nose, intakes on the flanks beside
// the cockpit, a tall centreline fin with a swept tailplane, twin nozzles in the tail and one compact silver bomb under the inboard wing.
void backfireBomber(Art& A) {
    const Color H = rgb(120, 124, 106), HD = rgb(88, 92, 72), SAND = rgb(176, 168, 132), DK = rgb(30, 32, 28), GLS = rgb(66, 100, 118);
    const Color STL = rgb(150, 152, 150), FLAME = rgb(255, 150, 50), YEL = rgb(236, 190, 62);
    // swept wing with a sand leading edge and olive patches
    symPoly(A, {{40, 27.8f}, {17.2f, 8.6f}, {13.4f, 10.2f}, {24, 27.6f}}, H, 2.4f, 0.6f);
    ellS(A, 23.5f, 17.8f, 3.4f, 1.6f, rgba(HD, 215));
    ellS(A, 30.0f, 24.5f, 3.0f, 1.4f, rgba(HD, 200));
    lineS(A, 39.5f, 27.6f, 17.8f, 9.4f, 1.3f, shade(SAND, 0.9f));
    lineS(A, 27.0f, 25.2f, 15.0f, 11.0f, 0.45f, rgba(DK, 140));
    // intakes on the flanks just forward of the wing root, beside the cockpit
    for (int m = 0; m < 2; m++) {
        float y = m ? far(28.6f) : 28.6f;
        A.box(46.2f, y, 2.6f, 1.0f, 0.4f, INK, 0.8f, 0.3f);
    }
    // long tapered fuselage with a glazed cockpit at the front
    A.cap(6.2f, 32, 47, 32, 3.6f, shade(H, 1.06f), 2.6f, 0.6f);
    symPoly(A, {{46, 28.6f}, {54, 29.5f}, {58.6f, 30.6f}, {62, 32}, {46, 32}}, shade(H, 1.12f), 1.8f, 0.55f);
    A.ell(54.2f, 32, 3.6f, 1.9f, GLS, 1.4f, 0.6f);
    A.ell(53.2f, 31.1f, 1.8f, 0.5f, rgba(rgb(206, 228, 242), 200), 1, 0);
    ellS(A, 36.0f, 29.2f, 5.0f, 1.0f, rgba(HD, 210));
    // swept tailplane and the centreline fin
    symPoly(A, {{17.0f, 28.6f}, {6.6f, 22.2f}, {4.4f, 24.4f}, {11.0f, 30.6f}}, shade(H, 1.04f), 1.1f, 0.5f);
    A.cap(12.0f, 32, 4.6f, 32, 1.4f, shade(H, 0.85f), 1.0f, 0.45f);
    A.box(6.6f, 32, 1.4f, 0.9f, 0.2f, A.team, 0.7f, 0.35f);
    // twin nozzles in the tail
    nozzle(A, 5.2f, 28.6f, 2.0f, shade(STL, 0.5f), FLAME);
    nozzle(A, 5.2f, 35.4f, 2.0f, shade(STL, 0.5f), FLAME);
    // team roundel on the wing
    teamMark(A, 25.2f, 21.2f, 1.7f);
    // the one compact silver bomb under the inboard wing
    nukePod(A, 26.6f, 37.0f, 25.6f, 1.9f, YEL);
}

}  // namespace

void artStrikeJetBody(Canvas& c, Color team, bool cyber) {
    Art A(c, team);
    if (cyber) valkyrie(A); else warhawk(A);
    sharpen(c, 0.6f);
    c.outline(rgb(8, 10, 14, 200));
}

// The Supersonic Bomber of both armies: a small, very fast fixed-wing nuclear bomber carrying one compact silver bomb
// under an inboard wing. It paints, sharpens and outlines itself like the strike jets (the generic unit layers are off it).
void artSupersonicBomberBody(Canvas& c, Color team, bool cyber) {
    Art A(c, team);
    if (cyber) auroraBomber(A); else backfireBomber(A);
    sharpen(c, 0.6f);
    c.outline(rgb(8, 10, 14, 200));
}

// The plain fighter of both armies (Raptor / Saber): about 54 px long with a wingspan of about 40 px, missiles on the wing pylons,
// twin engines and twin fins. Paints, sharpens and outlines itself like the other aircraft.
void artFighterBody(Canvas& c, Color team, bool cyber) {
    Art A(c, team);
    if (cyber) raptorFighter(A); else saberFighter(A);
    sharpen(c, 0.6f);
    c.outline(rgb(8, 10, 14, 200));
}

// The big strategic nuclear bomber of both armies (Nightfall / Thunderhead): fills most of the canvas, with a wingspan of about 50-60 px.
void artNukeBomberBody(Canvas& c, Color team, bool cyber) {
    Art A(c, team);
    if (cyber) nightfallBomber(A); else thunderheadBomber(A);
    sharpen(c, 0.6f);
    c.outline(rgb(8, 10, 14, 200));
}
