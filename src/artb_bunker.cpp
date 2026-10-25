#include "bld.h"

// The underground bunker of either army: a 3x3 surface apron over the buried shelter, and what each per-bunker upgrade adds to it.
// Same conventions as the other structure painters (bld.h): raised 3/4 view, light from the north-west. Design coordinates run 0..96 over
// the footprint; the BUNKER_* spots in data.h (px from the centre) are where the live parts are drawn and the upgrades sit.
// The base keeps every reserved spot plain (hatch well, gun and overdrive mounts, laser / medic / triple / guidance / radio spots) so the
// upgrade overlays can sit on them; the live parts are drawn over the sprite and are not part of it.
namespace {

const float CX = 48, CY = 48;   // the footprint centre in design px

// a chamfered square of half-size h round (cx, cy), its corners cut by c: the berm, the glacis and the armour rings are all of these
std::vector<P> oct(float cx, float cy, float h, float c) {
    return {{cx - h + c, cy - h}, {cx + h - c, cy - h}, {cx + h, cy - h + c}, {cx + h, cy + h - c},
            {cx + h - c, cy + h}, {cx - h + c, cy + h}, {cx - h, cy + h - c}, {cx - h, cy - h + c}};
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

// a convex volume standing on a plan polygon and lifted by ht (the lift of Bld::block): a soft shadow to the south-east, the faces that
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
        float nx = ey / l, ny = -ex / l;   // outward normal of this edge
        if (nx * ((p.x + q.x) * 0.5f - cx) + ny * ((p.y + q.y) * 0.5f - cy) < 0) { nx = -nx; ny = -ny; }
        if (ny < 0.02f) continue;          // a face that looks north is hidden behind the roof
        float lit = -(nx * 0.62f + ny * 0.78f);
        a.poly({p, q, {q.x, q.y - ht}, {p.x, p.y - ht}}, shade(wall, 1.0f + 0.16f * lit), 1, 0);
        a.line(p.x, p.y, q.x, q.y, 1.0f, rgba(BLACK, 110));   // foot line where the wall meets the ground
    }
    for (auto& p : plan) a.line(p.x, p.y, p.x, p.y - ht, 0.7f, rgba(BLACK, 90));
    std::vector<P> top;
    for (auto& p : plan) top.push_back({p.x, p.y - ht});
    a.poly(top, roof, 1.6f, 0.58f);
}

// soft shadow of a ring of outline O (moved by (dx, dy)) that falls only outside the inner outline I, so it never darkens what the ring encloses
void softRing(Bld& b, const std::vector<P>& O, const std::vector<P>& I, float dx, float dy, float feather, int alpha) {
    Art& a = b.a;
    std::vector<P> o = O, in = I;
    float mnx = 1e9f, mny = 1e9f, mxx = -1e9f, mxy = -1e9f;
    for (auto& p : o) { p.x = a.X(p.x + dx); p.y = a.Y(p.y + dy); mnx = std::min(mnx, p.x); mny = std::min(mny, p.y); mxx = std::max(mxx, p.x); mxy = std::max(mxy, p.y); }
    for (auto& p : in) { p.x = a.X(p.x); p.y = a.Y(p.y); }
    auto sgnOf = [](const std::vector<P>& v) {
        float area = 0;
        for (size_t i = 0; i < v.size(); i++) { const P& p = v[i]; const P& q = v[(i + 1) % v.size()]; area += p.x * q.y - q.x * p.y; }
        return area > 0 ? 1.0f : -1.0f;
    };
    float so = sgnOf(o), si = sgnOf(in), f = feather * a.k;
    a.soft((int)(mnx - f - 2), (int)(mny - f - 2), (int)(mxx + f + 2), (int)(mxy + f + 2),
           [o, in, so, si, f](float x, float y) { return std::max(sdPoly(o, x, y, so), f - sdPoly(in, x, y, si)); }, Color{0, 0, 0, (u8)alpha}, f);
}

// an annular slab round (cx, cy) between radii r0 and r1, from angle a0 to a1 (radians; 0 = east, pi/2 = south); keep the spans short
void sectorAt(Bld& b, float cx, float cy, float r0, float r1, float a0, float a1, Color c, float bevel, float kk) {
    auto at = [&](float r, float t) { return P{cx + std::cos(t) * r, cy + std::sin(t) * r}; };
    b.a.poly({at(r0, a0), at(r1, a0), at(r1, a1), at(r0, a1)}, c, bevel, kk);
}

