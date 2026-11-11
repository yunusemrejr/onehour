// One Hour - headless tests 25 (--sonartest): the sonar that finds a computer army's dived submarine. Only the human's ships within SONAR_RANGE (and aircraft
// within SONAR_AIR_RANGE) hear it; a contact can be shot at and is seen while it lasts and SONAR_LINGER seconds after; no computer army hears it, and
// an army that has nothing but submarines left is beaten
#include "ai.h"
#include "testutil.h"

static void run25(float sec) { for (int t = 0, n = (int)(sec * SIM_HZ); t < n && !g_sim.gameOver; t++) { g_sim.step(); g_sim.events.clear(); } }

// player 0 is the human; players 1 and 2 are computer armies on two other teams. The computer commanders do not run (no g_ai.update here): the boats
// and ships the cases place stay where they are put, unless the case orders them
static void scene25(u64 seed) {
    Faction fac[3] = { F_CYBER, F_CYBER, F_CYBER }; bool ai[3] = { false, true, true }; int diff[3] = { 1, 1, 1 }; int team[3] = { 0, 1, 2 };
    g_sim.init(3, fac, ai, diff, team, seed);
    g_ai.init(seed);
    g_sim.riotsOn = false;
    for (int p = 0; p < 3; p++) g_sim.players[p].money = 100000;
}

// a sea spot about 'tiles' from c (open sea, within a tile of the distance), or false if the sea offers none
static bool seaAt25(Vec2 c, float tiles, Vec2& out) {
    for (int k = 0; k < 72; k++) {   // straight out from c at 72 headings: the first spot that is open sea of the same body of water
        float a = k * 0.0872665f;
        Vec2 w = c + Vec2(std::cos(a), std::sin(a)) * (tiles * TILE);
        if (w.x < TILE || w.y < TILE || w.x > WORLD_W - TILE || w.y > WORLD_H - TILE) continue;
        if (!g_map.navOpen(tileOf(w.x), tileOf(w.y)) || g_map.compAt(w) != g_map.compAt(c)) continue;
        out = w; return true;
    }
    return false;
}

// a spot of open sea west of the main island with open sea all round it out to 15 tiles (so every case finds its 8, 14 and 30 tile spots)
static Vec2 openSea25() {
    for (int ty = MAP_H / 2, k = 0; k < MAP_H / 2 - 20; k++, ty = MAP_H / 2 + ((k & 1) ? (k + 1) / 2 : -(k / 2)))
        for (int tx = 20; tx < ISLE_X0 - 16; tx++) {
            bool ok = true;
            for (int a = 0; a < 16 && ok; a++) for (int r = 0; r <= 15 && ok; r += 3) {
                int x = tx + (int)std::lround(std::cos(a * 0.3927f) * r), y = ty + (int)std::lround(std::sin(a * 0.3927f) * r);
                ok = g_map.navOpen(x, y) && g_map.openSea(x, y);
            }
            if (ok) return tileCenter(tx, ty);
        }
    return g_map.nearestWater(Vec2((ISLE_X0 - 34) * TILE, WORLD_H * 0.5f), 14, true, Map::OPEN_SEA);
}

// a surfaced boat of 'owner' at c, dived (it has had its time to go under)
static Ref divedBoat25(int owner, Vec2 c) {
    Ref s = g_sim.spawnUnit(U_C_SUB, owner, c);
    if (!g_sim.get(s)) return NOREF;
    g_sim.cmdDive({s}, true);
    run25(SUB_DIVE_TIME + 0.5f);
    return s;
}

static const char* why25 = "";
#define NEED25(c, m) do { if (!(c)) { why25 = m; return false; } } while (0)
static char detail25[160] = "";

// ---- (a) the human's destroyer 8 tiles from a dived computer boat: the sonar finds it, the destroyer shoots it and hurts it
static bool caseA25(u64 seed) {
    scene25(seed);
    Vec2 c = openSea25();
    Ref sub = divedBoat25(1, c);
    NEED25(g_sim.get(sub) && Sim::submerged(*g_sim.get(sub)), "the computer boat did not dive");
    Vec2 d8; NEED25(seaAt25(c, 8, d8), "no sea 8 tiles off");
    Ref ds = g_sim.spawnUnit(U_C_DESTROYER, 0, d8);
    NEED25(g_sim.get(ds), "the destroyer did not spawn");
    run25(0.5f);
    NEED25(g_sim.visibleTo(*g_sim.get(sub), 0), "the sonar did not find the dived boat 8 tiles from a destroyer");
    const float hp0 = g_sim.get(sub)->hp;
    g_sim.cmdAttack({ds}, sub);
    bool canHit = false;
    for (int k = 0; k < 40 * SIM_HZ && g_sim.get(sub) && g_sim.get(sub)->hp >= hp0; k++) {
        g_sim.step(); g_sim.events.clear();
        if (g_sim.get(ds) && g_sim.get(sub) && g_sim.canTarget(*g_sim.get(ds), *g_sim.get(sub))) canHit = true;
    }
    NEED25(canHit, "the destroyer could never target the sonar contact");
    NEED25(!g_sim.get(sub) || g_sim.get(sub)->hp < hp0, "the destroyer never hurt the sonar contact");
    snprintf(detail25, sizeof detail25, "contact at 8 tiles, shot at, hull %.0f%% after %.1f s", g_sim.get(sub) ? 100.0f * g_sim.get(sub)->hp / hp0 : 0.0f, g_sim.time);
    return true;
}

