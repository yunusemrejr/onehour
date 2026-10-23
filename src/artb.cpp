#include "bld.h"

// Structure art for the base structures. The painter (Bld) and the palettes live in bld.h.
namespace {

// ================================================================= CYBER
// Real-materials palette for the four Cyber base structures: pale concrete, blue-grey steel, tinted glass and white ceramic panels.
// Cyan stays in a few functional lights; the cobalt family is kept off the big surfaces.
const Color R_PAVE = rgb(124, 130, 136), R_PAVE_SEAM = rgb(84, 90, 98);
const Color R_CONC = rgb(152, 156, 158), R_CONC_TOP = rgb(180, 184, 186), R_CONC_DK = rgb(110, 116, 122);
const Color R_PANEL = rgb(210, 216, 222), R_STEEL = rgb(96, 110, 128), R_STEEL_L = rgb(150, 162, 178), R_STEEL_D = rgb(56, 66, 82);
const Color R_GLASS = rgb(34, 54, 68), R_GLASS_T = rgb(96, 128, 142), R_DARK = rgb(24, 28, 36);

// a row of n tinted windows across [x0, x0+w] with its top at y0 (design px)
void winRow(Bld& b, float x0, float y0, float w, int n, float hh) {
    float pitch = w / n;
    for (int i = 0; i < n; i++) {
        float cx = x0 + pitch * (i + 0.5f), ww = pitch * 0.55f;
        b.a.rect(cx - ww * 0.5f - 0.6f, y0 - 0.6f, ww + 1.2f, hh + 1.2f, 0.5f, rgba(BLACK, 150), 0.5f, 0);
        b.a.rect(cx - ww * 0.5f, y0, ww, hh, 0.4f, R_GLASS, 0.7f, 0.25f);
    }
}
// ribs and a crown hatch over a containment dome already drawn with cyl()
void ribbedDome(Bld& b, float cx, float cy, float r) {
    for (int i = 0; i < 12; i++) {
        float an = i * 0.5236f;
        b.a.line(cx + std::cos(an) * r * 0.22f, cy + std::sin(an) * r * 0.22f, cx + std::cos(an) * r * 0.94f, cy + std::sin(an) * r * 0.94f, 0.8f, rgba(R_CONC_DK, 170));
    }
    b.a.ring(cx, cy, r * 0.62f, 0.5f, rgba(R_CONC_DK, 150), 0.5f, 0);
    b.a.circle(cx, cy, r * 0.2f, R_STEEL_D, 1.0f, 0.3f);   // crown hatch
}
// rooftop air cooler: a louvred steel box with a large fan on its lid
void coolUnit(Bld& b, float x, float y, float w, float h, float ht, float fr) {
    Roof u = b.block(x, y, w, h, ht, R_PANEL, R_STEEL);
    for (float yy = u.y + u.h + 2.0f; yy < u.y + u.h + ht - 1.0f; yy += 2.0f) b.a.line(u.x + 1.2f, yy, u.x + u.w - 1.2f, yy, 0.5f, rgba(BLACK, 70));   // louvres
    b.fan(u.x + u.w * 0.5f, u.y + u.h * 0.5f, fr, R_STEEL_D, R_STEEL_L);
}
// pad transformer: a finned tank with three bushings on its lid
void transformer(Bld& b, float x, float y, float w, float h) {
    Roof t = b.block(x, y, w, h, 5, R_STEEL_L, R_STEEL);
    b.wallSeams(t, 5, 2.2f, 80);   // cooling fins
    for (int i = 0; i < 3; i++) {
        float bx = t.x + 2.5f + i * (t.w - 5) * 0.5f;
        b.a.cap(bx, t.y + 2, bx, t.y - 2.2f, 0.9f, R_PANEL, 0.6f, 0.4f);
        b.a.dot(bx, t.y - 2.2f, 1.1f, R_PANEL);
    }
}
// lattice transmission pylon: two legs with diagonal bracing and a cross-arm carrying insulators
void pylon(Bld& b, float x, float base, float h) {
    float top = base - h, ay = top + h * 0.22f;
    b.a.line(x - 4.5f, base, x - 0.8f, top, 1.2f, R_STEEL_L); b.a.line(x + 4.5f, base, x + 0.8f, top, 1.2f, R_STEEL_L);
    for (int k = 1; k < 4; k++) {
        float yy = base - h * k * 0.25f, hw = 4.5f - 3.7f * k * 0.25f;
        b.a.line(x - hw, yy, x + hw, yy, 0.7f, R_STEEL_L);
    }
    b.a.line(x - 7, ay, x + 7, ay, 1.0f, R_STEEL_L);
    for (int s = -1; s <= 1; s += 2) {
        b.a.cap(x + s * 7.0f, ay, x + s * 7.0f, ay + 2.5f, 0.8f, R_PANEL, 0.5f, 0.4f);
        b.a.dot(x + s * 7.0f, ay + 2.5f, 0.9f, R_PANEL);
    }
}

// ---------------------------------------------------------------- Command Core
// a blast-hardened concrete bunker with a steel upper floor of tinted glass, a radome, dishes and a comms mast
void hqCyber(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(R_PAVE, R_PAVE_SEAM, 0.08f, 4);
    // lower floor: thick reinforced concrete, a parapet on the roof and narrow blast slits
    Roof lo = b.block(8, 56, 112, 62, 16, R_CONC_TOP, R_CONC);
    b.a.rect(lo.x + 3, lo.y + 3, lo.w - 6, lo.h - 6, 2, R_CONC, 1.6f, 0.5f);
    b.wallSeams(lo, 16, 14);
    b.windows(lo, 16, 14, R_GLASS, 0.22f);
    for (int i = 0; i < 6; i++) { float x = b.rng.f(12, W - 12.0f); b.a.line(x, lo.y + lo.h + 9, x, lo.y + lo.h + 15, 0.8f, rgba(BLACK, 22)); }   // weathering streaks
    b.door(64, 118, 16, 12, R_STEEL_D, rgb(20, 28, 36));
    b.light(64, 104.5f, 0.8f, C_CYAN, 70);
    // jersey barriers on the apron either side of the door
    b.block(42, 123, 14, 4, 3, R_CONC_TOP, R_CONC); b.block(72, 123, 14, 4, 3, R_CONC_TOP, R_CONC);
    // upper floor: steel cladding with tinted glass, its own roof with a helipad marking
    Roof up = b.block(30, 60, 68, 30, 12, R_PANEL, R_STEEL);
    b.wallSeams(up, 12, 8.5f);
    b.windows(up, 12, 8, R_GLASS, 0.6f);
    b.a.ring(64, 63, 9, 0.8f, rgba(R_STEEL, 220), 0.6f, 0);
    b.a.line(60.6f, 58.5f, 60.6f, 67.5f, 1.2f, rgba(R_STEEL, 220)); b.a.line(67.4f, 58.5f, 67.4f, 67.5f, 1.2f, rgba(R_STEEL, 220)); b.a.line(60.6f, 63, 67.4f, 63, 1.2f, rgba(R_STEEL, 220));
    b.antenna(up.x + 6, up.y + 9, 12, C_RED);
    b.mark(up.x + 4, up.y + 1.6f, up.w - 8, 3.2f);     // owner's colour band on the upper roof's north edge
    // radome on a drum, west of the upper floor
    P rd = b.cyl(19, 80, 8, 12, R_PANEL, R_CONC_TOP);
    b.domeTop(rd.x, rd.y, 7, R_PANEL);
    coolUnit(b, 10, 88, 16, 10, 5, 4.2f);
    // satellite dishes, cooling and a lattice comms mast on the lower roof
    b.dishTop(108, 57, 6.5f, R_STEEL_L, rgb(190, 200, 208));
    b.dishTop(111, 88, 4.5f, R_STEEL_L, rgb(190, 200, 208));
    coolUnit(b, 100, 70, 17, 11, 5, 4.4f);
    for (int s = -1; s <= 1; s += 2) b.a.line(103, 46, 103 + s * 5.0f, 17, 1.2f, R_STEEL_L);
    for (int k = 1; k < 4; k++) { float yy = 46 - k * 9.0f, hw = 5.0f * (46 - yy) / 29.0f; b.a.line(103 - hw, yy, 103 + hw, yy, 0.7f, R_STEEL_L); }
    b.antenna(103, 17, 8, C_RED);
    b.a.grain(0, 0, (float)W, (float)H, 0.05f, 7);
}

// ---------------------------------------------------------------- Fusion Reactor
// containment dome and a cylindrical vessel, cooling units on the back row, and a transformer yard with a lattice pylon
void powerCyber(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(R_PAVE, R_PAVE_SEAM, 0.08f, 4);
    coolUnit(b, 4, 6, 28, 12, 7, 5.2f);
    coolUnit(b, 38, 6, 22, 12, 7, 5.2f);
    b.mark(6, 13.2f, 9, 3.2f);     // owner's colour band on the first cooler's wall
    // ribbed containment dome, with a small cyan viewing port on its face
    P dome = b.cyl(28, 42, 12, 10, R_CONC_TOP, R_CONC);
    ribbedDome(b, dome.x, dome.y, 12);
    b.a.rect(25.5f, 38.5f, 5, 3.6f, 0.8f, R_GLASS, 0.8f, 0.3f);
    b.a.dot(28, 40.3f, 1.0f, C_CYAN); b.a.glow(28, 40.3f, 5, rgba(C_CYAN, 70));
    // steel containment vessel with banding
    P vs = b.cyl(70, 42, 10, 18, R_STEEL_L, R_CONC);
    b.a.line(vs.x - 9.4f, 34, vs.x + 9.4f, 34, 0.7f, rgba(BLACK, 60)); b.a.line(vs.x - 9.4f, 40, vs.x + 9.4f, 40, 0.7f, rgba(BLACK, 60));
    b.a.circle(vs.x, vs.y, 3.5f, R_STEEL, 1.2f, 0.4f);   // manway on the lid
    b.pipe(40, 36, 60, 36, 1.6f, R_STEEL_L); b.pipe(40, 40, 60, 40, 1.2f, R_STEEL);   // coolant lines between the vessels
    // transformer yard in front, with a pylon behind the cable run
    transformer(b, 6, 55, 13, 6); transformer(b, 23, 55, 13, 6); transformer(b, 40, 55, 13, 6);
    b.a.line(6, 63.5f, 84, 63.5f, 0.8f, rgba(BLACK, 130));   // trench cable
    pylon(b, 88, 62, 40);
    b.hazard(56, 62.2f, 22, 2.2f);
    b.a.grain(0, 0, (float)W, (float)H, 0.05f, 11);
}

// ---------------------------------------------------------------- Supply Hub
// steel-clad warehouse on a loading dock, roll-up doors, a control office, stacked containers, pallets and a forklift
void supplyCyber(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(R_PAVE, R_PAVE_SEAM, 0.08f, 4);
    // loading dock apron in front of the warehouse, edged in warning stripes
    b.a.rect(4, 53, 70, 10, 1.5f, R_CONC_TOP, 1.4f, 0.4f);
    b.hazard(4, 62.2f, 70, 2.2f);
    // warehouse: corrugated steel roof with skylights, wall cladding and three roll-up doors
    Roof wh = b.block(6, 6, 64, 46, 16, R_STEEL_L, R_STEEL);
    b.corrugated(wh, 4, R_STEEL_D, 80);
    b.skylights(wh, 4, R_GLASS_T, R_STEEL_D);
    b.wallSeams(wh, 16, 16);
    for (int i = 0; i < 3; i++) {
        float cx = 18 + i * 18.0f;
        b.door(cx, 52.6f, 12, 11, R_STEEL_D, rgb(24, 30, 38));
        for (float yy = 42; yy < 52; yy += 1.6f) b.a.line(cx - 5.4f, yy, cx + 5.4f, yy, 0.5f, rgba(R_STEEL_L, 120));   // roll-up slats
    }
    b.mark(wh.x + 4, wh.y + wh.h - 9.6f, 18, 3.2f);
    // control office on the east side
    Roof of = b.block(76, 10, 14, 34, 12, R_PANEL, R_STEEL);
    b.windows(of, 12, 2, R_GLASS, 0.6f);
    b.fan(83, 22, 3.6f, R_STEEL_D, R_STEEL_L);
    b.antenna(88, 2, 10, C_RED); b.light(90, 27, 0.8f, C_CYAN, 70);
    // stacked containers (blocks with ribbed lids) and loose containers in the yard
    Color cA = rgb(78, 100, 118), cB = rgb(96, 110, 124), cC = rgb(118, 126, 134);
    Roof s1 = b.block(8, 80, 24, 10, 9, cA, shade(cA, 0.72f)); b.corrugated(s1, 3, R_DARK, 60);
    Roof s2 = b.block(36, 80, 24, 10, 9, cB, shade(cB, 0.72f)); b.corrugated(s2, 3, R_DARK, 60);
    b.container(8, 90, 24, 6, cC); b.container(36, 90, 24, 6, cA);
    // pallets stacked by the dock
    Color tim = rgb(150, 132, 102);
    for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) b.crate(64 + i * 10.0f, 66 + j * 9.0f, 8, 7, shade(tim, 0.92f + 0.08f * (i + j)));
    // forklift parked on the dock
    b.a.rect(40, 66, 10, 6, 1.2f, rgb(214, 172, 74), 1.4f, 0.5f);
    b.a.line(41, 66.5f, 49, 66.5f, 0.6f, rgba(R_DARK, 150));
    b.a.line(50, 67, 56, 67, 0.8f, R_STEEL_D);
    b.a.dot(42, 72.5f, 1.0f, R_DARK); b.a.dot(48, 72.5f, 1.0f, R_DARK);
    b.a.grain(0, 0, (float)W, (float)H, 0.05f, 13);
}

