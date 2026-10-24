// One Hour - rendering of the newer features: warships and their wakes, jeep crews, protector drones, the Hotline Strike marker and the anti-nuke
// defences' shots. These are Game members (see ui.h); the rest of the world rendering is in ui.cpp.
#include "ui.h"
#include "art.h"
#include "art2.h"
#include "paint.h"
using paint::hyp;

static float hash01(u32 a) { a ^= a >> 16; a *= 0x7feb352du; a ^= a >> 15; a *= 0x846ca68bu; a ^= a >> 16; return (a & 0xFFFF) / 65535.0f; }

// ------------------------------------------------------------ warships
// The water around a moving hull: a bow wave that opens into two foam lines, a churned stern wash that hangs behind it for a while, and a
// faint ring where a ship sits still.
void Game::drawShipWake(const Entity& e, Vec2 p) {
    Gfx& g = g_gfx;
    const float sc = UNIT_SCALE * artScale(e.type);
    const bool patrol = e.type == U_C_PATROL || e.type == U_K_PATROL;   // (a patrol boat's hull is about two thirds of a warship's)
    const float HL = (patrol ? 19.5f : 30.0f) * sc, HB = (patrol ? 6.0f : 8.8f) * sc;   // half length and half beam on screen
    Vec2 f(std::cos(e.angle), std::sin(e.angle)), s(-f.y, f.x);
    float spd = (e.pos - e.prevPos).len() / SIM_DT;
    float k = clampf(spd / std::max(1.0f, e.ut().speed), 0.0f, 1.0f);
    float breathe = 0.5f + 0.5f * std::sin(wallTime * 2.1f + (float)e.gen);
    g.drawSized(g.fxs.ring, p.x, p.y + 1, HL * 2.0f + 12.0f + 6.0f * breathe, HB * 2.0f + 14.0f + 5.0f * breathe, e.angle, rgb(235, 245, 255), (u8)(34 + 20 * (1 - k)));
    if (k < 0.04f) return;
    Color foam = rgb(236, 246, 252);
    for (int side = -1; side <= 1; side += 2) {
        Vec2 a = p + f * (HL * 0.78f) + s * (side * HB * 0.82f);
        Vec2 b = p - f * (HL * (0.15f + 1.7f * k)) + s * (side * (HB + 5.0f + 20.0f * k));
        g.thickLine(a.x, a.y, b.x, b.y, Color{foam.r, foam.g, foam.b, (u8)(50 + 110 * k)}, 2.4f);
        Vec2 b2 = p - f * (HL * (0.1f + 1.1f * k)) + s * (side * (HB + 1.5f + 10.0f * k));
        g.thickLine(a.x, a.y, b2.x, b2.y, Color{foam.r, foam.g, foam.b, (u8)(30 + 80 * k)}, 1.4f);
    }
    // the bow wave and the churned water behind the stern
    g.glowAdd(p.x + f.x * HL * 0.95f, p.y + f.y * HL * 0.95f, 9 + 8 * k, Color{220, 240, 255, (u8)(40 + 70 * k)});
    if (frameDt > 0) {
        Vec2 stern = e.pos - Vec2(f.x, f.y) * (HL * 0.92f);
        for (float q = fxRng.f(); q < (16.0f + 38.0f * k) * frameDt; q += 1.0f)
            emitP(stern.x + fxRng.f(-HB, HB) * 0.7f, stern.y + fxRng.f(-HB, HB) * 0.7f, -f.x * (6 + 22 * k) + fxRng.f(-8, 8), -f.y * (6 + 22 * k) + fxRng.f(-8, 8),
                  fxRng.f(1.3f, 2.4f), 4.0f + 2.0f * k, 12.0f + 12.0f * k, rgb(238, 247, 252, 150), PK_SMOKE, (u8)fxRng.range(0, 3), 0, 0.5f, fxRng.f(0, 6), fxRng.f(-0.3f, 0.3f));
    }
}