// ---- (b) the same distance from a computer enemy's destroyer: that army never finds the boat, and is never hurt by it
static bool caseB25(u64 seed) {
    scene25(seed);
    Vec2 c = openSea25();
    Ref sub = divedBoat25(1, c);
    NEED25(g_sim.get(sub) && Sim::submerged(*g_sim.get(sub)), "the computer boat did not dive");
    Vec2 d8; NEED25(seaAt25(c, 8, d8), "no sea 8 tiles off");
    Ref foe = g_sim.spawnUnit(U_C_DESTROYER, 2, d8);
    NEED25(g_sim.get(foe), "the enemy destroyer did not spawn");
    g_sim.get(foe)->order = O_IDLE;
    const float hp0 = g_sim.get(sub)->hp;
    for (int k = 0; k < 20 * SIM_HZ; k++) {
        g_sim.step(); g_sim.events.clear();
        NEED25(g_sim.get(sub), "the boat was sunk by a computer army's destroyer");
        NEED25(!g_sim.visibleTo(*g_sim.get(sub), 2), "a computer army found a dived boat with its sonar");
        NEED25(g_sim.get(sub)->hp >= hp0, "a computer army's destroyer hurt a dived boat");
    }
    NEED25(!g_sim.canTarget(*g_sim.get(foe), *g_sim.get(sub)), "a computer army's destroyer could target a dived boat");
    snprintf(detail25, sizeof detail25, "a computer enemy 8 tiles off neither finds it nor hurts it in 20 s");
    return true;
}

// ---- (c) the human's own dived boat next to a computer army's destroyer: that army cannot see it or hit it
static bool caseC25(u64 seed) {
    scene25(seed);
    Vec2 c = openSea25();
    Ref sub = divedBoat25(0, c);
    NEED25(g_sim.get(sub) && Sim::submerged(*g_sim.get(sub)), "our boat did not dive");
    Vec2 d8; NEED25(seaAt25(c, 8, d8), "no sea 8 tiles off");
    Ref foe = g_sim.spawnUnit(U_C_DESTROYER, 1, d8);
    NEED25(g_sim.get(foe), "the destroyer did not spawn");
    g_sim.get(foe)->order = O_IDLE;
    const float hp0 = g_sim.get(sub)->hp;
    for (int k = 0; k < 15 * SIM_HZ; k++) {
        g_sim.step(); g_sim.events.clear();
        NEED25(g_sim.get(sub), "a computer army sank our dived boat");
        NEED25(!g_sim.visibleTo(*g_sim.get(sub), 1), "a computer army's sonar found our dived boat");
        NEED25(g_sim.get(sub)->hp >= hp0, "a computer army's destroyer hurt our dived boat");
    }
    NEED25(g_sim.visibleTo(*g_sim.get(sub), 0), "we cannot see our own dived boat");
    snprintf(detail25, sizeof detail25, "our dived boat next to a computer destroyer: unseen and unhurt for 15 s");
    return true;
}

// ---- (d) at 14 tiles there is no contact: the sonar reaches 11
static bool caseD25(u64 seed) {
    scene25(seed);
    Vec2 c = openSea25();
    Ref sub = divedBoat25(1, c);
    NEED25(g_sim.get(sub) && Sim::submerged(*g_sim.get(sub)), "the computer boat did not dive");
    Vec2 d14; NEED25(seaAt25(c, 14, d14), "no sea 14 tiles off");
    Ref ds = g_sim.spawnUnit(U_C_DESTROYER, 0, d14);
    NEED25(g_sim.get(ds), "the destroyer did not spawn");
    g_sim.get(ds)->order = O_IDLE;
    const float hp0 = g_sim.get(sub)->hp;
    for (int k = 0; k < 3 * SIM_HZ; k++) {
        g_sim.step(); g_sim.events.clear();
        NEED25(!g_sim.sonarContact(*g_sim.get(sub), 0), "a destroyer 14 tiles off made a contact");
    }
    NEED25(!g_sim.visibleTo(*g_sim.get(sub), 0), "a destroyer 14 tiles off can see a dived boat");
    NEED25(!g_sim.canTarget(*g_sim.get(ds), *g_sim.get(sub)), "a destroyer 14 tiles off can target a dived boat");
    NEED25(g_sim.get(sub)->hp >= hp0, "a destroyer 14 tiles off hurt a dived boat");
    snprintf(detail25, sizeof detail25, "no contact at 14 tiles (sonar reaches %.0f)", SONAR_RANGE);
    return true;
}