// ---------------------------------------------------------------- Barracks
// two-storey barracks with flat roof plant, a water tank, an entrance canopy, a flagpole and a training yard
void barracksCyber(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(R_PAVE, R_PAVE_SEAM, 0.08f, 4);
    // training yard: gravel with tyres, a hurdle and a balance beam
    b.a.rect(4, 46, 40, 15, 1.5f, rgb(136, 130, 116), 1.2f, 0.4f);
    for (int i = 0; i < 3; i++) b.a.ring(10 + i * 9.0f, 56, 2.2f, 1.0f, rgb(40, 44, 50), 0.8f, 0.3f);   // tyres
    b.block(12, 47, 10, 3, 3, R_CONC_TOP, R_CONC);   // concrete hurdle
    b.a.line(30, 49.5f, 42, 49.5f, 1.6f, rgb(120, 98, 72));   // balance beam
    // two-storey block: pale walls with a floor band and two rows of windows, white ceramic roof
    Roof bk = b.block(4, 8, 88, 30, 20, R_PANEL, R_CONC);
    b.a.line(bk.x, 28, bk.x + bk.w, 28, 0.8f, rgba(BLACK, 70));
    winRow(b, bk.x + 4, 21, bk.w - 8, 10, 4);
    winRow(b, bk.x + 4, 31, bk.w - 8, 10, 4);
    b.door(48, 38.6f, 12, 10, R_STEEL_D, rgb(22, 30, 38));
    // canopy over the entrance on two posts
    b.a.shadowRect(40, 38.6f, 16, 4, 2, 2, 1.5f, 80);
    b.a.rect(40, 38.6f, 16, 3.2f, 0.8f, R_STEEL_L, 1.2f, 0.4f);
    b.a.rect(41, 41.8f, 1.6f, 2.4f, 0.4f, R_STEEL_D, 0.6f, 0.2f); b.a.rect(53.4f, 41.8f, 1.6f, 2.4f, 0.4f, R_STEEL_D, 0.6f, 0.2f);
    b.light(48, 40.2f, 0.7f, C_CYAN, 60);
    // concrete path from the entrance and a parade strip with lane lines
    b.a.rect(44, 44, 8, 18, 0.5f, R_CONC_TOP, 1.0f, 0.3f);
    b.a.rect(56, 48, 28, 12, 0.8f, R_CONC_TOP, 1.2f, 0.3f);
    for (int i = 0; i < 4; i++) b.a.line(58, 50.5f + i * 2.8f, 82, 50.5f + i * 2.8f, 0.5f, rgba(R_STEEL, 150));
    // roof plant: two louvred units with fans and a water tank on a steel stand
    coolUnit(b, 12, 4, 14, 9, 5, 3.2f);
    coolUnit(b, 60, 4, 14, 9, 5, 3.2f);
    P wt = b.cyl(38, 10, 6, 9, R_STEEL, R_STEEL_L);
    b.a.line(wt.x - 5.5f, wt.y - 1.5f, wt.x + 5.5f, wt.y - 1.5f, 0.6f, rgba(BLACK, 60));
    b.mark(bk.x + 4, bk.y + 1.4f, 20, 3.2f);     // owner's colour band on the roof's north edge
    b.antenna(bk.x + bk.w - 6, bk.y + 4, 12, C_RED);
    // flagpole in the front yard
    b.a.line(88, 60, 88, 30, 0.9f, R_STEEL_L); b.a.dot(88, 29.5f, 1.1f, R_PANEL);
    b.a.rect(88.5f, 31, 7, 4.6f, 0.3f, rgb(206, 214, 222), 0.6f, 0.2f);
    b.a.line(88.5f, 33.3f, 95.5f, 33.3f, 0.6f, rgba(R_STEEL, 160));
    b.a.grain(0, 0, (float)W, (float)H, 0.05f, 17);
}

// ---- CYBER realistic base structures (Assembly Plant, Drone Pad, Data Center, Bitcoin Datacenter). The palette and the f2 helpers are
// private to these painters: concrete slabs, white ceramic cladding, tinted glass and steel plant, with only a few small cyan lights.
const Color f2Conc = rgb(104, 112, 124), f2ConcSeam = rgb(62, 68, 78), f2ConcDk = rgb(84, 91, 102);   // slabs, joints, dark concrete
const Color f2Wall = rgb(182, 191, 200), f2WallDk = rgb(136, 146, 158), f2Roof = rgb(132, 144, 158), f2Unit = rgb(198, 206, 214);   // ceramic cladding, roof sheet, casings
const Color f2Steel = rgb(72, 80, 94), f2Dark = rgb(34, 40, 50), f2Glass = rgb(40, 60, 76), f2GlassHi = rgb(98, 124, 140), f2Paint = rgb(232, 236, 240);

// a white and charcoal striped frame round a rectangle (hazard tape in the Cyber palette)
void f2Frame(Bld& b, float x0, float y0, float x1, float y1) {
    b.hazard(x0, y0, x1 - x0, 2.4f, 4, f2Paint, f2Dark); b.hazard(x0, y1 - 2.4f, x1 - x0, 2.4f, 4, f2Paint, f2Dark);
    b.hazard(x0, y0, 2.4f, y1 - y0, 4, f2Paint, f2Dark); b.hazard(x1 - 2.4f, y0, 2.4f, y1 - y0, 4, f2Paint, f2Dark);
}
// horizontal louvre slats (plant screens, container sides, roller doors)
void f2Louvres(Bld& b, float x0, float y0, float x1, float y1, float pitch) {
    for (float y = y0; y < y1; y += pitch) { b.a.line(x0, y, x1, y, 0.6f, rgba(f2Dark, 150)); b.a.line(x0, y + 0.8f, x1, y + 0.8f, 0.4f, rgba(WHITE, 45)); }
}
// a tinted window with a dark reveal and a sky reflection along its top edge
void f2Win(Bld& b, float x, float y, float w, float h) {
    b.a.rect(x - 0.6f, y - 0.6f, w + 1.2f, h + 1.2f, 0.5f, rgba(BLACK, 120), 0.6f, 0);
    b.a.rect(x, y, w, h, 0.4f, f2Glass, 0.6f, 0.2f);
    b.a.line(x + 0.5f, y + 0.6f, x + w - 0.5f, y + 0.6f, 0.4f, rgba(f2GlassHi, 170));
}
// rooftop unit: a white casing with a fan grille or louvres
void f2Hvac(Bld& b, float x, float y, float w, float h, bool fan) {
    b.a.shadowRect(x, y, w, h, 1.2f, 1.0f, 1.2f, 80);
    b.a.rect(x, y, w, h, 0.8f, f2Unit, 1.6f, 0.5f);
    if (fan) b.fan(x + w * 0.5f, y + h * 0.5f, std::min(w, h) * 0.36f, f2Steel, f2Wall);
    else f2Louvres(b, x + 1.5f, y + 1.5f, x + w - 1.5f, y + h - 1.5f, 1.8f);
}
// two dark ruts a heavy tyre leaves on concrete, from (x0, y0) to (x1, y1)
void f2Ruts(Bld& b, float x0, float y0, float x1, float y1) {
    for (float dx : { -3.0f, 3.0f }) b.a.line(x0 + dx, y0, x1 + dx, y1, 1.2f, rgba(f2Dark, 75));
}
// chain-link compound fence: faint mesh, steel rails and posts
void f2Fence(Bld& b, float x0, float y0, float x1, float y1) {
    for (float x = x0; x <= x1; x += 3.0f) b.a.line(x, y0, x, y1, 0.35f, rgba(f2Steel, 90));
    for (float y = y0; y <= y1; y += 3.0f) b.a.line(x0, y, x1, y, 0.35f, rgba(f2Steel, 90));
    b.a.line(x0, y0, x1, y0, 0.9f, rgba(f2Steel, 230)); b.a.line(x0, y1, x1, y1, 0.9f, rgba(f2Steel, 230));
    b.a.line(x0, y0, x0, y1, 0.9f, rgba(f2Steel, 230)); b.a.line(x1, y0, x1, y1, 0.9f, rgba(f2Steel, 230));
    for (float x = x0; x <= x1; x += 8.0f) { b.a.dot(x, y0, 0.9f, f2Steel); b.a.dot(x, y1, 0.9f, f2Steel); }
    for (float y = y0; y <= y1; y += 8.0f) { b.a.dot(x0, y, 0.9f, f2Steel); b.a.dot(x1, y, 0.9f, f2Steel); }
}