// The rotating mounts of a ship, at their places on the hull
void Game::drawShipMounts(const Entity& e, Vec2 p, Color mod) {
    Gfx& g = g_gfx;
    const Sprite& tur = g.unitTurret[e.type][slotOf(e.owner < 0 ? 0 : e.owner)];
    if (!tur.tex) return;
    bool cruiser = e.type == U_C_CRUISER || e.type == U_K_CRUISER, patrol = e.type == U_C_PATROL || e.type == U_K_PATROL;
    ShipMounts m = patrol ? patrolMounts() : shipMounts(cruiser);
    const float sc = UNIT_SCALE * artScale(e.type);
    float ca = std::cos(e.angle), sa = std::sin(e.angle);
    for (int i = 0; i < m.n; i++) {
        float lx = m.x[i] * sc, ly = m.y[i] * sc;
        float rec = 0;
        if (e.ut().weapon >= 0 && !paused) { const Weapon& w = WEAPONS[e.ut().weapon]; float since = w.cooldown - e.cooldown; if (w.cooldown > 0.6f && since >= 0 && since < 0.18f) rec = 1.0f - since / 0.18f; }   // a barrel kicks back after a salvo
        Vec2 q(p.x + lx * ca - ly * sa - std::cos(e.turret) * 3.0f * rec, p.y + lx * sa + ly * ca - std::sin(e.turret) * 3.0f * rec);
        g.draw(tur, q.x, q.y, e.turret, 0.82f, mod);
    }
}

// ------------------------------------------------------------ jeeps
// The soldiers on the seats: a helmet each (the army's own look, the owner's colour in a dot), a short rocket tube on the shoulder for the rocket troopers,
// a scope glint for a sniper, and a muzzle flash on the one that has just fired.
void Game::drawJeepCrew(const Entity& e, Vec2 p) {
    Gfx& g = g_gfx;
    if (e.passengers.empty()) return;
    const float sc = UNIT_SCALE * artScale(e.type);
    Vec2 f(std::cos(e.angle), std::sin(e.angle)), s(-f.y, f.x);
    bool cy = e.ut().faction == F_CYBER;
    Color helm = cy ? rgb(172, 204, 232) : rgb(122, 134, 84), team = playerColor(e.owner < 0 ? 0 : e.owner);
    int seat = 0;
    for (Ref r : e.passengers) {
        if (seat >= JEEP_SEATS) break;
        const Entity* u = g_sim.get(r);
        if (!u) continue;
        Vec2 q = p + f * ((JEEP_SEAT_X[seat] - 32.0f) * sc) + s * ((JEEP_SEAT_Y[seat] - 32.0f) * sc);
        seat++;
        const UnitType& ut = u->ut();
        const Weapon* w = ut.weapon >= 0 ? &WEAPONS[ut.weapon] : nullptr;
        g.fillCircle(q.x, q.y + 0.8f, 2.7f, rgb(10, 12, 16, 150));
        g.fillCircle(q.x, q.y, 2.5f, rgb(24, 26, 30));
        g.fillCircle(q.x, q.y - 0.3f, 2.0f, ut.sniper ? shade(helm, 0.55f) : helm);
        g.fillCircle(q.x - 0.5f, q.y - 0.9f, 0.7f, shade(helm, 1.5f));
        g.fillCircle(q.x + 0.2f, q.y + 0.4f, 0.9f, team);
        if (w && w->proj == PJ_ROCKET) { Vec2 b = q - f * 3.2f + s * 1.1f; g.thickLine(b.x, b.y, b.x - f.x * 3.0f, b.y - f.y * 3.0f, rgb(222, 150, 70), 1.6f); }   // a launcher tube over the shoulder
        if (ut.sniper) g.fillCircle(q.x + f.x * 2.8f, q.y + f.y * 2.8f, 0.7f, rgb(255, 90, 80));
        if (w && u->cooldown > w->cooldown - 0.14f && w->cooldown > 0) {   // it has just fired
            Vec2 d(std::cos(u->turret), std::sin(u->turret));
            g.thickLine(q.x, q.y, q.x + d.x * 6.0f, q.y + d.y * 6.0f, rgb(30, 30, 34), 1.4f);
            g.glowAdd(q.x + d.x * 7.0f, q.y + d.y * 7.0f, 6.0f, Color{w->color.r, w->color.g, w->color.b, 190});
        }
    }
}

// ------------------------------------------------------------ protector drones
void Game::drawEscorts(const Entity& e, Vec2 p) {
    Gfx& g = g_gfx;
    int n = g_sim.droneCount(e);
    if (n <= 0) return;
    float t = g_sim.time + renderAlpha * SIM_DT;
    bool cy = e.ut().faction == F_CYBER;
    int slot = slotOf(e.owner < 0 ? 0 : e.owner);
    float sc = e.ut().naval ? 0.95f : 0.72f;
    Color ready = cy ? rgb(120, 236, 255) : rgb(255, 214, 120);
    for (int i = 0; i < n; i++) {
        Vec2 off = g_sim.dronePosAt(e, i, t) - e.pos;
        Vec2 d(p.x + off.x, p.y + off.y - 10.0f);   // they fly a little above the deck
        bool loaded = e.droneAt[i] <= g_sim.time;
        g.drawSized(g.shadowSmall, d.x + 3, d.y + 14, 12 * sc, 6 * sc, 0, rgb(255, 255, 255), 90);
        g.draw(g.guardDrone[cy ? F_CYBER : F_CLANKER][slot], d.x, d.y, 0, sc);
        float spin = wallTime * 38.0f + (float)i;
        g.draw(g.rotorDisc, d.x, d.y, 0, 0.42f * sc, rgb(255, 255, 255), 130);
        g.draw(g.rotorBlades, d.x, d.y, spin, 0.42f * sc, rgb(214, 244, 255), 100);
        g.fillCircle(d.x, d.y, 1.1f, loaded ? ready : rgb(255, 90, 70));
        if (loaded) g.glowAdd(d.x, d.y, 5.0f * sc, Color{ready.r, ready.g, ready.b, 70});
    }
}

