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

// The Cyber bomber: a slender hypersonic waverider - long pale ceramic nose, thin clipped delta, ceramic leading edges
// and cyan emitter lines in the Valkyrie's language but on a much smaller airframe. Twin canted stealth fins, two
// slotted vectored exhausts and a small rear-defence turret. Its single compact silver bomb hangs under the south wing.
void specterBomber(Art& A) {
    const Color H = rgb(50, 62, 88), HL = rgb(94, 112, 144), CER = rgb(176, 190, 206), RAM = rgb(14, 17, 26);
    const Color GL = rgb(112, 240, 255), GLS = rgb(28, 80, 106), STL = rgb(132, 140, 152);
    // thin blended delta: swept leading edge, clipped tip, swept trailing edge
    symPoly(A, {{51, 29.3f}, {43, 23.3f}, {34, 16.1f}, {25, 11.2f}, {20.2f, 12.0f}, {11.0f, 31.3f}}, H, 2.2f, 0.6f);
    // dark radar-absorbing root panels and a panel seam
    symPoly(A, {{41, 27.2f}, {30, 18.2f}, {34.5f, 24.4f}}, RAM, 0.9f, 0.3f);
    symPoly(A, {{27.5f, 14.4f}, {21.5f, 12.4f}, {23.8f, 16.4f}}, RAM, 0.9f, 0.3f);
    lineS(A, 39.5f, 21.4f, 18.0f, 11.0f, 0.45f, rgba(INK, 130));
    // pale ceramic leading edge, with the cyan emitter line hugging it just inboard
    lineS(A, 50.4f, 29.0f, 43.0f, 23.1f, 1.25f, shade(CER, 0.94f));
    lineS(A, 43.0f, 23.1f, 34.0f, 15.9f, 1.25f, shade(CER, 0.94f));
    lineS(A, 34.0f, 15.9f, 25.1f, 11.0f, 1.25f, shade(CER, 0.94f));
    lineS(A, 50.0f, 28.8f, 42.6f, 22.9f, 2.6f, rgba(GL, 40));
    lineS(A, 42.6f, 22.9f, 33.6f, 15.7f, 2.6f, rgba(GL, 40));
    lineS(A, 48.9f, 30.0f, 41.5f, 24.1f, 0.75f, rgba(GL, 235));
    lineS(A, 41.5f, 24.1f, 32.6f, 16.9f, 0.75f, rgba(GL, 225));
    lineS(A, 32.6f, 16.9f, 24.4f, 12.2f, 0.7f, rgba(GL, 215));
    // wingtip pod with an emitter, the team panel and the roundel
    capS(A, 27.2f, 11.6f, 21.0f, 12.3f, 1.05f, shade(H, 0.84f), 0.9f, 0.4f);
    A.dot(20.8f, 12.3f, 0.55f, GL); A.dot(20.8f, far(12.3f), 0.55f, GL);
    A.glow(20.6f, 12.3f, 2.8f, rgba(GL, 120)); A.glow(20.6f, far(12.3f), 2.8f, rgba(GL, 120));
    symPoly(A, {{26.4f, 12.8f}, {21.4f, 13.6f}, {24.0f, 16.6f}}, A.team, 0.8f, 0.35f);
    teamMark(A, 31.4f, 19.2f, 1.75f); teamMark(A, 31.4f, far(19.2f), 1.75f);
    // pale ceramic elevon panel along the trailing edge, its hinge seam and a faint cyan edge light
    symPoly(A, {{18.8f, 20.6f}, {21.2f, 15.8f}, {23.4f, 17.6f}, {20.4f, 21.8f}}, shade(CER, 0.8f), 0.8f, 0.35f);
    lineS(A, 21.0f, 16.4f, 12.0f, 29.0f, 0.5f, rgba(INK, 160));
    lineS(A, 19.6f, 12.6f, 11.4f, 30.4f, 0.4f, rgba(GL, 110));
    // slender lifting-body fuselage with a bright dorsal spine
    A.cap(8.4f, 32, 45, 32, 3.5f, shade(H, 1.06f), 2.6f, 0.6f);
    A.cap(13.5f, 32, 46, 32, 1.85f, shade(HL, 1.03f), 1.8f, 0.5f);
    A.line(14, 32, 47, 32, 0.5f, rgba(GL, 170));
    A.box(25.5f, 32, 2.4f, 0.7f, 0.2f, RAM, 0.6f, 0.2f);
    A.box(25.5f, 32, 0.65f, 1.5f, 0.2f, rgba(GL, 170), 0.5f, 0.2f);
    // long pale ceramic nose with a dark centre seam, anti-glare panel and the needle pitot
    symPoly(A, {{45.0f, 28.8f}, {52, 29.6f}, {57, 30.8f}, {62.7f, 32}, {45, 32}}, CER, 1.5f, 0.5f);
    A.line(46, 32, 62.2f, 32, 0.4f, rgba(INK, 185));
    A.box(51.0f, 30.3f, 1.5f, 0.35f, 0.1f, rgba(RAM, 220), 0.4f, 0.2f);   // anti-glare panel on the nose
    A.dot(58.6f, 31.3f, 0.4f, rgba(WHT, 210));
    A.dot(62.4f, 32, 0.3f, shade(CER, 0.55f));                            // pitot tip
    // flush canopy behind the nose
    A.ell(39.6f, 32, 5.4f, 2.35f, GLS, 1.5f, 0.65f);
    A.ell(38.5f, 31.0f, 2.9f, 0.62f, rgba(WHT, 200), 1, 0);
    A.line(34.4f, 32, 44.8f, 32, 0.35f, rgba(INK, 190));
    A.line(36.8f, 30.2f, 42.4f, 30.2f, 0.4f, rgba(HL, 220));
    // wing-root intakes with a cyan lip
    for (int m = 0; m < 2; m++) {
        float y = m ? far(27.4f) : 27.4f;
        A.box(33.0f, y, 3.0f, 0.95f, 0.25f, INK, 0.8f, 0.2f);
        A.line(29.8f, y + (m ? 0.55f : -0.55f), 36.2f, y + (m ? 0.55f : -0.55f), 0.4f, rgba(GL, 160));
    }
    // swept twin fins with team-coloured tips
    capS(A, 16.0f, 27.4f, 10.2f, 19.8f, 1.2f, shade(CER, 0.84f), 0.9f, 0.4f);
    capS(A, 10.9f, 20.6f, 9.7f, 17.2f, 0.95f, A.team, 0.7f, 0.4f);
    A.dot(9.6f, 17.0f, 0.5f, rgba(WHT, 215)); A.dot(9.6f, far(17.0f), 0.5f, rgba(WHT, 215));
    // two slotted 2D exhausts with cyan glow
    for (int m = 0; m < 2; m++) {
        float y = m ? 35.6f : 28.4f;
        A.box(9.0f, y, 2.4f, 1.3f, 0.55f, shade(RAM, 2.6f), 0.9f, 0.45f);
        A.box(7.5f, y, 0.9f, 0.55f, 0.2f, rgba(GL, 230), 0.6f, 0.2f);
        A.glow(6.4f, y, 4.4f, rgba(GL, 110));
    }
    // rear-defence turret: a small drum with two cannon muzzles pointing aft
    A.cap(14.0f, 32, 9.6f, 32, 1.2f, shade(H, 1.22f), 0.9f, 0.4f);
    A.box(11.0f, 32, 1.0f, 0.7f, 0.2f, shade(RAM, 2.1f), 0.5f, 0.2f);
    A.line(9.2f, 31.2f, 5.2f, 31.2f, 0.5f, STL); A.line(9.2f, 32.8f, 5.2f, 32.8f, 0.5f, STL);
    A.dot(5.1f, 31.2f, 0.35f, INK); A.dot(5.1f, 32.8f, 0.35f, INK);
    // hatches, panel lines and rivet rows
    A.box(21.0f, 32, 1.6f, 0.7f, 0.2f, rgba(RAM, 220), 0.5f, 0.2f);
    A.line(17.0f, 29.7f, 32.0f, 29.7f, 0.45f, rgba(INK, 140));
    A.line(12.0f, 35.1f, 31.0f, 35.1f, 0.4f, rgba(INK, 120));
    for (int i = 0; i < 5; i++) { A.dot(20.5f + i * 4.3f, 34.3f, 0.3f, rgba(HL, 215)); A.dot(20.5f + i * 4.3f, far(34.3f), 0.3f, rgba(HL, 215)); }
    // the one compact silver bomb under the south inboard wing
    nukePod(A, 18.0f, 30.4f, 37.8f, 2.0f, GL);
}