void factoryCyber(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(f2Conc, f2ConcSeam, 0.08f, 4);
    // apron: truck ruts from the bay to the south edge, an oil drip and a striped kerb along the south edge
    f2Ruts(b, 56, 60, 52, 92); f2Ruts(b, 72, 60, 78, 92);
    b.oilStain(96, 78, 8, 3);
    b.hazard(86, H - 6.0f, 30, 3.0f, 4, f2Paint, f2Dark);
    // gantry crane over the apron: two legs, a girder, and a trolley with its hook down to the chassis
    b.a.rect(44, 66, 4, 22, 1, f2Steel, 1.4f, 0.5f); b.a.rect(118, 66, 4, 22, 1, f2Steel, 1.4f, 0.5f);
    b.a.rect(44, 65.5f, 78, 3.2f, 0.8f, f2Steel, 1.6f, 0.5f);
    b.a.rect(58, 64.4f, 12, 6, 1, f2Unit, 1.4f, 0.5f);
    b.a.line(64, 70.5f, 64, 79.0f, 0.6f, f2Dark); b.a.dot(64, 79.5f, 1.4f, f2Steel);
    // a heavy vehicle chassis under the hook: wheels, frame and a cab
    b.a.ell(56, 88.5f, 3.2f, 2.0f, f2Dark, 1, 0.3f); b.a.ell(72, 88.5f, 3.2f, 2.0f, f2Dark, 1, 0.3f);
    b.a.rect(52, 76, 28, 11, 2.5f, f2Steel, 1.6f, 0.5f); b.a.rect(54, 78, 24, 6.5f, 2, f2Wall, 1.4f, 0.4f);
    // assembly hall: a sawtooth roof of three teeth, each a glazed north slope over a solid south slope with lapped sheets
    Roof hall = b.block(8, 6, W - 16.0f, 52, 18, f2Roof, f2Wall);   // roof (8,-12,112,52), south wall 40..58
    for (int k = 0; k < 3; k++) {
        float th = hall.h / 3.0f, y0 = hall.y + k * th;
        b.a.rect(hall.x + 2, y0 + 1.5f, hall.w - 4, th * 0.42f, 0.6f, f2Glass, 0.8f, 0.2f);
        b.a.line(hall.x + 2, y0 + 1.5f, hall.x + hall.w - 2, y0 + 1.5f, 0.8f, rgba(f2GlassHi, 220));
        for (float x = hall.x + 9.0f; x < hall.x + hall.w - 4; x += 9.0f) b.a.line(x, y0 + 1.5f, x, y0 + 1.5f + th * 0.42f, 0.5f, rgba(f2Steel, 210));   // glazing bars
        for (float y = y0 + th * 0.42f + 3.0f; y < y0 + th - 0.5f; y += 2.5f) b.a.line(hall.x + 2, y, hall.x + hall.w - 2, y, 0.5f, rgba(f2Dark, 70));   // sheet laps
    }
    // roof ventilators and two exhaust stacks (one with a red aviation light)
    P s1 = b.cyl(30, 2, 3.6f, 22, f2Unit, f2Steel);
    b.a.ring(s1.x, s1.y, 2.6f, 0.6f, f2Dark, 0.5f, 0.2f); b.light(s1.x, s1.y, 0.7f, C_RED, 60);
    P s2 = b.cyl(46, 4, 3.2f, 16, f2Unit, f2Steel);
    b.a.ring(s2.x, s2.y, 2.2f, 0.6f, f2Dark, 0.5f, 0.2f);
    for (float x : { 70.0f, 88.0f, 106.0f }) { P v = b.cyl(x, 36, 4.0f, 4, f2Unit, f2Steel); b.a.dot(v.x, v.y, 2.0f, f2Dark); }
    // walls: seams, a dark plinth at the foot, clerestory windows either side of the bay
    b.wallSeams(hall, 18, 12, 90);
    b.a.rect(hall.x, 52, hall.w, 6, 0.3f, f2WallDk, 1.0f, 0.3f);
    for (float x = 14.0f; x < 40.0f; x += 6.0f) f2Win(b, x, 43.6f, 4.0f, 5.8f);
    for (float x = 90.0f; x < 114.0f; x += 6.0f) f2Win(b, x, 43.6f, 4.0f, 5.8f);
    // vehicle bay door: a roller shutter in a white and charcoal striped frame, with one cyan light above
    f2Frame(b, 41.5f, 41.5f, 86.5f, 58.0f);
    b.a.rect(43.8f, 43.8f, 40.4f, 14.2f, 0.5f, rgb(22, 28, 36), 1.0f, 0.2f);
    for (float y = 45.5f; y < 58.0f; y += 2.2f) b.a.line(44.0f, y, 84.0f, y, 0.6f, rgba(f2Steel, 200));
    b.light(64, 38.6f, 0.9f, C_CYAN, 60);
    b.mark(hall.x + hall.w - 26, hall.y + 2.2f, 22, 3.6f);   // owner's colour band on the top of the glazing
    // paint and ventilation annex to the west: louvred intake, exhaust fan and a rooftop unit
    Roof an = b.block(8, 66, 32, 24, 9, f2Roof, f2WallDk);   // roof (8,57,32,24), wall 81..90
    b.fan(an.x + 9, an.y + 9, 6, f2Steel, f2Wall); f2Hvac(b, an.x + 18, an.y + 4, 11, 8, false);
    f2Louvres(b, an.x + 4, 82.5f, an.x + an.w - 4, 88.5f, 2.2f);   // intake louvres on the annex wall
    // cooling water tank on the south-east of the apron
    P tank = b.cyl(106, 86, 9, 14, f2Unit, f2Steel);
    b.a.ring(tank.x, tank.y, 6.5f, 0.8f, f2Steel, 0.6f, 0.2f);
    b.a.grain(0, 0, (float)W, (float)H, 0.05f, 19);
}

void airfieldCyber(Bld& b) {
    int W = b.W, H = b.H;
    // concrete apron: slab joints both ways, tyre scuffs and oil drips in the gaps between the pads
    b.a.shadowRect(0, 0, (float)W, (float)H, 3, 3, 4, 85);
    b.a.rect(0, 0, (float)W, (float)H, 4, f2Conc, 2.6f, 0.5f);
    for (int i = 0; i < 60; i++) { float x = b.rng.f(4, W - 4.0f), y = b.rng.f(4, H - 4.0f); b.a.ell(x, y, b.rng.f(3, 9), b.rng.f(1.5f, 3.5f), rgba(f2ConcDk, 70), 1, 0); }
    for (int x = 48; x < W; x += 48) b.a.line((float)x, 2, (float)x, H - 2.0f, 0.7f, rgba(f2ConcSeam, 110));
    for (int y = 56; y < H; y += 56) b.a.line(2, (float)y, W - 2.0f, (float)y, 0.7f, rgba(f2ConcSeam, 110));
    b.oilStain(97, 61, 8, 3); b.oilStain(139, 95, 8, 3); b.oilStain(181, 129, 8, 3); b.oilStain(223, 163, 8, 3); b.oilStain(265, 61, 8, 3); b.oilStain(265, 129, 8, 3);
    b.a.grain(0, 0, (float)W, (float)H, 0.10f, 23);
    // two runways with white centre dashes
    for (int r = 0; r < 2; r++) {
        float y = H * (0.36f + 0.28f * r);
        for (int i = 0; i < W / 14; i++) b.a.rect(8 + i * 14.0f, y - 0.9f, 8, 1.8f, 0.3f, rgba(f2Paint, 150), 0.5f, 0);
    }
    // 25 landing pads in the same 5x5 grid the aircraft use (Sim::padSlot): painted rings on dark concrete, an H in each
    for (int row = 0; row < 5; row++) for (int col = 0; col < 5; col++) {
        float cx = W * 0.5f + (col - 2) * 42, cy = H * 0.5f + (row - 2) * 34;
        b.a.circle(cx, cy, 12.5f, rgb(82, 88, 98), 1.6f, 0.35f);
        b.a.ring(cx, cy, 10.8f, 0.8f, rgba(f2Paint, 150), 0.6f, 0);
        b.a.line(cx - 4, cy - 4.5f, cx - 4, cy + 4.5f, 1.3f, rgba(f2Paint, 185)); b.a.line(cx + 4, cy - 4.5f, cx + 4, cy + 4.5f, 1.3f, rgba(f2Paint, 185)); b.a.line(cx - 4, cy, cx + 4, cy, 1.3f, rgba(f2Paint, 185));
    }
    for (int i = 0; i < 18; i++) { float x = 8 + i * (W - 16.0f) / 17; b.light(x, 4.5f, 0.8f, C_CYAN, 60); b.light(x, H - 4.5f, 0.8f, C_CYAN, 60); }
    // control tower: a concrete shaft with a glazed cab, a mast and a team band on the cab roof
    Roof tw = b.block(10, 12, 26, 18, 24, f2Roof, f2Wall);   // roof (10,-12,26,18), shaft wall 6..30
    b.wallSeams(tw, 24, 9, 90);
    b.windows(tw, 24, 2, f2Glass, 0.5f);
    b.door(23, 30, 8, 9, f2Steel, rgb(22, 28, 36));
    Roof cab = b.block(tw.x + 3, tw.y + 3, tw.w - 6, tw.h - 6, 9, f2Steel, f2Glass);   // the cab: its walls are tinted glass
    for (float x = cab.x + 5.0f; x < cab.x + cab.w - 2; x += 5.0f) b.a.line(x, cab.y + cab.h - 9.0f, x, cab.y + cab.h, 0.5f, rgba(f2Steel, 200));
    b.mark(cab.x + 2, cab.y - 7.5f, cab.w - 4, 2.6f);
    b.antenna(tw.x + tw.w - 2, tw.y + 1, 12, C_RED);
    // hangar with a curved roof (light on the crown, falling off to the south) and a roller door in a striped frame
    Roof hg = b.block(W - 126.0f, 6, 96, 20, 15, f2Unit, f2WallDk, 6);   // roof (194,-9,96,20), wall 11..26
    b.a.ramp(hg.x, hg.y, hg.x + hg.w, hg.y + hg.h, 1.10f, 0.84f);
    for (int i = 0; i < 13; i++) { float x = hg.x + 5 + i * 7.2f; b.a.line(x, hg.y + 1, x, hg.y + hg.h - 1, 0.6f, rgba(f2Steel, 130)); b.a.line(x + 0.8f, hg.y + 1, x + 0.8f, hg.y + hg.h - 1, 0.4f, rgba(WHITE, 60)); }
    f2Frame(b, 207.5f, 12.5f, 276.5f, 26.0f);
    b.a.rect(210, 15, 64, 11, 0.5f, rgb(22, 28, 36), 1.0f, 0.2f);
    for (float y = 16.5f; y < 26.0f; y += 2.0f) b.a.line(211, y, 273, y, 0.5f, rgba(f2Steel, 200));
    b.mark(hg.x + hg.w - 16, hg.y + 2, 12, 3);
    // munitions shelters on the south-west: a low block with four roller doors and two rooftop units
    Roof sv = b.block(10, H - 42.0f, 62, 26, 9, f2Roof, f2WallDk);   // roof (10,173,62,26), wall 199..208
    f2Hvac(b, 14, 178, 14, 8, true); f2Hvac(b, 34, 178, 14, 8, false);
    b.mark(sv.x + 3, sv.y + sv.h - 5.2f, 20, 3.4f);
    for (int i = 0; i < 4; i++) { float x = sv.x + 6 + i * 14.0f; b.a.rect(x, 200.5f, 9, 7, 0.4f, f2Dark, 1, 0.2f); f2Louvres(b, x + 0.6f, 201.2f, x + 8.4f, 206.8f, 1.7f); }
    // fuel farm on the south-east: two white tanks in a raised concrete bund, a bowser with a red band
    b.a.rect(264, 190, 42, 22, 1.5f, f2ConcDk, 2.0f, 0.4f);
    P t1 = b.cyl(276, 206, 6, 10, f2Unit, f2Wall); b.a.ring(t1.x, t1.y, 4.2f, 0.7f, f2Steel, 0.5f, 0.2f);
    P t2 = b.cyl(294, 206, 6, 10, f2Unit, f2Wall); b.a.ring(t2.x, t2.y, 4.2f, 0.7f, f2Steel, 0.5f, 0.2f);
    b.block(262, 174, 16, 9, 5, f2Unit, f2WallDk);   // bowser: a box with a cab
    b.stripe(264, 172.6f, 276, 172.6f, 1.2f, C_RED, 0);
    b.pipe(278, 182, 282, 196, 0.8f, f2Dark);
    b.hazard(W - 70.0f, H - 12.0f, 58, 3.0f, 5, f2Paint, f2Dark);
    b.a.grain(0, 0, (float)W, (float)H, 0.05f, 29);
}