// ------------------------------------------------------------ the Hotline Strike marker
// Where a strike has been called in: a pulsing ground reticle with the countdown of the call, for whoever knows the spot (the caller's team, and
// anybody who has explored it).
void Game::drawStrikeMarks() {
    Gfx& g = g_gfx;
    for (auto& m : g_sim.strikeMarks) {
        bool mine = m.owner >= 0 && g_sim.players[m.owner].team == g_sim.players[g_sim.humanPlayer].team;
        if (!mine && !g_sim.explored(g_sim.humanPlayer, clampi(tileOf(m.pos.x), 0, MAP_W - 1), clampi(tileOf(m.pos.y), 0, MAP_H - 1))) continue;
        Vec2 s = worldToScreen(m.pos);
        if (s.x < -200 || s.y < -200 || s.x > SCREEN_W + 200 || s.y > VIEW_H + 200) continue;
        float age = g_sim.time - m.t0, life = Sim::STRIKE_MARK_LIFE;
        float fade = clampf((life - age) / 6.0f, 0.0f, 1.0f);
        float pulse = 0.5f + 0.5f * std::sin(wallTime * 7.0f);
        Color c = mine ? rgb(120, 255, 150) : rgb(255, 86, 70);
        float R = STRIKE_SPREAD * TILE + 10.0f;
        g.discFill(s.x, s.y, R, Color{c.r, c.g, c.b, (u8)((16 + 14 * pulse) * fade)});
        g.circle(s.x, s.y, R, Color{c.r, c.g, c.b, (u8)(200 * fade)}, 40);
        g.dashedCircle(s.x, s.y, R + 9.0f + 3.0f * pulse, Color{c.r, c.g, c.b, (u8)(150 * fade)}, wallTime * 26.0f, 7, 7);
        for (int k = 0; k < 4; k++) { float a = k * 1.5708f + wallTime * 0.8f; g.line(s.x + std::cos(a) * (R - 8), s.y + std::sin(a) * (R - 8), s.x + std::cos(a) * (R + 14), s.y + std::sin(a) * (R + 14), Color{c.r, c.g, c.b, (u8)(220 * fade)}); }
        g.glowAdd(s.x, s.y, 18.0f + 6.0f * pulse, Color{c.r, c.g, c.b, (u8)(80 * fade)});
        char buf[48]; snprintf(buf, sizeof buf, "AIR STRIKE %s", mine ? "" : "(INCOMING)");
        g.text((int)(s.x - g.textW(buf) / 2), (int)(s.y - R - 22), buf, Color{c.r, c.g, c.b, (u8)(255 * fade)});
    }
}

// ------------------------------------------------------------ anti-nuke defences
// A circle of cover around a defence: what a warhead aimed inside it can be shot down over
void Game::drawCoverRing(Vec2 sp, float radiusPx, Color col, bool powered, const char* label) {
    Gfx& g = g_gfx;
    Color f = col; f.a = (u8)(powered ? 18 : 10);
    g.discFill(sp.x, sp.y, radiusPx, f);
    Color ring = col; ring.a = 210;
    if (powered) g.dashedCircle(sp.x, sp.y, radiusPx, ring, wallTime * 14.0f, 10, 6);
    else g.dashedCircle(sp.x, sp.y, radiusPx, Color{150, 150, 160, 170}, wallTime * 6.0f, 5, 9);
    if (label) g.text((int)clampf(sp.x - g.textW(label) / 2, 4, SCREEN_W - g.textW(label) - 4), (int)clampf(sp.y - radiusPx - 12, 24, VIEW_H - 14), label, powered ? col : rgb(150, 150, 160));
}

