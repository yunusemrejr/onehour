// One Hour - sonar contacts: a computer army's dived submarine that one of the human's ships, naval yards or aircraft has heard (SONAR_RANGE in
// data.h). For the human it is drawn with its dived shadow (where the fog would otherwise hide it), a thin pulsing ring and its hull's health bar, and
// on the minimap as a blinking dot. A dived computer boat that is moving over water the human can see, but is no contact, leaves a faint surface
// feather every couple of seconds, so a boat cruising under your nose is worth watching. Nothing here is drawn outside the human's sight, except the
// contacts themselves, which the sonar has found. These are Game members (see ui.h); the call sites are in ui.cpp.
#include "ui.h"
#include "art.h"

// the hull's health bar, as hpBar in ui.cpp draws it (same place and size as a surfaced hull's)
static void sonarHpBar(Gfx& g, float x, float y, float w, float frac) {
    g.fill((int)x, (int)y, (int)w, 3, rgb(0, 0, 0, 200));
    Color c = frac > 0.6f ? rgb(80, 220, 80) : (frac > 0.3f ? rgb(240, 200, 60) : rgb(230, 70, 60));
    g.fill((int)x + 1, (int)y + 1, (int)((w - 2) * clampf(frac, 0, 1)), 1, c);
}

// The sonar's colour: a cool cyan, the same as the spotted stealth units
static const Color SONAR_COL = { 110, 235, 255, 255 };

void Game::drawSonar() {
    Gfx& g = g_gfx;
    const int hp = g_sim.humanPlayer;
    if (hp < 0 || hp >= g_sim.numPlayers) return;
    const bool inSightAny = !g_sim.inSight.empty();
    const float pulse = 0.5f + 0.5f * std::sin(wallTime * 6.2831853f);   // one beat a second
    for (auto& e : g_sim.ents) {
        if (!e.alive || !Sim::submerged(e) || e.owner < 0 || !g_sim.players[e.owner].isAI) continue;   // (a computer army's dived boats only)
        const Vec2 p = worldToScreen(entPos(e));
        if (p.x < -80 || p.y < -80 || p.x > SCREEN_W + 80 || p.y > VIEW_H + 80) continue;
        if (g_sim.sonarContact(e, hp)) {
            const UnitType& ut = e.ut();
            const int fi = ut.faction == F_CYBER ? 0 : 1, sl = slotOf(e.owner);
            // out of the fog's sight the dived shadow is not drawn by the world pass (its shown-to-you rule does not know about the sonar)
            if (!g_sim.shownToHuman(e) && g.subArt[fi][sl][0].tex) g.draw(g.subArt[fi][sl][0], p.x, p.y, e.angle, 1, rgb(255, 255, 255), (u8)(255 * clampf(e.fade, 0, 1)));
            const float r = e.radius() * 1.5f + 3.0f + 2.0f * pulse;
            g.circle(p.x, p.y, r, Color{ SONAR_COL.r, SONAR_COL.g, SONAR_COL.b, (u8)(110 + 100 * pulse) }, 32);
            sonarHpBar(g, p.x - 18, p.y - e.radius() - 7, 36, e.hp / e.maxHp);
            continue;
        }
        // no contact: a dived boat that moves over sea the human sees gives its position away now and then, as a faint feather of surface water
        if (!inSightAny || (e.pos - e.prevPos).len2() <= 0.01f) continue;   // (moving, and on a tile the human sees right now: not merely explored)
        const int tx = clampi(tileOf(e.pos.x), 0, MAP_W - 1), ty = clampi(tileOf(e.pos.y), 0, MAP_H - 1);
        if (!g_sim.inSight[ty * MAP_W + tx]) continue;
        const float ph = std::fmod(wallTime + (float)(e.gen % 97) * 0.37f, 2.0f);   // one feather every two seconds, each boat at its own time
        if (ph >= 1.4f) continue;
        const float env = clampf(std::min(ph / 0.35f, (1.4f - ph) / 0.6f), 0, 1);
        const Vec2 f(std::cos(e.angle), std::sin(e.angle)), s(-f.y, f.x);
        const Vec2 stern = p - f * (e.radius() * 0.7f);
        for (int side = -1; side <= 1; side += 2) {   // two thin lines that open out behind it
            Vec2 a = stern + s * (side * 2.0f);
            Vec2 b = stern - f * 18.0f + s * (side * 9.0f);
            g.line(a.x, a.y, b.x, b.y, Color{ 220, 236, 246, (u8)(70 * env) });
        }
    }
}

void Game::drawSonarMinimap(int x, int y, int size) {
    Gfx& g = g_gfx;
    const int hp = g_sim.humanPlayer;
    if (((int)(wallTime * 2.0f)) & 1) return;   // blinks: on half a second, off half a second
    for (auto& e : g_sim.ents) {
        if (!e.alive || !Sim::submerged(e) || !g_sim.sonarContact(e, hp)) continue;
        int mx = (int)(x + e.pos.x / WORLD_W * size), my = (int)(y + e.pos.y / WORLD_H * size);
        g.fill(mx - 2, my - 2, 4, 4, rgb(SONAR_COL.r, SONAR_COL.g, SONAR_COL.b));
    }
}