void techCyber(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(f2Conc, f2ConcSeam, 0.08f, 4);
    // fenced compound on the south side: three backup generators, each with an exhaust stack, and a day tank
    f2Fence(b, 4, 66, 92, 92);
    for (int i = 0; i < 3; i++) {
        float x = 10 + i * 26.0f, y = 74;
        b.a.shadowRect(x, y, 18, 12, 1.4f, 1.2f, 1.4f, 90);
        b.a.rect(x, y, 18, 12, 1.0f, f2Steel, 1.8f, 0.5f);   // generator canopy
        f2Louvres(b, x + 2, y + 2, x + 16, y + 10, 1.8f);
        b.pipe(x + 14, y + 1, x + 14, y - 5, 1.0f, f2Unit);
    }
    P dt = b.cyl(84, 84, 4, 8, f2Unit, f2Steel); b.a.dot(dt.x, dt.y, 1.2f, f2Dark);
    // secure research block: tinted glass bands on the south wall, a reinforced plinth and a card-reader door
    Roof hall = b.block(8, 14, 80, 48, 14, f2Roof, f2Wall);   // roof (8,0,80,48), south wall 48..62
    // rooftop HVAC in two rows, cooling fans on some of them; the north-east corner is kept clear for the live dish
    for (float x : { 12.0f, 24.0f, 36.0f }) f2Hvac(b, x, 28, 10, 7, x == 24.0f);
    for (float x : { 12.0f, 24.0f, 36.0f, 48.0f, 60.0f, 72.0f }) f2Hvac(b, x, 38, 10, 7, x == 24.0f || x == 60.0f);
    // antenna farm and two satellite dishes on the north-west roof
    b.dishTop(22, 9, 5.0f, f2Unit, rgb(86, 96, 110)); b.dishTop(34, 14, 3.8f, f2Unit, rgb(86, 96, 110));
    b.antenna(12, 10, 22, C_RED); b.antenna(44, 8, 16, C_CYAN);
    // mount under the live-rotating dish (its pivot sits at (70,12) on the roof)
    b.a.circle(70, 12, 9.2f, f2Steel, 1.6f, 0.5f); b.a.circle(70, 12, 6, f2Roof, 1.6f, 0.5f);
    b.wallSeams(hall, 14, 16, 90);
    b.a.rect(hall.x, 58, hall.w, 4, 0.3f, f2WallDk, 1.0f, 0.3f);
    f2Win(b, 12, 50, 22, 5); f2Win(b, 48, 50, 36, 5);
    for (float x = 17.0f; x < 34.0f; x += 5.5f) b.a.line(x, 50.6f, x, 54.4f, 0.5f, rgba(f2Steel, 200));
    for (float x = 53.0f; x < 84.0f; x += 5.5f) b.a.line(x, 50.6f, x, 54.4f, 0.5f, rgba(f2Steel, 200));
    b.door(40, 62, 12, 11, f2Steel, rgb(16, 22, 30));
    b.a.rect(29.5f, 54.5f, 2.4f, 4, 0.3f, f2Dark, 0.6f, 0.2f); b.a.dot(30.7f, 55.6f, 0.5f, C_CYAN);   // card reader
    b.mark(56, 59, 22, 2.8f);
    b.a.grain(0, 0, (float)W, (float)H, 0.05f, 31);
}

void laserBase(Bld& b) {   // 1x1 turret pedestal: armoured ring on a hex pad
    b.a.shadowCircle(16, 16, 15, 2.5f, 2.5f, 3, 95);
    b.a.ngon(16, 16, 15.4f, 8, 0.3927f, C_PAD, 2.2f, 0.5f);
    b.a.circle(16, 16, 12.2f, C_STEEL, 2.4f, 0.6f);
    b.a.ring(16, 16, 10.2f, 1.0f, rgba(C_CYAN, 210), 0.8f, 0); b.a.glow(16, 16, 15, rgba(C_CYAN, 70));
    b.a.circle(16, 16, 9, C_ROOF2, 2.4f, 0.55f);
    for (int i = 0; i < 4; i++) { float an = i * 1.5708f + 0.785f; b.a.dot(16 + std::cos(an) * 13.2f, 16 + std::sin(an) * 13.2f, 1.2f, C_ROOF); }
    b.markDisc(16, 27.5f, 1.6f);
}

void patriotCyber(Bld& b) {   // 2x2: armoured plinth with generator and radar mast; the launcher head rotates over the middle
    int W = b.W;
    b.pad(C_PAD, C_SEAM, 0.1f, 5);
    Roof pl = b.block(10, 18, 44, 36, 7, C_ROOF2, C_HULL_D);
    b.a.rect(pl.x + 3, pl.y + 3, pl.w - 6, pl.h - 6, 2, C_STEEL, 1.4f, 0.4f);
    b.stripe(pl.x + 5, pl.y + 5, pl.x + pl.w - 5, pl.y + 5, 0.7f, C_CYAN, 70);
    b.windows(pl, 7, 5, C_CYAN, 0.5f);
    Roof gen = b.block(4, 6, 14, 10, 6, C_ROOF, C_HULL_D); b.vent(gen.x + 2, gen.y + 2, 10, 6, rgba(C_CYAN, 150));
    b.antenna(W - 8.0f, 14, 22, C_RED); b.dishTop(W - 12.0f, 8, 4.4f, C_ROOF2, rgb(30, 40, 56));
    for (int i = 0; i < 4; i++) { float x = i < 2 ? 4.0f : W - 8.0f, y = (i & 1) ? 54.0f : 24.0f; b.a.rect(x, y, 4, 6, 0.8f, C_ROOF, 1.2f, 0.5f); }
    b.mark(W - 22.0f, 54, 14, 3.2f);
}

void minerCyber(Bld& b) {   // containerised server farm: three louvred containers with chillers and big fans, a painted coin, a transformer
    int W = b.W;
    b.pad(f2Conc, f2ConcSeam, 0.08f, 4);
    for (int i = 0; i < 3; i++) {
        float x = 3 + i * 31.0f;
        b.block(x, 6, 30, 34, 12, f2Roof, f2Wall);   // roof (x,-6,30,34), louvred south wall 28..40
        f2Louvres(b, x + 2.5f, 30.5f, x + 27.5f, 38.5f, 2.2f);
        b.a.line(x + 1.2f, 28.6f, x + 1.2f, 39.5f, 0.6f, rgba(f2Dark, 120)); b.a.line(x + 28.8f, 28.6f, x + 28.8f, 39.5f, 0.6f, rgba(f2Dark, 120));   // corner posts
        // rooftop chiller with two big fans, a roof vent and a small cyan status light on the wall
        b.a.rect(x + 3, 16, 24, 11, 1.2f, f2Unit, 1.6f, 0.5f);
        b.fan(x + 9, 21.5f, 4.4f, f2Steel, f2Wall); b.fan(x + 21, 21.5f, 4.4f, f2Steel, f2Wall);
        b.vent(x + 16, -3.5f, 11, 3, rgba(WHITE, 70));
        b.light(x + 26.0f, 33.0f, 0.7f, C_CYAN, 40);
        if (i == 0) b.mark(x + 3, -4.2f, 11, 2.6f);   // owner's colour band on the first container's roof
    }
    // the coin: a small painted gold disc with a B-stroke on the middle container's roof (its centre is the live glow anchor)
    float cx = 48.0f, cy = 10.0f;
    b.a.circle(cx, cy, 5.6f, rgb(170, 124, 40), 0.7f, 0.12f);
    b.a.circle(cx, cy, 4.7f, rgb(214, 170, 82), 0.6f, 0.08f);
    b.a.line(cx - 1.4f, cy - 3.0f, cx - 1.4f, cy + 3.0f, 0.9f, rgb(118, 80, 24));
    b.a.line(cx - 1.4f, cy - 2.2f, cx + 1.6f, cy - 2.2f, 0.8f, rgb(118, 80, 24)); b.a.line(cx - 1.4f, cy + 0.4f, cx + 1.9f, cy + 0.4f, 0.8f, rgb(118, 80, 24)); b.a.line(cx - 1.4f, cy + 2.4f, cx + 1.4f, cy + 2.4f, 0.8f, rgb(118, 80, 24));
    b.a.line(cx - 0.4f, cy - 4.8f, cx - 0.4f, cy - 3.2f, 0.7f, rgb(118, 80, 24)); b.a.line(cx + 0.9f, cy - 4.8f, cx + 0.9f, cy - 3.2f, 0.7f, rgb(118, 80, 24));
    b.a.line(cx - 0.4f, cy + 3.2f, cx - 0.4f, cy + 4.8f, 0.7f, rgb(118, 80, 24)); b.a.line(cx + 0.9f, cy + 3.2f, cx + 0.9f, cy + 4.8f, 0.7f, rgb(118, 80, 24));
    // cable tray and coolant loop along the front of the containers
    b.a.rect(3, 41.4f, W - 6.0f, 2.2f, 0.4f, f2Steel, 0.8f, 0.3f);
    for (float x = 6.0f; x < W - 4.0f; x += 4.0f) b.a.line(x, 41.8f, x, 43.2f, 0.4f, rgba(f2Dark, 150));
    b.pipe(3, 46.0f, W - 3.0f, 46.0f, 1.3f, f2Steel); b.pipe(3, 49.0f, W - 3.0f, 49.0f, 1.0f, rgb(150, 178, 196));
    // two dry coolers with fans on the south-west
    for (int i = 0; i < 2; i++) {
        float x = 6 + i * 16.0f;
        b.block(x, 52, 13, 9, 7, f2Roof, f2WallDk);
        b.fan(x + 6.5f, 56.5f, 3.4f, f2Steel, f2Wall);
    }
    // transformer on the south-east: radiator fins on the tank, bushings on the lid, a team band on the lid
    b.block(64, 52, 26, 12, 8, f2Roof, f2Wall);   // lid 44..56, tank 56..64
    for (float x = 67.0f; x < 89.0f; x += 2.2f) b.a.line(x, 56.6f, x, 63.4f, 0.5f, rgba(f2Dark, 120));
    for (float dx : { 69.0f, 75.0f, 81.0f }) b.a.cap(dx, 44.0f, dx, 40.5f, 0.9f, f2Unit, 0.8f, 0.5f);
    b.mark(66, 46.5f, 12, 2.6f);
    b.a.grain(0, 0, (float)W, 64.0f, 0.05f, 37);
}
void nukeCyber(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(C_PAD, C_SEAM);
    b.hazard(4, 4, W - 8.0f, 3.4f); b.hazard(4, H - 7.4f, W - 8.0f, 3.4f);
    // silo pit with two sliding doors drawn partly open, the warhead showing in the shaft
    float cx = 48, cy = 50;
    b.a.circle(cx, cy, 29, C_PAD2, 2.6f, 0.5f);
    b.a.circle(cx, cy, 24, rgb(10, 12, 16), 1.6f, 0.3f);
    for (int i = 0; i < 3; i++) b.a.ring(cx, cy, 8.0f + i * 5.5f, 0.8f, rgba(C_CYAN, 60), 0.6f, 0);   // the empty shaft: concentric guide rings
    for (int i = 0; i < 12; i++) { float an = i * 0.5236f; b.a.dot(cx + std::cos(an) * 22.0f, cy + std::sin(an) * 22.0f, 0.9f, i % 3 ? rgba(C_CYAN, 160) : rgba(C_RED, 200)); }
    for (int s = -1; s <= 1; s += 2) {   // blast doors, slid aside to either hand
        b.a.rect(cx + s * 21 - 12, cy - 22, 24, 44, 3, C_ROOF2, 2.2f, 0.55f);
        b.a.rect(cx + s * 21 - 10, cy - 20, 20, 40, 2, C_STEEL, 1.4f, 0.4f);
        b.a.line(cx + s * 21, cy - 19, cx + s * 21, cy + 19, 0.7f, rgba(C_CYAN, 130));
    }
    b.a.ring(cx, cy, 27, 1.2f, rgba(C_RED, 200), 0.8f, 0);
    // gantry towers and fuel tanks
    Roof g1 = b.block(5, 12, 12, 56, 18, C_ROOF, C_HULL_D); Roof g2 = b.block(W - 17.0f, 12, 12, 56, 18, C_ROOF, C_HULL_D);
    for (Roof* r : { &g1, &g2 }) { for (int i = 0; i < 5; i++) b.a.line(r->x + 1, r->y + 4 + i * 10.0f, r->x + r->w - 1, r->y + 4 + i * 10.0f, 0.7f, rgba(C_STEEL, 160)); b.light(r->x + r->w * 0.5f, r->y + 3, 1.2f, C_RED, 130); b.windows(*r, 18, 1, C_CYAN, 0.6f); }
    b.cyl(34, H - 8.0f, 6, 7, C_ROOF, C_HULL); b.cyl(62, H - 8.0f, 6, 7, C_ROOF, C_HULL);
    b.mark(g1.x + 1.5f, g1.y + g1.h - 6, 9, 3); b.mark(g2.x + 1.5f, g2.y + g2.h - 6, 9, 3);
    b.a.grain(0, 0, (float)W, (float)H, 0.05f, 41);
}