// The shots in the air: laser beams and interceptor rockets rising to meet a warhead, and the burning pieces of one that was hit
void Game::drawInterceptors() {
    Gfx& g = g_gfx;
    float now = g_sim.time + renderAlpha * SIM_DT;
    for (auto& it : g_sim.interceptors) {
        float tt = now - it.launchT;
        if (tt < 0) continue;
        Color col = it.laser ? rgb(130, 240, 255) : rgb(255, 214, 140);
        Vec2 a = worldToScreen(it.from), b = worldToScreen(it.aim);
        if (it.laser) {
            if (tt > it.dur + 0.25f) continue;
            float al = tt < it.dur ? 1.0f : clampf(1.0f - (tt - it.dur) / 0.25f, 0, 1);
            float len = hyp(b.x - a.x, b.y - a.y); if (len < 1) continue;
            float ang = std::atan2(b.y - a.y, b.x - a.x);
            Vec2 mid = (a + b) * 0.5f;
            g.drawSized(g.fxs.beam, mid.x, mid.y, len, 26, ang, col, (u8)(120 * al));
            g.drawSized(g.fxs.beam, mid.x, mid.y, len, 11, ang, col, (u8)(255 * al));
            g.drawSized(g.fxs.beam, mid.x, mid.y, len, 4, ang, rgb(255, 255, 255), (u8)(255 * al));
            g.glowAdd(a.x, a.y, 22, Color{col.r, col.g, col.b, (u8)(160 * al)});
            g.glowAdd(b.x, b.y, 18, Color{255, 255, 255, (u8)(140 * al)});
        } else {
            float u = clampf(tt / it.dur, 0, 1);
            if (tt > it.dur + 0.1f) continue;
            Vec2 pos = a + (b - a) * u;
            pos.y -= std::sin(u * 3.14159f) * 22.0f;   // a little loft off the silo
            Vec2 d = (b - a).norm();
            g.drawSized(g.fxs.streak, pos.x - d.x * 10, pos.y - d.y * 10, 30, 5.0f, std::atan2(d.y, d.x), rgb(255, 230, 190), 255);
            g.glowAdd(pos.x, pos.y, 12, Color{255, 190, 110, 180});
            g.fillCircle(pos.x, pos.y, 2.4f, rgb(255, 250, 235));
            if (frameDt > 0 && onScreen(pos.x + cam.x, pos.y + cam.y, 80))
                emitP(pos.x + cam.x - d.x * 12, pos.y + cam.y - d.y * 12, fxRng.f(-6, 6), fxRng.f(-6, 6), fxRng.f(0.9f, 1.5f), 3, 11, rgb(232, 230, 226, 190), PK_SMOKE, (u8)fxRng.range(0, 3), 0, 0.4f, fxRng.f(0, 6), 0.2f);
            if (tt < 0.25f) g.glowAdd(a.x, a.y, 24 * (1.0f - tt / 0.25f), Color{255, 190, 110, 180});   // launch flash
        }
    }
    // the pieces of a warhead that was shot down keep falling along its path to the target
    for (auto& n : g_sim.nukes) {
        if (!n.broken) continue;
        float k = clampf(n.t / Sim::flightOf(n), 0, 1), k0 = clampf(n.killT / Sim::flightOf(n), 0, 1);
        float since = std::max(0.0f, (n.t - n.killT) / Sim::flightOf(n));
        for (int i = 0; i < 9; i++) {
            float h1 = hash01((u32)(n.id * 31 + i * 7 + 1)), h2 = hash01((u32)(n.id * 17 + i * 13 + 5)), h3 = hash01((u32)(n.id * 5 + i * 3 + 9));
            Vec2 w = Sim::nukeAirPos(n, n.t);
            float spread = (30.0f + 120.0f * since * 3.0f) * (0.4f + h1);
            Vec2 off(std::cos(h2 * 6.283f) * spread, std::sin(h2 * 6.283f) * spread * 0.6f + since * 80.0f * h3);
            Vec2 sp = worldToScreen(w + off);
            if (sp.x < -60 || sp.y < -60 || sp.x > SCREEN_W + 60 || sp.y > VIEW_H + 60) continue;
            int fr = ((int)(wallTime * 20) + i * 2) % 6;
            g.draw(g.fxs.flame[fr], sp.x, sp.y - 6, 0, 0.75f + 0.55f * h1, rgb(255, 190, 120), 235);
            g.glowAdd(sp.x, sp.y, 14 + 8 * h1, Color{255, 140, 60, 175});
            g.glowAdd(sp.x, sp.y, 5 + 2 * h3, Color{255, 240, 200, 200});
            g.fillCircle(sp.x, sp.y, 2.0f + 1.4f * h3, rgb(60, 56, 52));
            if (frameDt > 0 && fxRng.f() < 8.0f * frameDt) emitP(w.x + off.x, w.y + off.y, fxRng.f(-10, 10), fxRng.f(-20, -4), fxRng.f(1.0f, 1.8f), 3, 10, rgb(70, 66, 62, 190), PK_SMOKE, (u8)fxRng.range(0, 3), 0, 0.6f, fxRng.f(0, 6), 0.2f);
        }
        (void)k; (void)k0;
    }
}