// ---- (e) the contact drops SONAR_LINGER seconds after the last ship has gone out of range
static bool caseE25(u64 seed) {
    scene25(seed);
    Vec2 c = openSea25();
    Ref sub = divedBoat25(1, c);
    NEED25(g_sim.get(sub) && Sim::submerged(*g_sim.get(sub)), "the computer boat did not dive");
    Vec2 d8; NEED25(seaAt25(c, 8, d8), "no sea 8 tiles off");
    Vec2 far; NEED25(seaAt25(c, 30, far), "no sea 30 tiles off");
    Ref ds = g_sim.spawnUnit(U_C_DESTROYER, 0, d8);
    NEED25(g_sim.get(ds), "the destroyer did not spawn");
    g_sim.get(ds)->order = O_IDLE;
    run25(1.0f);
    NEED25(g_sim.sonarContact(*g_sim.get(sub), 0), "no contact with the destroyer 8 tiles off");
    Entity* D = g_sim.get(ds);
    D->pos = far; D->prevPos = far;   // the destroyer sails off out of range
    const double tLeave = g_sim.time;
    double tDrop = -1;
    for (int k = 0; k < 10 * SIM_HZ && tDrop < 0; k++) {
        g_sim.step(); g_sim.events.clear();
        if (!g_sim.sonarContact(*g_sim.get(sub), 0)) tDrop = g_sim.time;
    }
    NEED25(tDrop >= 0, "the contact never dropped");
    const double after = tDrop - tLeave;
    NEED25(after >= SONAR_LINGER - 0.5 && after <= SONAR_LINGER + 0.5, "the contact did not drop about SONAR_LINGER seconds after the destroyer left");
    snprintf(detail25, sizeof detail25, "contact dropped %.1f s after the destroyer left (linger %.0f s)", after, SONAR_LINGER);
    return true;
}

// ---- (f) an army with nothing but a submarine is beaten (and surrenders after SURRENDER_GRACE); with one structure more, it is not
static bool caseF25(u64 seed) {
    scene25(seed);
    Vec2 c = openSea25();
    Ref sub = g_sim.spawnUnit(U_C_SUB, 1, c);
    NEED25(g_sim.get(sub), "the boat did not spawn");
    for (auto& e : g_sim.ents) if (e.alive && e.owner == 1 && e.kind != EK_RESOURCE && g_sim.refOf(e) != sub) g_sim.destroy(e, false);
    NEED25(g_sim.armyBeaten(1), "an army with nothing but a submarine is not beaten");
    Vec2 g2 = openGroundNear(g_sim.players[1].basePos + Vec2(-12 * TILE, 0), 4);
    Ref bk = NOREF;
    for (int r = 0; r < 10 && !bk.valid(); r++) for (int dy = -r; dy <= r && !bk.valid(); dy++) for (int dx = -r; dx <= r && !bk.valid(); dx++)
        if (g_sim.canPlace(1, B_C_BARRACKS, tileOf(g2.x) + dx, tileOf(g2.y) + dy)) bk = g_sim.placeBuilding(B_C_BARRACKS, 1, tileOf(g2.x) + dx, tileOf(g2.y) + dy, true);
    NEED25(g_sim.get(bk), "no room for the structure");
    NEED25(!g_sim.armyBeaten(1), "an army with a submarine and a structure is beaten");
    g_sim.destroy(*g_sim.get(bk), false);
    NEED25(g_sim.armyBeaten(1), "an army that lost its structure again is not beaten");
    // and it lays down its arms once it has been beaten for SURRENDER_GRACE seconds
    for (int k = 0; k < (int)((SURRENDER_GRACE * 0.5f) * SIM_HZ); k++) { g_sim.step(); g_sim.events.clear(); }
    NEED25(g_sim.get(sub), "the boat was lost before the surrender grace ran out");
    for (int k = 0; k < (int)((SURRENDER_GRACE * 0.75f) * SIM_HZ) && g_sim.get(sub); k++) { g_sim.step(); g_sim.events.clear(); }
    NEED25(!g_sim.get(sub), "a beaten army with only a submarine did not surrender");
    snprintf(detail25, sizeof detail25, "submarine alone: beaten, surrendered after the grace; with a structure: not beaten");
    return true;
}

static bool runCase25(const char* tag, const char* name, bool (*fn)(u64), u64 seed) {
    why25 = "";
    detail25[0] = 0;
    bool ok = fn(seed);
    if (ok) printf("sonartest: (%s) PASS %s - %s\n", tag, name, detail25);
    else printf("sonartest: (%s) FAIL %s - %s\n", tag, name, why25);
    return ok;
}

bool sonarTest(u64 seed) {
    bool ok = true;
    ok &= runCase25("a", "human's destroyer finds and hurts a dived computer boat 8 tiles off", caseA25, seed);
    ok &= runCase25("b", "a computer enemy's destroyer 8 tiles off finds and hurts nothing", caseB25, seed + 1);
    ok &= runCase25("c", "our dived boat next to a computer destroyer stays unseen", caseC25, seed + 2);
    ok &= runCase25("d", "no contact at 14 tiles", caseD25, seed + 3);
    ok &= runCase25("e", "a contact lasts SONAR_LINGER seconds after the destroyer leaves", caseE25, seed + 4);
    ok &= runCase25("f", "an army with only a submarine is beaten", caseF25, seed + 5);
    printf("sonartest: %s\n", ok ? "ok" : "FAILED");
    return ok;
}
