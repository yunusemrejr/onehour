// One Hour - the look of a forest fire (the fire itself lives in sim_fire.cpp): the canopy charring under the flames, tongues of flame
// licking up through each burning tile, a hot glow at its foot, and the smoke and embers it throws downwind. Its light on the ground round
// it is part of the light buffer (light.cpp).
#include "ui.h"

void Game::drawWildfire() {
    if (g_sim.fires.empty()) return;
    Gfx& g = g_gfx;
    const int tx0 = std::max(0, (int)(cam.x / TILE) - 2), ty0 = std::max(0, (int)(cam.y / TILE) - 2);
    const int tx1 = std::min(MAP_W - 1, (int)((cam.x + SCREEN_W) / TILE) + 2), ty1 = std::min(MAP_H - 1, (int)((cam.y + VIEW_H) / TILE) + 2);
    const int hp = g_sim.humanPlayer;
    static std::vector<const FireCell*> vis; vis.clear();
    for (const FireCell& c : g_sim.fires)
        if (c.tx >= tx0 && c.tx <= tx1 && c.ty >= ty0 && c.ty <= ty1 && g_sim.explored(hp, c.tx, c.ty)) vis.push_back(&c);
    if (vis.empty()) return;
    // the canopy blackening as the tile burns (a scorch splotch that darkens with the fuel used)
    for (const FireCell* c : vis) {
        const float burnt = clampf(1.0f - c->fuel / Sim::FIRE_FUEL, 0.0f, 1.0f);
        const u32 h = (u32)c->tx * 73856093u ^ (u32)c->ty * 19349663u;
        const Vec2 s = worldToScreen(Vec2((c->tx + 0.5f) * TILE, (c->ty + 0.5f) * TILE));
        g.draw(g.fxs.scorch[h % 3], s.x, s.y - 4, 0, TILE * 1.7f / 96.0f, rgb(255, 255, 255), (u8)(120 + 135 * burnt));
        if (detail >= 2) g.draw(g.fxs.scorch[(h >> 4) % 3], s.x + 3, s.y - 10, 0, TILE * 1.2f / 96.0f, rgb(255, 255, 255), (u8)(70 + 150 * burnt));   // (the crown chars too)
    }
    // flames: three tongues per tile on their own beats, taller the hotter it burns; a hot glow at the foot
    g.beginAdd();
    for (const FireCell* c : vis) {
        const u32 h = (u32)c->tx * 2654435761u ^ (u32)c->ty * 40503u;
        const Vec2 s = worldToScreen(Vec2((c->tx + 0.5f) * TILE, (c->ty + 0.5f) * TILE));
        const float heat = clampf(c->heat, 0, 1);
        g.glowAdd(s.x, s.y + 2, TILE * (0.55f + 0.35f * heat), Color{255, 120, 40, (u8)(120 * heat)});
        const int tongues = detail >= 2 ? 3 : 2;   // (fewer when the frame is struggling: see Game::detail)
        for (int k = 0; k < tongues; k++) {
            const u32 hk = h * (k + 1) * 0x9E3779B9u;
            const float ox = ((hk >> 8 & 255) / 255.0f - 0.5f) * TILE * 0.8f, oy = ((hk >> 16 & 255) / 255.0f - 0.5f) * TILE * 0.6f + 6;
            const int fr = (int)(wallTime * (11.0f + k * 2.3f) + (hk & 15)) % 6;
            const float sc = (0.55f + 0.75f * heat) * (0.75f + 0.25f * std::sin(wallTime * (5.0f + k) + (hk & 63)));
            g.draw(g.fxs.flame[fr], s.x + ox, s.y + oy, 0, sc, rgb(255, 255, 255), (u8)(150 + 100 * heat));
        }
    }
    g.flushAdd();
    // smoke and embers, carried off on the wind (cosmetic particles; only for what is on screen, thinned in a big blaze)
    if (frameDt <= 0) return;
    const float wa = g_sim.windAngle, ws = 10.0f + 26.0f * g_sim.windSpeed;
    const float wx = std::cos(wa) * ws, wy = std::sin(wa) * ws;
    const float thin = std::min(1.0f, 60.0f / (float)vis.size());
    for (const FireCell* c : vis) {
        const float heat = clampf(c->heat, 0, 1);
        const float x = (c->tx + 0.5f) * TILE, y = (c->ty + 0.5f) * TILE;
        if (fxRng.f() < 2.2f * heat * thin * frameDt)
            emitP(x + fxRng.f(-10, 10), y + fxRng.f(-8, 4) - 10, wx + fxRng.f(-5, 5), wy - fxRng.f(16, 30), fxRng.f(3.5f, 6.0f), 12, 50 + 16 * heat,
                  fxRng.f() < 0.5f ? rgb(64, 60, 56, 215) : rgb(112, 108, 102, 190), PK_SMOKE, (u8)fxRng.range(0, 3), 0, 0.35f, fxRng.f(0, 6), fxRng.f(-0.3f, 0.3f));
        if (fxRng.f() < 2.4f * heat * thin * frameDt)
            emitP(x + fxRng.f(-10, 10), y + fxRng.f(-10, 6) - 8, wx * 1.4f + fxRng.f(-12, 12), wy * 1.4f - fxRng.f(30, 60), fxRng.f(0.8f, 1.6f), 1.6f, 0.6f,
                  rgb(255, 170, 70, 255), PK_SPARK, 0, 10.0f, 0.6f, 0, 0);
    }
}