// ------------------------------------------------------------ naval yards
// The side of a Naval Yard that faces the lake: the one with the most open water along it. (The structure sprite is the same from every side; the
// pier that reaches into the water is drawn live, so it always points at the water the ships will leave into.)
void Game::drawDock(const Entity& e, Vec2 p) {
    Gfx& g = g_gfx;
    const BuildType& bt = e.bt();
    static const int DX[4] = { 1, 0, -1, 0 }, DY[4] = { 0, 1, 0, -1 };
    int best = -1, bestN = 0;
    for (int side = 0; side < 4; side++) {
        int n = 0;
        for (int k = 0; k < (side % 2 == 0 ? bt.h : bt.w); k++) for (int depth = 1; depth <= 2; depth++) {
            int x = side == 0 ? e.tx + bt.w - 1 + depth : (side == 2 ? e.tx - depth : e.tx + k);
            int y = side == 1 ? e.ty + bt.h - 1 + depth : (side == 3 ? e.ty - depth : e.ty + k);
            if (g_map.navTile(x, y)) n++;
        }
        if (n > bestN) { bestN = n; best = side; }
    }
    if (best < 0) return;
    Vec2 d((float)DX[best], (float)DY[best]), s(-d.y, d.x);
    float half = (best % 2 == 0 ? bt.w : bt.h) * TILE * 0.5f;   // from the middle of the structure to its edge on that side
    bool cy = bt.faction == F_CYBER;
    Color deck = cy ? rgb(98, 112, 134) : rgb(122, 98, 66), rail = cy ? rgb(150, 224, 246) : rgb(222, 178, 60), pile = cy ? rgb(40, 50, 66) : rgb(62, 48, 34);
    float len = 44.0f, wid = 40.0f;
    Vec2 c = p + d * (half + len * 0.5f - 2.0f);
    float ang = std::atan2(d.y, d.x);
    g.drawSized(g.shadowLarge, c.x + 3, c.y + 4, len + 14, wid + 14, ang, rgb(255, 255, 255), 80);
    // pilings, the deck, its planks (timber for Clanker, plates for Cyber), edge rails and mooring bollards
    for (int sd = -1; sd <= 1; sd += 2) for (int k = 0; k < 3; k++) { Vec2 q = c + s * (sd * (wid * 0.5f - 1)) + d * (-len * 0.5f + 6 + k * (len - 12) * 0.5f); g.fillCircle(q.x, q.y, 2.6f, pile); }
    Vec2 corners[4] = { c - d * (len * 0.5f) - s * (wid * 0.5f), c - d * (len * 0.5f) + s * (wid * 0.5f), c + d * (len * 0.5f) + s * (wid * 0.5f), c + d * (len * 0.5f) - s * (wid * 0.5f) };
    g.thickLine(c.x - d.x * len * 0.5f, c.y - d.y * len * 0.5f, c.x + d.x * len * 0.5f, c.y + d.y * len * 0.5f, deck, wid);
    for (int k = 1; k < 7; k++) { Vec2 a = c - d * (len * 0.5f) + d * (k * len / 7.0f) - s * (wid * 0.5f), b = a + s * wid; g.line(a.x, a.y, b.x, b.y, shade(deck, 0.7f)); }
    for (int sd = -1; sd <= 1; sd += 2) { Vec2 a = c - d * (len * 0.5f) + s * (sd * wid * 0.5f), b = c + d * (len * 0.5f) + s * (sd * wid * 0.5f); g.thickLine(a.x, a.y, b.x, b.y, rail, 1.6f); }
    Vec2 tip = c + d * (len * 0.5f);
    g.thickLine(tip.x - s.x * wid * 0.5f, tip.y - s.y * wid * 0.5f, tip.x + s.x * wid * 0.5f, tip.y + s.y * wid * 0.5f, rail, 1.6f);
    for (int sd = -1; sd <= 1; sd += 2) { Vec2 q = c + d * (len * 0.25f) + s * (sd * (wid * 0.5f - 4)); g.fillCircle(q.x, q.y, 2.2f, rgb(30, 32, 36)); g.fillCircle(q.x - 0.5f, q.y - 0.5f, 1.2f, shade(deck, 1.5f)); }
    (void)corners;
    Color lamp = e.constructed ? rgb(120, 255, 150) : rgb(255, 190, 90);
    float pulse = 0.5f + 0.5f * std::sin(wallTime * 3.0f + (float)e.gen);
    g.glowAdd(tip.x + s.x * 14, tip.y + s.y * 14, 9, Color{lamp.r, lamp.g, lamp.b, (u8)(70 + 80 * pulse)});
}