// ================================================================= CLANKER
void hqClanker(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(K_PAD, K_SEAM, 0.16f, 3);
    b.rust(4, 4, W - 4.0f, H - 4.0f, 14, 50);
    // rear fuel tanks
    for (int i = 0; i < 3; i++) { P tp = b.cyl(34 + i * 30.0f, 22, 8, 8, shade(K_STEEL, 1.1f), K_RUST); b.a.ring(tp.x, tp.y, 5, 0.8f, K_DARK, 0.6f, 0.2f); b.a.dot(tp.x, tp.y, 1.3f, K_YEL); }
    // radio shack, generator shed and the main bunker
    Roof rs = b.block(8, 44, 24, 28, 10, K_ROOF, K_OLIVE_D);
    b.corrugated(rs, 3.2f, K_DARK); b.windows(rs, 10, 2, rgb(255, 214, 120), 0.5f);
    Roof gs = b.block(96, 48, 24, 32, 10, K_ROOF, K_OLIVE_D);
    b.corrugated(gs, 3.2f, K_DARK); b.a.circle(gs.x + 8, gs.y + 8, 4, K_STEEL, 1.6f, 0.55f); b.a.dot(gs.x + 8, gs.y + 8, 1.6f, K_DARK);
    b.pipe(gs.x + 8, gs.y + 8, gs.x + 8, gs.y - 12, 2.2f, K_STEEL);
    Roof mb = b.block(34, 36, 60, 56, 16, K_KHAKI, K_OLIVE);
    b.a.rect(mb.x + 3, mb.y + 3, mb.w - 6, mb.h - 6, 2, K_ROOF, 1.8f, 0.5f);
    b.corrugated(Roof{mb.x + 3, mb.y + 3, mb.w - 6, mb.h - 6}, 4.0f, K_DARK);
    b.a.rect(mb.x + 6, mb.y + 6, 12, 9, 1, K_STEEL, 1.4f, 0.5f); b.a.line(mb.x + 12, mb.y + 6, mb.x + 12, mb.y + 15, 0.6f, rgba(K_DARK, 160));   // roof hatch
    b.a.circle(mb.x + mb.w - 12, mb.y + 12, 5, K_STEEL, 1.6f, 0.55f); b.a.dot(mb.x + mb.w - 12, mb.y + 12, 2, K_DARK);                              // vent stack
    b.a.rect(mb.x + 6, mb.y + mb.h - 14, 22, 8, 1, K_OLIVE_D, 1.4f, 0.4f);
    b.mark(mb.x + mb.w - 26, mb.y + mb.h - 13, 20, 6);
    for (int i = 0; i < 4; i++) b.a.rect(mb.x + 6 + i * 13.0f, mb.y + mb.h + 16 * 0.25f, 8, 2.6f, 0.5f, rgb(255, 214, 120), 0.8f, 0.2f);   // firing slits on the south face
    b.door(64, mb.y + mb.h + 16, 14, 12, K_DARK, rgb(20, 20, 16));
    b.hazard(55, mb.y + mb.h + 16 + 1.5f, 18, 3);
    // radio mast with guy wires and a dish
    b.antenna(rs.x + 6, rs.y + 6, 34, K_RED);
    b.a.line(rs.x + 6, rs.y + 6 - 28, rs.x - 3, rs.y + 14, 0.5f, rgba(K_DARK, 190)); b.a.line(rs.x + 6, rs.y + 6 - 28, rs.x + 20, rs.y + 14, 0.5f, rgba(K_DARK, 190));
    b.a.circle(rs.x + 17, rs.y + 17, 7.2f, K_STEEL, 1.6f, 0.5f); b.a.circle(rs.x + 17, rs.y + 17, 4.8f, shade(K_STEEL, 0.7f), 1.4f, 0.4f);   // turntable for the live radar bar
    for (int i = 0; i < 5; i++) b.barrel(8 + (i % 2) * 6.0f, 82 + (i / 2) * 5.0f, 2.8f, i % 2 ? K_RUST : K_OLIVE_D);
    // sandbag perimeter with the gate left open
    b.sandbags(4, H - 5.0f, 44, H - 5.0f); b.sandbags(84, H - 5.0f, W - 4.0f, H - 5.0f); b.sandbags(4, 8, 4, H - 6.0f); b.sandbags(W - 4.0f, 8, W - 4.0f, H - 6.0f);
    b.a.grain(0, 0, (float)W, (float)H, 0.10f, 43);
}

void powerClanker(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(K_PAD, K_SEAM, 0.16f, 3);
    b.oilStain(30, 54, 14, 4);
    Roof hall = b.block(5, 12, 62, 38, 14, K_ROOF, K_OLIVE_D);
    b.corrugated(hall, 3.2f, K_DARK); b.rust(hall.x, hall.y, hall.x + hall.w, hall.y + hall.h, 7, 80);
    b.vent(hall.x + 5, hall.y + 6, 16, 8, rgba(WHITE, 90));
    b.windows(hall, 14, 4, rgb(255, 214, 120), 0.5f); b.wallSeams(hall, 14, 14);
    b.door(36, hall.y + hall.h + 14, 20, 12, K_DARK, rgb(24, 22, 18)); b.hazard(26, hall.y + hall.h + 14 + 1.2f, 20, 3);
    b.mark(hall.x + hall.w - 22, hall.y + 3, 18, 3.6f);
    // exhaust stacks and the day tank
    for (int i = 0; i < 2; i++) { P tp = b.cyl(76 + i * 11.0f, 34, 4.8f, 26 - i * 4, rgb(50, 46, 42), K_RUST); b.a.ring(tp.x, tp.y, 3.4f, 0.8f, K_DARK, 0.5f, 0.2f); b.a.glow(tp.x, tp.y, 7, rgba(rgb(255, 120, 40), 60)); }
    P tank = b.cyl(78, 52, 8, 11, K_STEEL, K_OLIVE); b.a.dot(tank.x, tank.y, 2.2f, K_YEL);
    b.pipe(66, 40, 74, 48, 1.8f, K_STEEL); b.pipe(hall.x + hall.w, hall.y + 10, 76, 30, 1.6f, K_RUST);
    for (int i = 0; i < 3; i++) b.barrel(8 + i * 5.0f, 58, 2.6f, i == 1 ? K_RUST : K_OLIVE_D);
    b.a.grain(0, 0, (float)W, (float)H, 0.10f, 47);
}

void supplyClanker(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(K_PAD, K_SEAM, 0.16f, 3);
    b.rust(4, 50, W - 4.0f, H - 4.0f, 10, 45);
    Roof wh = b.block(8, 8, W - 16.0f, 42, 14, K_ROOF, K_OLIVE_D);
    b.corrugated(wh, 3.4f, K_DARK); b.a.line(wh.x + 1, wh.y + wh.h * 0.5f, wh.x + wh.w - 1, wh.y + wh.h * 0.5f, 1.0f, rgba(WHITE, 40));
    b.rust(wh.x, wh.y, wh.x + wh.w, wh.y + wh.h, 8, 80);
    b.wallSeams(wh, 14, 10);
    b.door(28, wh.y + wh.h + 14, 24, 12, K_DARK, rgb(22, 20, 16)); b.door(64, wh.y + wh.h + 14, 16, 11, K_DARK, rgb(22, 20, 16));
    b.hazard(16, wh.y + wh.h + 14 + 1.2f, 24, 3);
    b.mark(wh.x + wh.w - 24, wh.y + 3, 20, 3.6f);
    b.vent(wh.x + 8, wh.y + 8, 10, 6, rgba(WHITE, 80));
    // stacked pallets and a tarp-covered heap in the yard
    for (int j = 0; j < 2; j++) for (int i = 0; i < 4; i++) b.crate(10 + i * 11.0f, 64 + j * 11.0f, 9, 9, shade(rgb(158, 124, 74), 0.9f + ((i + j) % 3) * 0.08f));
    b.a.ell(70, 78, 15, 9, K_TARP, 3, 0.6f); b.a.line(58, 77, 82, 80, 0.8f, rgba(K_DARK, 110)); b.a.line(64, 71, 76, 85, 0.8f, rgba(K_DARK, 90));
    b.a.rect(56, 64, 3, 3, 0.4f, K_STEEL, 1, 0.5f); b.pipe(57.5f, 65.5f, 88, 65.5f, 1.0f, K_STEEL);   // small derrick arm
    b.sandbags(8, H - 5.0f, 40, H - 5.0f);
    b.a.grain(0, 0, (float)W, (float)H, 0.10f, 53);
}

void barracksClanker(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(K_PAD, K_SEAM, 0.16f, 3);
    Roof hut = b.block(6, 8, W - 12.0f, 36, 12, K_ROOF, K_OLIVE_D);
    // pitched roof: the north slope catches the light, the south slope sits in shade, a ridge between
    b.a.rect(hut.x, hut.y + hut.h * 0.5f, hut.w, hut.h * 0.5f, 1.2f, shade(K_ROOF, 0.78f), 1.4f, 0.4f);
    b.a.line(hut.x + 1, hut.y + hut.h * 0.5f, hut.x + hut.w - 1, hut.y + hut.h * 0.5f, 1.4f, rgba(WHITE, 60));
    b.corrugated(hut, 3.2f, K_DARK, 70); b.rust(hut.x, hut.y, hut.x + hut.w, hut.y + hut.h, 6, 70);
    b.wallSeams(hut, 12, 12); b.windows(hut, 12, 6, rgb(255, 214, 120), 0.4f);
    b.door(W * 0.5f, hut.y + hut.h + 12, 12, 10, K_DARK, rgb(22, 20, 16));
    P ch = b.cyl(hut.x + 12, hut.y + 12, 3.4f, 9, rgb(50, 46, 42), K_BRICK); (void)ch;
    b.mark(hut.x + hut.w - 22, hut.y + 3, 18, 3.4f);
    b.antenna(hut.x + hut.w - 6, hut.y + 10, 14, K_RED);
    b.sandbags(W * 0.5f - 18, hut.y + hut.h + 17, W * 0.5f - 7, hut.y + hut.h + 17, 2); b.sandbags(W * 0.5f + 7, hut.y + hut.h + 17, W * 0.5f + 18, hut.y + hut.h + 17, 2);
    for (int i = 0; i < 3; i++) b.crate(6 + i * 9.0f, 52, 7, 7, shade(rgb(158, 124, 74), 0.88f + i * 0.08f));
    b.barrel(W - 10.0f, 56, 3, K_RUST); b.barrel(W - 16.0f, 57, 3, K_OLIVE_D);
    b.a.grain(0, 0, (float)W, (float)H, 0.10f, 59);
}

