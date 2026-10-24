#include "bld.h"

// The map's special structures: the Island Nuke Launcher in its laser chamber, the Mountain Fortress dug into the hills and the
// indestructible Bastion Wall. Same conventions as the other structure painters (bld.h): raised 3/4 view, light from the north-west.
namespace {

const Color S_CONC = rgb(146, 148, 146), S_CONC_D = rgb(92, 94, 94), S_STEEL = rgb(70, 76, 86), S_STEEL_L = rgb(128, 136, 146);
const Color S_RED = rgb(255, 98, 64), S_RED_D = rgb(196, 42, 36), S_CORE = rgb(255, 214, 180), S_MISSILE = rgb(238, 242, 246);
const Color G_SLOT = rgb(12, 12, 14);
const Color G_ROCK = rgb(98, 94, 88), G_ROCK_D = rgb(64, 61, 57);
const Color FT_SLAB = rgb(176, 174, 166), FT_SLAB_L = rgb(194, 192, 184), FT_WALL = rgb(132, 131, 125), FT_STAIN = rgb(64, 60, 52), FT_MOSS = rgb(92, 116, 62);

// the corners of a regular n-sided polygon of circumradius r round (cx, cy), the first one at angle rot
std::vector<P> regular(float cx, float cy, float r, int n, float rot) {
    std::vector<P> v;
    for (int i = 0; i < n; i++) { float t = rot + i * 6.2831853f / n; v.push_back({cx + std::cos(t) * r, cy + std::sin(t) * r}); }
    return v;
}

// soft shadow of a convex plan polygon, moved by (dx, dy) (the same falloff as Art::shadowRect)
void softPoly(Bld& b, std::vector<P> v, float dx, float dy, float feather, int alpha) {
    Art& a = b.a;
    float area = 0, mnx = 1e9f, mny = 1e9f, mxx = -1e9f, mxy = -1e9f;
    for (auto& p : v) { p.x = a.X(p.x + dx); p.y = a.Y(p.y + dy); mnx = std::min(mnx, p.x); mny = std::min(mny, p.y); mxx = std::max(mxx, p.x); mxy = std::max(mxy, p.y); }
    for (size_t i = 0; i < v.size(); i++) { const P& p = v[i]; const P& q = v[(i + 1) % v.size()]; area += p.x * q.y - q.x * p.y; }
    float sgn = area > 0 ? 1.0f : -1.0f, f = feather * a.k;
    a.soft((int)(mnx - f - 2), (int)(mny - f - 2), (int)(mxx + f + 2), (int)(mxy + f + 2), [v, sgn](float x, float y) { return sdPoly(v, x, y, sgn); }, Color{0, 0, 0, (u8)alpha}, f);
}

// A convex volume standing on a plan polygon and lifted by ht, drawn the way Bld::block lifts a box: a soft shadow to the south-east, the
// faces that look south as walls (darker away from the north-west light), the top face as the roof. Gun slits sit on each visible wall.
void prism(Bld& b, const std::vector<P>& plan, float ht, Color roof, Color wall, int slits = 0) {
    Art& a = b.a;
    softPoly(b, plan, ht * 0.55f + 1, ht * 0.4f + 1, 1.5f + ht * 0.2f, 100);
    size_t n = plan.size();
    float cx = 0, cy = 0;
    for (auto& p : plan) { cx += p.x; cy += p.y; }
    cx /= n; cy /= n;
    for (size_t i = 0; i < n; i++) {
        P p = plan[i], q = plan[(i + 1) % n];
        float ex = q.x - p.x, ey = q.y - p.y, l = hyp(ex, ey);
        float nx = ey / l, ny = -ex / l;   // outward normal of this edge
        if (nx * ((p.x + q.x) * 0.5f - cx) + ny * ((p.y + q.y) * 0.5f - cy) < 0) { nx = -nx; ny = -ny; }
        if (ny < 0.02f) continue;          // a face that looks north is hidden behind the roof
        float lit = -(nx * 0.62f + ny * 0.78f);
        a.poly({p, q, {q.x, q.y - ht}, {p.x, p.y - ht}}, shade(wall, 1.0f + 0.16f * lit), 1, 0);
        a.line(p.x, p.y, q.x, q.y, 1.0f, rgba(BLACK, 110));   // foot line where the wall meets the ground
        for (int s = 1; s <= slits; s++) {
            float t = (float)s / (slits + 1), x = p.x + (q.x - p.x) * t, y = p.y + (q.y - p.y) * t - ht * 0.5f;
            a.rect(x - 2.2f, y - 2.0f, 4.4f, 4.0f, 0.3f, G_SLOT, 0.5f, 0.1f);
        }
    }
    for (auto& p : plan) a.line(p.x, p.y, p.x, p.y - ht, 0.7f, rgba(BLACK, 90));   // vertical joints (the back ones are hidden by the roof)
    std::vector<P> top;
    for (auto& p : plan) top.push_back({p.x, p.y - ht});
    a.poly(top, roof, 1.6f, 0.58f);
}

// a rock: a convex heptagon with a shadow to the south-east and a lit bevel
void boulder(Bld& b, float cx, float cy, float r, Color c) {
    b.a.shadowCircle(cx, cy, r, r * 0.25f, r * 0.2f, 2 + r * 0.2f, 100);
    b.a.ngon(cx, cy, r, 7, b.rng.f(0, 6.28f), c, r * 0.35f, 0.8f);
}

// formwork on a poured concrete wall: vertical panel joints every `pitch` with tie-holes at the top and bottom, and a lift line half way up
void formwork(Bld& b, float x0, float y0, float x1, float y1, float pitch) {
    for (float x = x0 + pitch; x < x1 - 2; x += pitch) {
        b.a.line(x, y0 + 0.8f, x, y1 - 0.8f, 0.5f, rgba(BLACK, 60));
        b.a.dot(x, y0 + 2.4f, 0.45f, rgba(BLACK, 120));
        b.a.dot(x, y1 - 2.4f, 0.45f, rgba(BLACK, 120));
    }
    float my = (y0 + y1) * 0.5f;
    b.a.line(x0, my, x1, my, 0.5f, rgba(BLACK, 40));
}

// a rain stain or seep: a faint dark streak down a wall from under a roof edge
void stain(Bld& b, float x, float y, float w, float h, int alpha = 60) { b.a.rect(x, y, w, h, w * 0.5f, rgba(FT_STAIN, alpha), 1.0f, 0.0f); }

// a long firing slit in a wall: a dark slot with a warm light on its lower lip and a pale concrete sill under it
void slit(Bld& b, float x, float y, float w) {
    b.a.rect(x, y, w, 3.0f, 0.4f, G_SLOT, 0.4f, 0.1f);
    b.a.line(x + 0.5f, y + 2.5f, x + w - 0.5f, y + 2.5f, 0.6f, rgba(rgb(255, 178, 98), 190));
    b.a.rect(x - 0.7f, y + 3.0f, w + 1.4f, 0.8f, 0.2f, rgb(214, 212, 204), 0.4f, 0.3f);
}

// a steel blast door standing on the wall foot baseY, w wide and h high: a dark recess, a hazard-striped frame, a dark steel leaf with
// two horizontal stiffener ribs, a centre seam and a wheel handle
void blastDoor(Bld& b, float x, float baseY, float w, float h) {
    float top = baseY - h;
    b.a.rect(x - 1.2f, top - 1.2f, w + 2.4f, h + 1.2f, 0.4f, rgb(54, 56, 60), 0.8f, 0.3f);
    b.hazard(x - 3.2f, top - 3.2f, w + 6.4f, 2.0f);
    b.hazard(x - 3.2f, top - 1.2f, 2.0f, h + 1.2f);
    b.hazard(x + w + 1.2f, top - 1.2f, 2.0f, h + 1.2f);
    b.a.rect(x, top, w, h, 0.3f, S_STEEL, 1.0f, 0.5f);
    for (float yy = top + h / 3; yy < baseY - 1; yy += h / 3) b.a.line(x + 0.8f, yy, x + w - 0.8f, yy, 0.5f, rgba(BLACK, 120));
    b.a.line(x + w * 0.5f, top + 0.8f, x + w * 0.5f, baseY - 0.8f, 0.5f, rgba(BLACK, 130));
    b.a.dot(x + w * 0.5f, baseY - 2.4f, 0.8f, S_STEEL_L);
}

// an open gun pit on a roof (centre cx, cy): a concrete lip, a dark floor, two courses of sandbags on the near (south) rim and a static
// twin anti-aircraft gun on a round mount, its barrels raised towards the north-east
void gunPit(Bld& b, float cx, float cy, float r) {
    b.a.circle(cx, cy, r + 1.8f, FT_SLAB_L, 1.2f, 0.45f);
    b.a.circle(cx, cy, r, rgb(38, 38, 36), 2.2f, 0.5f);
    for (int row = 0; row < 2; row++)
        for (int i = 0; i < 8; i++) {
            float an = 0.25f + i * (2.65f / 7), rr = r + 3.6f + row * 3.4f;
            b.a.ell(cx + std::cos(an) * rr, cy + std::sin(an) * rr, 3.2f, 2.2f, shade(K_SAND, 0.9f + b.rng.f(0, 0.16f)), 1.3f, 0.6f);
        }
    b.a.circle(cx, cy, r * 0.42f, S_STEEL, 1.2f, 0.5f);
    float ca = std::cos(-0.75f), sa = std::sin(-0.75f);
    for (float off : {-1.6f, 1.6f}) {
        float ox = -sa * off, oy = ca * off;
        b.a.cap(cx + ox, cy + oy, cx + ox + ca * 10.0f, cy + oy + sa * 10.0f, 0.7f, S_STEEL_L, 0.5f, 0.3f);
        b.a.dot(cx + ox + ca * 10.0f, cy + oy + sa * 10.0f, 0.7f, rgb(14, 14, 16));
    }
    b.a.rect(cx - 2.2f, cy - 2.0f, 4.4f, 4.0f, 0.8f, S_STEEL, 0.6f, 0.4f);
}

// camouflage netting draped over a patch of roof: a mottled olive sheet with a ragged hem and a few sagging creases
void camoNet(Bld& b, float x0, float y0, float x1, float y1) {
    std::vector<P> v;
    for (int i = 0; i <= 6; i++) v.push_back({x0 + (x1 - x0) * i / 6.0f, y0 + b.rng.f(-1.0f, 1.0f)});
    for (int i = 6; i >= 0; i--) v.push_back({x0 + (x1 - x0) * i / 6.0f, y1 + b.rng.f(-1.8f, 0.8f)});
    b.a.poly(v, rgb(92, 104, 66), 1.0f, 0.3f, 0.93f);
    for (int i = 0; i < 16; i++) {
        float px = b.rng.f(x0 + 4, x1 - 4), py = b.rng.f(y0 + 2, y1 - 2);
        b.a.ell(px, py, b.rng.f(2.5f, 5.0f), b.rng.f(1.5f, 2.8f), rgba(i & 1 ? rgb(56, 66, 40) : rgb(126, 128, 92), 150), 1.0f, 0.2f);
    }
    for (float x = x0 + 9; x < x1; x += 14) b.a.line(x, y0 + 1, x - 3, y1 - 1, 0.5f, rgba(BLACK, 70));
}

// a soft patch of earth or moss lying on a roof or the ground
void patch(Bld& b, float cx, float cy, float rx, float ry, Color c, int alpha) { b.a.ell(cx, cy, rx, ry, rgba(c, alpha), 2.5f, 0.3f); }

// a ventilation stack on a roof plane, its foot at (cx, by): a concrete collar with a louvred grille on top
void ventStack(Bld& b, float cx, float by, float r, float ht) {
    P t = b.cyl(cx, by, r, ht, FT_SLAB_L, FT_WALL);
    b.a.circle(t.x, t.y, r * 0.62f, rgb(22, 24, 26), 1.0f, 0.3f);
    for (float yy = t.y - r * 0.4f; yy <= t.y + r * 0.45f; yy += r * 0.42f) b.a.line(t.x - r * 0.5f, yy, t.x + r * 0.5f, yy, 0.5f, rgba(FT_SLAB_L, 170));
}

// a periscope stub on a roof: a steel stalk with a small tinted head
void periscope(Bld& b, float x, float y, float ht) {
    b.a.cap(x, y, x, y - ht, 0.8f, S_STEEL_L, 0.6f, 0.3f);
    b.a.rect(x - 1.5f, y - ht - 1.4f, 3.0f, 2.2f, 0.4f, rgb(30, 34, 36), 0.6f, 0.3f);
    b.a.dot(x - 0.4f, y - ht - 0.4f, 0.6f, rgba(rgb(120, 180, 210), 220));
}

// a chipped corner: a small dark bite out of a concrete edge at (x, y), s px across
void chip(Bld& b, float x, float y, float s) { b.a.poly({{x, y}, {x + s, y}, {x + s * 0.35f, y + s * 0.9f}}, rgb(92, 91, 86), 0.4f, 0.2f); }

// a flag on a pole: the cloth is dark, and the team mask makes it the owner's colour (as the civic flags do)
void flagpole(Bld& b, float x, float y, float ht, float fw, float fh) {
    b.a.line(x + 0.9f, y + 0.9f, x + 0.9f, y - ht + 0.9f, 0.9f, rgba(BLACK, 60));
    b.a.cap(x, y, x, y - ht, 0.6f, rgb(206, 210, 216), 0.6f, 0.4f);
    float ty = y - ht + 1.5f;
    std::vector<P> cloth = {{x, ty}, {x + fw, ty + 1.0f}, {x + fw, ty + fh + 1.0f}, {x, ty + fh}};
    b.a.poly(cloth, rgb(22, 24, 30), 0.8f, 0.2f);
    b.t.poly(cloth, WHITE, 1.2f, 0.45f);
}

// 96 x 96, on a lake island: an armoured octagonal base with a small nuke standing in its erector cradle, and a laser chamber of six
// emitter pylons round the footprint edge whose beams form a cage round the missile.
void islandLauncher(Bld& b) {
    int W = b.W, H = b.H;
    const float cx = 48, cy = 50;   // plan centre of the base
    const float PR = 38, PH = 16;   // pylons stand on a hexagon of radius PR and are PH tall
    b.pad(rgb(72, 78, 80), rgb(38, 42, 46), 0.14f, 6);
    b.hazard(3, 3, W - 6.0f, 2.6f); b.hazard(3, H - 5.6f, W - 6.0f, 2.6f);
    b.hazard(3, 5.6f, 2.6f, H - 11.2f); b.hazard(W - 5.6f, 5.6f, 2.6f, H - 11.2f);
    b.vent(7, 8, 11, 6, S_STEEL_L); b.vent(78, 80, 11, 6, S_STEEL_L);

    auto pylonAt = [&](int i) { float an = -1.5708f + i * 1.0472f; return P{cx + std::cos(an) * PR, cy + std::sin(an) * PR}; };
    auto pylon = [&](int i) {
        P g = pylonAt(i);
        P t = b.cyl(g.x, g.y, 3.2f, PH, rgb(96, 102, 110), rgb(64, 68, 76));
        b.a.glow(t.x, t.y, 7, rgba(S_RED, 120));
        b.a.dot(t.x, t.y, 1.9f, S_RED); b.a.dot(t.x - 0.6f, t.y - 0.6f, 0.7f, rgba(WHITE, 230));
        return t;
    };
    P top[6];   // emitter tops, in ring order (N, NE, SE, S, SW, NW)
    for (int i : {0, 1, 5}) top[i] = pylon(i);   // the far pylons stand behind the base

    // the octagonal base: concrete walls, a steel band on each front face, a plated roof with bolts and two vents
    std::vector<P> oct = regular(cx, cy, 31, 8, 0.3927f);
    prism(b, oct, 9, S_CONC, S_CONC_D);
    for (int i = 0; i < 3; i++) {   // the front faces (SE, S, SW): a steel armour band at mid height
        P p = oct[i], q = oct[i + 1];
        b.a.line(p.x, p.y - 4.5f, q.x, q.y - 4.5f, 1.1f, S_STEEL);
    }
    std::vector<P> ring = regular(cx, cy - 9, 24, 8, 0.3927f);
    b.a.poly(ring, shade(S_CONC, 0.9f), 1.2f, 0.35f);
    for (auto& p : ring) b.a.dot(p.x, p.y, 0.8f, shade(S_CONC, 1.35f));
    b.vent(26, 36, 8, 5, S_STEEL_L); b.vent(62, 36, 8, 5, S_STEEL_L);

    // the erector cradle on the roof, the warhead standing in it: white body with a red band, a nose cone and four fins
    const float rx = cx, ry = cy - 9;
    b.a.glow(rx, ry, 13, rgba(S_RED, 45));
    b.a.cap(rx - 4.4f, ry + 1, rx - 6.4f, ry - 14, 1.1f, S_STEEL, 0.8f, 0.4f);   // erector arms
    b.a.cap(rx + 4.4f, ry + 1, rx + 6.4f, ry - 14, 1.1f, S_STEEL, 0.8f, 0.4f);
    b.a.rect(rx - 6.0f, ry - 1.0f, 12.0f, 4.0f, 1.0f, S_STEEL_L, 1.0f, 0.5f);   // cradle saddle
    b.a.poly({{rx - 2.5f, ry - 8}, {rx - 6.2f, ry - 2}, {rx - 2.5f, ry - 3}}, S_STEEL, 0.6f, 0.2f);
    b.a.poly({{rx + 2.5f, ry - 8}, {rx + 6.2f, ry - 2}, {rx + 2.5f, ry - 3}}, S_STEEL, 0.6f, 0.2f);
    b.a.cap(rx, ry - 3, rx, ry - 27, 2.7f, S_MISSILE, 1.2f, 0.5f);
    b.a.cap(rx, ry - 19, rx, ry - 16, 2.85f, S_RED_D, 0.5f, 0.3f);   // red band
    b.a.poly({{rx - 2.7f, ry - 27}, {rx, ry - 37}, {rx + 2.7f, ry - 27}}, S_MISSILE, 0.8f, 0.5f);
    b.a.dot(rx, ry - 37.5f, 0.6f, S_RED);
    b.mark(31, 57, 14, 3); b.mark(51, 57, 14, 3);   // owner's colour bands on the roof's south edge

    for (int i : {2, 3, 4}) top[i] = pylon(i);   // the near pylons stand in front of the base

    // the laser cage: a bright core and a red glow along each edge of the hexagon of emitters
    for (int i = 0; i < 6; i++) {
        P p = top[i], q = top[(i + 1) % 6];
        b.stripe(p.x, p.y, q.x, q.y, 0.9f, S_RED, 130);
        b.a.line(p.x, p.y, q.x, q.y, 0.35f, rgba(S_CORE, 230));
    }
    b.a.grain(0, 0, (float)W, (float)H, 0.08f, 71u);
}

// 160 x 128: a concrete bunker complex dug into the rock. A long front casemate on a glacis with two blast doors and two roof gun pits, a
// main casemate behind it with a third door and a camouflage net over one roof, and a high command bunker at the back with an observation
// cupola and a radio mast. Tunnel mouths open on the west and east flanks, and the rock rises behind and beside it.
void fortress(Bld& b) {
    int W = b.W, H = b.H;
    // the rock: massifs behind the complex and on both flanks, boulders on their faces
    b.a.ell(80, -6, 92, 19, G_ROCK_D, 3, 0.6f);
    b.a.ell(78, -2, 80, 12, G_ROCK, 3, 0.6f);
    b.a.ell(-4, 60, 11, 56, G_ROCK_D, 3, 0.6f);
    b.a.ell(164, 60, 11, 56, G_ROCK_D, 3, 0.6f);
    for (int i = 0; i < 8; i++) boulder(b, 12 + i * 20 + b.rng.f(-3, 3), -17 + b.rng.f(-2, 2), b.rng.f(7, 10), G_ROCK);
    boulder(b, -3, 30, 9, G_ROCK_D); boulder(b, 163, 26, 8, G_ROCK); boulder(b, -2, 104, 10, G_ROCK); boulder(b, 164, 108, 9, G_ROCK_D);

    // packed earth and moss on the ground round the complex
    b.a.shadowRect(0, 0, (float)W, (float)H, 3, 3, 4, 85);
    b.a.rect(0, 0, (float)W, (float)H, 6, rgb(112, 104, 88), 2.6f, 0.5f);
    for (int i = 0; i < 12; i++) patch(b, b.rng.f(6, 154), b.rng.f(4, 124), b.rng.f(8, 16), b.rng.f(3, 6), rgb(128, 116, 92), 110);
    for (int i = 0; i < 9; i++) patch(b, b.rng.f(6, 154), b.rng.f(4, 124), b.rng.f(5, 10), b.rng.f(2, 4), FT_MOSS, 120);

    // tunnel mouths on the flanks: a concrete lip, a dark throat with a warm light on its lower edge, a hazard-striped side
    b.a.rect(-8, 58, 22, 22, 2, FT_SLAB_L, 1.4f, 0.5f);
    b.a.rect(-3, 63, 12, 12, 1.0f, G_SLOT, 1.2f, 0.1f);
    b.a.line(-2.5f, 74.6f, 8.5f, 74.6f, 0.7f, rgba(rgb(255, 178, 98), 170));
    b.hazard(-8, 58, 2.0f, 22);
    b.a.rect(146, 58, 22, 22, 2, FT_SLAB_L, 1.4f, 0.5f);
    b.a.rect(151, 63, 12, 12, 1.0f, G_SLOT, 1.2f, 0.1f);
    b.a.line(151.5f, 74.6f, 161.5f, 74.6f, 0.7f, rgba(rgb(255, 178, 98), 170));
    b.hazard(166, 58, 2.0f, 22);

    // the back row: the high command bunker, a heavy slab roof with earth heaped on its west end, a cupola, the radio mast and a flag
    b.block(30, 18, 100, 30, 30, FT_SLAB, FT_WALL, 5);
    formwork(b, 30, 18, 130, 48, 16);
    slit(b, 38, 23, 22); slit(b, 70, 23, 26); slit(b, 106, 23, 18);
    b.mark(74, 28, 16, 3);
    patch(b, 36, 4, 8, 12, rgb(116, 104, 82), 165);   // earth heaped against the west end of the roof
    patch(b, 36, -2, 6, 5, FT_MOSS, 150);
    patch(b, 120, 12, 9, 3.5f, FT_MOSS, 150);
    chip(b, 30, -12, 4);
    P cup = b.cyl(92, 8, 6, 9, FT_SLAB_L, FT_WALL);
    b.a.rect(cup.x - 4.6f, cup.y + 4.0f, 9.2f, 2.0f, 0.4f, rgb(36, 84, 100), 0.6f, 0.2f);   // observation slit
    b.antenna(64, 6, 26, rgb(255, 96, 80));
    periscope(b, 52, 12, 6); periscope(b, 124, 2, 5);
    ventStack(b, 112, 14, 3.4f, 4);
    flagpole(b, 40, 6, 16, 11, 7);

    // the main casemate: a heavy slab roof under earth and moss with a camouflage net, a stained wall with two slits and a blast door
    b.block(14, 50, 132, 26, 18, FT_SLAB, FT_WALL, 5);
    formwork(b, 14, 50, 146, 76, 18);
    stain(b, 22, 58, 3, 14); stain(b, 60, 58, 4, 9); stain(b, 118, 58, 3, 12);
    slit(b, 22, 63, 32); slit(b, 94, 63, 34);
    b.a.poly({{62, 76.6f}, {88, 76.6f}, {93, 84}, {57, 84}}, FT_SLAB_L, 1.0f, 0.3f);   // the ramp down to the door
    blastDoor(b, 66, 76.6f, 18, 12);
    ventStack(b, 26, 46, 3.6f, 6); ventStack(b, 62, 52, 4.2f, 7);
    patch(b, 22, 35, 9, 4, FT_MOSS, 150); patch(b, 138, 55, 9, 3, FT_MOSS, 140); patch(b, 68, 40, 11, 4, rgb(120, 108, 84), 130);
    periscope(b, 44, 40, 7);
    patch(b, 136, 44, 9, 10, rgb(116, 104, 82), 160);   // earth banked over the east end of the roof
    patch(b, 136, 38, 6, 4, FT_MOSS, 150);
    camoNet(b, 84, 36, 128, 53);
    chip(b, 140, 32, 4);

    // the front casemate: the long slab with slits, two blast doors on their ramps, a team strip and two roof gun pits
    b.block(4, 96, 152, 24, 14, FT_SLAB, FT_WALL, 5);
    formwork(b, 4, 96, 156, 120, 16);
    slit(b, 6, 110, 10); slit(b, 52, 110, 28); slit(b, 86, 110, 22); slit(b, 140, 110, 12);
    stain(b, 62, 106, 3, 10); stain(b, 100, 106, 4, 9); stain(b, 146, 106, 3, 12);
    blastDoor(b, 22, 120.6f, 20, 11); blastDoor(b, 114, 120.6f, 20, 11);
    b.mark(60, 115.5f, 34, 3);
    gunPit(b, 36, 92, 7); gunPit(b, 124, 92, 7);
    ventStack(b, 80, 98, 3.0f, 4);
    patch(b, 12, 88, 7, 3, FT_MOSS, 150); patch(b, 150, 102, 7, 3, FT_MOSS, 150);
    chip(b, 4, 82, 4);

    // the glacis: a sloped concrete apron in front of the slab, the door ramps running down to it, boulders and rubble at its edge
    b.a.poly({{0, 120.6f}, {160, 120.6f}, {164, 124.5f}, {-4, 124.5f}}, rgb(170, 168, 160), 1.2f, 0.4f);
    b.a.poly({{-4, 124.5f}, {164, 124.5f}, {168, 131}, {-8, 131}}, rgb(140, 138, 130), 1.2f, 0.4f);
    b.a.line(0, 120.8f, 160, 120.8f, 1.0f, rgba(WHITE, 70));
    b.a.poly({{14, 120.6f}, {46, 120.6f}, {50, 130}, {10, 130}}, FT_SLAB_L, 1.0f, 0.3f);
    b.a.poly({{108, 120.6f}, {140, 120.6f}, {144, 130}, {104, 130}}, FT_SLAB_L, 1.0f, 0.3f);
    boulder(b, 4, 127, 5, G_ROCK_D); boulder(b, 152, 127, 6, G_ROCK); boulder(b, 86, 129, 3.5f, G_ROCK);
    b.a.grain(0, 0, (float)W, (float)H, 0.08f, 83u);
}

// 32 x 32: a massive armoured block. A dark top with one hex armour plate per tile (so a row of segments repeats evenly), a steel bolt in
// each plate and a chamfered highlight along the top. Cyber: a faint cyan seam of light along the face. Clanker: yellow-black chevrons
// down the face and rust streaks on the top. No guns and no turrets.
void bastionWall(Bld& b) {
    int W = b.W, H = b.H;
    bool cy = b.cy;
    Color top = cy ? rgb(66, 72, 84) : rgb(82, 78, 68), plate = cy ? rgb(92, 100, 114) : rgb(104, 100, 88), face = cy ? rgb(36, 40, 48) : rgb(44, 40, 34);
    Roof r = b.block(0, 0, (float)W, (float)H, 12, top, face, 0.8f);
    b.a.ngon(16, 4, 11.6f, 6, 0, rgb(16, 18, 22), 1.0f, 0);   // the dark gap round the plate
    b.a.ngon(16, 4, 10.2f, 6, 0, plate, 1.5f, 0.8f);
    for (int i = 0; i < 6; i++) { float an = i * 1.0472f; b.a.dot(16 + std::cos(an) * 8.4f, 4 + std::sin(an) * 8.4f, 0.7f, shade(plate, 1.7f)); }
    b.a.dot(16, 4, 1.4f, shade(plate, 1.35f));
    b.a.line(0, r.y + 1.2f, (float)W, r.y + 1.2f, 0.8f, rgba(WHITE, 40));   // chamfer highlight along the top edge
    b.mark(12, r.y + 2.0f, 8, 2.4f);                                           // owner's colour strip on the top
    if (cy) {
        b.a.line(0, 26, (float)W, 26, 2.2f, rgba(C_CYAN, 34));                 // faint seam of light along the face
        b.a.line(0, 26, (float)W, 26, 0.5f, rgba(C_CYAN, 130));
    } else {
        for (int k = 0; k < 4; k++) {                                          // yellow-black chevrons, one period per 8 px so tiles join
            float x = k * 8.0f;
            b.a.poly({{x, 32.6f}, {x + 4, 32.6f}, {x + 8, 20.6f}, {x + 4, 20.6f}}, K_YEL, 0.8f, 0.2f);
        }
        b.rust(2, 14, 30, 19, 5, 110);
    }
    b.a.grain(0, 0, (float)W, (float)H, 0.1f, 97u);
}

// The Waymaker on its patch: survey stakes and hazard tape round the 3x3 footprint, and in the middle a compact tracked rock grinder (a
// toothed drum on two arms at the front, the engine deck behind it, a beacon) that chews the ground flat. No pad: it stands on the rock it clears.
void waymaker(Bld& b) {
    Art& a = b.a;
    int W = b.W, H = b.H;
    bool cy = b.cy;
    const Color body = cy ? rgb(206, 196, 120) : rgb(226, 178, 52), bodyD = cy ? rgb(132, 126, 80) : rgb(150, 112, 30);
    const Color steel = rgb(84, 86, 90), dark = rgb(30, 30, 32), tape = rgb(244, 206, 60);
    // the tape between the corner stakes, yellow and black, sagging a little
    const float in = 5;
    P cn[4] = { {in, in}, {W - in, in}, {W - in, H - in}, {in, H - in} };
    for (int s = 0; s < 4; s++) {
        P p = cn[s], q = cn[(s + 1) % 4];
        float L = hyp(q.x - p.x, q.y - p.y);
        int n = (int)(L / 4.0f);
        for (int k = 0; k < n; k++) {
            float t0 = k / (float)n, t1 = (k + 1) / (float)n, sag = 1.6f * std::sin(3.14159f * (t0 + t1) * 0.5f);
            a.line(p.x + (q.x - p.x) * t0, p.y + (q.y - p.y) * t0 + sag, p.x + (q.x - p.x) * t1, p.y + (q.y - p.y) * t1 + sag, 1.3f, (k & 1) ? dark : tape);
        }
    }
    for (auto& p : cn) {   // timber stakes with orange tops
        a.line(p.x + 1.2f, p.y + 1.6f, p.x + 3.0f, p.y + 2.6f, 1.4f, rgba(BLACK, 70));
        a.rect(p.x - 1.1f, p.y - 5.0f, 2.2f, 6.0f, 0.4f, rgb(150, 112, 70), 0.8f, 0.4f);
        a.rect(p.x - 1.2f, p.y - 6.0f, 2.4f, 2.0f, 0.4f, rgb(255, 120, 40), 0.8f, 0.4f);
    }
    // the grinder: tracks, engine deck, cab, the toothed drum out front (facing east)
    const float cx = W * 0.5f - 4, cyy = H * 0.5f + 2;
    a.shadowRect(cx - 18, cyy - 11, 40, 24, 4, 4, 4, 90);
    for (int s = -1; s <= 1; s += 2) {
        a.rect(cx - 18, cyy + s * 9.5f - 3.2f, 30, 6.4f, 1.6f, dark, 1.2f, 0.4f);
        for (float x = cx - 16.5f; x < cx + 11; x += 2.4f) a.line(x, cyy + s * 9.5f - 2.8f, x, cyy + s * 9.5f + 2.8f, 0.7f, rgb(70, 70, 66));
    }
    a.rect(cx - 16, cyy - 7.5f, 25, 15, 1.6f, body, 2.0f, 0.55f);
    a.rect(cx - 15, cyy - 6.5f, 9, 13, 1.0f, bodyD, 1.0f, 0.3f);                       // engine deck
    for (float y = cyy - 5; y < cyy + 5.5f; y += 2.0f) a.line(cx - 14, y, cx - 7, y, 0.6f, rgba(dark, 170));
    a.rect(cx - 4, cyy - 5, 8, 10, 1.4f, shade(body, 0.85f), 1.4f, 0.5f);           // cab
    a.rect(cx - 2.8f, cyy - 4, 5.6f, 3.2f, 0.6f, rgb(60, 120, 150), 0.8f, 0.4f);      // cab glass
    a.cap(cx + 8, cyy - 7, cx + 15, cyy - 7, 1.3f, steel, 1.0f, 0.5f);                 // drum arms
    a.cap(cx + 8, cyy + 7, cx + 15, cyy + 7, 1.3f, steel, 1.0f, 0.5f);
    a.rect(cx + 13, cyy - 10, 6.5f, 20, 3.0f, rgb(110, 110, 112), 1.8f, 0.6f);         // the drum
    for (float y = cyy - 9; y < cyy + 9.5f; y += 2.2f) a.dot(cx + 19.6f, y, 0.9f, rgb(190, 190, 186));   // its teeth
    for (float y = cyy - 8; y < cyy + 8.5f; y += 4.4f) a.line(cx + 13.6f, y, cx + 19, y + 1.4f, 0.6f, rgba(dark, 150));
    b.light(cx - 1, cyy - 8.6f, 1.0f, rgb(255, 170, 40), 110);                         // amber beacon on the cab roof
    b.mark(cx - 15, cyy + 4.4f, 6, 2.0f);                                              // the owner's strip on the deck
    // grit thrown up by the drum
    for (int k = 0; k < 14; k++) { float an = -1.2f + k * 0.17f, r = 6 + (k * 37 % 9); a.dot(cx + 21 + std::cos(an) * r, cyy + std::sin(an) * r, 0.7f + (k % 3) * 0.3f, rgba(rgb(150, 136, 112), 200)); }
}

}  // namespace

bool artSpecialBuilding(Canvas& c, Canvas& mask, int type) {
    const BuildType& bt = BUILDS[type];
    if (bt.role == BR_HEALANT) return artHealAntenna(c, mask, bt.faction == F_CYBER, 9100u + (u32)type * 29u);
    if (bt.role != BR_ISLANDNUKE && bt.role != BR_FWALL && bt.role != BR_WAYMAKER && type != B_C_FORTRESS) return false;
    Bld b(c, mask, bt.w * TILE, bt.h * TILE, bt.faction == F_CYBER, 9100u + (u32)type * 29u);
    if (bt.role == BR_ISLANDNUKE) islandLauncher(b);
    else if (bt.role == BR_FWALL) bastionWall(b);
    else if (bt.role == BR_WAYMAKER) waymaker(b);
    else fortress(b);
    c.outline(rgb(8, 10, 14, 150));
    return true;
}