// ------------------------------------------------------------ the towns and cities round the lake
// A townsperson seen from above (the same little figure the civic blocks' townsfolk are): shadow, legs swinging with the stride, arms, shirt,
// head and hair. 'carry' adds what they have in hand: 1 a box, 2 a shopping bag, 3 a hard hat and hammer (a builder), 4 a plank on the shoulder.
static void drawCivilian(Gfx& g, Vec2 at, u32 h, float stride, bool moving, int carry, float hammer = 0) {
    static const Color SHIRT[8] = { rgb(220, 70, 60), rgb(70, 120, 220), rgb(240, 200, 70), rgb(90, 180, 110), rgb(236, 236, 236), rgb(160, 90, 200), rgb(240, 140, 60), rgb(60, 60, 70) };
    static const Color SKIN[4] = { rgb(246, 210, 178), rgb(220, 170, 128), rgb(170, 116, 80), rgb(110, 74, 50) };
    Color shirt = carry == 3 || carry == 4 ? ((h >> 3) & 1 ? rgb(255, 150, 40) : rgb(236, 220, 60)) : SHIRT[h % 8];   // builders wear hi-vis
    Color skin = SKIN[(h >> 4) % 4], legs = (h >> 7) & 1 ? rgb(52, 58, 80) : rgb(90, 76, 60);
    float sw = moving ? std::sin(stride) * 2.0f : 0;
    g.fillCircle(at.x + 1.0f, at.y + 0.8f, 2.8f, rgb(0, 0, 0, 80));
    g.line(at.x - 1.0f, at.y - 3.4f, at.x - 1.0f + sw, at.y + 0.4f, legs);
    g.line(at.x + 1.0f, at.y - 3.4f, at.x + 1.0f - sw, at.y + 0.4f, legs);
    if (carry == 3) {   // a builder: one arm swings the hammer
        float up = hammer;
        g.line(at.x - 2.3f, at.y - 6.6f, at.x - 2.6f, at.y - 3.6f, skin);
        g.line(at.x + 2.3f, at.y - 6.6f, at.x + 4.0f, at.y - 6.0f - 3.5f * up, skin);
        g.line(at.x + 4.0f, at.y - 6.0f - 3.5f * up, at.x + 6.2f, at.y - 7.0f - 3.0f * up, rgb(70, 60, 50));
        g.fill((int)(at.x + 5.6f), (int)(at.y - 8.0f - 3.0f * up), 2, 2, rgb(150, 154, 160));
    } else {
        g.line(at.x - 2.3f, at.y - 6.6f, at.x - 2.6f - sw * 0.6f, at.y - 3.6f, skin);
        g.line(at.x + 2.3f, at.y - 6.6f, at.x + 2.6f + sw * 0.6f, at.y - 3.6f, skin);
    }
    g.fill((int)std::floor(at.x - 2.0f), (int)std::floor(at.y - 7.6f), 4, 5, shirt);
    if (carry == 1) g.fill((int)(at.x - 2.5f), (int)(at.y - 7.0f), 5, 4, rgb(176, 132, 82));                   // a cardboard box held in front
    if (carry == 2) g.fill((int)(at.x + 2.4f), (int)(at.y - 5.0f), 2, 3, rgb(232, 232, 220));                  // a shopping bag
    if (carry == 4) g.line(at.x - 5.0f, at.y - 9.0f, at.x + 5.0f, at.y - 7.6f, rgb(196, 160, 110));           // a plank on the shoulder
    g.fillCircle(at.x, at.y - 9.4f, 1.8f, skin);
    if (carry == 3 || carry == 4) g.fill((int)std::floor(at.x - 2.0f), (int)std::floor(at.y - 11.6f), 4, 2, rgb(250, 200, 40));   // hard hat
    else if ((h >> 9) & 1) g.fill((int)std::floor(at.x - 1.8f), (int)std::floor(at.y - 11.4f), 4, 2, (h >> 10) & 1 ? rgb(60, 40, 30) : rgb(200, 170, 90));
}