void factoryClanker(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(K_PAD, K_SEAM, 0.18f, 3);
    b.oilStain(70, 80, 22, 5); b.rust(4, 60, W - 4.0f, H - 4.0f, 12, 45);
    Roof shed = b.block(8, 8, W - 16.0f, 48, 20, K_ROOF, K_OLIVE_D);
    // sawtooth roof: alternating bands, each lit on its north slope
    for (int i = 0; i < 6; i++) { float x = shed.x + 3 + i * (shed.w - 6) / 6, w = (shed.w - 6) / 6; b.a.rect(x, shed.y + 3, w * 0.6f, shed.h - 6, 0.5f, i % 2 ? shade(K_ROOF, 0.82f) : shade(K_RUST, 1.0f), 1.1f, 0.5f); b.a.rect(x + w * 0.6f, shed.y + 3, w * 0.4f, shed.h - 6, 0.5f, shade(K_ROOF, 0.62f), 0.8f, 0.3f); }
    b.rust(shed.x, shed.y, shed.x + shed.w, shed.y + shed.h, 8, 75);
    b.wallSeams(shed, 20, 12);
    b.door(64, shed.y + shed.h + 20, 38, 18, K_DARK, rgb(24, 22, 18));
    for (int i = 0; i < 4; i++) b.a.line(45.5f, shed.y + shed.h + 20 - 4 * (i + 0.5f), 82.5f, shed.y + shed.h + 20 - 4 * (i + 0.5f), 0.55f, rgba(K_STEEL, 150));
    b.hazard(44, shed.y + shed.h + 20 + 1.4f, 40, 3.2f);
    b.windows(shed, 20, 3, rgb(255, 214, 120), 0.4f);
    b.mark(shed.x + shed.w - 26, shed.y + 3, 22, 3.6f);
    for (int i = 0; i < 2; i++) { P tp = b.cyl(22 + i * 14.0f, 18, 5.2f, 26 - i * 4, rgb(52, 48, 44), K_BRICK); b.a.ring(tp.x, tp.y, 3.6f, 0.8f, K_DARK, 0.5f, 0.2f); }
    // gantry crane across the forecourt, scrap hulls and a tyre stack
    b.pipe(10, 66, 10, 90, 1.5f, K_STEEL); b.pipe(10, 69, 40, 69, 1.5f, K_STEEL); b.a.rect(24, 67, 6, 5, 0.6f, K_RUST, 1, 0.5f);
    b.a.rect(14, 76, 26, 11, 3, K_OLIVE_D, 1.6f, 0.5f); b.a.rect(16, 78, 22, 7, 2, K_DARK, 1.2f, 0.4f);
    for (int i = 0; i < 3; i++) b.barrel(W - 14.0f, 68 + i * 7.0f, 3.2f, i == 1 ? K_RUST : shade(K_DARK, 1.6f));
    b.sandbags(W - 40.0f, H - 5.0f, W - 6.0f, H - 5.0f);
    b.a.grain(0, 0, (float)W, (float)H, 0.12f, 61);
}

void airfieldClanker(Bld& b) {
    int W = b.W, H = b.H;
    b.a.shadowRect(0, 0, (float)W, (float)H, 3, 3, 4, 85);
    b.a.rect(0, 0, (float)W, (float)H, 3, rgb(96, 94, 86), 2.6f, 0.5f);
    for (int i = 0; i < 90; i++) { float x = b.rng.f(4, W - 4.0f), y = b.rng.f(4, H - 4.0f); b.a.ell(x, y, b.rng.f(3, 10), b.rng.f(1.5f, 4), rgba(rgb(70, 66, 58), 60), 1, 0); }   // tyre scuffs
    for (int x = 48; x < W; x += 48) b.a.line((float)x, 2, (float)x, H - 2.0f, 0.7f, rgba(K_SEAM, 100));
    b.a.grain(0, 0, (float)W, (float)H, 0.14f, 67);
    // runway centre dashes and the 5x5 parking grid the aircraft use
    for (int r = 0; r < 2; r++) {
        float y = H * (0.36f + 0.28f * r);
        for (int i = 0; i < W / 14; i++) b.a.rect(8 + i * 14.0f, y - 0.9f, 8, 1.8f, 0.3f, rgba(WHITE, 130), 0.5f, 0);
    }
    for (int row = 0; row < 5; row++) for (int col = 0; col < 5; col++) {
        float cx = W * 0.5f + (col - 2) * 42, cy = H * 0.5f + (row - 2) * 34;
        b.a.ring(cx, cy, 11.5f, 1.4f, rgba(K_YEL, 225), 0.8f, 0); b.a.ring(cx, cy, 11.5f, 0.5f, rgba(K_DARK, 60), 0.5f, 0);
        b.a.line(cx - 3.5f, cy - 4, cx - 3.5f, cy + 4, 1.4f, rgba(K_YEL, 220)); b.a.line(cx + 3.5f, cy - 4, cx + 3.5f, cy + 4, 1.4f, rgba(K_YEL, 220)); b.a.line(cx - 3.5f, cy, cx + 3.5f, cy, 1.4f, rgba(K_YEL, 220));
    }
    for (int i = 0; i < 18; i++) { float x = 8 + i * (W - 16.0f) / 17; b.a.rect(x - 0.7f, 3, 1.4f, 3, 0.3f, K_YEL, 0.5f, 0.2f); b.a.rect(x - 0.7f, H - 6.0f, 1.4f, 3, 0.3f, K_YEL, 0.5f, 0.2f); }
    // control tower: a tall steel cab on a concrete leg
    Roof tw = b.block(10, 10, 18, 16, 28, K_KHAKI, K_OLIVE);
    b.wallSeams(tw, 28, 5); b.windows(tw, 28, 2, rgb(255, 214, 120), 0.6f);
    b.a.rect(tw.x + 2, tw.y + 2, tw.w - 4, tw.h - 4, 1.5f, rgb(74, 98, 110), 2, 0.6f); b.a.line(tw.x + 3, tw.y + 3.4f, tw.x + tw.w - 4, tw.y + 3.4f, 0.8f, rgba(WHITE, 130));
    b.antenna(tw.x + tw.w - 2, tw.y + 2, 14, K_RED);
    b.mark(tw.x + 2, tw.y + tw.h - 4, tw.w - 4, 2.6f);
    // wide Quonset hangar
    Roof hg = b.block(W - 126, 10, 96, 34, 16, K_ROOF, K_OLIVE_D, 6);
    for (int i = 0; i < 14; i++) b.a.line(hg.x + 4 + i * 6.6f, hg.y + 1.5f, hg.x + 4 + i * 6.6f, hg.y + hg.h - 1.5f, 0.7f, rgba(K_DARK, 120));
    b.a.line(hg.x + 2, hg.y + hg.h * 0.5f, hg.x + hg.w - 2, hg.y + hg.h * 0.5f, 1.2f, rgba(WHITE, 50));
    b.door(hg.x + hg.w * 0.5f, hg.y + hg.h + 18, 60, 14, K_DARK, rgb(22, 20, 16)); b.hazard(hg.x + 10, hg.y + hg.h + 19.2f, 64, 3);
    b.mark(hg.x + hg.w - 16, hg.y + 2, 12, 3);
    // fuel dump along the south-west edge
    b.cyl(16, H - 26, 8, 9, K_STEEL, K_OLIVE); b.cyl(36, H - 26, 8, 9, K_STEEL, K_RUST); b.pipe(16, H - 35, 36, H - 35, 1.2f, K_STEEL);
    for (int i = 0; i < 5; i++) b.barrel(54 + i * 6.0f, H - 18, 2.7f, i == 1 ? K_RUST : K_OLIVE_D);
    b.a.line(W - 14.0f, H - 10, W - 14.0f, H - 22, 0.9f, K_STEEL);
    b.a.poly({ {W - 14.0f, (float)(H - 22)}, {W - 5.0f, (float)(H - 20)}, {W - 5.0f, (float)(H - 17)}, {W - 14.0f, (float)(H - 18)} }, rgb(224, 110, 46), 1.4f, 0.5f);
    b.a.rect(W - 10.0f, H - 20.6f, 1.8f, 3.4f, 0.2f, WHITE, 0.6f, 0.2f);
    b.sandbags(W * 0.42f, H - 6.0f, W * 0.72f, H - 6.0f);
    b.a.grain(0, 0, (float)W, (float)H, 0.08f, 71);
}

void techClanker(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(K_PAD, K_SEAM, 0.16f, 3);
    Roof lab = b.block(8, 30, 54, 44, 14, K_ROOF, K_OLIVE_D);
    b.corrugated(lab, 3.4f, K_DARK); b.rust(lab.x, lab.y, lab.x + lab.w, lab.y + lab.h, 6, 70);
    b.wallSeams(lab, 14, 11); b.windows(lab, 14, 4, rgb(255, 214, 120), 0.5f);
    b.door(lab.x + 14, lab.y + lab.h + 14, 12, 11, K_DARK, rgb(22, 20, 16)); b.hazard(lab.x + 8, lab.y + lab.h + 14 + 1.2f, 12, 3);
    b.mark(lab.x + 3, lab.y + lab.h - 5.4f, 20, 3.4f);
    b.vent(lab.x + lab.w - 16, lab.y + 4, 12, 7, rgba(WHITE, 80));
    // observatory radome on a drum
    P dr = b.cyl(56, 48, 15, 10, K_ROOF, K_STEEL); (void)dr;
    b.domeTop(56, 38, 12, rgb(214, 216, 214));
    b.a.rect(53.5f, 37, 5, 2.4f, 0.3f, rgba(K_DARK, 90), 0.5f, 0);
    // ordnance test barrel poking out to the east, with an ammunition rack
    b.pipe(68, 62, 92, 56, 2.6f, shade(K_DARK, 1.7f)); b.pipe(88, 57, 93, 56, 3.4f, shade(K_DARK, 2.0f)); b.a.rect(62, 58, 8, 10, 1, K_STEEL, 1.4f, 0.5f);
    for (int i = 0; i < 4; i++) b.a.cap(70, 72 + i * 3.4f, 86, 72 + i * 3.4f, 1.3f, i % 2 ? K_BRICK : K_YEL, 0.8f, 0.5f);
    // mast array
    b.antenna(14, 28, 26, K_RED); b.antenna(22, 22, 20, K_RED); b.antenna(30, 26, 16, K_YEL);
    b.pipe(8, 78, 40, 78, 1.3f, K_RUST);
    for (int i = 0; i < 3; i++) b.barrel(46 + i * 5.0f, 82, 2.6f, i == 1 ? K_RUST : K_OLIVE_D);
    b.sandbags(8, H - 5.0f, 38, H - 5.0f);
    b.a.grain(0, 0, (float)W, (float)H, 0.10f, 73);
}

void gunBase(Bld& b) {   // 1x1 sandbag ring round a pit, the MG head turns over it
    b.a.shadowCircle(16, 16, 14.5f, 2, 2, 3, 85);
    b.a.circle(16, 16, 14.8f, K_PAD2, 2, 0.5f);
    b.a.circle(16, 16, 11, rgb(54, 46, 36), 2.2f, 0.5f);
    for (int i = 0; i < 16; i++) { float an = i * 0.3927f; float px = 16 + std::cos(an) * 12.6f, py = 16 + std::sin(an) * 12.6f; b.a.ell(px, py, 3.4f, 2.4f, shade(K_SAND, 0.86f + b.rng.f(0, 0.16f)), 1.4f, 0.6f); }
    for (int i = 0; i < 3; i++) b.a.line(7 + i * 4.0f, 18, 7 + i * 4.0f, 25, 0.9f, rgba(rgb(120, 92, 56), 160));   // duckboards
    b.markDisc(16, 27, 1.5f);
}

void rocketBase(Bld& b) {   // 2x2: concrete slab, ammunition stacks and sandbags; the rocket pod turns over the middle
    int W = b.W, H = b.H;
    b.pad(K_PAD, K_SEAM, 0.16f, 4);
    b.a.rect(8, 10, W - 16.0f, H - 20.0f, 3, K_PAD2, 1.8f, 0.45f);
    b.hazard(10, H - 11.0f, W - 20.0f, 2.6f);
    for (int i = 0; i < 3; i++) b.crate(6 + i * 8.0f, 4, 7, 6, shade(K_OLIVE, 0.9f + i * 0.08f));
    for (int i = 0; i < 4; i++) b.a.cap(W - 22.0f, 5 + i * 2.6f, W - 6.0f, 5 + i * 2.6f, 1.1f, i % 2 ? K_BRICK : K_YEL, 0.8f, 0.5f);
    b.sandbags(4, H - 4.0f, W - 4.0f, H - 4.0f, 2); b.sandbags(3, 12, 3, H - 8.0f); b.sandbags(W - 3.0f, 12, W - 3.0f, H - 8.0f);
    b.a.ring(W - 12.0f, H - 15.0f, 3.4f, 1.2f, K_STEEL, 0.8f, 0.3f);   // cable reel
    b.markDisc(8, 13, 1.7f);
}

