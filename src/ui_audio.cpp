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
}