// The point 'u' px along the loop round a block's footprint, 'off' px outside it (clockwise from the north-west corner), and the heading there.
static Vec2 blockLoop(float x0, float y0, float fw, float fh, float off, float u, Vec2& dir) {
    float w = fw + 2 * off, h = fh + 2 * off, L = 2 * (w + h);
    u = std::fmod(u, L); if (u < 0) u += L;
    float ax = x0 - off, ay = y0 - off;
    if (u < w) { dir = Vec2(1, 0); return Vec2(ax + u, ay); }
    u -= w; if (u < h) { dir = Vec2(0, 1); return Vec2(ax + w, ay + u); }
    u -= h; if (u < w) { dir = Vec2(-1, 0); return Vec2(ax + w - u, ay + h); }
    u -= w; dir = Vec2(0, -1); return Vec2(ax, ay + h - u);
}

// The streets: people walking the pavements round the blocks (shoppers with bags, a courier with a box, folk out for a stroll) and a few cars
// on the roads. How many are out follows the town's population, which grows back as its blocks are rebuilt; when the town is under fire the
// cars stop and the people run. Everything comes from the clock and a hash: nothing is stored, nothing is simulated.
void Game::drawDistrictLife() {
    Gfx& g = g_gfx;
    const std::vector<u8>& ex = g_sim.players[g_sim.humanPlayer].explored;
    const bool rev = g_sim.revealed(g_sim.humanPlayer);
    float night = nightLevel();
    for (int d = 0; d < (int)g_sim.districts.size(); d++) {
        const District& D = g_sim.districts[d];
        Vec2 c = worldToScreen(D.center);
        float R = D.radius + 80;
        if (c.x < -R || c.y < -R || c.x > SCREEN_W + R || c.y > VIEW_H + R || D.blocks.empty()) continue;
        if (!rev && !ex[clampi(tileOf(D.center.y), 0, MAP_H - 1) * MAP_W + clampi(tileOf(D.center.x), 0, MAP_W - 1)]) continue;
        bool panic = g_sim.time - D.lastHit < 6.0f;
        int walkers = std::min(60, D.people / 2), cars = std::max(1, (int)D.blocks.size() / 3);
        if (panic) walkers /= 2;   // (half of them have taken cover)
        for (int i = 0; i < walkers + cars; i++) {
            bool car = i >= walkers;
            u32 h = (u32)(d * 7919u + i * 104729u + 977u); h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
            const Entity* b = g_sim.get(D.blocks[h % D.blocks.size()]);
            if (!b) continue;
            const BuildType& bt = b->bt();
            float fw = bt.w * TILE, fh = bt.h * TILE, x0 = b->tx * TILE - cam.x, y0 = b->ty * TILE - cam.y;
            float dirS = (h >> 5) & 1 ? 1.0f : -1.0f;
            float speed = car ? (panic ? 0.0f : 34.0f + (h >> 7) % 20) : (12.0f + (h >> 7) % 9) * (panic ? 2.6f : 1.0f);
            float u = dirS * (wallTime * speed + (h >> 11) % 997);
            Vec2 dir, at = blockLoop(x0, y0, fw, fh, car ? 18.0f : 9.0f + (h >> 3) % 3, u, dir);
            if (dirS < 0) dir = dir * -1.0f;
            if (at.x < -20 || at.y < -20 || at.x > SCREEN_W + 20 || at.y > VIEW_H + 20) continue;
            if (car) {   // a small saloon or van, its lights on after dark
                static const Color BODY[6] = { rgb(200, 52, 46), rgb(46, 98, 188), rgb(226, 226, 220), rgb(40, 40, 46), rgb(236, 190, 50), rgb(60, 140, 90) };
                Color bc = BODY[(h >> 9) % 6];
                float ang = std::atan2(dir.y, dir.x);
                Vec2 f = dir, s(-dir.y, dir.x);
                Vec2 p0 = at + s * 3.0f;   // drive on the right
                g.thickLine(p0.x - f.x * 6 + 1, p0.y - f.y * 6 + 2, p0.x + f.x * 6 + 1, p0.y + f.y * 6 + 2, rgb(0, 0, 0, 70), 7.0f);
                g.thickLine(p0.x - f.x * 6, p0.y - f.y * 6, p0.x + f.x * 6, p0.y + f.y * 6, bc, 6.0f);
                g.thickLine(p0.x + f.x * 1, p0.y + f.y * 1, p0.x + f.x * 3.5f, p0.y + f.y * 3.5f, rgb(30, 40, 54), 4.4f);   // windscreen
                if (night > 0.3f && !panic) { g.glowAdd(p0.x + f.x * 12, p0.y + f.y * 12, 9, Color{255, 236, 170, (u8)(120 * night)}); }
                (void)ang;
                continue;
            }
            int carry = ((h >> 13) % 7 == 0) ? 1 : ((h >> 13) % 5 == 0) ? 2 : 0;
            drawCivilian(g, at, h, u * 0.55f, true, panic ? 0 : carry);
        }
    }
}

