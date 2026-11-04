// One Hour - the ambience beds follow what is on screen and the sky: sea surf from the water in view, rain and storm from the weather,
// the distant battle from the blasts in view
#include "ui.h"

void Game::updateAmbience() {
    if (state != GS_PLAYING) { g_audio.setAmbience(0, 0, 0, 0); return; }
    // sea: sample the view on a coarse 8 x 6 grid; the share of big water (the sea or the lake) is the surf's level
    int wet = 0, tot = 0;
    for (int gy = 0; gy < 6; gy++)
        for (int gx = 0; gx < 8; gx++) {
            int tx = tileOf(cam.x + (gx + 0.5f) * SCREEN_W / 8.0f), ty = tileOf(cam.y + (gy + 0.5f) * VIEW_H / 6.0f);
            tot++;
            if (g_map.bigWater(tx, ty)) wet++;
        }
    float sea = clampf(wet / (float)tot * 1.4f, 0, 1);   // half the view in water is already a full surf
    // weather: the rain bed follows the downpour; above 0.75 it is a storm, with gusts and heavier rain
    float rain = clampf(weather.rain, 0, 1);
    float storm = clampf((rain - 0.75f) * 4, 0, 1);
    // battle: the blasts in view (recent explosion effects), a dozen or more is a full rumble
    int blasts = 0;
    for (const Fx& f : g_sim.fx) {
        if (f.type != FX_EXPLODE) continue;
        if (f.a.x < cam.x - 120 || f.a.x > cam.x + SCREEN_W + 120 || f.a.y < cam.y - 120 || f.a.y > cam.y + VIEW_H + 120) continue;
        if (++blasts >= 12) break;
    }
    float battle = clampf(blasts / 6.0f, 0, 1);
    g_audio.setAmbience(sea, rain, storm, battle);
    // the sounds of what just happened on the battlefield: every blast by its size, a nuke's detonation, a structure falling in, a ship going
    // down (the sim adds an effect the tick it happens; anything younger than the ticks run since the last frame is new)
    static u32 lastTick = 0;
    const u32 now = g_sim.tick;
    if (now < lastTick) lastTick = 0;   // (a new match)
    const float fresh = (now - lastTick) * SIM_DT + 1e-4f;
    lastTick = now;
    if (fresh > 1.0f) return;   // (the first frame of a match, or after a long stall: no burst of old sounds)
    int n = 0;
    for (const Fx& f : g_sim.fx) {
        if (f.t > fresh) continue;
        switch (f.type) {
        case FX_EXPLODE: if (f.vel.y >= 0 && ++n <= 6) g_audio.explosion(f.a, f.size); break;   // (a carpet-bomb flash marked "light" makes no extra boom)
        case FX_MUSHROOM: g_audio.nuke(f.a); break;
        case FX_RUBBLE: g_audio.play(SND_COLLAPSE, f.a, false, 0.9f); break;
        case FX_WRECK: if (f.vel.y > 1.5f) { g_audio.play(SND_SPLASH, f.a, false, 0.9f); g_audio.play(SND_COLLAPSE, f.a, false, 0.6f); } break;   // (a ship going down; vel.y 1 marks a fallen soldier)
        default: break;
        }
    }
}
