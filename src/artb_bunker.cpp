#include "bld.h"

// The underground bunker of either army: a buried command shelter under a low earth-and-concrete berm, drawn in the same raised 3/4 view as
// the other structures (light from the north-west, shadows to the south-east). The berm is three sloped solids stacked on one another: an
// armour plinth at the foot, the earth glacis and the concrete revetment up to the deck. Every visible face is a convex quad, so each one
// takes its own light and reads as a facet. On the deck: a round blast hatch in a steel collar (the renderer draws the leaves), recessed gun
// bays, flush overdrive hatches, and the plain pads the upgrades sit on. A vehicle ramp runs down the south-east glacis to a blast door.
// Cyber is pale ceramic hex plating with cyan light strips; Clanker is olive steel, khaki earth, riveted concrete, rust and hazard paint.
// The team colour goes only on the flag and the door's rail.
//
// Reserved spots (px from the apron centre, data.h): the hatch well r 22, the gun bays (+-28,0), the overdrive hatches (-28,-26) and
// (+28,+26), the laser pads (+28,-26) and (-28,+26), the hospital pad (0,-36), the triple tubes (-13..13,+36), the guidance pad (-40,0) and
// the radio mast foot (+40,0). The base keeps them free of anything that would show through an upgrade.
namespace {

const float CX = 48, CY = 48;   // the footprint centre in design px
const float CHAMF = 0.5858f;    // how much a chamfer grows per px of outward offset of a chamfered square (2 - sqrt 2)

// a chamfered square of half-size h round (cx, cy), its corners cut by c (vertex order: top edge first, clockwise on screen)
std::vector<P> oct(float cx, float cy, float h, float c) {
    return {{cx - h + c, cy - h}, {cx + h - c, cy - h}, {cx + h, cy - h + c}, {cx + h, cy + h - c},
            {cx + h - c, cy + h}, {cx - h + c, cy + h}, {cx - h, cy + h - c}, {cx - h, cy - h + c}};
}

P lerpP(P p, P q, float t) { return P{p.x + (q.x - p.x) * t, p.y + (q.y - p.y) * t}; }

float polyArea(const std::vector<P>& v) {
    float area = 0;
    for (size_t i = 0; i < v.size(); i++) { const P& p = v[i]; const P& q = v[(i + 1) % v.size()]; area += p.x * q.y - q.x * p.y; }
    return area;
}

// soft shadow of a convex plan polygon, moved by (dx, dy) (the same falloff as Art::shadowRect)
void softPoly(Bld& b, std::vector<P> v, float dx, float dy, float feather, int alpha) {
    Art& a = b.a;
    float mnx = 1e9f, mny = 1e9f, mxx = -1e9f, mxy = -1e9f;
    for (auto& p : v) { p.x = a.X(p.x + dx); p.y = a.Y(p.y + dy); mnx = std::min(mnx, p.x); mny = std::min(mny, p.y); mxx = std::max(mxx, p.x); mxy = std::max(mxy, p.y); }
    float sgn = polyArea(v) > 0 ? 1.0f : -1.0f, f = feather * a.k;
    a.soft((int)(mnx - f - 2), (int)(mny - f - 2), (int)(mxx + f + 2), (int)(mxy + f + 2), [v, sgn](float x, float y) { return sdPoly(v, x, y, sgn); }, Color{0, 0, 0, (u8)alpha}, f);
}

// soft shadow of a ring of outline O (moved by (dx, dy)) that falls only outside the inner outline I, so it never darkens what the ring encloses
void softRing(Bld& b, const std::vector<P>& O, const std::vector<P>& I, float dx, float dy, float feather, int alpha) {
    Art& a = b.a;
    std::vector<P> o = O, in = I;
    float mnx = 1e9f, mny = 1e9f, mxx = -1e9f, mxy = -1e9f;
    for (auto& p : o) { p.x = a.X(p.x + dx); p.y = a.Y(p.y + dy); mnx = std::min(mnx, p.x); mny = std::min(mny, p.y); mxx = std::max(mxx, p.x); mxy = std::max(mxy, p.y); }
    for (auto& p : in) { p.x = a.X(p.x); p.y = a.Y(p.y); }
    float so = polyArea(o) > 0 ? 1.0f : -1.0f, si = polyArea(in) > 0 ? 1.0f : -1.0f, f = feather * a.k;
    a.soft((int)(mnx - f - 2), (int)(mny - f - 2), (int)(mxx + f + 2), (int)(mxy + f + 2),
           [o, in, so, si, f](float x, float y) { return std::max(sdPoly(o, x, y, so), f - sdPoly(in, x, y, si)); }, Color{0, 0, 0, (u8)alpha}, f);
}

// a convex block standing on a plan polygon and lifted by ht (the lift of Bld::block): a soft shadow to the south-east, the faces that
// look south as walls, the top face as the roof
void volume(Bld& b, const std::vector<P>& plan, float ht, Color roof, Color wall) {
    Art& a = b.a;
    softPoly(b, plan, ht * 0.55f + 1, ht * 0.4f + 1, 1.5f + ht * 0.2f, 100);
    size_t n = plan.size();
    float cx = 0, cy = 0;
    for (auto& p : plan) { cx += p.x; cy += p.y; }
    cx /= n; cy /= n;
    for (size_t i = 0; i < n; i++) {
        P p = plan[i], q = plan[(i + 1) % n];
        float ex = q.x - p.x, ey = q.y - p.y, l = hyp(ex, ey);
        float nx = ey / l, ny = -ex / l;
        if (nx * ((p.x + q.x) * 0.5f - cx) + ny * ((p.y + q.y) * 0.5f - cy) < 0) { nx = -nx; ny = -ny; }
        if (ny < 0.02f) continue;
        float lit = -(nx * 0.62f + ny * 0.78f);
        a.poly({p, q, {q.x, q.y - ht}, {p.x, p.y - ht}}, shade(wall, 1.0f + 0.16f * lit), 1, 0);
        a.line(p.x, p.y, q.x, q.y, 1.0f, rgba(BLACK, 110));
    }
    std::vector<P> top;
    for (auto& p : plan) top.push_back({p.x, p.y - ht});
    a.poly(top, roof, 1.6f, 0.58f);
}

// a sloped solid: its top outline is the chamfered square (cx, cy, h, c) as it reads on screen; its foot sits ht lower on screen and spreads s
// further out on every side. Each top vertex joins the foot vertex with the same index, so every face is a convex quad
struct Solid { std::vector<P> top, base; };
Solid slope(float cx, float cy, float h, float c, float ht, float s) { return Solid{oct(cx, cy, h, c), oct(cx, cy + ht, h + s, c + CHAMF * s)}; }

// paints the visible faces of a solid (the ones that look south or sideways; the back faces hide behind the top), each face a lit quad with
// joints along it; the top face too when asked. The foot's cast shadow falls to the south-east first
void solid(Bld& b, const Solid& s, Color faceC, Color topC, bool topFace) {
    Art& a = b.a;
    softPoly(b, s.base, 2.2f, 2.2f, 3.0f, 95);
    size_t n = s.base.size();
    float cx = 0, cy = 0;
    for (auto& p : s.base) { cx += p.x; cy += p.y; }
    cx /= n; cy /= n;
    for (size_t i = 0; i < n; i++) {
        P B0 = s.base[i], B1 = s.base[(i + 1) % n], T0 = s.top[i], T1 = s.top[(i + 1) % n];
        float ex = B1.x - B0.x, ey = B1.y - B0.y, l = hyp(ex, ey);
        float nx = ey / l, ny = -ex / l;
        if (nx * ((B0.x + B1.x) * 0.5f - cx) + ny * ((B0.y + B1.y) * 0.5f - cy) < 0) { nx = -nx; ny = -ny; }
        if (ny < -0.02f) continue;
        float lit = -(nx * 0.62f + ny * 0.78f);
        a.poly({B0, B1, T1, T0}, shade(faceC, 1.0f + 0.18f * lit), 0.8f, 0);
        a.line(B0.x, B0.y, B1.x, B1.y, 1.0f, rgba(BLACK, 120));
        int joints = std::max(1, (int)(l / 13.0f));
        for (int k = 1; k < joints; k++) { float t = (float)k / joints; P T = lerpP(T0, T1, t), B = lerpP(B0, B1, t); a.line(T.x, T.y, B.x, B.y, 0.5f, rgba(BLACK, 95)); }
        a.line(T0.x, T0.y, B0.x, B0.y, 0.5f, rgba(BLACK, 70));   // the corner where this face meets the next
    }
    if (topFace) a.poly(s.top, topC, 1.6f, 0.58f);
}

// a region in canvas px: the design outline and its sign, for the clip tests of the details painted onto a face
struct Region {
    std::vector<P> v; float sgn;
    Region(Art& a, const std::vector<P>& design) {
        for (auto& p : design) v.push_back({a.X(p.x), a.Y(p.y)});
        sgn = polyArea(v) > 0 ? 1.0f : -1.0f;
    }
    float d(float x, float y) const { return sdPoly(v, x, y, sgn); }   // negative inside
};

// flat-top hexagon distance (canvas px): a hexagon of circumradius R centred at (cx, cy)
float sdHex(float px, float py, float cx, float cy, float R) {
    float x = std::abs(px - cx), y = std::abs(py - cy);
    return std::max(y, 0.8660254f * x + 0.5f * y) - 0.8660254f * R;
}

// a field of flat-top hex plates of design circumradius r, clipped to the canvas region 'clip' (negative inside). Each plate is shaded on
// its own (a little lighter or darker), and the seam between two plates shows the colour the face was painted with underneath
template <class F> void hexField(Bld& b, float x0, float y0, float x1, float y1, float r, F clip, Color base) {
    Art& a = b.a;
    float colW = 1.5f * r, rowH = 1.7320508f * r;
    int i0 = (int)std::floor((x0 - r) / colW), i1 = (int)std::ceil((x1 + r) / colW);
    for (int i = i0; i <= i1; i++) {
        float cx = i * colW, off = (i & 1) ? rowH * 0.5f : 0.0f;
        int j0 = (int)std::floor((y0 - r - off) / rowH), j1 = (int)std::ceil((y1 + r - off) / rowH);
        for (int j = j0; j <= j1; j++) {
            float cy = j * rowH + off;
            if (cx < x0 - r || cx > x1 + r || cy < y0 - r || cy > y1 + r) continue;
            float px = a.X(cx), py = a.Y(cy), R = (r - 0.45f) * a.k, ext = R + 2;
            Color col = shade(base, b.rng.f(0.94f, 1.06f));
            a.shape((int)(px - ext), (int)(py - ext), (int)(px + ext), (int)(py + ext),
                    [=](float x, float y) { return std::max(sdHex(x, y, px, py, R), clip(x, y)); }, col, 0.9f * a.k, 0.5f, 1.0f);
        }
    }
}

// a soft blob (earth, moss, rust) clipped to a region
template <class F> void blob(Bld& b, float cx, float cy, float r, F clip, Color c, int alpha) {
    Art& a = b.a;
    float px = a.X(cx), py = a.Y(cy), R = r * a.k;
    a.shape((int)(px - R - 2), (int)(py - R - 2), (int)(px + R + 2), (int)(py + R + 2),
            [=](float x, float y) { return std::max(hyp(x - px, y - py) - R, clip(x, y)); }, rgba(c, alpha), 1.0f, 0.0f, 1.0f);
}

// a sector of an annulus round (cx, cy) between radii r0 and r1, from angle a0 to a1 (radians; 0 = east, pi/2 = south); keep the spans short
void sectorAt(Bld& b, float cx, float cy, float r0, float r1, float a0, float a1, Color c, float bevel, float kk) {
    auto at = [&](float r, float t) { return P{cx + std::cos(t) * r, cy + std::sin(t) * r}; };
    b.a.poly({at(r0, a0), at(r1, a0), at(r1, a1), at(r0, a1)}, c, bevel, kk);
}

// a stencil on a face: seven-segment digits 2.4 x 4.4 px, the segment map a..g, strokes of width w
void stencil(Bld& b, float x, float y, const char* digits, Color c, float w) {
    static const char* seg[10] = {"abcdef", "bc", "abged", "abgcd", "fgbc", "afgcd", "afgedc", "abc", "abcdefg", "abcdfg"};
    const float W = 2.4f, H = 4.4f;
    int pos = 0;
    for (const char* d = digits; *d; d++, pos++) {
        if (*d < '0' || *d > '9') continue;
        float x0 = x + pos * (W + 1.6f), x1 = x0 + W, y0 = y, ym = y + H * 0.5f, y2 = y + H;
        for (const char* s = seg[*d - '0']; *s; s++) {
            P p, q;
            switch (*s) {
            case 'a': p = {x0, y0}; q = {x1, y0}; break;
            case 'b': p = {x1, y0}; q = {x1, ym}; break;
            case 'c': p = {x1, ym}; q = {x1, y2}; break;
            case 'd': p = {x0, y2}; q = {x1, y2}; break;
            case 'e': p = {x0, ym}; q = {x0, y2}; break;
            case 'f': p = {x0, y0}; q = {x0, ym}; break;
            default: p = {x0, ym}; q = {x1, ym}; break;
            }
            b.a.line(p.x, p.y, q.x, q.y, w, c);
        }
    }
}

// a camouflage net draped over a patch of deck: an olive sheet (a convex quad with a slightly ragged hem), mottled with dark and light blotches
// and a few sagging creases
void camoNet(Bld& b, float x0, float y0, float x1, float y1) {
    Art& a = b.a;
    std::vector<P> v = {{x0, y0 + b.rng.f(-0.5f, 0.5f)}, {x1, y0 + b.rng.f(-0.5f, 0.5f)}, {x1 + b.rng.f(-0.4f, 0.4f), y1 + b.rng.f(0.2f, 1.0f)}, {x0, y1 + b.rng.f(0.2f, 1.0f)}};
    a.poly(v, rgb(92, 104, 66), 1.0f, 0.3f, 0.94f);
    for (int i = 0; i < 9; i++) {
        float px = b.rng.f(x0 + 3, x1 - 3), py = b.rng.f(y0 + 2, y1 - 2);
        a.ell(px, py, b.rng.f(1.6f, 3.2f), b.rng.f(1.0f, 1.8f), rgba(i & 1 ? rgb(56, 66, 40) : rgb(126, 128, 92), 150), 0.8f, 0.2f);
    }
    for (float x = x0 + 6; x < x1 - 2; x += 7.0f) a.line(x, y0 + 0.8f, x - 1.6f, y1 - 0.8f, 0.5f, rgba(BLACK, 70));
}

// a ventilation / air-scrubber stack standing on the ground at (x, by): a steel cylinder with banding, a louvred cap and a filter grille
void stack(Bld& b, float x, float by, float r, float ht) {
    Art& a = b.a;
    bool cy = b.cy;
    Color top = cy ? rgb(190, 202, 214) : rgb(176, 166, 132), side = cy ? rgb(122, 136, 154) : rgb(134, 124, 96);
    P t = b.cyl(x, by, r, ht, top, side);
    for (float f : {0.3f, 0.62f}) a.line(x - r * 0.9f, by - ht * f, x + r * 0.9f, by - ht * f, 0.6f, rgba(BLACK, 90));
    a.ring(t.x, t.y, r * 0.6f, 0.6f, shade(top, 0.6f), 0.4f, 0.2f);
    a.dot(t.x, t.y, r * 0.22f, rgb(20, 22, 26));
    if (cy) b.light(x, by - ht * 0.62f, 0.7f, C_CYAN, 140);
    else { a.circle(x, by - ht * 0.3f, r * 0.42f, K_YEL, 0.3f, 0.2f); a.dot(x, by - ht * 0.3f, r * 0.12f, rgb(28, 26, 22)); }
}

// a floodlight on a mast standing at (x, y) and lifted ht: a pole, a lamp head and the pool of light it throws
void floodlight(Bld& b, float x, float y, float ht) {
    Art& a = b.a;
    Color warm = b.cy ? rgb(228, 252, 255) : rgb(255, 232, 170);
    a.line(x + 1.0f, y + 0.8f, x + 1.0f, y - ht + 0.8f, 1.0f, rgba(BLACK, 60));
    a.cap(x, y, x, y - ht, 0.55f, rgb(150, 156, 166), 0.6f, 0.4f);
    a.rect(x - 2.4f, y - ht - 1.6f, 4.8f, 3.0f, 0.5f, rgb(70, 76, 86), 0.8f, 0.4f);
    a.glow(x, y - ht + 0.4f, 9.0f, rgba(warm, 50));
    a.dot(x - 1.4f, y - ht - 0.1f, 1.0f, warm);
}

// a team flag on a pole (the cloth is dark; its team mask makes it the owner's colour, as the civic flags do)
void flag(Bld& b, float x, float y, float ht) {
    Art& a = b.a;
    a.cap(x, y, x, y - ht, 0.5f, rgb(206, 210, 216), 0.5f, 0.4f);
    float ty = y - ht + 0.6f;
    std::vector<P> cloth = {{x, ty}, {x + 6.5f, ty + 0.8f}, {x + 6.5f, ty + 5.6f}, {x, ty + 4.8f}};
    a.poly(cloth, rgb(22, 24, 30), 0.6f, 0.2f);
    b.t.poly(cloth, WHITE, 1.0f, 0.4f);
}

// a sliding blast door across the lane mouth, from the base points l to r and h px tall: two steel leaves with a centre seam, a frame
// (hazard-striped on Clanker, cyan on Cyber) and the team rail along its top
void blastDoor(Bld& b, P l, P r, float h) {
    Art& a = b.a;
    Color steel = b.cy ? rgb(116, 130, 150) : rgb(104, 106, 92);
    P m{(l.x + r.x) * 0.5f, (l.y + r.y) * 0.5f};
    auto up = [&](P p, float d) { return P{p.x, p.y - d}; };
    a.poly({{l.x - 1.0f, l.y + 0.8f}, {r.x + 1.0f, r.y + 0.8f}, up(r, h + 1.2f), up(l, h + 1.2f)}, rgb(30, 34, 40), 0.6f, 0.2f);
    if (!b.cy) a.line(up(l, h + 0.6f).x, up(l, h + 0.6f).y, up(r, h + 0.6f).x, up(r, h + 0.6f).y, 1.2f, K_YEL);
    else a.line(l.x, l.y + 0.6f, r.x, r.y + 0.6f, 0.6f, C_CYAN);
    a.poly({l, m, up(m, h), up(l, h)}, steel, 1.0f, 0.5f);
    a.poly({m, r, up(r, h), up(m, h)}, steel, 1.0f, 0.5f);
    a.line(up(l, h * 0.55f).x, up(l, h * 0.55f).y, up(m, h * 0.55f).x, up(m, h * 0.55f).y, 0.5f, rgba(BLACK, 110));   // the stiffener ribs
    a.line(up(m, h * 0.55f).x, up(m, h * 0.55f).y, up(r, h * 0.55f).x, up(r, h * 0.55f).y, 0.5f, rgba(BLACK, 110));
    a.line(m.x, m.y + 0.2f, m.x, m.y - h, 0.6f, rgba(BLACK, 160));   // the centre seam
    a.dot(m.x - 1.4f, m.y - h * 0.3f, 0.5f, rgb(170, 176, 184));
    a.dot(m.x + 1.4f, m.y - h * 0.3f, 0.5f, rgb(170, 176, 184));
    std::vector<P> seat = {up(l, h + 0.2f), up(r, h + 0.2f), up(r, h - 1.2f), up(l, h - 1.2f)};
    a.poly(seat, rgb(20, 22, 28), 0.5f, 0.2f);
    b.t.poly({up(lerpP(l, r, 0.1f), h - 0.2f), up(lerpP(l, r, 0.9f), h - 0.2f), up(lerpP(l, r, 0.9f), h - 1.0f), up(lerpP(l, r, 0.1f), h - 1.0f)}, WHITE, 0.4f, 0.0f);
}

// ---------------------------------------------------------------- the base apron
void apron(Bld& b) {
    Art& a = b.a;
    const bool cy = b.cy;
    const Color ink = rgb(8, 10, 14);
    const Color deck = cy ? rgb(224, 231, 238) : rgb(124, 128, 96);
    const Color deckSeam = cy ? rgb(94, 106, 124) : rgb(66, 70, 50);
    const Color revet = cy ? rgb(56, 66, 82) : rgb(156, 144, 114);        // Cyber: the dark seam under the ceramic plates; Clanker: poured concrete
    const Color revetTile = rgb(206, 218, 230);
    const Color glacis = cy ? rgb(70, 82, 100) : rgb(116, 102, 72);       // Cyber: the seam under the pale plates; Clanker: earth
    const Color glacisTile = rgb(150, 166, 184);
    const Color steel = cy ? rgb(98, 112, 132) : rgb(108, 110, 96);
    const Color bay = cy ? rgb(22, 28, 38) : rgb(36, 34, 26);
    const Color hz1 = K_YEL, hz2 = rgb(36, 36, 32);
    const Color lampC = cy ? C_CYAN : K_RED;
    const Color vent = cy ? rgb(150, 166, 184) : rgb(150, 140, 110);

    // the three solids of the berm: the armour plinth at the foot, the glacis, and the revetment up to the deck (its top centred on the hatch)
    const Solid S = slope(CX, 58, 44, 11.7f, 2.5f, 1.2f);
    const Solid G = slope(CX, 54, 38, 8.17f, 4, 6);
    const Solid R = slope(CX, CY, 36, 7, 6, 2);
    const Region rG(a, G.base), rR(a, R.base), rRt(a, R.top);
    auto glacisRing = [=](float x, float y) { return std::max(rG.d(x, y), -rR.d(x, y)); };   // the glacis face: inside its foot, outside the revetment
    auto revetRing = [=](float x, float y) { return std::max(rR.d(x, y), -rRt.d(x, y)); };   // the revetment face: inside its foot, outside the deck

    // ground layer first: the tank traps on the west, the sandbag line on the south-west
    for (float y : {58.0f, 68.0f, 78.0f}) volume(b, {{-6.0f, y}, {-1.0f, y}, {-1.0f, y + 6.0f}, {-6.0f, y + 6.0f}}, 4.0f, cy ? rgb(176, 186, 198) : rgb(164, 152, 116), cy ? rgb(116, 128, 144) : rgb(120, 110, 84));
    b.sandbags(0.0f, 92.0f, 14.0f, 107.0f, cy ? 1 : 2);

    // the berm, bottom up: the plinth, the glacis, its hex plates (Cyber), the revetment, its hex plates (Cyber) and the deck
    solid(b, S, steel, steel, false);
    solid(b, G, glacis, glacis, false);
    if (cy) hexField(b, 0, 40, 96, 106, 3.4f, glacisRing, glacisTile);
    solid(b, R, revet, deck, true);
    a.ramp(0, 84, 96, 106, 1.08f, 0.72f);   // the slopes fall off into shade toward the ground: the mound's body reads as curved earth
    if (cy) hexField(b, 0, 10, 96, 96, 3.0f, revetRing, revetTile);

    // the deck: panel seams, a light rim (Cyber) or riveted rim (Clanker), hazard paint on the north edge (Clanker)
    for (float x : {30.0f, 66.0f}) { a.line(x, 13.0f, x, 83.0f, 0.7f, rgba(deckSeam, 170)); a.line(x + 0.8f, 13.0f, x + 0.8f, 83.0f, 0.5f, rgba(WHITE, 40)); }
    for (float y : {30.0f, 66.0f}) { a.line(13.0f, y, 83.0f, y, 0.7f, rgba(deckSeam, 170)); a.line(13.0f, y + 0.8f, 83.0f, y + 0.8f, 0.5f, rgba(WHITE, 40)); }
    if (cy) {
        a.line(26.0f, 14.6f, 70.0f, 14.6f, 0.7f, rgba(C_CYAN, 210));
        a.line(26.0f, 81.4f, 70.0f, 81.4f, 0.7f, rgba(C_CYAN, 210));
        a.line(14.6f, 26.0f, 14.6f, 70.0f, 0.7f, rgba(C_CYAN, 210));
        a.line(81.4f, 26.0f, 81.4f, 70.0f, 0.7f, rgba(C_CYAN, 210));
    } else {
        for (float x = 26.0f; x <= 70.0f; x += 4.0f) { a.dot(x, 14.6f, 0.45f, rgb(40, 44, 30)); a.dot(x, 81.4f, 0.45f, rgb(40, 44, 30)); }
        for (float y = 26.0f; y <= 70.0f; y += 4.0f) { a.dot(14.6f, y, 0.45f, rgb(40, 44, 30)); a.dot(81.4f, y, 0.45f, rgb(40, 44, 30)); }
        b.hazard(22, 12.6f, 16, 1.4f, 3, hz1, hz2);
        b.hazard(58, 12.6f, 14, 1.4f, 3, hz1, hz2);
    }

    // the hatch collar: a raised armoured ring with bolts (Cyber: alternate cyan lamps); the leaves are drawn live over its well
    a.shadowCircle(CX, CY, 28, 1.6f, 1.8f, 2.4f, 110);
    a.circle(CX, CY, 27.5f, steel, 2.6f, 0.6f);
    for (int k = 0; k < 16; k++) {
        float t = k * 0.3927f, x = CX + std::cos(t) * 24.6f, y = CY + std::sin(t) * 24.6f;
        if (cy && (k & 1)) b.light(x, y, 0.6f, C_CYAN, 110);
        else a.dot(x, y, 0.8f, shade(steel, 1.5f));
    }
    a.circle(CX, CY, 21.5f, ink, 1.4f, 0.3f);
    a.ring(CX, CY, 21.3f, 0.5f, shade(steel, 0.4f), 0.5f, 0.2f);

    // recessed gun bays at (+-28, 0): a dark pocket in the deck edge with a bolted frame; the miniguns are drawn in the middle
    for (int s = -1; s <= 1; s += 2) {
        float bx = CX + s * BUNKER_GUN_X - 8, by = CY - 8;
        a.rect(bx, by, 16, 16, 1.0f, bay, 1.4f, 0.4f);
        a.rect(bx + 1.6f, by + 1.6f, 12.8f, 12.8f, 0.8f, shade(bay, 1.5f), 1.0f, 0.3f);
        for (float dx : {2.4f, 13.6f}) for (float dy : {2.4f, 13.6f}) a.dot(bx + dx, by + dy, 0.5f, shade(steel, 0.7f));
        if (cy) a.line(bx + 1.4f, by + 15.2f, bx + 14.6f, by + 15.2f, 0.7f, rgba(C_CYAN, 210));
        else b.hazard(bx - 0.4f, by - 2.6f, 16.8f, 1.2f, 3, hz1, hz2);
    }
    // flush overdrive hatches in the two free corners: a steel rim round the pop-up's pad
    const P od[2] = {{CX - BUNKER_OD_X, CY - BUNKER_OD_Y}, {CX + BUNKER_OD_X, CY + BUNKER_OD_Y}};
    for (const P& p : od) {
        a.circle(p.x, p.y, 7.0f, shade(steel, 0.8f), 1.0f, 0.5f);
        a.circle(p.x, p.y, 6.0f, shade(steel, 0.55f), 0.8f, 0.4f);
    }

    // vent grilles between the spots
    b.vent(13, 60, 6, 4.5f, vent);
    b.vent(58, 22.5f, 5.5f, 4, vent);
    if (cy) b.vent(78, 60, 6, 4.5f, vent);   // (Clanker lays its camouflage net over this spot)

    // the glacis: earth, grass and rust (Clanker), and the stencilled numbers on its south band
    if (!cy) {
        for (int i = 0; i < 16; i++) blob(b, b.rng.f(8, 88), b.rng.f(92, 103), b.rng.f(2.0f, 4.5f), glacisRing, rgb(88, 102, 56), 150);
        for (int i = 0; i < 12; i++) blob(b, b.rng.f(6, 90), b.rng.f(92, 103), b.rng.f(1.2f, 2.2f), glacisRing, rgb(150, 136, 98), 140);
        b.rust(20, 96, 76, 101, 8, 90);
        b.rust(60, 84, 84, 92, 4, 70);
    }
    stencil(b, 34, 93.6f, "07", cy ? C_CYAN : rgb(226, 220, 196), 0.6f);

    // the vehicle ramp: a lane running out of the south-east corner, from the blast door at the foot of the revetment chamfer down to the
    // ground. Tread bars across it, two tyre tracks that run on past its end, kerbs (cyan light on Cyber, hazard on Clanker) and a guide lamp
    // at each kerb
    const P T{82.0f, 86.0f}, dir{0.6f, 0.8f}, across{0.8f, -0.6f};
    const float half = 5.5f, len = 26.0f;
    auto at = [&](float along, float cross) { return P{T.x + dir.x * along + across.x * cross, T.y + dir.y * along + across.y * cross}; };
    a.poly({at(0, -half), at(0, half), at(len, half), at(len, -half)}, cy ? rgb(66, 78, 94) : rgb(120, 114, 92), 1.0f, 0.4f);
    for (float t = 3.4f; t < len - 1.0f; t += 3.4f) a.line(at(t, -half + 0.8f).x, at(t, -half + 0.8f).y, at(t, half - 0.8f).x, at(t, half - 0.8f).y, 0.5f, rgba(BLACK, 90));
    for (float c : {-2.2f, 2.2f}) {
        a.line(at(0.5f, c).x, at(0.5f, c).y, at(len + 7.0f, c).x, at(len + 7.0f, c).y, 1.1f, rgba(BLACK, 70));
        a.line(at(0.5f, c + 0.8f).x, at(0.5f, c + 0.8f).y, at(len + 7.0f, c + 0.8f).x, at(len + 7.0f, c + 0.8f).y, 0.4f, rgba(WHITE, 26));
    }
    for (float side : {-1.0f, 1.0f}) {
        if (cy) {
            a.line(at(0, side * (half - 0.5f)).x, at(0, side * (half - 0.5f)).y, at(len, side * (half - 0.5f)).x, at(len, side * (half - 0.5f)).y, 0.6f, rgba(C_CYAN, 230));
        } else {
            for (float t = 0; t < len; t += 2.6f) {
                P p0 = at(t, side * (half - 1.4f)), p1 = at(t + 1.3f, side * (half - 1.4f)), p2 = at(t + 1.3f, side * half), p3 = at(t, side * half);
                a.poly({p0, p1, p2, p3}, (int)(t / 2.6f) & 1 ? rgb(36, 36, 32) : hz1, 0.4f, 0.0f);
            }
        }
        P lamp = at(12.0f, side * (half + 1.6f));
        b.light(lamp.x, lamp.y, 0.6f, lampC, 110);
    }
    blastDoor(b, at(0, -half), at(0, half), 5.0f);

    // stacks on the ground either side of the bunker, a floodlight in each far corner, the team flag on the north edge
    stack(b, 2.0f, 12.0f, 4.0f, 22.0f);
    stack(b, 96.0f, 18.0f, 4.0f, 18.0f);
    floodlight(b, 110.0f, 40.0f, 30.0f);
    floodlight(b, -4.0f, 110.0f, 24.0f);
    flag(b, 74.0f, 13.0f, 22.0f);
    b.antenna(106.0f, 92.0f, 22.0f, b.cy ? C_CYAN : K_RED);   // a whip aerial with its warning light, south-east of the ramp
    // a periscope stub on the west glacis face: a steel stalk with a tinted head
    a.cap(7.0f, 87.0f, 7.0f, 79.0f, 0.6f, rgb(170, 178, 188), 0.6f, 0.3f);
    a.rect(5.4f, 76.4f, 3.2f, 2.4f, 0.4f, rgb(30, 34, 36), 0.6f, 0.3f);
    a.dot(6.2f, 77.2f, 0.5f, rgba(rgb(120, 180, 210), 220));

    // the conduit from the radio mast foot down the glacis to the lane's door
    b.pipe(88.0f, 50.0f, 90.0f, 60.0f, 0.7f, cy ? rgb(70, 80, 96) : rgb(84, 80, 62));
    b.pipe(90.0f, 60.0f, 88.0f, 74.0f, 0.7f, cy ? rgb(70, 80, 96) : rgb(84, 80, 62));

    // two access hatches with wheel handles in the free deck ground west and east of the collar
    for (float hx : {13.0f, 74.0f}) {
        a.rect(hx, 31.0f, 7.0f, 7.0f, 0.8f, shade(steel, 0.85f), 1.0f, 0.5f);
        a.rect(hx + 1.2f, 32.2f, 4.6f, 4.6f, 0.5f, shade(steel, 0.6f), 0.8f, 0.3f);
        a.circle(hx + 3.5f, 34.5f, 1.4f, cy ? C_CYAN : rgb(60, 62, 56), 0.5f, 0.3f);
    }
    if (!cy) camoNet(b, 66.0f, 56.0f, 84.0f, 64.0f);
    // the revetment: formwork joints on the concrete (Clanker), and the trim line where it meets the glacis
    if (!cy) for (float x = 22.0f; x < 74.0f; x += 9.0f) a.line(x, 84.6f, x, 91.4f, 0.5f, rgba(BLACK, 80));

    a.grain(0, 0, (float)b.W, (float)b.H, 0.08f, 93);
}

}  // namespace

