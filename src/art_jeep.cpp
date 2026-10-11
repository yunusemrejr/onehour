#include "art2.h"
#include "paint.h"

using namespace paint;

// Jeep and protector drone art, painted like the rest of the unit sprites (see art.cpp): 64x64 canvas, facing +x, centre (32,32), lit from
// the north-west. The jeep is an open-top 4x4 seen from above with seven seats. Cyber: graphite and cobalt composite with a pale ceramic bonnet,
// cyan emitter strips and hex-tread wheels with glowing rims. Clanker: olive drab, rust and sand, hazard-yellow trim, riveted steel, a brush
// guard, jerry cans and an exhaust pipe.
namespace {

// The player's mark: the same jewel as teamDot in art.cpp (dark seat ring, team colour lit from the north-west, pinpoint highlight)
void teamJewel(Art& A, float cx, float cy, float r) {
    A.circle(cx, cy, r + 1.3f, rgba(rgb(20, 24, 32), 235), 1.0f, 0.2f);
    A.circle(cx, cy, r, A.team, r, 0.75f);
    A.dot(cx - r * 0.32f, cy - r * 0.36f, r * 0.24f, rgba(mix(A.team, rgb(255, 255, 255), 0.75f), 235));
}

// One wheel seen from above: a tyre band lying across the axle, with the tread running along the rolling direction. ph (0..1) slides the tread
// by one pitch, so the two walk frames differ by half a pitch. Cyber: hex nubs in two staggered rows and a glowing outer rim. Clanker: chunky
// lugs, mud on the outer edge.
void wheel(Art& A, float cx, float cy, float ph, bool cyber, bool top) {
    const float hl = 4.7f, hh = 3.4f, x0 = cx - hl + 0.8f, x1 = cx + hl - 0.8f;
    const float outerY = top ? cy - hh + 0.7f : cy + hh - 0.7f;
    Color tyre = cyber ? rgb(30, 34, 42) : rgb(40, 38, 32);
    A.box(cx, cy, hl, hh, 1.4f, tyre, 1.2f, 0.5f);
    float pitch = cyber ? 2.6f : 3.4f;
    for (float x = x0 + ph * pitch; x < x1; x += pitch) {
        if (cyber) {   // hex nubs, alternate rows staggered by half a pitch
            A.circle(x, cy - 1.1f, 0.95f, rgb(96, 112, 136), 0.6f, 0.5f);
            if (x + pitch * 0.5f < x1) A.circle(x + pitch * 0.5f, cy + 1.1f, 0.95f, rgb(96, 112, 136), 0.6f, 0.5f);
        } else {       // chunky lugs across the tyre
            A.box(x, cy, 0.9f, 2.3f, 0.3f, rgb(72, 68, 54), 0.7f, 0.4f);
        }
    }
    if (cyber) {
        Color cyan = rgb(110, 232, 255);
        A.line(cx - hl + 1.2f, outerY, cx + hl - 1.2f, outerY, 0.8f, rgba(cyan, 230));
        A.glow(cx, outerY, 4.5f, rgba(cyan, 45));
    } else {
        A.ell(cx + 1.2f, outerY, 2.6f, 0.9f, rgba(rgb(70, 54, 38), 130), 1, 0);
    }
}

// n rivet heads evenly spaced along a line (same helper as art.cpp)
void rivets(Art& A, float x0, float y0, float x1, float y1, int n, Color c) {
    for (int i = 0; i < n; i++) { float t = n > 1 ? i / (float)(n - 1) : 0.5f; A.dot(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, 0.75f, c); }
}

// Rust blotch on the Clanker shell: a translucent stain over the paint under it
void rustPatch(Art& A, float x, float y, float r, Color rust) {
    A.ell(x, y, r, r * 0.7f, rgba(rust, 120), 1, 0);
}

}  // namespace

