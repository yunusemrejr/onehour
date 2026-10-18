// One Hour - headless tests for the Anti-Radiation Truck (--radtest)
//  * both armies build it at the Assembly Plant / War Factory; it is an unarmed support vehicle with a decontamination field
//  * the field shields units and structures inside it from fallout while the truck takes the absorbed radiation as damage
//  * the fallout zone it touches is scrubbed away (strength and radius decay, the haze shrinks) and radiation elsewhere is untouched
//  * a silver warhead's radiation is heavier: the same field takes clearly more damage out of a truck while it cleans one
#include "sim.h"
#include "map.h"
#include "testutil.h"

static bool g_ok9 = true;
static bool fail9(const char* m) { fprintf(stderr, "radtest: %s\n", m); g_ok9 = false; return false; }
#define CHECK9(c, m) do { if (!(c)) return fail9(m); } while (0)

static void run9(float sec) { int n = (int)(sec * SIM_HZ); for (int t = 0; t < n; t++) { g_sim.step(); g_sim.events.clear(); } }
static bool spot9(int player, int type, Vec2 near, int& tx, int& ty) {
    int cx = tileOf(near.x), cy = tileOf(near.y);
    for (int r = 0; r < 24; r++) for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
        if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
        if (g_sim.canPlace(player, type, cx + dx, cy + dy)) { tx = cx + dx; ty = cy + dy; return true; }
    }
    return false;
}
static Ref place9(int type, int owner, Vec2 near) {
    int tx, ty;
    if (!spot9(owner, type, near, tx, ty)) return NOREF;
    return g_sim.placeBuilding(type, owner, tx, ty, true);
}
// two armies, nothing on the field but their structures, everything explored, plenty of money; player 0 is a human
static void scene9(Faction f0, Faction f1, u64 seed) {
    Faction fac[2] = { f0, f1 };
    bool ai[2] = { false, true }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
    g_sim.init(2, fac, ai, diff, team, seed);
    for (int p = 0; p < 2; p++) { g_sim.players[p].money = 400000; std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); }
    for (auto& e : g_sim.ents) if (e.alive && e.isUnit()) g_sim.destroy(e, false);
    g_sim.fallouts.clear(); g_sim.nukes.clear();
}
static Vec2 arena9() {
    Vec2 mid = openGroundNear(Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f), 12);
    flattenArena(mid, 22, 18);   // open ground: the truck, a few bystanders and a building around the crater
    return mid;
}
// a bystander that will not walk out of the fallout on its own (the test is about the field, not about the AI deciding to flee)
static Ref frozen9(int type, int owner, Vec2 p) {
    Vec2 at = g_map.nearestFree(p, 6);
    Ref r = g_sim.spawnUnit(type, owner, at);
    if (Entity* e = g_sim.get(r)) { e->disabledUntil = 1e9f; e->pos = at; e->prevPos = at; }
    return r;
}
static int radType(Faction f) { return f == F_CYBER ? U_C_RADTRUCK : U_K_RADTRUCK; }

