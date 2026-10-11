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

}  // namespace

void artStrikeJetBody(Canvas& c, Color team, bool cyber) {
    Art A(c, team);
    if (cyber) valkyrie(A); else warhawk(A);
    sharpen(c, 0.6f);
    c.outline(rgb(8, 10, 14, 200));
}
