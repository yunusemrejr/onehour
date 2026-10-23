#include "bld.h"

// The map's special structures: the Island Nuke Launcher in its laser chamber, the Mountain Fortress dug into the hills and the
// indestructible Bastion Wall. Same conventions as the other structure painters (bld.h): raised 3/4 view, light from the north-west.
namespace {

const Color S_CONC = rgb(146, 148, 146), S_CONC_D = rgb(92, 94, 94), S_STEEL = rgb(70, 76, 86), S_STEEL_L = rgb(128, 136, 146);
const Color S_RED = rgb(255, 98, 64), S_RED_D = rgb(196, 42, 36), S_CORE = rgb(255, 214, 180), S_MISSILE = rgb(238, 242, 246);
const Color G_WALL = rgb(74, 76, 82), G_ROOF = rgb(106, 108, 114), G_DARK = rgb(50, 52, 58), G_SLOT = rgb(12, 12, 14);
const Color G_ROCK = rgb(98, 94, 88), G_ROCK_D = rgb(64, 61, 57), G_PAVE = rgb(118, 114, 104);

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

// a gun embrasure: a dark slot with a pale sill under it (on a wall that faces the viewer)
void embrasure(Bld& b, float x, float y) {
    b.a.rect(x, y, 4.6f, 4.2f, 0.3f, G_SLOT, 0.5f, 0.1f);
    b.a.rect(x - 0.8f, y + 4.2f, 6.2f, 1.1f, 0.3f, rgb(152, 150, 144), 0.5f, 0.3f);
}

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

// 160 x 128: a granite fortress cut into the mountainside. Curtain walls with gun embrasures round a paved parade ground, angled bastions
// at the corners, a radar keep, an armoured gatehouse on the south face and a rock massif behind and beside it.
void fortress(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(rgb(94, 92, 86), rgb(58, 56, 52), 0.2f, 4);
    // the mountain: massifs behind the north wall and on both flanks, with boulders on their faces
    b.a.ell(80, 2, 88, 15, G_ROCK_D, 3, 0.6f);
    b.a.ell(4, 68, 9, 50, G_ROCK_D, 3, 0.6f);
    b.a.ell(156, 68, 9, 50, G_ROCK_D, 3, 0.6f);
    // the rock: a row of boulders behind the north wall, and along both flanks
    for (int i = 0; i < 10; i++) boulder(b, 4 + i * 17.5f + b.rng.f(-3, 3), -4 + b.rng.f(-3, 3), b.rng.f(9, 12), G_ROCK);
    for (float y = 26; y < H; y += 17) {
        boulder(b, 2 + b.rng.f(-1, 1), y + b.rng.f(-3, 3), b.rng.f(8, 10), G_ROCK_D);
        boulder(b, W - 2 + b.rng.f(-1, 1), y + 8 + b.rng.f(-3, 3), b.rng.f(8, 11), G_ROCK);
    }
    // the parade ground, flagged
    b.a.rect(34, 37, 92, 57, 1.5f, G_PAVE, 1.2f, 0.4f);
    for (float y = 46; y < 94; y += 12) b.a.line(34, y, 126, y, 0.45f, rgba(G_DARK, 110));
    for (float x = 46; x < 126; x += 20) b.a.line(x, 37, x, 94, 0.45f, rgba(G_DARK, 90));

    // curtain walls (their roofs lifted north, the south faces visible)
    b.block(14, 22, 132, 14, 18, G_ROOF, G_WALL, 1.5f);   // north curtain
    for (float x = 18; x < 146; x += 9) b.a.rect(x, 1, 4.8f, 5, 0.5f, G_ROOF, 1.0f, 0.5f);   // merlons
    for (float x : {38.0f, 48.0f, 110.0f, 120.0f}) embrasure(b, x, 24);
    b.block(14, 22, 18, 88, 16, G_ROOF, G_WALL, 1.5f);    // west curtain
    b.block(128, 22, 18, 88, 16, G_ROOF, G_WALL, 1.5f);   // east curtain
    // the keep: a tall command block with a radar dome and an observation cupola
    b.block(56, 44, 48, 34, 34, G_ROOF, G_WALL, 1.5f);
    for (float x : {76.0f, 84.0f, 92.0f}) embrasure(b, x, 50);
    P cup = b.cyl(64, 36, 5.5f, 10, G_ROOF, G_WALL);
    b.a.rect(cup.x - 2.6f, cup.y + 3.5f, 5.2f, 3.2f, 0.4f, rgb(40, 92, 112), 0.6f, 0.2f);   // observation glass
    b.domeTop(80, 27, 11, rgb(190, 196, 200));
    flagpole(b, 100, 26, 24, 13, 8);
    // the south curtain with its merlons, the gatehouse towers and the armoured gate between them
    b.block(14, 96, 132, 18, 18, G_ROOF, G_WALL, 1.5f);
    for (float x = 18; x < 146; x += 9) b.a.rect(x, 74.5f, 4.8f, 4.5f, 0.5f, G_ROOF, 1.0f, 0.5f);
    for (float x : {40.0f, 52.0f, 108.0f, 120.0f}) embrasure(b, x, 100);
    b.mark(34, 107, 16, 3);   // owner's colour on the south face
    b.block(56, 92, 16, 22, 26, G_ROOF, G_WALL, 1.5f);   // left gatehouse
    b.block(88, 92, 16, 22, 26, G_ROOF, G_WALL, 1.5f);   // right gatehouse
    for (float y : {92.0f, 104.0f}) { embrasure(b, 62, y); embrasure(b, 96, y); }
    // the armoured gate: a steel frame, a dark opening and a portcullis
    b.door(80, 114.6f, 16, 15, rgb(96, 100, 108), G_SLOT);
    for (float x = 74; x <= 86; x += 3) b.a.line(x, 100, x, 113.5f, 0.6f, S_STEEL_L);
    b.a.line(72.5f, 106, 87.5f, 106, 0.8f, S_STEEL); b.a.line(72.5f, 111, 87.5f, 111, 0.8f, S_STEEL);
    b.a.rect(70, 98, 2.4f, 16.6f, 0.4f, rgb(70, 74, 80), 0.6f, 0.3f); b.a.rect(87.6f, 98, 2.4f, 16.6f, 0.4f, rgb(70, 74, 80), 0.6f, 0.3f);   // hinge posts
    // the four corner bastions: angled pentagons pointing out of the corners
    const float PI = 3.14159265f;
    prism(b, regular(14, 22, 16, 5, 5 * PI / 4), 30, G_ROOF, G_WALL, 2);   // NW
    prism(b, regular(146, 22, 16, 5, 7 * PI / 4), 30, G_ROOF, G_WALL, 2);  // NE
    prism(b, regular(146, 114, 16, 5, PI / 4), 24, G_ROOF, G_WALL, 2);     // SE
    prism(b, regular(14, 114, 16, 5, 3 * PI / 4), 24, G_ROOF, G_WALL, 2);  // SW
    // boulders at the foot of the south wall
    boulder(b, 44, 125, 5, G_ROCK_D); boulder(b, 122, 126, 6, G_ROCK);
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

}  // namespace

bool artSpecialBuilding(Canvas& c, Canvas& mask, int type) {
    const BuildType& bt = BUILDS[type];
    if (bt.role != BR_ISLANDNUKE && bt.role != BR_FWALL && type != B_C_FORTRESS) return false;
    Bld b(c, mask, bt.w * TILE, bt.h * TILE, bt.faction == F_CYBER, 9100u + (u32)type * 29u);
    if (bt.role == BR_ISLANDNUKE) islandLauncher(b);
    else if (bt.role == BR_FWALL) bastionWall(b);
    else fortress(b);
    c.outline(rgb(8, 10, 14, 150));
    return true;
}