// ---- the unit's data row, the decontamination constants, and that the factory actually builds it
static bool constantsTest(Faction me, u64 seed) {
    Faction foe = me == F_CYBER ? F_CLANKER : F_CYBER;
    scene9(me, foe, seed);
    int rt = radType(me);
    const UnitType& u = UNITS[rt];
    CHECK9(std::string(u.name) == "Anti-Radiation Truck" && std::string(u.hotkey) == "R", "the support vehicle is not the Anti-Radiation Truck (key R)");
    CHECK9(u.role == UR_DECON && u.kind == UK_VEH && u.weapon < 0 && u.faction == me, "the Anti-Radiation Truck is not an unarmed support vehicle of this army");
    CHECK9(u.builtBy == BR_FACTORY && u.requires_ < 0, "the Anti-Radiation Truck is not built at the vehicle factory");
    CHECK9(u.hp > 0 && u.cost > 0 && u.buildTime > 0 && std::string(u.desc).find("radiation") != std::string::npos, "the Anti-Radiation Truck has no usable stats or description");
    CHECK9(UNITS[U_C_RADTRUCK].faction == F_CYBER && UNITS[U_K_RADTRUCK].faction == F_CLANKER, "both armies do not have an Anti-Radiation Truck");
    CHECK9(RADTRUCK_RADIUS > 0.0f && RADTRUCK_SCRUB > 0.0f && RADTRUCK_SCRUB < 1.0f && RADTRUCK_DAMAGE > 0.0f && RADTRUCK_DRAIN_CAP > 0.0f, "the decontamination constants are not usable");
    // built at the factory, with power, like every other vehicle
    int b0 = firstBuildOf(me);
    Vec2 mid = arena9();
    Ref pw = place9(b0 + BR_POWER, 0, mid + Vec2(-8 * TILE, -6 * TILE));
    Ref f = place9(b0 + BR_FACTORY, 0, mid + Vec2(-4 * TILE, -8 * TILE));
    CHECK9(pw.valid() && f.valid(), "could not build the factory for the production test");
    g_sim.updatePowerPublic();
    CHECK9(g_sim.unitAvailable(0, rt), "the Anti-Radiation Truck is not available with a factory");
    CHECK9(g_sim.cmdTrain(f, rt), "the factory refused to build the Anti-Radiation Truck");
    Ref made;
    for (int t = 0; t < 20 * 40 && !made.valid(); t++) { g_sim.step(); g_sim.events.clear(); for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit() && e.type == rt) made = g_sim.refOf(e); }
    CHECK9(made.valid(), "the Anti-Radiation Truck was never produced");
    return true;
}

// ---- the field: shielded units and structures take no fallout damage, the truck absorbs it, and the zone is scrubbed
static bool fieldTest(Faction me, u64 seed) {
    Faction foe = me == F_CYBER ? F_CLANKER : F_CYBER;
    scene9(me, foe, seed);
    Vec2 mid = arena9();
    g_sim.fallouts.push_back({ mid, 8.0f * TILE, 0.0f, 0.0f, 1.0f, -1, false, 80.0f });   // an 8-tile crater, 25% of a ramp nuke's life
    Ref truck = g_sim.spawnUnit(radType(me), 0, mid + Vec2(4 * TILE, 0));
    Entity* T = g_sim.get(truck);
    CHECK9(T != nullptr, "the test truck is missing");
    float truckHp0 = T->hp;
    Ref inf = frozen9(firstUnitOf(me) + 2, 0, mid + Vec2(2 * TILE, 1 * TILE));          // in the field, in the fallout
    Ref tank = frozen9(firstUnitOf(me) + 5, 0, mid + Vec2(3 * TILE, -2 * TILE));        // in the field, in the fallout
    Ref foeInf = frozen9(firstUnitOf(foe) + 2, 1, mid + Vec2(-1 * TILE, 2 * TILE));     // the field does not care whose side it is
    Ref ctrl = frozen9(firstUnitOf(me) + 2, 0, mid + Vec2(-2.1f * TILE, 0));            // in the fallout but outside the field
    CHECK9(inf.valid() && tank.valid() && foeInf.valid() && ctrl.valid(), "could not place the test bystanders");
    int b0 = firstBuildOf(me);
    Ref barracks = place9(b0 + BR_BARRACKS, 0, mid + Vec2(0, -1.5f * TILE));            // a structure inside the field too
    CHECK9(barracks.valid(), "could not place the test barracks");
    int medicT = me == F_CYBER ? U_C_MEDIC : U_K_MEDIC;
    Ref idler = g_sim.spawnUnit(medicT, 0, g_map.nearestFree(mid + Vec2(2 * TILE, 2 * TILE), 4));   // idle (not frozen, unarmed): it should not flee the fallout either
    CHECK9(idler.valid(), "no idle soldier for the field test");
    Vec2 idlerAt = g_sim.get(idler)->pos;
    float barHp0 = g_sim.get(barracks)->hp;
    float inf0 = g_sim.get(inf)->hp, tank0 = g_sim.get(tank)->hp, foe0 = g_sim.get(foeInf)->hp, ctrl0 = g_sim.get(ctrl)->hp;
    run9(1.5f);
    CHECK9(g_sim.get(idler) && g_sim.get(idler)->hp >= g_sim.get(idler)->maxHp - 0.01f && dist(g_sim.get(idler)->pos, idlerAt) < 2 * TILE, "an idle unit in the decontamination field fled the fallout instead of staying under the truck's shield");
    CHECK9(g_sim.get(inf) && g_sim.get(inf)->hp >= inf0 - 0.01f, "a soldier inside the decontamination field still took fallout damage");
    CHECK9(g_sim.get(tank) && g_sim.get(tank)->hp >= tank0 - 0.01f, "a tank inside the decontamination field still took fallout damage");
    CHECK9(g_sim.get(foeInf) && g_sim.get(foeInf)->hp >= foe0 - 0.01f, "an enemy soldier inside the decontamination field still took fallout damage");
    CHECK9(g_sim.get(barracks) && g_sim.get(barracks)->hp >= barHp0 - 0.01f, "a structure inside the decontamination field still took fallout damage");
    CHECK9(g_sim.get(ctrl) && g_sim.get(ctrl)->hp < ctrl0 - 0.5f, "a soldier in the fallout but outside the field took no damage");
    CHECK9(g_sim.get(truck) && g_sim.get(truck)->hp < truckHp0 - 1.0f, "the Anti-Radiation Truck did not take damage from the radiation it absorbed");
    CHECK9(!g_sim.fallouts.empty() && g_sim.fallouts[0].power < 1.0f && g_sim.fallouts[0].r < 8.0f * TILE, "the truck did not scrub the fallout zone it stood in");
    // drive the truck away and the same soldier starts taking damage: the protection came from its field
    Entity* T2 = g_sim.get(truck);
    T2->pos = mid + Vec2(40 * TILE, 0); T2->prevPos = T2->pos;
    float infHp = g_sim.get(inf) ? g_sim.get(inf)->hp : 0.0f;
    run9(1.5f);
    CHECK9(g_sim.get(inf) == nullptr || g_sim.get(inf)->hp < infHp - 0.5f, "the fallout still did no damage after the decontamination field left");
    return true;
}