// A razed block: its people clear the rubble and put it up again (builders in hard hats hammering round the site, one carrying planks). A damaged
// block that is out of the fire gets a couple of builders patching it. They down tools and run while the town is under fire.
void Game::drawRebuildCrew(const Entity& e, Vec2 p) {
    Gfx& g = g_gfx;
    if (e.district < 0 || p.x < -120 || p.y < -120 || p.x > SCREEN_W + 120 || p.y > VIEW_H + 120) return;
    bool ruin = !e.constructed;
    if (!ruin && (e.hp >= e.maxHp * 0.9f || g_sim.time - e.lastDamaged < DISTRICT_MEND_DELAY)) return;
    if (ruin && g_sim.time - e.lastDamaged < DISTRICT_REBUILD_DELAY) return;
    const BuildType& bt = e.bt();
    float fw = bt.w * TILE, fh = bt.h * TILE, x0 = p.x - fw * 0.5f, y0 = p.y - fh * 0.5f;
    int n = ruin ? 3 + bt.w * bt.h / 3 : 2;
    for (int i = 0; i < n; i++) {
        u32 h = (u32)(e.gen * 2654435761u) ^ (u32)(i * 40503u + 17u);
        float u = (i + 0.5f) / n * 2 * (fw + fh + 8) + (h % 23);
        Vec2 dir, at = blockLoop(x0, y0, fw, fh, 5.0f, u, dir);
        bool porter = ruin && i == n - 1;
        if (porter) {   // one carries planks to and fro along the site
            float t = wallTime * 0.35f + (h % 100) * 0.01f, k = std::fabs(std::fmod(t, 2.0f) - 1.0f);
            at = blockLoop(x0, y0, fw, fh, 9.0f, k * (fw + fh), dir);
            drawCivilian(g, at, h, wallTime * 6.0f, true, 4);
            continue;
        }
        float ph = wallTime * (5.0f + (h % 5)) + (h % 31);
        float up = 0.5f + 0.5f * std::sin(ph);
        drawCivilian(g, at, h, 0, false, 3, up);
        if (up < 0.08f && ((int)(wallTime * 10.0f + i) % 3) == 0) g.glowAdd(at.x + 6.0f, at.y - 6.0f, 4.0f, Color{255, 220, 150, 170});   // the blow lands
    }
}

// Each town's name over its streets, in the colour of whoever holds it, with its state (rebuilding, too damaged to pay).
void Game::drawDistrictLabels() {
    Gfx& g = g_gfx;
    const std::vector<u8>& ex = g_sim.players[g_sim.humanPlayer].explored;
    const bool rev = g_sim.revealed(g_sim.humanPlayer);
    for (int d = 0; d < (int)g_sim.districts.size(); d++) {
        const District& D = g_sim.districts[d];
        Vec2 c = worldToScreen(D.center);
        if (c.x < -300 || c.y < -200 || c.x > SCREEN_W + 300 || c.y > VIEW_H + 200) continue;
        if (!rev && !ex[clampi(tileOf(D.center.y), 0, MAP_H - 1) * MAP_W + clampi(tileOf(D.center.x), 0, MAP_W - 1)]) continue;
        char buf[128];
        const char* who = D.owner < 0 ? "neutral" : D.owner == g_sim.humanPlayer ? "ours" : g_sim.enemies(g_sim.humanPlayer, D.owner) ? "enemy" : "ally";
        snprintf(buf, sizeof buf, "%s  (%s)", D.name, who);
        Color col = D.owner < 0 ? rgb(236, 228, 200) : playerColor(D.owner);
        float top = c.y - D.radius - 10;
        int w = g.textW(buf, 1);
        g.fill((int)c.x - w / 2 - 4, (int)top - 2, w + 8, 12, rgb(10, 12, 16, 150));
        g.text((int)c.x - w / 2, (int)top, buf, col);
        const char* st = nullptr; char sb[96];
        if (D.owner >= 0 && D.integrity < DISTRICT_PAY_MIN) { snprintf(sb, sizeof sb, "too damaged to pay (%d%%)", (int)(D.integrity * 100)); st = sb; }
        else if (D.ruins > 0) { snprintf(sb, sizeof sb, "rebuilding %d block%s", D.ruins, D.ruins == 1 ? "" : "s"); st = sb; }
        else if (g_sim.time - D.lastHit < 6.0f) st = "under fire";
        if (st) { int sw = g.textW(st, 1); g.text((int)c.x - sw / 2, (int)top + 11, st, rgb(255, 196, 120)); }
    }
}