// ---------------------------------------------------------------- Supersonic Bomber (Clanker)

// The Clanker bomber: a Hustler-like four-engine delta - long glazed bombardier nose, two big under-wing nacelles with
// twin exhausts each, a broad tailplane with a single centreline fin, a rear gun turret, and plenty of rivets, rust and
// hazard yellow. Its one compact silver bomb hangs from the north inboard pylon.
void ironwingBomber(Art& A) {
    const Color H = rgb(112, 118, 88), HD = rgb(82, 88, 64), SAND = rgb(184, 166, 118), DK = rgb(32, 34, 28);
    const Color RUST = rgb(166, 92, 48), YEL = rgb(236, 190, 62), STL = rgb(152, 154, 152);
    const Color GLS = rgb(70, 106, 122), FLAME = rgb(255, 150, 50);
    // clean delta wing in olive over sand with camouflage patches
    symPoly(A, {{47, 28.9f}, {41, 22.6f}, {30, 13.2f}, {22.2f, 8.6f}, {17.0f, 9.8f}, {10.0f, 31.3f}}, H, 2.5f, 0.6f);
    ellS(A, 35.0f, 17.0f, 6.2f, 2.2f, rgba(SAND, 190));
    ellS(A, 25.5f, 22.4f, 4.4f, 2.0f, rgba(HD, 185));
    ellS(A, 32.0f, 26.4f, 3.4f, 1.5f, rgba(SAND, 165));
    ellS(A, 23.0f, 13.0f, 2.4f, 1.1f, rgba(HD, 150));
    // pale leading edge, seams, flap hinge and a wing fence
    lineS(A, 45.6f, 28.2f, 41.2f, 22.8f, 1.5f, shade(SAND, 0.92f));
    lineS(A, 41.2f, 22.8f, 30.2f, 13.4f, 1.5f, shade(SAND, 0.92f));
    lineS(A, 30.2f, 13.4f, 22.4f, 8.8f, 1.5f, shade(SAND, 0.92f));
    lineS(A, 42.2f, 26.6f, 17.8f, 9.8f, 0.55f, rgba(DK, 150));
    lineS(A, 36.4f, 21.8f, 13.0f, 29.6f, 0.75f, rgba(DK, 165));
    lineS(A, 29.6f, 16.6f, 15.8f, 25.4f, 0.5f, rgba(DK, 130));
    lineS(A, 26.0f, 9.4f, 23.0f, 11.6f, 0.7f, rgba(RUST, 180));
    // wingtip hazard band, wingtip lights, the team panel and the roundel
    lineS(A, 23.0f, 8.7f, 19.6f, 10.0f, 1.35f, YEL);
    A.dot(20.6f, 8.2f, 0.8f, rgb(255, 80, 70)); A.dot(20.6f, far(8.2f), 0.8f, rgb(80, 255, 120));
    symPoly(A, {{28.8f, 11.8f}, {23.6f, 9.8f}, {21.8f, 13.0f}}, A.team, 0.8f, 0.35f);
    teamMark(A, 35.8f, 23.4f, 1.85f); teamMark(A, 35.8f, far(23.4f), 1.85f);
    // two big under-wing nacelles: steel intake forward, twin exhausts aft, a hazard band
    for (int m = 0; m < 2; m++) {
        float y = m ? far(18.0f) : 18.0f;
        A.soft(12, (int)(y - 5.5f), 32, (int)(y + 5.5f), [=](float px, float py) { return sdCap(px, py, 14.0f, y + 1.2f, 29.0f, y + 1.2f, 3.0f); }, rgba(INK, 95), 1.6f);
        A.cap(28.2f, y, 13.6f, y, 2.85f, shade(SAND, 0.86f), 2.0f, 0.55f);
        A.circle(28.6f, y, 2.25f, DK, 1.2f, 0.4f);
        A.ring(28.6f, y, 2.25f, 0.45f, STL, 0.8f, 0);
        A.dot(29.3f, y, 0.95f, shade(DK, 2.0f));
        A.box(18.4f, y, 0.85f, 2.75f, 0.2f, YEL, 0.5f, 0.2f);
        for (int k = 0; k < 2; k++) nozzle(A, 11.9f, y + (k ? 1.4f : -1.4f), 1.35f, shade(STL, 0.5f), FLAME);
    }
    // thick tubular fuselage with a raised spine and the long tapered nose
    A.cap(6.4f, 32, 48.5f, 32, 4.1f, shade(H, 1.05f), 2.8f, 0.6f);
    A.cap(14.0f, 32, 52.0f, 32, 2.5f, shade(H, 1.12f), 2.0f, 0.5f);
    symPoly(A, {{47.0f, 28.2f}, {55.0f, 29.2f}, {61.0f, 30.6f}, {62.8f, 32}, {47.0f, 32}}, shade(H, 1.1f), 1.8f, 0.55f);
    // glazed bombardier greenhouse in the nose
    A.ell(55.4f, 32, 4.8f, 2.6f, GLS, 1.5f, 0.65f);
    A.ell(54.6f, 31.3f, 2.6f, 0.7f, rgba(rgb(206, 228, 242), 190), 1, 0);
    A.line(51.0f, 32, 60.2f, 32, 0.4f, rgba(DK, 190));
    // yellow hazard band and the pale radome seam behind the glass
    A.box(48.6f, 32, 0.75f, 2.7f, 0.2f, YEL, 0.5f, 0.2f);
    for (int i = 0; i < 3; i++) A.box(48.6f, 30.8f + i * 1.15f, 0.78f, 0.3f, 0.1f, DK, 0.3f, 0.1f);
    A.line(47.4f, 29.2f, 61.4f, 30.9f, 0.4f, rgba(SAND, 200));
    // cockpit canopy behind the bombardier nose
    A.ell(41.4f, 32, 4.8f, 2.3f, GLS, 1.5f, 0.65f);
    A.ell(40.4f, 31.1f, 2.6f, 0.6f, rgba(WHT, 200), 1, 0);
    A.line(37.0f, 32, 45.6f, 32, 0.4f, rgba(DK, 200));
    // broad tailplane and the single centreline fin, seen from above as a blade with a team-coloured tip
    symPoly(A, {{16.0f, 30.6f}, {7.0f, 25.6f}, {3.6f, 27.4f}, {9.6f, 31.6f}}, shade(H, 1.06f), 1.2f, 0.5f);
    A.line(14.5f, 30.0f, 4.4f, 26.6f, 0.45f, rgba(DK, 150));
    A.cap(13.4f, 32, 4.6f, 32, 1.4f, shade(H, 0.86f), 1.0f, 0.45f);
    A.line(12.8f, 31.2f, 5.4f, 31.2f, 0.5f, shade(H, 1.55f));   // the lit top edge of the vertical fin
    A.box(5.8f, 32, 1.6f, 1.0f, 0.25f, A.team, 0.7f, 0.35f);
    A.line(11.6f, 32.7f, 6.2f, 32.7f, 0.4f, rgba(DK, 160));
    // rear gun turret: a dome with two cannon muzzles pointing aft
    A.circle(4.2f, 32, 1.9f, shade(H, 0.82f), 1.2f, 0.45f);
    A.line(3.2f, 31.2f, 1.4f, 31.2f, 0.5f, STL); A.line(3.2f, 32.8f, 1.4f, 32.8f, 0.5f, STL);
    A.dot(1.3f, 31.2f, 0.35f, DK); A.dot(1.3f, 32.8f, 0.35f, DK);
    // hatch, rivet rows, rust streaks and scratches
    A.box(31.5f, 32, 1.7f, 0.75f, 0.2f, rgba(DK, 190), 0.5f, 0.2f);
    A.box(25.0f, 29.0f, 1.4f, 0.5f, 0.15f, rgba(STL, 170), 0.4f, 0.2f);
    for (int i = 0; i < 7; i++) { A.dot(16.0f + i * 4.2f, 28.8f, 0.3f, rgba(DK, 200)); A.dot(16.0f + i * 4.2f, far(28.8f), 0.3f, rgba(DK, 200)); }
    for (int i = 0; i < 5; i++) { A.dot(35.0f + i * 2.9f, 34.6f, 0.28f, rgba(SAND, 220)); A.dot(35.0f + i * 2.9f, far(34.6f), 0.28f, rgba(SAND, 220)); }
    A.line(11.0f, 29.4f, 24.0f, 28.0f, 0.5f, rgba(RUST, 135));
    A.line(17.5f, 35.8f, 32.0f, 36.5f, 0.4f, rgba(rgb(220, 220, 200), 90));
    A.grain(5, 20, 60, 46, 0.10f, 41);
    // the one compact silver bomb under the north inboard wing
    nukePod(A, 16.4f, 29.0f, 26.8f, 2.05f, YEL);
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
    if (cyber) specterBomber(A); else ironwingBomber(A);
    sharpen(c, 0.6f);
    c.outline(rgb(8, 10, 14, 200));
}