void artJeepBody(Canvas& c, Color team, bool cyber, int frame) {
    Art A(c, team);
    // wheel phase: the standing frame sits between the two walk frames, which are half a tread pitch apart
    float ph = frame == 1 ? 0.0f : (frame == 2 ? 0.5f : 0.25f);
    Color shell = cyber ? rgb(56, 74, 106) : rgb(98, 106, 64);
    Color shellD = cyber ? rgb(34, 40, 56) : rgb(58, 60, 42);
    Color deck = cyber ? rgb(150, 166, 186) : rgb(116, 122, 78);
    Color floorC = cyber ? rgb(30, 36, 48) : rgb(60, 58, 46);
    Color seat = cyber ? rgb(104, 124, 160) : rgb(176, 158, 110);
    Color steel = cyber ? rgb(112, 124, 142) : rgb(150, 152, 150);
    Color trim = cyber ? rgb(110, 232, 255) : rgb(232, 186, 58);
    Color glass = cyber ? rgb(44, 86, 120) : rgb(84, 98, 90);
    Color tail = cyber ? rgb(255, 70, 80) : rgb(200, 60, 40);
    Color lamp = cyber ? rgb(230, 250, 255) : rgb(255, 240, 190);

    // wheels first: the hull covers their inner halves, so they read as sitting in the arches
    for (float wx : { 14.0f, 46.0f }) {
        wheel(A, wx, 15.5f, ph, cyber, true);
        wheel(A, wx, 48.5f, ph, cyber, false);
    }

    // the shell
    if (cyber) {
        // low angular composite: a wedge hood falling to the nose, flat sides
        A.poly({ {8, 22.5f}, {14, 18.0f}, {41, 18.0f}, {46, 20.2f}, {56.5f, 26.0f}, {58, 30.0f}, {58, 34.0f}, {56.5f, 38.0f}, {46, 43.8f}, {41, 46.0f}, {14, 46.0f}, {8, 41.5f} },
               shell, 2.6f, 0.6f);
        A.poly({ {46.2f, 21.2f}, {52, 24.8f}, {56.4f, 27.8f}, {57.4f, 30.6f}, {57.4f, 33.4f}, {56.4f, 36.2f}, {52, 39.2f}, {46.2f, 42.8f} }, deck, 1.6f, 0.6f);
        A.line(46.6f, 32, 56, 32, 0.5f, rgba(shellD, 200));                                    // bonnet centre seam
        A.line(46.2f, 22.6f, 55.6f, 27.6f, 0.8f, rgba(trim, 215)); A.line(46.2f, 41.4f, 55.6f, 36.4f, 0.8f, rgba(trim, 215));   // light bars along the hood
        A.glow(52, 25.2f, 6, rgba(trim, 40)); A.glow(52, 38.8f, 6, rgba(trim, 40));
        A.box(11.0f, 32, 3.0f, 10.6f, 1.2f, shellD, 1.6f, 0.5f);                              // rear deck
        A.line(8.8f, 27, 13.2f, 27, 0.5f, rgba(shell, 160)); A.line(8.8f, 37, 13.2f, 37, 0.5f, rgba(shell, 160));   // stowage straps
        for (float y = 22; y <= 42; y += 4) A.line(16, y, 44, y, 0.4f, rgba(shellD, 130));   // sill ribs
        A.line(14, 19.4f, 44, 19.4f, 1.0f, rgba(trim, 210)); A.line(14, 44.6f, 44, 44.6f, 1.0f, rgba(trim, 210));   // emitter strips
        A.glow(29, 19.4f, 9, rgba(trim, 55)); A.glow(29, 44.6f, 9, rgba(trim, 55));
    } else {
        // boxy rugged body: square panels, a flat bonnet with a slatted grille
        A.box(33, 32, 25, 14, 3.0f, shell, 2.8f, 0.6f);
        A.box(51.3f, 32, 6.6f, 12.6f, 1.8f, deck, 2.0f, 0.6f);                               // bonnet
        for (int i = 0; i < 5; i++) A.line(55.2f, 25.4f + i * 2.4f, 57.6f, 25.4f + i * 2.4f, 0.9f, rgba(shellD, 230));   // grille slats
        A.box(11.0f, 32, 3.0f, 10.6f, 1.2f, shellD, 1.6f, 0.5f);                              // rear deck
        // hazard-yellow stripe checker along both sides, under the rim
        for (int i = 0; i < 12; i++) {
            float x = 14.6f + i * 2.5f;
            A.box(x, 19.6f, 1.2f, 0.9f, 0.2f, i % 2 ? trim : rgb(34, 32, 24), 0.5f, 0.2f);
            A.box(x, 44.4f, 1.2f, 0.9f, 0.2f, i % 2 ? trim : rgb(34, 32, 24), 0.5f, 0.2f);
        }
    }

    // the open cabin: a dark floor that the seats sit on, a roll bar and the windscreen
    A.box(29.2f, 32, 15.2f, 11.0f, 2.2f, floorC, 2.2f, 0.7f);
    if (cyber) {
        for (float x = 17; x <= 43; x += 4.8f) A.line(x, 22.2f, x, 41.8f, 0.4f, rgba(rgb(46, 56, 74), 170));   // floor ribs
    } else {
        for (int i = 0; i < 9; i++) for (int j = 0; j < 7; j++)                              // diamond plate
            A.dot(15.6f + i * 3.2f + (j & 1) * 1.6f, 22.6f + j * 3.1f, 0.45f, rgb(64, 62, 50));
    }
    A.line(23.2f, 19.8f, 23.2f, 44.2f, 1.1f, steel); A.line(35.6f, 19.8f, 35.6f, 44.2f, 1.1f, steel);   // roll bar hoops, clear of the seats
    A.dot(23.2f, 19.8f, 1.0f, steel); A.dot(23.2f, 44.2f, 1.0f, steel); A.dot(35.6f, 19.8f, 1.0f, steel); A.dot(35.6f, 44.2f, 1.0f, steel);
    A.poly({ {43.6f, 21.8f}, {46.2f, 21.4f}, {46.2f, 42.6f}, {43.6f, 42.2f} }, glass, 0.8f, 0.4f);   // windscreen
    if (cyber) A.line(45, 22.6f, 45, 41.4f, 0.4f, rgba(trim, 150));
    else { A.line(44.2f, 26.8f, 45.4f, 31.4f, 0.4f, rgba(rgb(220, 226, 220), 150)); A.line(44.6f, 31.4f, 45.2f, 34.8f, 0.4f, rgba(rgb(220, 226, 220), 130)); }   // cracked glass

    // exterior: panel lines, weld seams and weathering, all painted before the seats go on so the seats stay plain
    if (cyber) {
        A.line(14, 22.2f, 14, 41.8f, 0.5f, rgba(shellD, 220)); A.line(46.2f, 22.2f, 46.2f, 41.8f, 0.5f, rgba(shellD, 220));   // panel seams
        A.grain(8, 18, 58, 46, 0.05f, 11);
    } else {
        A.line(13.8f, 23.2f, 44.2f, 23.2f, 0.5f, rgba(shellD, 200));       // weld seam with bead dots
        for (int i = 0; i < 14; i++) A.dot(14.4f + i * 2.2f, 23.7f, 0.4f, shade(shell, 1.6f));
        rivets(A, 14, 40.9f, 44, 40.9f, 9, shade(shell, 1.35f));
        A.grain(8, 18, 58, 46, 0.14f, 33);
        Rng r(77);   // fixed seed: the same rust and chips on every frame
        for (int i = 0; i < 6; i++) rustPatch(A, r.f(12, 52), r.f(20, 44), r.f(1.2f, 2.4f), rgb(156, 90, 48));
        for (int i = 0; i < 6; i++) A.dot(r.f(14, 52), r.f(19.5f, 44.5f), 0.45f, rgba(rgb(150, 152, 150), 210));
    }

    // seats: plain discs on the floor, drawn last so nothing lies on top of them (the renderer puts a helmet on each occupied seat)
    for (int i = 0; i < 7; i++) A.circle(JEEP_SEAT_X[i], JEEP_SEAT_Y[i], 3.0f, seat, 1.0f, 0.35f);

    // exterior fittings, clear of the seats: mirrors, lamps, tail lights, sensor mast or spare and jerry cans, exhaust
    for (int s = -1; s <= 1; s += 2) {
        float my = s < 0 ? 16.8f : 47.2f, ey = s < 0 ? 20.4f : 43.6f;
        A.line(43.6f, ey, 40.9f, my, 0.7f, rgba(shellD, 230));                                       // mirror stalk
        A.box(40.2f, my, 1.4f, 1.0f, 0.4f, cyber ? rgb(18, 22, 30) : rgb(40, 40, 34), 0.8f, 0.3f);
    }
    A.box(9.0f, 22.2f, 0.7f, 1.6f, 0.4f, tail, 0.6f, 0.2f); A.box(9.0f, 41.8f, 0.7f, 1.6f, 0.4f, tail, 0.6f, 0.2f);   // tail lights
    if (cyber) {
        A.dot(57.0f, 27.8f, 1.2f, lamp); A.dot(57.0f, 36.2f, 1.2f, lamp);
        A.glow(57.0f, 27.8f, 4.0f, rgba(trim, 90)); A.glow(57.0f, 36.2f, 4.0f, rgba(trim, 90));
        A.dot(11.0f, 26.5f, 0.8f, steel);                                                              // slim sensor mast, seen from above
        A.glow(11.0f, 26.5f, 3.2f, rgba(trim, 150)); A.dot(11.0f, 26.5f, 0.5f, rgb(240, 252, 255));
    } else {
        A.ring(56.8f, 27.6f, 1.5f, 1.0f, steel, 0.8f, 0.4f); A.dot(56.8f, 27.6f, 0.9f, lamp);   // round lamps in steel bezels
        A.ring(56.8f, 35.6f, 1.5f, 1.0f, steel, 0.8f, 0.4f); A.dot(56.8f, 35.6f, 0.9f, lamp);
        A.ring(9.8f, 32, 3.2f, 1.8f, rgb(30, 30, 26), 1.2f, 0.4f);                                     // spare on the tail
        A.circle(9.8f, 32, 1.6f, steel, 1.0f, 0.4f);
        A.box(11.5f, 21.9f, 2.3f, 1.4f, 0.5f, rgb(120, 96, 56), 1.2f, 0.4f);                          // jerry cans
        A.box(11.5f, 42.1f, 2.3f, 1.4f, 0.5f, rgb(120, 96, 56), 1.2f, 0.4f);
        A.circle(37.6f, 47.4f, 1.3f, steel, 1.2f, 0.4f); A.dot(37.6f, 47.4f, 0.6f, rgb(20, 18, 14));   // exhaust pipe
        A.line(59.2f, 22.2f, 59.2f, 41.8f, 1.2f, steel);                                              // brush guard bar
        A.dot(58.8f, 22.2f, 0.9f, steel); A.dot(58.8f, 41.8f, 0.9f, steel);
    }

    teamJewel(A, 50.8f, 32, 2.0f);
    c.outline(rgb(8, 10, 14, 200));
}

