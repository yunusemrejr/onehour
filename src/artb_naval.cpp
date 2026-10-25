#include "bld.h"

// Structure art for the Naval Yards (5x4 tiles: a quay slab with a covered slipway shed, a gantry crane straddling it, a control tower,
// hull sections and tanks) and the anti-nuke defences (2x2 tiles: an energy tower on a hex plinth, a missile silo). Painted with the Bld
// painter from bld.h, like artb.cpp. A Naval Yard looks the same from any side: the quay runs all round and the renderer adds the pier.
namespace {

const Color N_CERAM = rgb(206, 218, 228), N_CERAM_D = rgb(146, 164, 182);            // pale composite hull plates (Cyber)
const Color N_CONC = rgb(150, 146, 134), N_CONC_D = rgb(112, 108, 98), N_CONC_L = rgb(176, 172, 160);   // reinforced concrete (Clanker silo)
const Color N_SCORCH = rgb(34, 26, 22), N_FLAME = rgb(255, 150, 70), N_MISSILE = rgb(214, 218, 212), N_NOSE = rgb(240, 240, 234);
const Color N_TRIM_Y = rgb(255, 206, 80), N_DARKEN = rgb(30, 32, 36);

// a steel bollard on the quay edge: a post with its shadow and a highlight
void bollard(Bld& b, float x, float y, Color steel) {
    b.a.shadowCircle(x, y, 2.0f, 1.2f, 1.4f, 1.2f, 85);
    b.a.circle(x, y, 2.0f, steel, 1.0f, 0.6f);
    b.a.dot(x - 0.6f, y - 0.6f, 0.6f, rgba(WHITE, 170));
}

// a lamp post standing on (x, y): a thin mast with its lamp on top
void lampPost(Bld& b, float x, float y, float ht, Color steel, Color lamp) {
    b.a.line(x + 1.0f, y + 1.0f, x + 1.0f, y - ht + 1.0f, 0.9f, rgba(BLACK, 60));
    b.a.cap(x, y, x, y - ht, 0.6f, steel, 0.6f, 0.4f);
    b.light(x, y - ht, 1.0f, lamp, 130);
}

// a missile standing in a silo, leaning from its base (bx, by) out to its nose (tx, ty): body, warhead band, nose cone and two swept fins
void missile(Bld& b, float bx, float by, float tx, float ty, float r) {
    float dx = tx - bx, dy = ty - by, len = hyp(dx, dy), ux = dx / len, uy = dy / len, nx = -uy, ny = ux;
    float sx = bx + dx * 0.72f, sy = by + dy * 0.72f;                 // where the nose cone starts
    for (int s = -1; s <= 1; s += 2) {                                   // fins at the tail, first so the body overlaps them
        float fx = bx + ux * len * 0.18f, fy = by + uy * len * 0.18f;
        b.a.poly({ {bx + nx * r * 0.6f * s, by + ny * r * 0.6f * s}, {fx + nx * (r + 3.2f) * s, fy + ny * (r + 3.2f) * s + 0.8f}, {bx + ux * len * 0.36f + nx * r * 0.5f * s, by + uy * len * 0.36f + ny * r * 0.5f * s} }, shade(K_STEEL, 0.75f), 0.8f, 0.4f);
    }
    b.a.cap(bx, by, sx, sy, r, N_MISSILE, 1.0f, 0.5f);                  // body tube
    b.a.cap(bx + dx * 0.56f, by + dy * 0.56f, bx + dx * 0.62f, by + dy * 0.62f, r + 0.3f, K_RED, 0.5f, 0.2f);   // red warhead band
    b.a.poly({ {sx + nx * r, sy + ny * r}, {tx, ty}, {sx - nx * r, sy - ny * r} }, N_NOSE, 1.0f, 0.5f);          // nose cone
}

// a ship's hull section on the quay: a deck lifted by ht with a pointed bow to the east and a lit side band beneath it
void hullSection(Bld& b, float x, float y, float w, float h, float ht, Color deck, Color side) {
    float bow = std::min(6.0f, w * 0.22f);
    b.a.shadowRect(x, y, w, h, ht * 0.5f + 1, ht * 0.4f + 1, 1.5f + ht * 0.2f, 100);
    b.a.poly({ {x, y - ht + h}, {x + w - bow, y - ht + h}, {x + w, y - ht + h * 0.5f}, {x + w, y + h * 0.5f}, {x + w - bow, y + h}, {x, y + h} }, shade(side, 0.7f), 1.4f, 0.4f);
    b.a.poly({ {x, y - ht}, {x + w - bow, y - ht}, {x + w, y - ht + h * 0.5f}, {x + w - bow, y - ht + h}, {x, y - ht + h} }, deck, 2.0f, 0.5f);
    for (float xx = x + 4; xx < x + w - bow - 1; xx += 4.5f) b.a.line(xx, y - ht + 1.2f, xx, y - ht + h - 1.2f, 0.6f, rgba(BLACK, 110));   // frames
    b.a.line(x + 2, y - ht + h * 0.5f, x + w - bow, y - ht + h * 0.5f, 0.5f, rgba(WHITE, 90));                                          // keel line
}

// ================================================================= NAVAL YARD
// Cyber: the Nano Drydock, pale composite hulls under robotic welding gantries with cyan arcs and clean roofs.
// Clanker: the Shipyard, a riveted girder crane, corrugated sheds, a chimney with smoke staining and rusty hull ribs on stocks.
// Layout (160 x 128 design px, 5 x 4 tiles): the covered drydock shed fills the middle with its bay doors on the south wall, a slipway runs
// from the bay down onto the berth on the south quay where a hull lies, the gantry bridge straddles the shed from rail to rail, the control
// tower and fuel tanks stand on the east side and stacked containers on the south-west quay.
void navalYard(Bld& b, bool cy) {
    int W = b.W, H = b.H;
    const Color pad = cy ? C_PAD : K_PAD, seam = cy ? C_SEAM : K_SEAM;
    const Color shedRoof = cy ? C_ROOF2 : K_ROOF, shedWall = cy ? C_HULL : K_OLIVE_D;
    const Color towerRoof = cy ? C_ROOF : K_KHAKI, towerWall = cy ? C_HULL_D : K_OLIVE;
    const Color steel = cy ? C_STEEL : K_STEEL, dark = cy ? rgb(22, 26, 36) : K_DARK;
    const Color hull = cy ? N_CERAM : rgb(122, 110, 90), hullD = cy ? N_CERAM_D : rgb(70, 62, 50);   // Clanker hulls: weathered steel, rust-streaked
    const Color win = cy ? C_CYAN : rgb(255, 214, 120);
    const Color tank = cy ? C_ROOF : K_STEEL, tankSide = cy ? C_HULL : K_RUST;
    const Color contA = cy ? C_HULL : K_OLIVE, contB = cy ? C_ROOF2 : K_KHAKI;

    b.pad(pad, seam, cy ? 0.08f : 0.18f, 4);
    if (!cy) b.rust(3, 3, W - 3.0f, H - 3.0f, 22, 50);
    // quay edges: rubber fenders along the north and both side quays, a fender bar and bollards on the south berth
    b.a.rect(10, 1.2f, W - 20.0f, 2.2f, 1.0f, dark, 1.0f, 0.4f);
    b.a.rect(1.2f, 10, 2.2f, H - 20.0f, 1.0f, dark, 1.0f, 0.4f); b.a.rect(W - 3.4f, 10, 2.2f, H - 20.0f, 1.0f, dark, 1.0f, 0.4f);
    b.a.rect(14, H - 19.5f, W - 28.0f, 2.2f, 1.0f, dark, 1.0f, 0.4f);
    for (float x = 20; x < W - 14; x += 16) bollard(b, x, H - 15.0f, steel);
    for (float y = 20; y < H - 30; y += 22) bollard(b, 6.5f, y, steel);
    // gantry rails along the north and south quays, with sleepers
    auto rail = [&](float y) {
        b.a.line(4, y - 1.4f, W - 4.0f, y - 1.4f, 0.9f, steel); b.a.line(4, y + 1.4f, W - 4.0f, y + 1.4f, 0.9f, steel);
        for (float x = 6; x < W - 5; x += 4.0f) b.a.line(x, y - 2.6f, x, y + 2.6f, 0.8f, rgba(dark, 200));
    };
    rail(6.0f); rail(H - 6.0f);

    // control tower (east side): drawn first, its foot is clear of the shed
    Roof tw = b.block(130, 32, 24, 26, 26, towerRoof, towerWall);
    b.wallSeams(tw, 26, 8);
    b.windows(tw, 26, 4, win, 0.6f);
    b.mark(tw.x + 3, tw.y + tw.h - 5.0f, tw.w - 6, 3.2f);
    if (cy) {
        b.dishTop(138, tw.y + 4, 4.6f, C_ROOF2, rgb(30, 40, 56));            // fixed link dish and a mast
        b.antenna(147, tw.y + 4, 12, C_RED);
    } else {
        P top = b.cyl(144, tw.y + 12, 3.4f, 18, K_STEEL, K_RUST);            // chimney rising from the roof
        b.a.ell(142, tw.y + 12, 5.0f, 2.2f, rgba(BLACK, 70), 1, 0);          // soft soot stain round its foot
        b.a.ell(149, top.y - 2.5f, 6.0f, 3.0f, rgba(rgb(92, 88, 82), 70), 1, 0);   // smoke drifting north-east
        b.a.ell(155, top.y - 5.0f, 4.0f, 2.2f, rgba(rgb(92, 88, 82), 45), 1, 0);
        b.a.ring(top.x, top.y, 3.4f, 0.9f, K_DARK, 0.6f, 0.2f);
    }
    // covered slipway shed: a long ridged roof over the drydock, its big bay doors on the south wall
    Roof sh = b.block(12, 30, 112, 56, 18, shedRoof, shedWall);
    if (cy) {
        b.skylights(sh, 12, C_GLASS, C_STEEL);
        b.a.line(sh.x + 3, sh.y + sh.h * 0.5f, sh.x + sh.w - 3, sh.y + sh.h * 0.5f, 1.0f, rgba(C_WHITE, 110));   // ridge
    } else {
        for (float y = sh.y + 4; y < sh.y + sh.h - 2; y += 4.6f) b.a.line(sh.x + 3, y, sh.x + sh.w - 3, y, 0.7f, rgba(K_DARK, 90));   // corrugated roof ribs
        b.rust(sh.x, sh.y, sh.x + sh.w, sh.y + sh.h, 12, 60);
    }
    b.mark(sh.x + sh.w - 24, sh.y + 3, 18, 3.4f);
    b.wallSeams(sh, 18, 14, 90);
    b.door(64, sh.y + sh.h + 18, 60, 14, cy ? C_STEEL : K_DARK, rgb(8, 14, 20));
    if (cy) { b.stripe(12, 88.4f, 124, 88.4f, 0.8f, C_CYAN, 70); b.stripe(34, 89.6f, 94, 89.6f, 0.7f, C_CYAN, 80); }
    else b.hazard(12, sh.y + sh.h + 18.8f, 112, 2.2f);

    // slipway: a concrete ramp down from the bay doors to the berth, with two rails and sleepers
    b.a.rect(56, 86, 16, 22, 0.6f, cy ? C_PAD2 : K_PAD2, 1.4f, 0.5f);
    b.a.line(60, 86, 60, 108, 0.9f, steel); b.a.line(68, 86, 68, 108, 0.9f, steel);
    for (float y = 88; y < 108; y += 4.0f) b.a.line(56, y, 72, y, 0.6f, rgba(dark, 170));

    // the gantry: a north leg on the north rail, the bridge straddling the shed from rail to rail and a south leg on the south rail
    Roof br = b.block(84, 4, 8, 118, 16, cy ? C_ROOF2 : steel, cy ? C_HULL_D : dark);
    if (cy) b.stripe(br.x + 4, br.y + 3, br.x + 4, br.y + br.h - 3, 0.8f, C_CYAN, 70);
    else for (float y = br.y + 2; y < br.y + br.h - 5; y += 6) {
        b.a.line(br.x + 1, y, br.x + br.w - 1, y + 5, 0.5f, rgba(dark, 170)); b.a.line(br.x + br.w - 1, y, br.x + 1, y + 5, 0.5f, rgba(dark, 170));   // lattice bracing
        b.a.dot(br.x + 1.6f, y, 0.4f, rgba(WHITE, 140)); b.a.dot(br.x + br.w - 1.6f, y, 0.4f, rgba(WHITE, 140));                             // rivets
    }
    b.block(78, 0, 20, 10, 22, steel, dark);
    b.block(78, 114, 20, 10, 22, steel, dark);

    // the berth on the south quay: a cradle under a hull section lying along the quay, bow to the east
    b.a.rect(34, 106, 90, 2.6f, 0.6f, K_OLIVE_D, 1.0f, 0.4f);
    hullSection(b, 36, 96, 88, 10, 6, hull, hullD);
    if (cy) {
        for (float x : { 50.0f, 70.0f, 90.0f, 110.0f }) {   // robotic welding heads on posts, arcing onto the hull
            b.a.cap(x, 112, x, 103, 0.9f, C_STEEL, 0.8f, 0.4f); b.a.dot(x, 102.5f, 1.2f, C_STEEL);
            b.a.glow(x, 100, 6, rgba(C_CYAN, 120)); b.a.dot(x, 99.5f, 0.9f, C_WHITE);
            b.a.line(x - 2, 98.5f, x + 1, 97.0f, 0.5f, C_CYAN); b.a.line(x + 1, 97.0f, x + 2.5f, 98.5f, 0.5f, C_CYAN);
        }
    } else {
        b.rust(40, 96, 124, 106, 8, 70);
        for (float x = 42; x < 124; x += 14) b.a.line(x, 108.6f, x, 111.0f, 1.2f, K_RUST_D);   // stock legs
    }
    // the hook and cable, carried by the trolley that rides the bridge
    Roof tr = b.block(74, 48, 28, 12, 6, steel, dark);
    b.a.line(88, tr.y + tr.h, 88, 94, 0.8f, rgba(dark, 220));
    b.a.ring(88, 95.5f, 2.2f, 0.7f, steel, 0.6f, 0.3f);

    // south-west quay: two stacks of containers, the top box a little offset from the one under it
    b.container(14, 92, 18, 5, contA);
    b.container(15, 92.6f, 16, 4, contB);
    b.container(14, 100, 18, 5, contB);
    b.container(15, 100.6f, 16, 4, contA);
    // east quay: a stack of containers on the south-east corner, and the fuel tanks by the shed with a pipe to its corner
    b.container(128, 116, 22, 5, contA);
    b.container(130, 116.6f, 18, 4, contB);
    b.cyl(138, 86, 6.0f, 14, tank, tankSide);
    b.cyl(152, 86, 6.0f, 14, tank, tankSide);
    b.pipe(124, 80, 134, 82, 1.2f, steel);
    b.barrel(34, 22, 2.4f, cy ? C_ROOF2 : K_RUST);
    if (!cy) b.sandbags(3.5f, 28, 3.5f, 44, 2);                                 // sandbags at the north-west corner of the shed
    lampPost(b, 4, 52, 14, steel, cy ? C_CYAN : K_YEL);
    lampPost(b, 156, 104, 14, steel, cy ? C_CYAN : K_YEL);
    // team mark on the shed's south wall
    b.mark(20, sh.y + sh.h + 4.0f, 14, 2.6f);
    b.a.grain(0, 0, (float)W, (float)H, cy ? 0.05f : 0.10f, cy ? 103u : 107u);
}

// ================================================================= ANTI-NUKE LASER (Cyber)
// A hex plinth with a hazard-trimmed edge, a slim pylon carrying a focusing lens with a cyan glow, capacitor banks and cyan conduits.
void antiNukeLaser(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(C_PAD, C_SEAM, 0.08f, 3);
    const float cx = 32, cy = 34, R = 24;
    b.a.ngon(cx, cy + 7, R, 6, 0, C_HULL_D, 2.0f, 0.5f);          // side band of the plinth
    b.a.ngon(cx, cy, R, 6, 0, C_ROOF2, 2.4f, 0.55f);              // armoured top deck
    b.a.ngon(cx, cy, R - 4.5f, 6, 0, C_STEEL, 1.6f, 0.4f);        // recessed deck inside the armour
    // hazard trim along the top edge of the plinth: yellow and black dashes round the six sides
    for (int i = 0; i < 6; i++) for (int k = 0; k < 4; k++) {
        float t = i * 1.0472f, t2 = (i + 1) * 1.0472f;
        P a{cx + std::cos(t) * (R - 1.0f), cy + std::sin(t) * (R - 1.0f)}, z{cx + std::cos(t2) * (R - 1.0f), cy + std::sin(t2) * (R - 1.0f)};
        float f0 = k / 4.0f, f1 = (k + 0.5f) / 4.0f;
        b.a.line(a.x + (z.x - a.x) * f0, a.y + (z.y - a.y) * f0, a.x + (z.x - a.x) * f1, a.y + (z.y - a.y) * f1, 1.8f, ((i + k) & 1) ? N_DARKEN : N_TRIM_Y);
    }
    // the pylon: a slim column with emitter rings, topped by the focusing lens
    P top = b.cyl(cx, cy + 2, 3.6f, 36, C_ROOF, C_ROOF2);
    b.a.ring(cx, cy + 2, 5.8f, 0.9f, rgba(C_CYAN, 220), 0.8f, 0); b.a.glow(cx, cy + 2, 9, rgba(C_CYAN, 80));
    for (float y : { 14.0f, 22.0f }) b.a.line(cx - 3.2f, y, cx + 3.2f, y, 0.6f, rgba(C_STEEL, 200));
    float lx = top.x, ly = top.y - 12;
    b.a.glow(lx, ly, 16, rgba(C_CYAN, 105));
    b.a.cap(top.x, top.y, lx, ly + 2, 1.6f, C_STEEL, 1.0f, 0.5f);          // yoke
    b.a.circle(lx, ly, 11, C_ROOF2, 2.2f, 0.6f);                             // dish housing
    b.a.circle(lx, ly, 8.4f, C_STEEL, 2.6f, 0.7f);                           // dish bowl
    b.a.circle(lx, ly, 6.4f, C_GLASS, 2.6f, 0.7f);                           // focusing lens
    b.a.ring(lx, ly, 6.0f, 0.8f, rgba(C_CYAN, 230), 0.6f, 0);
    b.a.circle(lx, ly, 3.2f, C_WHITE, 1.8f, 0.5f);                           // hot focus
    b.a.glow(lx, ly, 7, rgba(C_WHITE, 150));
    b.a.dot(lx - 3.2f, ly - 3.4f, 1.0f, rgba(WHITE, 210));
    // two capacitor banks flanking the pylon, with cyan conduits running to its base
    Roof c1 = b.block(14, 28, 9, 12, 7, C_ROOF, C_HULL_D), c2 = b.block(41, 28, 9, 12, 7, C_ROOF, C_HULL_D);
    Roof c3 = b.block(26, 43, 12, 8, 5, C_ROOF, C_HULL_D);   // third bank in front of the pylon
    for (Roof* r : { &c1, &c2, &c3 }) {
        for (int i = 0; i < 3; i++) b.a.line(r->x + 1.5f, r->y + r->h + 2.0f + i * 3.0f, r->x + r->w - 1.5f, r->y + r->h + 2.0f + i * 3.0f, 0.5f, rgba(BLACK, 110));   // cooling fins on the wall
        b.light(r->x + r->w * 0.5f, r->y + 2.5f, 0.9f, C_CYAN, 110);
    }
    b.stripe(c1.x + c1.w * 0.5f, c1.y + c1.h * 0.5f, top.x - 4, cy + 2, 0.9f, C_CYAN, 90);
    b.stripe(c2.x + c2.w * 0.5f, c2.y + c2.h * 0.5f, top.x + 4, cy + 2, 0.9f, C_CYAN, 90);
    // control hatch on the south band of the plinth, a cyan light beside it
    b.door(cx, 61, 8, 6, C_STEEL, rgb(8, 16, 24));
    b.light(cx + 8, 57, 0.7f, C_CYAN, 100);
    b.mark(c3.x + 2, c3.y + c3.h * 0.5f - 1.3f, c3.w - 4, 2.6f);                  // team mark on the front bank's roof
    // ground: corner lamps on the pad
    for (float x : { 5.0f, W - 5.0f }) for (float y : { 5.0f, H - 5.0f }) b.light(x, y, 0.8f, C_CYAN, 80);
    b.a.grain(0, 0, (float)W, (float)H, 0.05f, 97);
}

// ================================================================= ANTI-NUKE MISSILES (Clanker)
// A reinforced concrete silo with a slanted, open hatch showing four interceptor missiles, an exhaust scorch on the slab,
// sandbags, ammunition crates, a radar mast and hazard stripes.
void antiNukeMissiles(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(K_PAD, K_SEAM, 0.16f, 4);
    b.rust(3, 3, W - 3.0f, H - 3.0f, 14, 50);
    // silo block: the roof is the launch slab, the south wall is poured concrete
    Roof sl = b.block(6, 16, 52, 40, 12, N_CONC, N_CONC_D);
    b.wallSeams(sl, 12, 10, 110);
    b.rust(sl.x + 2, sl.y + sl.h - 2, sl.x + sl.w - 2, sl.y + sl.h + 9, 8, 70);
    b.hazard(sl.x + 2, sl.y + sl.h - 2.6f, sl.w - 4, 1.6f, 4, N_TRIM_Y, N_DARKEN);   // hazard edge on the slab front
    // the open hatch: a steel rim round the well, the flap raised toward the north and the four missiles standing in the well
    b.a.rect(20, 12, 24, 14, 1.2f, K_STEEL, 1.4f, 0.5f);
    b.a.rect(22, 14, 20, 10, 1.0f, rgb(8, 9, 10), 1.0f, 0.2f);
    b.a.poly({ {18, 26}, {46, 26}, {49, 2}, {15, 2} }, N_CONC_L, 1.8f, 0.5f);     // the raised hatch flap
    b.a.line(20, 24, 45, 6, 0.5f, rgba(BLACK, 80)); b.a.line(28, 24, 40, 6, 0.5f, rgba(BLACK, 60));
    b.a.rect(21, 23.5f, 22, 3.5f, 0.8f, rgb(8, 9, 10), 1.0f, 0.2f);                 // the dark well in front of the flap
    // exhaust blast: a scorch on the slab in front of the well
    b.a.ell(32, 33, 14, 4.5f, rgba(N_SCORCH, 170), 1, 0);
    b.a.glow(32, 30, 8, rgba(N_FLAME, 70));
    // four interceptors, leaning out from the well (nose cones and fins show in the 3/4 view)
    missile(b, 25, 22, 14, -4, 2.1f);
    missile(b, 31, 21, 25, -7, 2.1f);
    missile(b, 38, 21, 40, -7, 2.1f);
    missile(b, 44, 23, 53, -2, 2.1f);
    // radar mast on the north-east corner of the slab
    b.antenna(56, 14, 10, K_RED);
    b.dishTop(56, 9, 4.2f, K_STEEL, rgb(40, 44, 40));
    // team mark on the slab
    b.mark(9, 37, 20, 3.2f);
    // ground: sandbag corners, ammunition crates, a hazard stripe along the front and rust streaks
    b.sandbags(3, 22, 3, 44, 2); b.sandbags(3, 60, 13, 60, 2); b.sandbags(47, 60, 61, 60, 2);
    b.hazard(4, 58.4f, W - 8.0f, 2.2f);
    b.crate(58.5f, 18, 5, 6, shade(K_OLIVE, 1.0f)); b.crate(59.5f, 26, 5, 5, shade(K_OLIVE, 1.12f)); b.crate(3.5f, 46, 5, 5, shade(K_OLIVE_D, 1.2f));
    b.barrel(60, 42, 2.6f, K_RUST);
    b.a.grain(0, 0, (float)W, (float)H, 0.12f, 109);
}

}  // namespace

bool artNavalBuilding(Canvas& c, Canvas& mask, int type) {
    const BuildType& bt = BUILDS[type];
    if (bt.role != BR_NAVAL && bt.role != BR_ANTINUKE) return false;
    bool cy = bt.faction == F_CYBER;
    Bld b(c, mask, bt.w * TILE, bt.h * TILE, cy, 7000u + (u32)type * 31u);
    if (bt.role == BR_NAVAL) navalYard(b, cy);
    else if (cy) antiNukeLaser(b);
    else antiNukeMissiles(b);
    c.outline(rgb(8, 10, 14, 150));
    return true;
}