bool artBunker(Canvas& c, Canvas& mask, int type) {
    const BuildType& bt = BUILDS[type];
    if (bt.role != BR_BUNKER) return false;
    Bld b(c, mask, bt.w * TILE, bt.h * TILE, bt.faction == F_CYBER, 9300u + (u32)type * 31u);
    apron(b);
    c.outline(rgb(8, 10, 14, 150));
    return true;
}

// ---------------------------------------------------------------- the per-bunker upgrades (each painted alone, drawn over the base)
namespace {

// super-thick shell: a ring of armour slabs round the perimeter (their south-east chamfer leaves a gap for the ramp), and a heavier
// segmented hatch collar from r 22.4 to 31, open where the miniguns' plates stand
void shellArmour(Bld& b) {
    Art& a = b.a;
    Color plate = b.cy ? rgb(128, 142, 160) : rgb(150, 146, 130);
    std::vector<P> O = oct(CX, CY, 47, 16), I = oct(CX, CY, 41, 10);
    softRing(b, O, I, 2.5f, 2.5f, 3, 105);
    for (int i = 0; i < 8; i++) {
        P o1 = O[i], o2 = O[(i + 1) % 8], i1 = I[i], i2 = I[(i + 1) % 8];
        std::vector<std::pair<float, float>> spans = {{0.0f, 1.0f}};
        if (i == 3) spans = {{0.0f, 0.22f}, {0.78f, 1.0f}};
        for (auto sp : spans) {
            float t0 = sp.first + 0.02f, t1 = sp.second - 0.02f;
            a.poly({lerpP(o1, o2, t0), lerpP(o1, o2, t1), lerpP(i1, i2, t1), lerpP(i1, i2, t0)}, shade(plate, (i & 1) ? 0.9f : 1.04f), 1.8f, 0.6f);
            for (float tr : {0.3f, 0.7f}) {
                float t = t0 + (t1 - t0) * tr;
                P R = lerpP(lerpP(o1, o2, t), lerpP(i1, i2, t), 0.4f);
                a.dot(R.x, R.y, 0.9f, shade(plate, 0.55f));
                a.dot(R.x - 0.3f, R.y - 0.3f, 0.35f, rgba(WHITE, 170));
            }
        }
    }
    for (int side = 0; side < 2; side++) {
        float a0 = side ? 3.4377f : 0.2967f, a1 = side ? 5.9865f : 2.8449f, st = (a1 - a0) / 10;   // 17..163 deg and 197..343 deg
        for (int k = 0; k < 10; k++) {
            float s0 = a0 + st * k + 0.012f, s1 = a0 + st * (k + 1) - 0.012f, mid = (s0 + s1) * 0.5f;
            sectorAt(b, CX, CY, 22.4f, 31.0f, s0, s1, shade(plate, (k & 1) ? 0.92f : 1.06f), 1.6f, 0.6f);
            a.dot(CX + std::cos(mid) * 26.7f, CY + std::sin(mid) * 26.7f, 0.8f, shade(plate, 0.5f));
        }
    }
}

// laser battery: two armoured turret bases at (76,22) and (20,74) with cooling fins, and power conduits running to the collar
void laserBattery(Bld& b) {
    Art& a = b.a;
    bool cy = b.cy;
    Color steel = cy ? rgb(122, 136, 154) : rgb(134, 130, 116), dark = cy ? rgb(40, 48, 62) : rgb(58, 54, 44);
    Color glow = cy ? C_CYAN : C_RED;
    const P tur[2] = {{76, 22}, {20, 74}};
    const P cab[3] = {{76, 30}, {74, 36}, {70, 39}};   // runs onto the collar's outer lip, clear of the well and the gun plate
    for (int s = 0; s < 2; s++) {
        auto m = [&](P p) { return s ? P{2 * CX - p.x, 2 * CY - p.y} : p; };   // the second set is the first turned half a turn
        P p0 = m(cab[0]), p1 = m(cab[1]), p2 = m(cab[2]), t = tur[s];
        b.pipe(p0.x, p0.y, p1.x, p1.y, 0.9f, dark);
        b.pipe(p1.x, p1.y, p2.x, p2.y, 0.9f, dark);
        a.line(p1.x, p1.y, p2.x, p2.y, 0.35f, rgba(glow, 150));
        a.shadowCircle(t.x, t.y, 8.0f, 1.6f, 1.8f, 2.2f, 110);
        a.circle(t.x, t.y, 8.0f, steel, 2.2f, 0.6f);
        for (int k = 0; k < 8; k++) {
            float an = k * 0.7854f;
            a.line(t.x + std::cos(an) * 6.0f, t.y + std::sin(an) * 6.0f, t.x + std::cos(an) * 7.6f, t.y + std::sin(an) * 7.6f, 1.0f, shade(steel, 0.55f));
        }
        a.circle(t.x, t.y, 4.6f, dark, 1.2f, 0.4f);   // the emitter well: the live barrels turn over it
        b.light(t.x - 5.4f, t.y - 5.4f, 0.7f, glow, 150);
    }
}

// field hospital: a raised white medical module at (48,12) with a green cross, a door, and a short mast with a green beacon
void fieldHospital(Bld& b) {
    Art& a = b.a;
    Color white = rgb(228, 236, 236), wallC = b.cy ? rgb(168, 184, 196) : rgb(184, 178, 158), green = rgb(40, 180, 92);
    Roof r = b.block(41, 11, 14, 8, 4, white, wallC, 1.0f);
    a.rect(r.x + 5.6f, r.y + 0.5f, 2.8f, r.h - 1.0f, 0.4f, green, 0.8f, 0.3f);
    a.rect(r.x + 3.5f, r.y + 2.6f, r.w - 7.0f, 2.8f, 0.4f, green, 0.8f, 0.3f);
    b.door(CX, 19.4f, 3.4f, 3.6f, rgb(70, 80, 90), rgb(28, 34, 42));
    b.antenna(CX, r.y, 10, rgb(96, 255, 140));
}

// triple launch: a raised steel deck along the south edge with three round silo lids (hazard rims) and radiation trefoils
void tripleLaunch(Bld& b) {
    Art& a = b.a;
    Color deck = b.cy ? rgb(118, 130, 146) : rgb(146, 140, 122), lid = b.cy ? rgb(150, 160, 172) : rgb(168, 162, 146);
    a.shadowRect(20, 76, 54, 15, 2.2f, 2.2f, 2.4f, 110);
    a.rect(20, 76, 54, 15, 1.2f, deck, 2.2f, 0.55f);
    for (float x = 28; x < 74; x += 8) a.line(x, 77.4f, x, 89.6f, 0.5f, rgba(BLACK, 110));
    for (float x : {24.5f, 70.5f}) {
        a.circle(x, 84, 2.6f, K_YEL, 0.8f, 0.25f);
        for (int k = 0; k < 3; k++) {
            float t = -1.5708f + k * 2.0944f;
            a.dot(x + std::cos(t) * 1.3f, 84 + std::sin(t) * 1.3f, 0.8f, rgb(28, 26, 22));
        }
        a.dot(x, 84, 0.45f, rgb(28, 26, 22));
    }
    for (float x : {35.0f, 48.0f, 61.0f}) {
        a.shadowCircle(x, 84, 6.2f, 1.4f, 1.4f, 1.8f, 110);
        for (int k = 0; k < 8; k++) sectorAt(b, x, 84, 4.9f, 6.4f, k * 0.7854f, (k + 1) * 0.7854f - 0.05f, (k & 1) ? rgb(28, 26, 22) : K_YEL, 0.6f, 0.2f);
        a.circle(x, 84, 4.9f, lid, 1.4f, 0.6f);
        a.ring(x, 84, 3.0f, 0.6f, shade(lid, 0.6f), 0.5f, 0.2f);
        a.dot(x, 84, 0.8f, rgb(40, 40, 40));
    }
    b.light(21.5f, 77.6f, 0.8f, b.cy ? C_CYAN : K_RED, 110);
    b.light(72.5f, 77.6f, 0.8f, b.cy ? C_CYAN : K_RED, 110);
}

// swift warheads: a tilted phased-array radar panel on a mount at (8,48), with red/white booster striping on its housing
void swiftGuidance(Bld& b) {
    Art& a = b.a;
    bool cy = b.cy;
    Color face = cy ? rgb(206, 218, 228) : rgb(196, 186, 150), frame = cy ? rgb(66, 78, 94) : rgb(84, 78, 64), grid = cy ? rgb(84, 102, 122) : rgb(116, 106, 84);
    a.shadowRect(4.6f, 56.0f, 6.8f, 3.6f, 1.2f, 1.2f, 1.4f, 100);
    a.rect(4.6f, 56.0f, 6.8f, 3.6f, 0.5f, frame, 1.0f, 0.4f);
    // the panel leans away to the north-east: its top edge runs 3 px higher on the right than the left, its bottom edge likewise
    std::vector<P> pan = {{2.0f, 41.6f}, {12.4f, 38.4f}, {12.4f, 53.8f}, {2.0f, 57.2f}};
    softPoly(b, pan, 0.6f, 0.6f, 1.0f, 80);
    a.poly(pan, face, 1.4f, 0.5f);
    auto yTop = [](float x) { return 41.6f + (38.4f - 41.6f) * (x - 2.0f) / 10.4f; };
    auto yBot = [](float x) { return 57.2f + (53.8f - 57.2f) * (x - 2.0f) / 10.4f; };
    for (int i = 1; i <= 2; i++) { float x = 2.0f + 10.4f * i / 3.0f; a.line(x, yTop(x), x, yBot(x), 0.45f, rgba(grid, 230)); }
    for (int j = 1; j <= 2; j++) { float f = j / 3.0f; a.line(2.0f, 41.6f + 15.6f * f, 12.4f, 38.4f + 15.4f * f, 0.45f, rgba(grid, 230)); }
    for (int i = 0; i < 6; i++) a.rect(2.0f + i * 1.733f, 56.4f, 1.733f, 1.4f, 0.0f, (i & 1) ? WHITE : rgb(214, 60, 52), 0.3f, 0.0f);
}

// para backup: a lattice radio mast at (88,48) rising north into the top padding, guy wires, a small dish and a red aviation light,
// and a yellow drop-zone chevron by its foot
void paraMast(Bld& b) {
    Art& a = b.a;
    bool cy = b.cy;
    Color steel = cy ? rgb(184, 194, 206) : rgb(176, 170, 152), dark = rgb(58, 64, 74), wire = rgb(30, 34, 40);
    a.line(88, 34, 84, 57, 0.35f, rgba(wire, 170));
    a.line(88, 34, 95, 46, 0.35f, rgba(wire, 170));
    a.shadowRect(85.6f, 45.4f, 4.8f, 4.8f, 1.4f, 1.4f, 1.5f, 100);
    a.rect(85.6f, 45.4f, 4.8f, 4.8f, 0.5f, dark, 1.0f, 0.4f);
    const float yb = 46.0f, yt = 24.0f;
    auto xl = [&](float y) { return 86.6f + (yb - y) / (yb - yt) * 0.9f; };
    auto xr = [&](float y) { return 89.4f - (yb - y) / (yb - yt) * 0.9f; };
    for (int i = 0; i < 4; i++) {
        float y1 = yb - 5.0f * i, y0 = y1 - 5.0f;
        a.line(xl(y1), y1, xr(y0), y0, 0.5f, rgba(steel, 230));
        a.line(xr(y1), y1, xl(y0), y0, 0.5f, rgba(steel, 230));
        a.line(xl(y0), y0, xr(y0), y0, 0.6f, steel);
    }
    a.line(xl(yb), yb, xl(yt), yt, 1.1f, steel);
    a.line(xr(yb), yb, xr(yt), yt, 1.1f, steel);
    a.line(xl(yt), yt, xr(yt), yt, 0.8f, steel);
    b.dishTop(88, 20.5f, 3.0f, steel, rgb(220, 226, 232));
    b.light(88, 14.5f, 0.9f, rgb(255, 70, 60), 160);
    // the chevron: two convex arms (poly() takes convex outlines only)
    a.poly({{81, 66}, {88, 59}, {88, 62}, {81, 69}}, K_YEL, 0.5f, 0.0f);
    a.poly({{88, 59}, {95, 66}, {95, 69}, {88, 62}}, K_YEL, 0.5f, 0.0f);
}

// rain rack: a launch rack on the north-east spot (x 57..68, y 10..18), its five small warheads standing up with red noses. The spot lies
// between the field hospital and the laser battery, clear of the hatch collar, so the rack never covers a live part
void rainRack(Bld& b) {
    Art& a = b.a;
    Color steel = b.cy ? rgb(96, 110, 128) : rgb(112, 106, 90), silver = rgb(228, 234, 242);
    a.shadowRect(57.0f, 16.2f, 10.9f, 2.2f, 1.0f, 1.0f, 1.2f, 100);
    a.rect(57.0f, 16.2f, 10.9f, 2.2f, 0.5f, steel, 1.0f, 0.4f);   // the rack's cradle
    for (int i = 0; i < 5; i++) {
        float x = 58.4f + i * 2.1f;
        a.cap(x, 16.6f, x, 11.8f, 0.95f, silver, 0.5f, 0.5f);   // the warhead body
        a.circle(x, 11.0f, 0.95f, K_RED, 0.4f, 0.3f);           // its red nose
    }
}

// heavy lift pad: a landing pad on the north-west spot (x 27..36, y 15..20), between the overdrive pop-up and the field hospital, clear of the
// hatch collar. Its north and south rims are hazard stripes, a landing ring marks the middle and a beacon light stands at its east end
void liftPad(Bld& b) {
    Art& a = b.a;
    Color hz1 = b.cy ? rgb(232, 188, 62) : rgb(204, 156, 56), hz2 = b.cy ? rgb(36, 36, 32) : rgb(96, 72, 44);
    a.shadowRect(26.8f, 15.2f, 9.4f, 4.8f, 1.0f, 1.0f, 1.2f, 100);
    a.rect(26.8f, 15.2f, 9.4f, 4.8f, 0.4f, b.cy ? rgb(92, 104, 122) : rgb(124, 118, 100), 1.0f, 0.4f);
    b.hazard(26.8f, 15.2f, 9.4f, 1.0f, 2, hz1, hz2);
    b.hazard(26.8f, 19.0f, 9.4f, 1.0f, 2, hz1, hz2);
    a.ring(31.5f, 17.6f, 1.3f, 0.5f, rgb(236, 240, 244), 0.4f, 0.2f);
    b.light(35.2f, 17.6f, 0.8f, b.cy ? C_CYAN : K_RED, 150);
}

}  // namespace

void artBunkerUpgrade(Canvas& c, int type, int upg) {
    const BuildType& bt = BUILDS[type];
    Canvas dummy(c.w, c.h);
    Bld b(c, dummy, bt.w * TILE, bt.h * TILE, bt.faction == F_CYBER, 9400u + (u32)type * 31u + (u32)upg * 7u);
    switch (upg) {
    case BU_SHELL:  shellArmour(b); break;
    case BU_LASERS: laserBattery(b); break;
    case BU_MEDIC:  fieldHospital(b); break;
    case BU_TRIPLE: tripleLaunch(b); break;
    case BU_SWIFT:  swiftGuidance(b); break;
    case BU_PARA:   paraMast(b); break;
    case BU_RAIN:   rainRack(b); break;
    case BU_TANKS:  liftPad(b); break;
    }
    c.outline(rgb(8, 10, 14, 120));
}