void oilwell(Bld& b) {   // 2x2: oil-stained pad, pit, derrick, storage tank; the pump-jack beam is drawn live
    int W = b.W, H = b.H;
    b.pad(rgb(84, 78, 66), K_SEAM, 0.18f, 4);
    b.oilStain(32, 40, 22, 14); b.oilStain(14, 18, 9, 5); b.oilStain(52, 52, 8, 4);
    b.a.ell(32, 44, 13, 9, rgb(14, 12, 12), 2.4f, 0.5f);
    b.a.ell(30, 42, 8, 4.4f, rgba(rgb(70, 62, 90), 140), 1, 0);                         // oil sheen
    b.a.ell(28, 41, 3.4f, 1.4f, rgba(WHITE, 90), 1, 0);
    // derrick: a lattice A-frame with a crown block
    for (int s = -1; s <= 1; s += 2) b.a.line(32 + s * 11.0f, 50, 32 + s * 2.0f, 8, 1.6f, K_STEEL);
    for (int i = 0; i < 6; i++) { float y = 46 - i * 6.2f, hw = 10 - i * 1.5f; b.a.line(32 - hw, y, 32 + hw, y, 0.8f, shade(K_STEEL, 0.85f)); b.a.line(32 - hw, y, 32 + hw - 1.6f, y - 6.2f, 0.6f, shade(K_STEEL, 0.7f)); }
    b.a.rect(29, 5, 6, 5, 0.8f, K_RUST, 1.4f, 0.5f); b.light(32, 4, 1.2f, K_RED, 130);
    b.a.line(33, 10, 33, 30, 0.6f, rgba(BLACK, 120));
    b.a.line(32, 50, 45, 56, 0.9f, rgba(BLACK, 50));
    P tk = b.cyl(52, 24, 7, 10, K_STEEL, K_RUST); b.a.dot(tk.x, tk.y, 1.8f, K_DARK); b.pipe(46, 44, 52, 28, 1.3f, K_STEEL);
    for (int i = 0; i < 3; i++) b.barrel(8 + i * 5.0f, 54, 2.6f, i == 1 ? K_RUST : K_OLIVE_D);
    b.hazard(4, H - 7.0f, 28, 2.8f);
    b.markDisc(W - 8.0f, H - 8.0f, 2.2f);
}

void nukeClanker(Bld& b) {   // launch ramp with a missile on its rail, exhaust pit, fuel tanks and bunkers
    int W = b.W, H = b.H;
    b.pad(K_PAD, K_SEAM, 0.18f, 3);
    b.rust(4, 4, W - 4.0f, H - 4.0f, 14, 50);
    b.hazard(4, 4, W - 8.0f, 3.2f); b.hazard(4, H - 7.2f, W - 8.0f, 3.2f);
    b.a.ell(24, 74, 15, 9, rgb(20, 18, 16), 2.4f, 0.5f); b.a.glow(24, 74, 16, rgba(rgb(255, 120, 50), 60));   // exhaust pit
    // launch rails (a long diagonal truss) and the warhead riding them
    float x0 = 22, y0 = 76, x1 = 70, y1 = 22;
    b.a.line(x0 + 4.4f, y0 + 4.4f, x1 + 4.4f, y1 + 4.4f, 5, rgba(BLACK, 70));
    b.pipe(x0 - 4, y0 + 2, x1 - 4, y1 + 2, 1.6f, K_STEEL); b.pipe(x0 + 4, y0 - 2, x1 + 4, y1 - 2, 1.6f, K_STEEL);
    for (int i = 0; i < 9; i++) { float k = i / 8.0f, px = x0 + (x1 - x0) * k, py = y0 + (y1 - y0) * k; b.a.line(px - 5, py + 2.4f, px + 5, py - 2.4f, 1.0f, shade(K_RUST, 0.9f)); }
    // fuel tanks, a blockhouse and sandbag revetments
    b.cyl(74, 74, 7, 9, K_STEEL, K_OLIVE); b.cyl(58, 80, 6, 8, K_STEEL, K_RUST); b.pipe(74, 67, 64, 30, 1.0f, K_STEEL);
    Roof bk = b.block(6, 10, 22, 18, 10, K_ROOF, K_OLIVE_D);
    b.corrugated(bk, 3.2f, K_DARK); b.windows(bk, 10, 2, rgb(255, 214, 120), 0.5f);
    b.mark(bk.x + 3, bk.y + bk.h - 5, 16, 3);
    b.sandbags(4, H - 10.0f, 30, H - 10.0f); b.sandbags(66, 8, W - 4.0f, 8);
    b.antenna(bk.x + bk.w - 4, bk.y + 4, 16, K_RED);
    b.a.grain(0, 0, (float)W, (float)H, 0.12f, 79);
}

// 2x2: the surface of an underground bunker. A flat reinforced apron with hazard borders, a steel-rimmed pit in the middle (the hatch leaves
// are drawn live over it, so the pit itself stays dark) and vents or hatch covers in the corners. Cyber: grey-blue concrete with cyan lights.
// Clanker: sandy concrete, sandbags at two corners and rusty yellow stripes.
void bunkerBase(Bld& b) {
    int W = b.W, H = b.H;
    bool cy = b.cy;
    Color slab = cy ? rgb(104, 118, 136) : rgb(190, 170, 122), seam = cy ? rgb(62, 72, 88) : rgb(124, 108, 78);
    Color hz1 = cy ? rgb(232, 188, 62) : rgb(204, 156, 56), hz2 = cy ? rgb(36, 36, 32) : rgb(96, 72, 44);
    Color hatch = cy ? C_STEEL : rgb(108, 84, 62);
    b.pad(cy ? rgb(84, 96, 112) : rgb(176, 156, 110), seam, 0.12f, 4);
    b.hazard(3, 3, W - 6.0f, 2.6f, 5, hz1, hz2); b.hazard(3, H - 5.6f, W - 6.0f, 2.6f, 5, hz1, hz2);
    b.hazard(3, 5.6f, 2.6f, H - 11.2f, 5, hz1, hz2); b.hazard(W - 5.6f, 5.6f, 2.6f, H - 11.2f, 5, hz1, hz2);
    b.a.rect(7, 7, W - 14.0f, H - 14.0f, 2.5f, slab, 1.8f, 0.45f);
    b.a.line(7, 22, W - 7.0f, 22, 0.5f, rgba(seam, 120)); b.a.line(7, 46, W - 7.0f, 46, 0.5f, rgba(seam, 120));
    b.a.grain(7, 7, W - 7.0f, H - 7.0f, 0.10f, 91);
    // the pit: a dark steel rim and a black well
    b.a.circle(32, 32, 19.5f, cy ? rgb(56, 62, 72) : rgb(92, 84, 66), 2.2f, 0.55f);
    b.a.circle(32, 32, 17, rgb(8, 9, 11), 1.6f, 0.2f);
    for (int i = 0; i < 8; i++) { float an = i * 0.7854f; b.a.dot(32 + std::cos(an) * 23.0f, 32 + std::sin(an) * 23.0f, 0.9f, shade(seam, 1.6f)); }   // bolts round the rim
    // corner features: a vent grille (TL), an ammunition hatch (TR) and a second vent (BR)
    auto vent = [&](float x, float y, float w, float h) { b.vent(x, y, w, h, shade(slab, 1.6f)); };
    auto plate = [&](float x, float y, float s) {
        b.a.rect(x, y, s, s, 1.2f, hatch, 1.4f, 0.5f);
        b.a.line(x + 1.6f, y + 1.6f, x + s - 1.6f, y + s - 1.6f, 0.5f, rgba(BLACK, 80));
        for (float dx : { 1.6f, s - 1.6f }) for (float dy : { 1.6f, s - 1.6f }) b.a.dot(x + dx, y + dy, 0.6f, shade(hatch, 1.5f));
    };
    if (cy) {
        vent(9, 9, 10, 6); plate(46, 8, 9); vent(46, 47, 9, 6);
        for (float p : { 11.0f, 53.0f }) b.light(p, 32, 1.1f, C_CYAN, 120);
        b.light(32, 10.5f, 1.1f, C_CYAN, 120);
        b.light(12, 52, 1.0f, C_CYAN, 90);
        b.mark(24, 53.5f, 16, 2.6f);
    } else {
        // sandbags along the top-left and bottom-right corners, two rows deep
        b.sandbags(5, 12, 19, 12, 2); b.sandbags(7, 16, 7, 26, 2);
        b.sandbags(W - 5.0f, H - 12.0f, W - 19.0f, H - 12.0f, 2); b.sandbags(W - 7.0f, H - 26.0f, W - 7.0f, H - 16.0f, 2);
        plate(46, 8, 9); vent(9, 47, 10, 6);
        b.rust(12, 40, 22, 50, 6, 60); b.rust(44, 14, 52, 24, 4, 60);
        b.mark(24, 53.5f, 16, 2.6f);
    }
    b.a.grain(0, 0, (float)W, (float)H, 0.08f, 93);
}

// 1x1: a thick wall segment. Heavy concrete, a steel cap and reinforcing ribs; the cap and bolts line up from tile to tile so a chain reads
// as one long rampart. The footprint is deliberately filled edge to edge (no pad gap) so segments join without a seam a unit could slip through.
void wallBase(Bld& b) {
    int W = b.W, H = b.H;
    bool cy = b.cy;
    Color slab = cy ? rgb(78, 92, 112) : rgb(126, 116, 92), cap = cy ? rgb(150, 166, 184) : rgb(176, 160, 120);
    Color seam = cy ? rgb(34, 42, 54) : rgb(62, 54, 42);
    b.a.shadowRect(0, 0, (float)W, (float)H, 4, 4, 4, 100);
    b.a.rect(0, 0, (float)W, (float)H, 1.5f, slab, 2.2f, 0.5f);
    Roof r = b.block(1, 1, W - 2.0f, H - 2.0f, 9, cap, shade(slab, 0.82f), 1.0f);
    // steel cap plates and reinforcing ribs across the top
    for (int i = 0; i < 3; i++) b.a.rect(4 + i * 8.5f, r.y + 3, 6, r.h - 6, 0.6f, shade(cap, 1.08f), 1.0f, 0.4f);
    for (int i = 0; i < 3; i++) b.a.line(3 + i * 9.0f, r.y + 1.5f, 3 + i * 9.0f, r.y + r.h - 1.5f, 0.7f, rgba(seam, 150));
    b.a.line(2, r.y + r.h * 0.5f, W - 2.0f, r.y + r.h * 0.5f, 0.8f, rgba(seam, 160));
    // firing slit and cap bolts
    b.a.rect(W * 0.5f - 7, r.y + r.h * 0.5f - 1.1f, 14, 2.2f, 0.4f, rgb(10, 12, 16), 0.6f, 0.2f);
    for (float x : { 3.0f, W - 3.0f }) for (float y : { r.y + 2.0f, r.y + r.h - 2.0f }) b.a.dot(x, y, 0.9f, shade(cap, 1.6f));
    b.hazard(2, H - 6.0f, W - 4.0f, 3.0f, 4);
    if (cy) { b.light(4.0f, 4.0f, 0.9f, C_CYAN, 80); b.light(W - 4.0f, 4.0f, 0.9f, C_CYAN, 80); }
    else { b.rust(4, 4, W - 4.0f, H - 6.0f, 5, 80); b.mark(W * 0.5f - 6, r.y + 2, 12, 2.4f); }
    b.a.grain(0, 0, (float)W, (float)H, 0.12f, 97);
}

}  // namespace

// ================================================================= entry points
BArtSize artBuildingSize(int type) {
    const BuildType& bt = BUILDS[type];
    int dw = bt.w * TILE + BART_PAD_L + BART_PAD_R, dh = bt.h * TILE + BART_PAD_T + BART_PAD_B;
    return BArtSize{ dw * BART_K, dh * BART_K, (BART_PAD_L + bt.w * TILE * 0.5f) * BART_K, (BART_PAD_T + bt.h * TILE * 0.5f) * BART_K };
}