// a radiation trefoil: a yellow disc with three black blades round a black hub
void trefoil(Bld& b, float x, float y, float r) {
    Color ink = rgb(28, 26, 22);
    b.a.circle(x, y, r, K_YEL, r * 0.3f, 0.25f);
    for (int k = 0; k < 3; k++) {
        float t = -1.5708f + k * 2.0944f;
        b.a.dot(x + std::cos(t) * r * 0.52f, y + std::sin(t) * r * 0.52f, r * 0.42f, ink);
    }
    b.a.dot(x, y, r * 0.22f, ink);
}

// a whip antenna on a small mount: a thin mast leaning a little, with a light at the tip
void whip(Bld& b, float x, float y, float ht) {
    Art& a = b.a;
    a.shadowCircle(x, y, 1.8f, 1.4f, 1.4f, 1.6f, 90);
    a.circle(x, y, 1.8f, rgb(92, 98, 108), 0.8f, 0.5f);
    float tx = x + ht * 0.08f, ty = y - ht;
    a.line(x + 1.2f, y + 0.8f, tx + 1.2f, ty + 0.8f, 0.5f, rgba(BLACK, 60));
    a.cap(x, y, tx, ty, 0.42f, rgb(190, 196, 204), 0.3f, 0.5f);
    b.light(tx, ty, 0.8f, b.cy ? C_CYAN : K_RED, 150);
}

// an anti-radiation filter stack: a short cylinder whose top sits on (x, y), its side carrying a radiation sign
void scrubber(Bld& b, float x, float y) {
    Color top = b.cy ? rgb(184, 196, 208) : rgb(206, 194, 154), side = b.cy ? rgb(122, 136, 154) : rgb(160, 146, 110);
    b.cyl(x, y + 6.0f, 4.0f, 6.0f, top, side);
    b.a.ring(x, y, 2.4f, 0.7f, shade(top, 0.6f), 0.4f, 0.2f);   // the filter grille
    b.a.dot(x, y, 0.8f, rgb(20, 22, 26));
    trefoil(b, x, y + 6.5f, 1.9f);
}