void artGuardDrone(Canvas& c, Color team, bool cyber) {
    Art A(c, team);
    Color body = cyber ? rgb(40, 44, 54) : rgb(118, 104, 64);      // matte graphite / riveted brass
    Color arm = cyber ? rgb(30, 33, 42) : rgb(96, 84, 52);
    Color housing = cyber ? rgb(54, 60, 72) : rgb(96, 104, 66);
    Color duct = cyber ? rgb(20, 24, 32) : rgb(52, 50, 36);
    Color lens = cyber ? rgb(110, 232, 255) : rgb(255, 176, 60);
    Color rivet = rgb(186, 160, 100);
    // four stub arms from the centre, then the rotor housings (the rotors are a live blur, not drawn), the pod and its sensor
    const float HX = 7.0f;
    for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) A.cap(32, 32, 32 + sx * HX, 32 + sy * HX, 1.0f, arm, 0.9f, 0.5f);
    for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) {
        float hx = 32 + sx * HX, hy = 32 + sy * HX;
        A.circle(hx, hy, 4.0f, housing, 1.6f, 0.5f);                                                    // housing rim
        A.circle(hx, hy, 2.9f, duct, 1.2f, 0.3f);                                                       // dark duct under the blur
        if (cyber) A.ring(hx, hy, 3.5f, 1.0f, rgba(lens, 200), 0.5f, 0.2f);                            // cyan rim light
        else for (int k = 0; k < 6; k++) { float a = k * 1.0472f; A.dot(hx + std::cos(a) * 3.4f, hy + std::sin(a) * 3.4f, 0.55f, rivet); }   // rivets
    }
    A.ell(32, 32, 6.2f, 4.6f, body, 2.2f, 0.6f);                                                          // pod
    A.ell(31, 32, 3.8f, 2.8f, shade(body, 1.3f), 1.2f, 0.4f);                                            // top deck
    if (!cyber) for (float y : { 28.8f, 35.2f }) { A.dot(28.0f, y, 0.4f, rgba(rivet, 220)); A.dot(35.6f, y, 0.4f, rgba(rivet, 220)); }   // pod plate rivets
    A.circle(37.2f, 32, 2.2f, duct, 1.2f, 0.4f);                                                           // sensor housing
    A.dot(37.4f, 31.8f, 1.2f, lens);
    A.glow(37.4f, 32, cyber ? 5.0f : 3.8f, rgba(lens, cyber ? 140 : 110));
    if (cyber) A.ring(37.2f, 32, 2.9f, 0.8f, rgba(lens, 230), 0.5f, 0.1f);                              // cyan lens ring
    teamJewel(A, 28.4f, 32, 1.9f);
    c.outline(rgb(8, 10, 14, 200));
}