void artBuilding(Canvas& c, Canvas& mask, int type) {
    const BuildType& bt = BUILDS[type];
    if (artNavalBuilding(c, mask, type)) return;
    if (artSpecialBuilding(c, mask, type)) return;
    if (artCivicBuilding(c, mask, type)) return;
    bool cy = bt.faction == F_CYBER;
    Bld b(c, mask, bt.w * TILE, bt.h * TILE, cy, 1000u + (u32)type * 31u);
    switch (bt.role) {
    case BR_HQ:       cy ? hqCyber(b)       : hqClanker(b);       break;
    case BR_POWER:    cy ? powerCyber(b)    : powerClanker(b);    break;
    case BR_SUPPLY:   cy ? supplyCyber(b)   : supplyClanker(b);   break;
    case BR_BARRACKS: cy ? barracksCyber(b) : barracksClanker(b); break;
    case BR_FACTORY:  cy ? factoryCyber(b)  : factoryClanker(b);  break;
    case BR_AIRFIELD: cy ? airfieldCyber(b) : airfieldClanker(b); break;
    case BR_TECH:     cy ? techCyber(b)     : techClanker(b);     break;
    case BR_TURRET:   cy ? laserBase(b)     : gunBase(b);         break;
    case BR_AATURRET: cy ? patriotCyber(b)  : rocketBase(b);      break;
    case BR_INCOME:   cy ? minerCyber(b)    : oilwell(b);         break;
    case BR_NUKE:     cy ? nukeCyber(b)     : nukeClanker(b);     break;
    case BR_BUNKER:   bunkerBase(b);                                 break;
    case BR_WALL:     wallBase(b);                                   break;
    default: break;   // (the naval yards and anti-nuke defences are drawn by artNavalBuilding)
    }
    c.outline(rgb(8, 10, 14, 150));
}

void artSite(Canvas& c, int type) {
    const BuildType& bt = BUILDS[type];
    Canvas dummy(4, 4);
    Bld b(c, dummy, bt.w * TILE, bt.h * TILE, bt.faction == F_CYBER, 2000u + (u32)type * 17u);
    int W = b.W, H = b.H;
    b.pad(rgb(96, 84, 64), rgb(60, 52, 40), 0.22f, 3);
    for (int i = 0; i < 24; i++) b.a.ell(b.rng.f(4, W - 4.0f), b.rng.f(4, H - 4.0f), b.rng.f(3, 10), b.rng.f(2, 5), rgba(rgb(70, 58, 42), 80), 1, 0);   // churned earth
    // concrete footing outline and re-bar grid
    b.a.rect(6, 6, W - 12.0f, H - 12.0f, 2, rgb(128, 124, 114), 1.6f, 0.5f);
    for (int x = 14; x < W - 8; x += 8) b.a.line((float)x, 8, (float)x, H - 8.0f, 0.6f, rgba(rgb(70, 66, 60), 150));
    for (int y = 14; y < H - 8; y += 8) b.a.line(8, (float)y, W - 8.0f, (float)y, 0.6f, rgba(rgb(70, 66, 60), 150));
    // stacked materials
    for (int i = 0; i < std::max(2, bt.w * bt.h / 3); i++) {
        float x = b.rng.f(10, W - 24.0f), y = b.rng.f(10, H - 18.0f);
        if (i & 1) { for (int k = 0; k < 3; k++) b.a.rect(x, y + k * 2.6f, 14, 2.2f, 0.4f, shade(rgb(176, 138, 84), 0.9f + 0.1f * k), 0.8f, 0.5f); }
        else b.crate(x, y, 8, 7, rgb(120, 112, 90));
    }
    // fence posts with hazard tape and a scaffold frame in the middle
    for (int x = 6; x <= W - 6; x += 12) { b.a.dot((float)x, 5, 1.2f, rgb(60, 60, 64)); b.a.dot((float)x, H - 5.0f, 1.2f, rgb(60, 60, 64)); }
    b.a.line(6, 5, W - 6.0f, 5, 0.8f, rgb(232, 188, 62)); b.a.line(6, H - 5.0f, W - 6.0f, H - 5.0f, 0.8f, rgb(232, 188, 62));
    for (int y = 6; y <= H - 6; y += 12) { b.a.dot(5, (float)y, 1.2f, rgb(60, 60, 64)); b.a.dot(W - 5.0f, (float)y, 1.2f, rgb(60, 60, 64)); }
    for (int i = 0; i <= 4; i++) { float x = W * (0.2f + 0.15f * i); b.pipe(x, H * 0.3f, x, H * 0.7f, 0.9f, rgb(150, 152, 156)); }
    b.pipe(W * 0.2f, H * 0.45f, W * 0.8f, H * 0.45f, 0.8f, rgb(150, 152, 156)); b.pipe(W * 0.2f, H * 0.62f, W * 0.8f, H * 0.62f, 0.8f, rgb(150, 152, 156));
    c.outline(rgb(8, 10, 14, 140));
}

void artRubble(Canvas& c, int tw, int th, u32 seed) {
    Canvas dummy(4, 4);
    Bld b(c, dummy, tw * TILE, th * TILE, true, seed);
    int W = b.W, H = b.H;
    b.a.soft(0, 0, c.w, c.h, [&](float x, float y) {
        float cx = (x / BART_K - BART_PAD_L) - W * 0.5f, cy = (y / BART_K - BART_PAD_T) - H * 0.5f;
        float d = (hyp(cx / (W * 0.52f), cy / (H * 0.52f)) - 1) * std::min(W, H) * 0.5f;
        return d;
    }, rgba(rgb(14, 12, 10), 200), 14.0f * BART_K);
    for (int i = 0; i < W * H / 90; i++) {
        float x = b.rng.f(6, W - 6.0f), y = b.rng.f(6, H - 6.0f), s = b.rng.f(2, 7);
        Color base = b.rng.f() < 0.35f ? rgb(60, 56, 52) : (b.rng.f() < 0.5f ? rgb(92, 88, 82) : rgb(46, 44, 42));
        b.a.poly({ {x, y}, {x + s, y + b.rng.f(-1, 1)}, {x + s * b.rng.f(0.5f, 1.0f), y + s * 0.8f}, {x - s * 0.2f, y + s * 0.6f} }, base, 1.4f, 0.6f);
    }
    for (int i = 0; i < 6 + W / 16; i++) { float x = b.rng.f(8, W - 8.0f), y = b.rng.f(8, H - 8.0f), an = b.rng.f(0, 6.28f), l = b.rng.f(4, 12); b.a.cap(x, y, x + std::cos(an) * l, y + std::sin(an) * l, 0.9f, rgb(70, 52, 40), 0.8f, 0.5f); }   // bent beams
    b.a.grain(0, 0, (float)W, (float)H, 0.18f, seed + 5);
}

// The warhead itself, drawn live over a nuke ramp only while the ramp is armed (same canvas geometry as the structure).
void artArmed(Canvas& c, int type) {
    const BuildType& bt = BUILDS[type];
    if (bt.role == BR_UNUKE) { artSiloArmed(c, type); return; }
    Canvas dummy(4, 4);
    Bld b(c, dummy, bt.w * TILE, bt.h * TILE, bt.faction == F_CYBER, 4000u + (u32)type);
    if (bt.faction == F_CYBER) {
        float cx = 48, cy = 50;
        b.a.glow(cx, cy, 24, rgba(rgb(255, 120, 60), 90));
        b.a.cap(cx, cy + 14, cx, cy - 16, 6, C_WHITE, 5, 0.65f);                 // missile body
        b.a.circle(cx, cy - 16, 6, rgb(214, 70, 60), 3, 0.65f);                  // nose cone
        b.a.rect(cx - 6, cy - 3, 12, 4, 0.5f, rgb(240, 196, 60), 0.9f, 0.4f);    // warhead band
        for (int s = -1; s <= 1; s += 2) b.a.poly({ {cx + s * 5.0f, cy + 12}, {cx + s * 11.0f, cy + 17}, {cx + s * 5.0f, cy + 6} }, C_ROOF2, 1, 0.4f);   // fins
    } else {
        float x0 = 22, y0 = 76, x1 = 70, y1 = 22;
        float mx0 = x0 + (x1 - x0) * 0.14f, my0 = y0 + (y1 - y0) * 0.14f, mx1 = x0 + (x1 - x0) * 0.86f, my1 = y0 + (y1 - y0) * 0.86f;
        b.a.cap(mx0, my0, mx1, my1, 4.6f, rgb(210, 212, 210), 4, 0.65f);
        b.a.rect(mx0 + (mx1 - mx0) * 0.45f - 1.6f, my0 + (my1 - my0) * 0.45f - 4, 3.2f, 8, 0.4f, rgb(236, 196, 60), 0.8f, 0.3f);
        b.a.circle(mx1, my1, 4.6f, rgb(216, 70, 56), 3, 0.65f);
        b.a.poly({ {mx0, my0}, {mx0 - 8, my0 - 1}, {mx0 - 2, my0 - 7} }, K_STEEL, 1, 0.4f); b.a.poly({ {mx0, my0}, {mx0 + 2, my0 + 8}, {mx0 + 7, my0 + 2} }, K_STEEL, 1, 0.4f);
    }
    c.outline(rgb(8, 10, 14, 150));
}

void artProp(Canvas& c, int which) {
    Canvas dummy(4, 4);
    Art a(c, WHITE, 2.0f, 0, 0);   // design space 32x32
    switch (which) {
    case 0: {   // cyber optical dish: a pale bowl on a swivel, emitter feed in the middle
        a.shadowCircle(16, 16, 11, 2, 2, 2, 70);
        a.circle(16, 16, 11, rgb(150, 170, 198), 2.8f, 0.6f);
        a.circle(16, 16, 8.4f, rgb(86, 108, 148), 2.4f, 0.4f);
        a.ring(16, 16, 6, 0.8f, rgba(C_CYAN, 200), 0.6f, 0);
        a.cap(16, 16, 27, 5, 0.8f, rgb(190, 204, 224), 0.8f, 0.5f);
        a.dot(27, 5, 1.5f, C_CYAN); a.glow(27, 5, 6, rgba(C_CYAN, 140));
        a.dot(16, 16, 2, C_WHITE);
        break; }
    case 1: {   // clanker radar: a lattice antenna bar turning on a drum
        a.shadowRect(2, 11, 28, 10, 2, 2, 2, 60);
        a.rect(2, 11, 28, 6, 0.8f, rgb(150, 150, 140), 1.6f, 0.55f);
        for (int i = 0; i < 7; i++) a.line(4.4f + i * 3.8f, 11.4f, 4.4f + i * 3.8f, 16.6f, 0.6f, rgba(K_DARK, 150));
        a.circle(16, 14, 3.4f, K_STEEL, 1.6f, 0.55f); a.dot(16, 14, 1.2f, K_DARK);
        a.dot(3, 14, 1, K_RED);
        break; }
    case 2: {   // pump-jack walking beam: the horsehead and the counterweight
        a.shadowRect(2, 13, 28, 6, 2, 2, 2, 60);
        a.cap(5, 16, 27, 16, 1.7f, K_RUST, 1.4f, 0.6f);
        a.rect(22, 11, 8, 10, 1.4f, K_STEEL, 1.6f, 0.55f);               // horsehead
        a.rect(2, 12, 7, 8, 1, K_STEEL, 1.4f, 0.5f);                     // counterweight
        a.circle(14, 16, 3, K_YEL, 1.6f, 0.55f); a.dot(14, 16, 1.2f, K_DARK);
        break; }
    case 3: {   // cooling fan (blades only, spun by the renderer)
        a.circle(16, 16, 12, rgba(rgb(20, 22, 26), 120), 1, 0);
        for (int i = 0; i < 4; i++) { float an = i * 1.5708f; a.cap(16, 16, 16 + std::cos(an) * 10, 16 + std::sin(an) * 10, 2.3f, rgb(150, 160, 176), 1.4f, 0.5f); }
        a.dot(16, 16, 2.2f, rgb(210, 220, 232));
        break; }
    }
    c.outline(rgb(8, 10, 14, 120));
}