// ---- scrubbing: the zone is gone after a while, the haze shrinks with it, the truck survives, and a far zone is untouched
static bool scrubTest(Faction me, u64 seed) {
    Faction foe = me == F_CYBER ? F_CLANKER : F_CYBER;
    scene9(me, foe, seed);
    Vec2 mid = arena9();
    // a real detonation so the zone comes with its hazy visual (FX_FALLOUT), tagged to it
    Sim::Nuke n; n.from = mid + Vec2(40 * TILE, 0); n.pos = mid; n.owner = 0; n.t = Sim::NUKE_FLIGHT - 0.05f;
    g_sim.nukes.push_back(n);
    run9(0.4f);
    CHECK9(!g_sim.fallouts.empty(), "the test nuke left no fallout");
    Vec2 farPos = mid + Vec2(70 * TILE, 0);
    if (farPos.x > WORLD_W - 4 * TILE) farPos = mid + Vec2(0, 70 * TILE);
    g_sim.fallouts.push_back({ farPos, 5.0f * TILE, 0.0f, 0.0f, 1.0f, -1, false, 80.0f });   // a second zone, far from the truck
    Ref truck = g_sim.spawnUnit(radType(me), 0, mid + Vec2(2 * TILE, 0));
    CHECK9(truck.valid(), "no truck for the scrub test");
    int visTag = g_sim.fallouts[0].visTag;
    float fx0 = 0.0f;
    for (auto& f : g_sim.fx) if (f.type == FX_FALLOUT && f.tag == visTag) { fx0 = f.size; break; }
    CHECK9(fx0 > 0.0f, "the fallout zone has no haze to shrink");
    run9(2.5f);
    float fx1 = 0.0f;
    for (auto& f : g_sim.fx) if (f.type == FX_FALLOUT && f.tag == visTag) { fx1 = f.size; break; }
    CHECK9(fx1 <= 0.0f || fx1 < fx0 * 0.9f, "the fallout haze did not shrink while the truck scrubbed it");
    // the far zone is out of the field's reach: unchanged
    for (auto& f : g_sim.fallouts) if (dist(f.pos, farPos) < TILE) CHECK9(f.power >= 0.99f && f.r >= 5.0f * TILE - 1.0f, "the truck scrubbed a fallout zone far outside its field");
    // keep scrubbing until the near zone is gone; the truck must still be alive (it takes damage, not instant death)
    bool gone = false;
    for (int t = 0; t < 20 * 30 && !gone; t++) {
        g_sim.step(); g_sim.events.clear();
        bool near = false;
        for (auto& f : g_sim.fallouts) if (dist(f.pos, mid) < 6 * TILE) near = true;
        gone = !near;
    }
    CHECK9(gone, "the Anti-Radiation Truck never scrubbed its crater away");
    CHECK9(g_sim.get(truck) != nullptr && g_sim.get(truck)->hp > 0.0f, "the Anti-Radiation Truck died scrubbing one tactical-nuke crater (it should survive with its health down)");
    CHECK9(g_sim.get(truck)->hp < g_sim.get(truck)->maxHp * 0.95f, "the truck scrubbed a whole crater without taking meaningful damage");
    // two trucks scrub faster than one: a fresh crater with two trucks is gone in about half the time
    scene9(me, foe, seed + 1);
    mid = arena9();
    g_sim.fallouts.push_back({ mid, 8.0f * TILE, 0.0f, 0.0f, 1.0f, -1, false, 80.0f });
    Ref t1 = g_sim.spawnUnit(radType(me), 0, mid + Vec2(-1 * TILE, 0)), t2 = g_sim.spawnUnit(radType(me), 0, mid + Vec2(1 * TILE, 0));
    CHECK9(t1.valid() && t2.valid(), "no trucks for the stacking test");
    run9(7.0f);
    CHECK9(g_sim.fallouts.empty(), "two trucks did not scrub a crater away faster than one");
    return true;
}