// ---------------------------------------------------------------- the base apron
void apron(Bld& b) {
    Art& a = b.a;
    int W = b.W, H = b.H;
    bool cy = b.cy;
    Color ground = cy ? rgb(70, 80, 94) : rgb(132, 118, 88), slab = cy ? rgb(112, 126, 144) : rgb(196, 178, 130);
    Color wallC = cy ? rgb(72, 84, 102) : rgb(150, 132, 94), seam = cy ? rgb(52, 62, 78) : rgb(112, 98, 70);
    Color steel = cy ? rgb(92, 104, 122) : rgb(120, 118, 108), slat = cy ? rgb(96, 110, 128) : rgb(140, 120, 88);
    Color ink = rgb(8, 10, 14);
    Color hz1 = cy ? rgb(232, 188, 62) : rgb(204, 156, 56), hz2 = cy ? rgb(36, 36, 32) : rgb(96, 72, 44);
    Color lamp = cy ? C_CYAN : K_RED;

    a.rect(0, 0, (float)W, (float)H, 3, ground, 2.4f, 0.45f);
    // the glacis: the sloped skirt the berm rises from, with a shadow to the south-east
    std::vector<P> skirt = oct(CX, 50, 45, 16);
    softPoly(b, skirt, 2.2f, 2.2f, 3, 95);
    a.poly(skirt, shade(slab, 0.82f), 6.0f, 0.55f);
    // the berm: a chamfered slab lifted 5 px, its top face centred on the footprint
    volume(b, oct(CX, 53, 42, 12), 5, slab, wallC);

    // panel seams across the top face (the live parts and the decorations sit over them)
    for (float x : {30.0f, 66.0f}) { a.line(x, 9, x, 70, 0.7f, rgba(seam, 170)); a.line(x + 0.8f, 9, x + 0.8f, 70, 0.5f, rgba(WHITE, 36)); }
    for (float y : {34.0f, 62.0f}) { a.line(9, y, 87, y, 0.7f, rgba(seam, 170)); a.line(9, y + 0.8f, 87, y + 0.8f, 0.5f, rgba(WHITE, 36)); }
    // hazard paint along the north edge, either side of the medic spot
    b.hazard(21, 6.4f, 13, 2.2f, 4, hz1, hz2);
    b.hazard(62, 6.4f, 14, 2.2f, 4, hz1, hz2);

    // the hatch collar: a raised armoured ring with bolts (Cyber: cyan strips); the pit is a black well of radius 22
    a.shadowCircle(CX, CY, 28, 2.2f, 2.2f, 2.6f, 100);
    a.circle(CX, CY, 28, steel, 3.0f, 0.6f);
    for (int k = 0; k < 16; k++) {
        float t = k * 0.3927f, x = CX + std::cos(t) * 25.0f, y = CY + std::sin(t) * 25.0f;
        if (cy && (k & 1)) b.light(x, y, 0.7f, C_CYAN, 110);
        else a.dot(x, y, 0.8f, shade(steel, 1.5f));
    }
    a.circle(CX, CY, BUNKER_HATCH_R + 2.0f, ink, 1.6f, 0.2f);
    a.ring(CX, CY, BUNKER_HATCH_R + 2.6f, 1.2f, shade(steel, 0.5f), 0.8f, 0.4f);

    // mount plates for the two miniguns, with bolts
    for (int s = -1; s <= 1; s += 2) {
        float x = CX + s * BUNKER_GUN_X, y = CY;
        a.shadowRect(x - 6, y - 6, 12, 12, 1.2f, 1.2f, 1.6f, 95);
        a.rect(x - 6, y - 6, 12, 12, 1.4f, shade(steel, 1.1f), 1.2f, 0.5f);
        for (float dx : {-4.2f, 4.2f}) for (float dy : {-4.2f, 4.2f}) a.dot(x + dx, y + dy, 0.6f, shade(steel, 0.6f));
    }
    // flush overdrive hatches in the two free corners of the hatch
    const P od[2] = {{CX - BUNKER_OD_X, CY - BUNKER_OD_Y}, {CX + BUNKER_OD_X, CY + BUNKER_OD_Y}};
    for (const P& p : od) {
        a.circle(p.x, p.y, 6.2f, shade(steel, 0.7f), 1.0f, 0.5f);
        a.circle(p.x, p.y, 4.9f, shade(slab, 0.92f), 1.2f, 0.45f);
        a.ring(p.x, p.y, 3.2f, 0.6f, shade(steel, 0.5f), 0.5f, 0.2f);
        a.dot(p.x, p.y, 0.7f, ink);
    }

    // decoration: anti-radiation stacks, whip antennas, vent grilles, warning lights
    scrubber(b, 17, 17);
    scrubber(b, 79, 79);
    whip(b, 84, 10, 16);
    whip(b, 11, 86, 16);
    b.vent(7, 26, 4, 9, slat);
    b.vent(7, 60, 4, 9, slat);
    b.vent(85, 34, 3, 9, slat);
    b.light(24.5f, 92.4f, 0.8f, lamp, 110);
    b.light(71.5f, 92.4f, 0.8f, lamp, 110);
    if (!cy) {   // Clanker: sandbags on the north and east edges, rust on the walls
        b.sandbags(20, 12, 34, 12, 1);
        b.sandbags(85, 54, 85, 66, 1);
        b.rust(10, 90, 86, 95, 10, 80);
        b.rust(2, 20, 7, 80, 4, 50);
    }
    // team colour strip on the south wall, between the lights and clear of the triple tubes
    b.mark(31, 91.2f, 34, 2.4f);
    b.a.grain(0, 0, (float)W, (float)H, 0.08f, 93);
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

// super-thick shell: a ring of armour slabs round the perimeter (their south face leaves a gap for the team mark), and a heavier
// segmented hatch collar from r 22.4 to 31, open where the miniguns' plates stand
void shellArmour(Bld& b) {
    Art& a = b.a;
    Color plate = b.cy ? rgb(128, 142, 160) : rgb(150, 146, 130);
    std::vector<P> O = oct(CX, CY, 47, 16), I = oct(CX, CY, 41, 10);
    softRing(b, O, I, 2.5f, 2.5f, 3, 105);
    auto lerpP = [](P p, P q, float t) { return P{p.x + (q.x - p.x) * t, p.y + (q.y - p.y) * t}; };
    for (int i = 0; i < 8; i++) {
        P o1 = O[i], o2 = O[(i + 1) % 8], i1 = I[i], i2 = I[(i + 1) % 8];
        std::vector<std::pair<float, float>> spans = {{0.0f, 1.0f}};
        if (i == 4) spans = {{0.0f, 0.2f}, {0.8f, 1.0f}};
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
    trefoil(b, 24.5f, 84, 2.6f);
    trefoil(b, 70.5f, 84, 2.6f);
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
    }
    c.outline(rgb(8, 10, 14, 120));
}
