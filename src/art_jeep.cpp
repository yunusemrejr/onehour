#include "art2.h"
#include "paint.h"

using namespace paint;

// Jeep and protector drone art, painted like the rest of the unit sprites (see art.cpp): 64x64 canvas, facing +x, centre (32,32), lit from
// the north-west. The jeep is an open-top 4x4 seen from above: a body narrower than its wheel track, fender pads curving over the four tyres,
// a narrow bonnet with a grille and round lamps, a windscreen frame, a crew tub with a raised sill and seven seats, and a spare on the tail.
// Cyber: a low composite utility with armoured door panels, a roof-less bay, a pintle ring and an antenna whip. Clanker: an olive open jeep
// with a rolled canvas top, jerry cans and a brush guard. Both carry a team-coloured band across the bonnet and a team-coloured spare hub.
// The drop shadow is soft and sits on its own layer, under the outline, so it never draws a dark border.
namespace {

// The player's mark for the protector drone: the same jewel as teamDot in art.cpp (dark seat ring, team colour lit from the north-west,
// pinpoint highlight). The jeep paints its team colour as a band instead.
void teamJewel(Art& A, float cx, float cy, float r) {
    A.circle(cx, cy, r + 1.3f, rgba(rgb(20, 24, 32), 235), 1.0f, 0.2f);
    A.circle(cx, cy, r, A.team, r, 0.75f);
    A.dot(cx - r * 0.32f, cy - r * 0.36f, r * 0.24f, rgba(mix(A.team, rgb(255, 255, 255), 0.75f), 235));
}

// A plan outline symmetric about y = 32, given as (x, half-width) pairs from the nose (+x) back to the tail
std::vector<P> mirrored(const std::vector<P>& half) {
    std::vector<P> v;
    for (const P& p : half) v.push_back({p.x, 32 - p.y});
    for (size_t i = half.size(); i-- > 0;) v.push_back({half[i].x, 32 + half[i].y});
    return v;
}

// One tyre seen from above: a dark rounded rectangle about 11.5 px along the rolling direction (x) and 5 px across the axle. Tread lugs
// run across it and slide by ph (0..1) of one pitch, so the two walk frames differ by half a pitch. The outer sidewall edge is lit a little
// lighter, so the tyre reads as rubber rather than a hole.
void wheel(Art& A, float cx, float cy, float ph, bool cyber, bool top) {
    const float hl = 5.75f, hh = 2.5f;
    Color tyre = cyber ? rgb(56, 62, 74) : rgb(66, 62, 50);
    float sideY = top ? cy - hh + 0.6f : cy + hh - 0.6f;
    A.box(cx, cy, hl, hh, 2.2f, tyre, 1.4f, 0.6f);
    float pitch = cyber ? 2.6f : 2.9f;
    Color lug = cyber ? rgb(120, 134, 156) : rgb(124, 116, 92);
    for (float x = cx - hl + 1.5f + ph * pitch; x < cx + hl - 1.5f; x += pitch) A.box(x, cy, 0.55f, hh - 0.5f, 0.2f, lug, 0.5f, 0.4f);
    A.line(cx - hl + 1.8f, sideY, cx + hl - 1.8f, sideY, 0.6f, rgba(cyber ? rgb(110, 124, 146) : rgb(112, 106, 84), 220));
}

// One seat seen from above: a low rounded cushion centred on the seat position, with a short backrest on its rear (-x) side. Kept flat, since
// the renderer puts a helmet over the cushion when a soldier sits there.
void seat(Art& A, float sx, float sy, Color cush, Color back) {
    A.box(sx, sy, 1.6f, 1.45f, 0.7f, cush, 0.8f, 0.45f);
    A.box(sx - 2.0f, sy, 0.5f, 1.75f, 0.3f, back, 0.5f, 0.35f);
}

// n rivet heads evenly spaced along a line (same helper as art.cpp)
void rivets(Art& A, float x0, float y0, float x1, float y1, int n, Color c) {
    for (int i = 0; i < n; i++) { float t = n > 1 ? i / (float)(n - 1) : 0.5f; A.dot(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, 0.6f, c); }
}

// Rust blotch on the Clanker shell: a translucent stain over the paint under it
void rustPatch(Art& A, float x, float y, float r, Color rust) {
    A.ell(x, y, r, r * 0.7f, rgba(rust, 120), 1, 0);
}

// local contrast on the opaque pixels (the unsharp mask of enhance() in art.cpp, repeated here because that one is private to art.cpp):
// crisps the seams, lugs and rivets at display size
void crisp(Canvas& c, float amount) {
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

// Lays the jeep over its own soft shadow: the shadow shows only through the jeep's transparent pixels
void underShadow(Canvas& c, const Canvas& sh) {
    for (size_t i = 0; i < c.px.size(); i++) {
        Color t = Canvas::unpack(c.px[i]), u = Canvas::unpack(sh.px[i]);
        if (u.a == 0 || t.a == 255) continue;
        if (t.a == 0) { c.px[i] = sh.px[i]; continue; }
        float ta = t.a / 255.0f, ua = u.a / 255.0f, oa = ta + ua * (1 - ta);
        Color o{(u8)((t.r * ta + u.r * ua * (1 - ta)) / oa), (u8)((t.g * ta + u.g * ua * (1 - ta)) / oa), (u8)((t.b * ta + u.b * ua * (1 - ta)) / oa), (u8)(oa * 255)};
        c.px[i] = Canvas::pack(o);
    }
}

}  // namespace

void artJeepBody(Canvas& c, Color team, bool cyber, int frame) {
    Canvas sh(c.w, c.h);   // the drop shadow is painted on its own layer, so the outline below never sees it
    Art A(c, team);
    // wheel phase: the standing frame sits between the two walk frames, which are half a tread pitch apart
    float ph = frame == 1 ? 0.0f : (frame == 2 ? 0.5f : 0.25f);
    Color shell = cyber ? rgb(58, 74, 98) : rgb(98, 106, 64);
    Color shellD = cyber ? rgb(34, 42, 58) : rgb(56, 58, 40);
    Color deck = cyber ? rgb(146, 162, 184) : rgb(122, 128, 84);
    Color floorC = cyber ? rgb(34, 40, 52) : rgb(44, 44, 34);           // dark floor of the tub
    Color upholstery = cyber ? rgb(58, 72, 96) : rgb(78, 76, 56);     // seat cushions
    Color upholsteryD = cyber ? rgb(38, 46, 64) : rgb(52, 50, 36);    // seat backrests
    Color steel = cyber ? rgb(112, 124, 142) : rgb(150, 152, 150);
    Color sFrame = cyber ? rgb(92, 104, 124) : steel;                  // windscreen frame
    Color glass = rgb(136, 162, 180);                                   // light blue-grey glass
    Color lamp = cyber ? rgb(230, 250, 255) : rgb(255, 240, 190);
    Color tail = cyber ? rgb(255, 70, 80) : rgb(200, 60, 40);
    Color tyre = cyber ? rgb(56, 62, 74) : rgb(66, 62, 50);
    Color trim = rgb(110, 232, 255);
    Color canvasC = rgb(104, 102, 76);                                  // Clanker: the folded canvas top
    Color jerry = rgb(84, 98, 62);                                       // Clanker: jerry cans
    Color ammo = rgb(70, 80, 96);                                        // Cyber: ammo boxes
    const float padHW = 6.0f, padHH = cyber ? 2.4f : 2.2f, padR = cyber ? 1.2f : 2.2f;   // fender pads over the axles

    // the plan outline of the hull as half-widths about y = 32, nose (x 57) to tail (x 9.6): the bonnet is narrower than the crew tub, and the
    // whole body is narrower than the wheel track (the tyres sit at |y - 32| = 10.5)
    std::vector<P> hull = cyber
        ? mirrored({ {57.0f, 5.2f}, {53.0f, 7.2f}, {47.0f, 8.0f}, {43.0f, 8.8f}, {41.5f, 9.4f}, {30.0f, 9.8f}, {20.0f, 9.8f}, {14.0f, 9.4f}, {10.6f, 8.9f}, {9.6f, 8.2f} })
        : mirrored({ {57.0f, 5.6f}, {53.5f, 7.0f}, {49.0f, 7.9f}, {44.5f, 8.6f}, {41.5f, 9.6f}, {30.0f, 10.0f}, {20.0f, 10.0f}, {15.0f, 9.6f}, {11.0f, 9.2f}, {9.6f, 8.2f} });
    float area = 0;
    for (size_t i = 0; i < hull.size(); i++) { const P& p = hull[i]; const P& q = hull[(i + 1) % hull.size()]; area += p.x * q.y - q.x * p.y; }
    float hsgn = area > 0 ? 1.0f : -1.0f;

    // signed distance to the whole plan (hull, fender pads and tyres, the nearest of them) for the drop shadow
    auto plan = [&](float x, float y) {
        float d = sdPoly(hull, x, y, hsgn);
        for (float ax : { 17.0f, 46.0f }) {
            for (float py : { 23.4f, 40.6f }) d = std::min(d, sdBox(x, y, ax, py, padHW, padHH, padR));
            for (float py : { 21.5f, 42.5f }) d = std::min(d, sdBox(x, y, ax, py, 5.75f, 2.5f, 2.2f));
        }
        return d;
    };
    Art S(sh, team);
    S.soft(0, 0, c.w - 1, c.h - 1, [&](float x, float y) { return plan(x - 1.8f, y - 2.0f); }, Color{0, 0, 0, 80}, 1.8f);   // soft, to the south-east

    // tyres first, then the fender pads: the pads cover the inner half of each tyre, so the outer part shows past them
    for (float ax : { 17.0f, 46.0f }) { wheel(A, ax, 21.5f, ph, cyber, true); wheel(A, ax, 42.5f, ph, cyber, false); }
    for (float ax : { 17.0f, 46.0f }) for (float py : { 23.4f, 40.6f }) A.box(ax, py, padHW, padHH, padR, shell, 1.4f, 0.6f);
    A.poly(hull, shell, 2.6f, 0.6f);

    // the bonnet: a raised deck between the wings, a centre ridge, the team band, the grille and the lamps set in the fenders
    A.poly(mirrored({ {56.0f, 4.4f}, {53.0f, 5.8f}, {49.0f, 6.7f}, {44.5f, 7.4f}, {43.5f, 7.4f} }), deck, 1.4f, 0.5f);
    A.cap(44.8f, 32, 55.0f, 32, 0.7f, shade(deck, 1.25f), 0.8f, 0.4f);                  // centre ridge
    A.box(50.8f, 32, 1.3f, 5.2f, 0.3f, A.team, 0.6f, 0.3f);                              // team band across the bonnet
    A.box(55.6f, 32, 0.9f, 3.6f, 0.3f, shellD, 0.6f, 0.3f);                              // grille face
    for (float gx : { 54.9f, 55.5f, 56.1f }) A.line(gx, 28.8f, gx, 35.2f, 0.4f, rgba(shade(shell, 1.5f), 200));   // grille slats
    for (float ly : { 23.6f, 40.4f }) {
        if (cyber) A.dot(50.8f, ly, 0.8f, lamp);
        else { A.ring(50.8f, ly, 1.0f, 0.9f, steel, 0.6f, 0.3f); A.dot(50.8f, ly, 0.6f, lamp); }
    }
    if (cyber) {
        A.line(56.5f, 27.5f, 53.6f, 25.0f, 0.6f, rgba(trim, 235));                       // thin running lights at the front corners
        A.line(56.5f, 36.5f, 53.6f, 39.0f, 0.6f, rgba(trim, 235));
    } else {
        A.line(58.0f, 24.0f, 58.0f, 40.0f, 0.9f, steel);                                  // brush guard bar
        A.line(57.0f, 27.2f, 58.0f, 24.6f, 0.6f, steel); A.line(57.0f, 36.8f, 58.0f, 39.4f, 0.6f, steel);
    }

    // windscreen: a steel frame across the body at x 42 to 43, a light blue-grey glass strip and a highlight on it
    A.box(42.7f, 32, 0.55f, 8.0f, 0.3f, sFrame, 1.0f, 0.4f);
    A.box(42.7f, 32, 0.4f, 7.4f, 0.15f, glass, 0.5f, 0.25f);
    A.line(42.45f, 25.2f, 42.45f, 29.0f, 0.35f, rgba(rgb(232, 242, 250), 180));

    // the crew tub: a dark floor inside a raised sill 1.8 px wide along both sides
    A.box(28.4f, 32, 13.4f, 8.2f, 1.0f, floorC, 1.6f, 0.5f);
    for (float sy : { 22.85f, 41.15f }) A.box(28.4f, sy, 13.4f, 0.95f, 0.4f, shade(shell, 1.25f), 0.9f, 0.7f);
    if (cyber) {
        A.line(14.6f, 23.8f, 14.6f, 40.2f, 0.8f, shellD);                                    // roof-less bay: a low rear bulkhead
        for (float sy : { 22.2f, 41.8f }) rivets(A, 16, sy, 40, sy, 6, shade(shell, 1.4f));   // armoured door bolts
        A.line(28.0f, 21.9f, 28.0f, 23.8f, 0.4f, rgba(shellD, 220)); A.line(28.0f, 40.2f, 28.0f, 42.1f, 0.4f, rgba(shellD, 220));   // door seams
        A.grain(9, 19, 58, 45, 0.05f, 11);
    } else {
        A.line(14.9f, 23.8f, 14.9f, 40.2f, 0.9f, steel);                                     // roll bar across the rear
        A.dot(14.9f, 23.8f, 0.8f, steel); A.dot(14.9f, 40.2f, 0.8f, steel);
        for (float sy : { 22.2f, 41.8f }) rivets(A, 16, sy, 40, sy, 7, shade(shell, 1.35f));   // sill rivets
        A.dot(49.5f, 27.0f, 0.5f, shade(shell, 1.5f)); A.dot(49.5f, 37.0f, 0.5f, shade(shell, 1.5f));   // bonnet-side rivets
        A.grain(8, 18, 58, 46, 0.08f, 33);
        A.ell(50.5f, 29.0f, 3.0f, 1.2f, rgba(rgb(176, 150, 102), 110), 1, 0);                // dust on the bonnet
        A.ell(46.8f, 35.6f, 2.2f, 1.0f, rgba(rgb(176, 150, 102), 90), 1, 0);
        Rng r(77);   // fixed seed: the same rust and chips on every frame, kept on the sill and the hull
        for (int i = 0; i < 4; i++) rustPatch(A, r.f(16, 40), (i & 1) ? 22.3f : 41.7f, r.f(1.0f, 1.8f), rgb(156, 90, 48));
        for (int i = 0; i < 3; i++) A.dot(r.f(16, 40), (i & 1) ? 22.3f : 41.7f, 0.4f, rgba(rgb(150, 152, 150), 210));
    }

    // the rear: a spare on the tail, a short dark capsule across it with a team-coloured hub, and tail lamps at the corners
    A.cap(8.6f, 27.4f, 8.6f, 36.6f, 1.25f, tyre, 1.0f, 0.5f);
    A.dot(8.6f, 32, 0.7f, A.team);
    for (float ty : { 22.4f, 41.6f }) A.box(9.6f, ty, 0.6f, 1.0f, 0.3f, tail, 0.5f, 0.2f);
    if (cyber) {
        for (float jy : { 25.2f, 38.8f }) A.box(11.0f, jy, 1.2f, 1.3f, 0.3f, ammo, 1.0f, 0.4f);   // ammo boxes beside the pintle
        A.ring(12.0f, 32, 1.6f, 1.0f, steel, 0.8f, 0.5f); A.dot(12.0f, 32, 0.55f, rgb(20, 22, 26));   // pintle mount ring at the rear
        A.line(12.0f, 23.6f, 9.0f, 19.2f, 0.5f, steel); A.dot(8.8f, 19.0f, 0.6f, trim);       // antenna whip, its tip lit
    } else {
        A.cap(12.9f, 23.2f, 12.9f, 40.8f, 1.2f, canvasC, 1.0f, 0.5f);                          // folded canvas top, rolled behind the rear seats
        A.line(11.7f, 27.0f, 14.1f, 27.0f, 0.4f, rgba(shellD, 200)); A.line(11.7f, 37.0f, 14.1f, 37.0f, 0.4f, rgba(shellD, 200));   // lashings
        for (float jy : { 25.2f, 38.8f }) A.box(11.0f, jy, 1.2f, 1.3f, 0.3f, jerry, 0.8f, 0.4f);   // jerry cans
    }

    // seats: low rounded cushions with a short backrest, drawn last so nothing lies on top of them (the renderer puts a helmet on each occupied seat)
    for (int i = 0; i < 7; i++) seat(A, JEEP_SEAT_X[i], JEEP_SEAT_Y[i], upholstery, upholsteryD);

    crisp(c, 0.6f);
    c.outline(rgb(8, 10, 14, 200));
    underShadow(c, sh);
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