// ---- a silver warhead's radiation is heavier: the same field drains more health out of the truck that cleans it
static float absorbedOver(Faction me, u64 seed, float power, float tiles) {
    Faction foe = me == F_CYBER ? F_CLANKER : F_CYBER;
    scene9(me, foe, seed);
    Vec2 mid = arena9();
    g_sim.fallouts.push_back({ mid, tiles * TILE, 0.0f, 0.0f, power, -1, false, 240.0f });
    Ref truck = g_sim.spawnUnit(radType(me), 0, mid);
    if (!truck.valid()) return -1.0f;
    float hp0 = g_sim.get(truck)->hp;
    run9(1.0f);
    return g_sim.get(truck) ? hp0 - g_sim.get(truck)->hp : hp0;
}
static bool silverTest(Faction me, u64 seed) {
    float plain = absorbedOver(me, seed, 1.0f, 8.0f), silver = absorbedOver(me, seed + 1, UNUKE_FALLOUT, 24.0f);
    CHECK9(plain > 1.0f && silver > 1.0f, "the truck absorbed no radiation at all");
    CHECK9(silver > plain * 1.25f, "a silver warhead's much heavier radiation did not hurt the truck more than a ramp's");
    // and it survives one silver crater (the dose is not instant death), with less than half its health left at most
    Faction foe = me == F_CYBER ? F_CLANKER : F_CYBER;
    scene9(me, foe, seed + 2);
    Vec2 mid = arena9();
    g_sim.fallouts.push_back({ mid, 24.0f * TILE, 0.0f, 0.0f, UNUKE_FALLOUT, -1, false, 240.0f });
    Ref truck = g_sim.spawnUnit(radType(me), 0, mid);
    bool gone = false;
    for (int t = 0; t < 20 * 30 && !gone; t++) { g_sim.step(); g_sim.events.clear(); gone = g_sim.fallouts.empty(); }
    CHECK9(gone, "the truck never scrubbed a silver fallout zone");
    CHECK9(g_sim.get(truck) != nullptr, "one silver cloud killed a full-health Anti-Radiation Truck while it scrubbed it");
    CHECK9(g_sim.get(truck)->hp < g_sim.get(truck)->maxHp * 0.85f, "the truck scrubbed a silver cloud almost without damage");
    return true;
}

bool radiationTest(u64 seed) {
    g_ok9 = true;
    for (int fi = 0; fi < 2; fi++) {
        Faction me = (Faction)fi;
        if (!constantsTest(me, seed + fi)) return false;
        if (!fieldTest(me, seed + 20 + fi)) return false;
        if (!scrubTest(me, seed + 40 + fi)) return false;
        if (!silverTest(me, seed + 60 + fi)) return false;
        printf("radtest %s: the Anti-Radiation Truck (field shielding, absorbed damage, scrubbing and silver radiation) ok\n", FACTION_NAME[me]);
    }
    printf("radtest: ok\n");
    return g_ok9;
}
