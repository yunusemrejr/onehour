// One Hour - headless tests for the nuclear-strike balance pass and the Supersonic Bomber (--supersonictest)
//  * the Anti-Nuke Laser is 40% weaker against warheads and an Anti-Rocket wall's laser 70% weaker (rockets/shells/aircraft unchanged)
//  * the Underground Nuke does 2.7x a ramp's damage (10% weaker than 3x) and the underground bunkers have 20% less health (and are not stealthy)
//  * the Supersonic Bomber: both armies build it at the airfield; it is the fastest thing in the sky until its one compact nuclear bomb is gone,
//    then it slows right down. The bomb carries an Underground Nuke's damage in a much smaller fireball, and the plane survives its own blast.
#include "sim.h"
#include "map.h"
#include "testutil.h"

static bool g_ok8 = true;
static bool fail8(const char* m) { fprintf(stderr, "supersonictest: %s\n", m); g_ok8 = false; return false; }
#define CHECK8(c, m) do { if (!(c)) return fail8(m); } while (0)

static void run8(float sec) { int n = (int)(sec * SIM_HZ); for (int t = 0; t < n; t++) { g_sim.step(); g_sim.events.clear(); } }

// two armies, nothing on the field but their structures, everything explored, plenty of money; player 0 is a human (its nukes hurt everybody)
static void scene8(Faction f0, Faction f1, u64 seed) {
    Faction fac[2] = { f0, f1 };
    bool ai[2] = { false, true }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
    g_sim.init(2, fac, ai, diff, team, seed);
    for (int p = 0; p < 2; p++) { g_sim.players[p].money = 400000; std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); }
    for (auto& e : g_sim.ents) if (e.alive && e.isUnit()) g_sim.destroy(e, false);
    g_sim.fallouts.clear();
}
static bool spot8(int player, int type, Vec2 near, int& tx, int& ty) {
    int cx = tileOf(near.x), cy = tileOf(near.y);
    for (int r = 0; r < 26; r++) for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
        if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
        if (g_sim.canPlace(player, type, cx + dx, cy + dy)) { tx = cx + dx; ty = cy + dy; return true; }
    }
    return false;
}
static Ref place8(int type, int owner, Vec2 near) {
    int tx, ty;
    if (!spot8(owner, type, near, tx, ty)) return NOREF;
    return g_sim.placeBuilding(type, owner, tx, ty, true);
}
static Entity* makeBomberScene(Faction me, u64 seed, Ref& af, Ref& target, Vec2& mid) {
    Faction foe = me == F_CYBER ? F_CLANKER : F_CYBER;
    scene8(me, foe, seed);
    int b0 = firstBuildOf(me), b1 = firstBuildOf(foe);
    mid = openGroundNear(g_sim.players[0].basePos + (g_sim.players[1].basePos - g_sim.players[0].basePos) * 0.5f, 14);
    flattenArena(mid, 30, 42);   // room for the 10x7 airfield and the target factories, and a long run-in for the bomber
    for (int dy = -11; dy < 11 && !g_sim.get(af); dy++) for (int dx = -11; dx < 11 && !g_sim.get(af); dx++) {
        int tx = tileOf(mid.x) + dx, ty = tileOf(mid.y) + dy;
        if (g_sim.canPlace(0, b0 + BR_AIRFIELD, tx, ty)) af = g_sim.placeBuilding(b0 + BR_AIRFIELD, 0, tx, ty, true);
    }
    if (!g_sim.get(af)) { fprintf(stderr, "scene: no airfield spot near %.0f,%.0f\n", mid.x, mid.y); return nullptr; }
    Ref p1 = place8(b0 + BR_POWER, 0, g_sim.get(af)->pos + Vec2(-7 * TILE, -4 * TILE));
    Ref p2 = place8(b0 + BR_POWER, 0, g_sim.get(af)->pos + Vec2(-7 * TILE, 2 * TILE));
    place8(b0 + BR_TECH, 0, g_sim.get(af)->pos + Vec2(0, -6 * TILE));
    (void)p1; (void)p2;
    // the nuclear target: a factory 34 tiles off the airfield (a run-up long enough for the bomber to reach its dash before the release
    // point), and a second one 20 tiles to its side (outside the compact fireball, inside the reach a full silver warhead would have had)
    Vec2 tc = g_sim.get(af)->pos + Vec2(0, 34 * TILE);
    target = place8(b1 + BR_FACTORY, 1, tc);
    place8(b1 + BR_FACTORY, 1, g_sim.get(af)->pos + Vec2(20 * TILE, 34 * TILE));
    g_sim.updatePowerPublic();
    return g_sim.get(af);
}

// ---- the changed numbers themselves, the new unit's data row, and the bunkers' lost health / stealth
static bool constantsTest(Faction me, u64 seed) {
    Faction foe = me == F_CYBER ? F_CLANKER : F_CYBER;
    scene8(me, foe, seed);
    int bunker = me == F_CYBER ? B_C_BUNKER : B_K_BUNKER, tech = firstBuildOf(me) + BR_TECH;
    CHECK8(UNITS[U_C_SBOMBER].faction == F_CYBER && UNITS[U_K_SBOMBER].faction == F_CLANKER, "the Supersonic Bomber is not an aircraft of both armies");
    int sbt = me == F_CYBER ? U_C_SBOMBER : U_K_SBOMBER;
    const UnitType& sb = UNITS[sbt];
    CHECK8(std::string(sb.name) == "Supersonic Bomber" && std::string(sb.hotkey) == "B", "the new aircraft is not called Supersonic Bomber (key B)");
    CHECK8(sb.kind == UK_AIR && sb.role == UR_COMBAT && sb.jet && sb.bomber && sb.nukeBomb && !sb.heli && !sb.stealth, "the Supersonic Bomber is not a fast fixed-wing nuclear bomber");
    CHECK8(sb.builtBy == BR_AIRFIELD && sb.requires_ == (me == F_CYBER ? B_C_TECH : B_K_TECH), "the Supersonic Bomber is not built at the airfield with a tech structure");
    CHECK8(sb.ammo == 1 && sb.weapon == W_SBOMBER_GUN && WEAPONS[W_SBOMBER_GUN].air && WEAPONS[W_SBOMBER_GUN].ground, "the Supersonic Bomber does not carry exactly one bomb and a defensive tail cannon");
    CHECK8(sb.sprintSpeed == SBOMBER_DASH && sb.speed == SBOMBER_SLOW && SBOMBER_DASH > 800.0f && SBOMBER_SLOW < 450.0f && SBOMBER_DASH > sb.speed * 2.0f, "the Supersonic Bomber is not very fast with its payload and slow without it");
    CHECK8(WEAPONS[W_NUKE_BOMB].proj == PJ_BOMB && std::string(WEAPONS[W_NUKE_BOMB].name) == "Nuclear Bomb", "there is no compact nuclear bomb weapon");
    CHECK8(SBOMBER_BOMB_SCALE > 0.0f && SBOMBER_BOMB_SCALE < UNUKE_SCALE * 0.5f, "the nuclear bomb's fireball is not much smaller than a silver warhead's");
    CHECK8(std::fabs(UNUKE_DAMAGE - 2.7f) < 1e-5f, "the Underground Nuke's damage is not 10% weaker (2.7x)");
    CHECK8(std::fabs(ANTINUKE_LASER_NUKE_MUL - 0.60f) < 1e-5f && std::fabs(WALL_PD_NUKE_MUL - 0.30f) < 1e-5f, "the anti-nuke laser / wall warhead nerfs are not 40% / 70%");
    CHECK8(std::fabs(BUILDS[bunker].hp - 11000.0f) < 0.01f, "the underground bunkers are not 11,000 hp");
    CHECK8(BUILDS[bunker].hp > BUILDS[tech].hp, "a bunker is not tougher than the tech structure");
    // durability pass: walls, the Command structure and the tech structure are far less tough; the Underground Nuke silo is the second-toughest
    // structure and its armoured silver warhead is harder to shoot down
    int wall = me == F_CYBER ? B_C_WALL : B_K_WALL, hq = firstBuildOf(me), silo = me == F_CYBER ? B_C_UNUKE : B_K_UNUKE;
    CHECK8(std::fabs(WALL_HP - 1800.0f) < 0.01f && std::fabs(BUILDS[wall].hp - WALL_HP) < 0.01f, "the walls are not 1,800 hp");
    CHECK8(BUILDS[hq].hp < 20000.0f && BUILDS[tech].hp < 10000.0f, "the Command structure / tech structure durability was not reduced");
    CHECK8(BUILDS[silo].hp >= 14000.0f && BUILDS[silo].hp < BUILDS[hq].hp, "the Underground Nuke silo is not in the expected band");
    CHECK8(UNUKE_DOOR_HP >= 5000.0f, "the Underground Nuke silo's blast doors were not strengthened");
    CHECK8(UNUKE_INTERCEPT_RESIST > 0.0f && UNUKE_INTERCEPT_RESIST < 1.0f, "the silver warhead is not armoured against interception");
    CHECK8(BUILDS[hq].hp > BUILDS[silo].hp && BUILDS[silo].hp > BUILDS[bunker].hp && BUILDS[bunker].hp > BUILDS[tech].hp && BUILDS[tech].hp > BUILDS[wall].hp,
           "the Command / silo / bunker / tech / wall durability order is wrong");
    // not stealthy: an enemy can see it and target it like any other building
    Ref bk = place8(bunker, 0, openGroundNear(Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f), 12));
    CHECK8(bk.valid(), "no spot for the test bunker");
    Entity& B = *g_sim.get(bk);
    CHECK8(Sim::stealthOf(B) == ST_NONE && g_sim.visibleTo(B, 1), "an underground bunker is still stealthy");
    CHECK8(std::fabs(B.maxHp - BUILDS[bunker].hp) < 0.5f, "a finished bunker does not have its (20% lower) health pool");
    Ref tank = g_sim.spawnUnit(firstUnitOf(foe) + 5, 1, g_map.nearestFree(B.pos + Vec2(6 * TILE, 0), 20));
    CHECK8(tank.valid() && g_sim.canTarget(*g_sim.get(tank), B), "an enemy cannot target a bunker as a normal building");
    return true;
}

// ---- the Anti-Nuke Laser (40% weaker against warheads) and the Anti-Rocket wall (70% weaker), while rockets/shells/aircraft keep their odds
static bool antiNukeNerfTest(Faction me, u64 seed) {
    Faction foe = me == F_CYBER ? F_CLANKER : F_CYBER;
    scene8(me, foe, seed);
    int b0 = firstBuildOf(me), anT = me == F_CYBER ? B_C_ANTINUKE : B_K_ANTINUKE, wall = me == F_CYBER ? B_C_WALL : B_K_WALL;
    Vec2 mid = openGroundNear(Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f), 12);
    for (int k = 0; k < 3; k++) place8(b0 + BR_POWER, 0, mid + Vec2((-7 + k * 2) * TILE, -8 * TILE));
    Ref an = place8(anT, 0, mid + Vec2(0, -3 * TILE));
    CHECK8(an.valid(), "no spot for the anti-nuke defence");
    g_sim.updatePowerPublic();
    Entity& A = *g_sim.get(an);
    CHECK8(g_sim.antiNukeWorking(A) && !g_sim.players[0].lowPower(), "the anti-nuke defence is not working/powered");
    float raw = g_sim.antiNukeShot(A), war = g_sim.antiNukeWarheadShot(A);
    if (me == F_CYBER) {   // the laser: 40% weaker against warheads
        CHECK8(std::fabs(raw - ANTINUKE_CHANCE) < 1e-5f, "a lone Anti-Nuke Laser's base chance changed");
        CHECK8(std::fabs(war - ANTINUKE_CHANCE * ANTINUKE_LASER_NUKE_MUL) < 1e-5f, "the Anti-Nuke Laser is not 40% weaker against warheads");
    } else {   // the Clanker missile silo keeps its chance
        CHECK8(std::fabs(raw - ANTINUKE_CHANCE) < 1e-5f && std::fabs(war - raw) < 1e-5f, "the Anti-Nuke Missiles were nerfed too");
    }
    Vec2 spot = A.pos + Vec2(15 * TILE, 0);   // inside the cover, outside the blast of a warhead landing there
    CHECK8(g_sim.antiNukeCovering(spot, 1) == 1, "the test warhead is not covered by exactly the one defence");
    CHECK8(std::fabs(g_sim.antiNukeChance(spot, 1) - war) < 1e-5f, "the stated warhead chance is not the nerfed one");
    // Anti-Rocket walls: 70% weaker against warheads, but their laser against rockets/shells/aircraft is unchanged (antiNukeShot)
    g_sim.players[0].upg[UPG_ANTIROCKET] = true;
    g_sim.updatePowerPublic();
    Vec2 wp = mid + Vec2(16 * TILE, 12 * TILE);
    Ref w = place8(wall, 0, wp);
    CHECK8(w.valid() && g_sim.isAntiRocketWall(*g_sim.get(w)), "no working Anti-Rocket wall for the test");
    Entity& W = *g_sim.get(w);
    CHECK8(g_sim.antiNukeLinked(W) == 0, "the test wall is linked to a neighbour");
    CHECK8(std::fabs(g_sim.antiNukeShot(W) - WALL_PD_CHANCE) < 1e-5f, "the wall's point-defence chance against rockets/shells/aircraft changed");
    CHECK8(std::fabs(g_sim.antiNukeWarheadShot(W) - WALL_PD_CHANCE * WALL_PD_NUKE_MUL) < 1e-5f, "the Anti-Rocket wall is not 70% weaker against warheads");
    Vec2 wspot = W.pos + Vec2(0, 15 * TILE);   // (inside its 10-tile cover plus the warhead's blast, outside the blast itself)
    CHECK8(g_sim.antiNukeCovering(wspot, 1) >= 1, "the wall does not cover the test warhead");
    CHECK8(std::fabs(g_sim.antiNukeChance(wspot, 1) - (1.0f - (1.0f - WALL_PD_CHANCE * WALL_PD_NUKE_MUL))) < 1e-4f, "the wall's stated warhead chance is not the nerfed one");
    // a falling compact nuclear bomb is a bomb, so the point-defence envelope burns it out of the air too (like rockets and shells)
    int bombsDown = 0, bombTrials = 80;
    for (int k = 0; k < bombTrials; k++) {
        g_sim.projs.clear(); g_sim.nukes.clear(); g_sim.fallouts.clear(); g_sim.interceptors.clear();
        if (Entity* D = g_sim.get(an)) D->pdAt = 0;
        Vec2 dest = A.pos + Vec2(13 * TILE, 0);   // (outside the bomb's own fireball, inside the defence's reach)
        Projectile bomb; bomb.pos = dest + Vec2(-20 * TILE, 0); bomb.prevPos = bomb.pos; bomb.vel = bomb.pos;
        bomb.dest = dest; bomb.weapon = W_NUKE_BOMB; bomb.owner = 1; bomb.arcLen = SBOMBER_BOMB_FALL; bomb.arcT = 0; bomb.life = SBOMBER_BOMB_FALL + 0.3f; bomb.alive = true;
        g_sim.projs.push_back(bomb);
        run8(SBOMBER_BOMB_FALL + 0.4f);
        if (g_sim.fallouts.empty()) bombsDown++;   // (it never went off: the defence got it)
    }
    CHECK8(bombsDown > 0, "the point-defence envelope never burned a falling nuclear bomb out of the air");
    // the Underground Nuke's silver warhead is armoured: every defence's chance against it is halved (and, alone among warheads, only lasers
    // can engage it at all - that part is covered by --civictest). Each check gets a clean scene with exactly one defence.
    if (me == F_CYBER) {
        scene8(me, foe, seed + 7);
        Vec2 mid2 = openGroundNear(Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f), 12);
        for (int k = 0; k < 4; k++) place8(b0 + BR_POWER, 0, mid2 + Vec2((-9 + k * 3) * TILE, -8 * TILE));
        Ref A2 = place8(anT, 0, mid2);
        g_sim.updatePowerPublic();
        CHECK8(A2.valid() && g_sim.antiNukeWorking(*g_sim.get(A2)), "no working laser for the silver-warhead check");
        Vec2 pspot = g_sim.get(A2)->pos + Vec2(15 * TILE, 0), dspot = g_sim.get(A2)->pos + Vec2(45 * TILE, 0);   // (the deep one keeps its 33-tile blast clear of the tower)
        float pPlain = g_sim.antiNukeChance(pspot, 1, false, 1.0f, false);
        float pDeep = g_sim.antiNukeChance(dspot, 1, false, UNUKE_SCALE, true);
        CHECK8(pPlain > 0.0f && std::fabs(pDeep - pPlain * UNUKE_INTERCEPT_RESIST) < 1e-4f, "the armoured silver warhead does not halve the Anti-Nuke Laser's chance");
        int trials = 400, down = 0;
        for (int k = 0; k < trials; k++) {
            for (auto& b : g_sim.ents) if (b.alive && b.isBuilding() && b.bt().role == BR_ANTINUKE) b.siloTimer = 0;
            g_sim.nukes.clear(); g_sim.interceptors.clear(); g_sim.fallouts.clear(); g_sim.fx.clear();
            Sim::Nuke n; n.from = g_sim.players[0].basePos; n.pos = dspot; n.owner = 1; n.t = Sim::NUKE_FLIGHT - 4.2f; n.scale = UNUKE_SCALE; n.deep = true;
            g_sim.nukes.push_back(n);
            for (int t = 0; t < 20 * 6 && !g_sim.nukes.empty(); t++) {
                g_sim.step(); g_sim.events.clear();
                if (!g_sim.nukes.empty() && g_sim.nukes[0].broken) { down++; break; }
            }
        }
        float rate = down / (float)trials, sigma = std::sqrt(pDeep * (1.0f - pDeep) / trials);
        CHECK8(std::fabs(rate - pDeep) < 4.0f * sigma + 0.03f, "the interception actually rolled against a silver warhead is not the armoured chance");
        CHECK8(rate < pPlain * 0.75f, "the silver warhead is not meaningfully harder to shoot down than a normal one");
        printf("supersonictest %s: a lone laser stops %.1f%% of silver warheads (%.1f%% of normal ones); %.1f%% of %d deep trials were shot down\n",
               FACTION_NAME[me], pDeep * 100.0f, pPlain * 100.0f, rate * 100.0f, trials);
    }
    {   // the same for an Anti-Rocket wall's laser, in a clean scene with exactly that wall
        scene8(me, foe, seed + 8);
        Vec2 mid3 = openGroundNear(Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f), 12);
        g_sim.players[0].upg[UPG_ANTIROCKET] = true;
        Ref W2 = place8(wall, 0, mid3);
        g_sim.updatePowerPublic();
        CHECK8(W2.valid() && g_sim.isAntiRocketWall(*g_sim.get(W2)), "no working Anti-Rocket wall for the silver-warhead check");
        Vec2 wspot = g_sim.get(W2)->pos + Vec2(0, 15 * TILE);
        float wpPlain = g_sim.antiNukeChance(wspot, 1, false, 1.0f, false);
        float wpDeep = g_sim.antiNukeChance(wspot, 1, false, UNUKE_SCALE, true);
        CHECK8(wpPlain > 0.0f && std::fabs(wpDeep - wpPlain * UNUKE_INTERCEPT_RESIST) < 1e-4f, "the Anti-Rocket wall's laser is not harder pressed by the armoured silver warhead");
    }
    return true;
}

// ---- the Underground Nuke hits 10% softer: measure the damage it does against the damage a ramp warhead does at the same relative distance
static float warheadLoss(bool deep, u64 seed, Faction me) {
    Faction foe = me == F_CYBER ? F_CLANKER : F_CYBER;
    scene8(me, foe, seed);
    Vec2 gz = openGroundNear(Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f), 12);
    flattenArena(gz, 34, 34);   // open ground all round for the target at 0.93 of either warhead's radius
    int facB = firstBuildOf(foe) + BR_FACTORY;
    float R = NUKE_RADIUS * TILE * (deep ? 3.0f : 1.0f);
    // a factory at about 0.93 R from ground zero (the falloff there is a gentle slope, and neither warhead's damage is clamped)
    Ref target;
    for (int ring = 0; ring < 30 && !target.valid(); ring++) for (int a = 0; a < 24 && !target.valid(); a++) {
        float ang = a * 0.2618f + ring * 0.13f;
        Vec2 p = gz + Vec2(std::cos(ang), std::sin(ang)) * (R * (0.93f + ring * 0.04f));
        int tx = tileOf(p.x) - BUILDS[facB].w / 2, ty = tileOf(p.y) - BUILDS[facB].h / 2;
        if (inMap(tx, ty) && g_sim.canPlace(1, facB, tx, ty)) target = g_sim.placeBuilding(facB, 1, tx, ty, true);
    }
    if (!target.valid()) return -1.0f;
    Entity* tg = g_sim.get(target);
    float hp0 = tg->hp, d = dist(tg->pos, gz);
    float f = d < R * 0.35f ? 1.0f : 1.0f - 0.75f * ((d - R * 0.35f) / (R * 0.65f));
    Sim::Nuke n; n.from = g_sim.players[0].basePos; n.pos = gz; n.owner = 0; n.t = Sim::NUKE_FLIGHT - 0.05f; n.scale = deep ? UNUKE_SCALE : 1.0f; n.deep = deep;
    g_sim.nukes.push_back(n);
    run8(0.3f);
    tg = g_sim.get(target);
    float loss = tg ? hp0 - tg->hp : hp0;
    (void)f;
    return loss;
}
static bool unukeDamageTest(Faction me, u64 seed) {
    float ramp = 0, deep = 0;
    for (int k = 0; k < 3; k++) {
        float r = warheadLoss(false, seed + k, me), d = warheadLoss(true, seed + 10 + k, me);
        if (r < 0 || d < 0) return fail8("could not set the silver-warhead damage scene up");
        ramp += r; deep += d;
    }
    ramp /= 3.0f; deep /= 3.0f;
    // the two warheads are measured at the same relative distance from ground zero, so the silver one must do exactly UNUKE_DAMAGE times the loss
    float want = UNUKE_DAMAGE;
    if (ramp <= 10.0f || std::fabs(deep / ramp - want) > want * 0.30f) {
        fprintf(stderr, "silver warhead did %.0f of damage, a ramp warhead %.0f (ratio %.2f, wanted %.2f)\n", deep, ramp, deep / std::max(1.0f, ramp), want);
        return fail8("the Underground Nuke's damage is not 2.7x a ramp warhead's");
    }
    return true;
}

// ---- the Supersonic Bomber flies, drops one compact nuclear bomb, survives its own fireball and slows right down afterwards
static bool bomberTest(Faction me, u64 seed) {
    Ref af = NOREF, target = NOREF; Vec2 mid;
    Entity* afp = makeBomberScene(me, seed, af, target, mid);
    CHECK8(afp && target.valid(), "could not set the Supersonic Bomber scene up");
    int sbt = me == F_CYBER ? U_C_SBOMBER : U_K_SBOMBER;
    g_sim.updatePowerPublic();
    CHECK8(g_sim.unitAvailable(0, sbt), "the Supersonic Bomber is not available with an airfield and a tech structure");
    CHECK8(g_sim.cmdTrain(af, sbt), "the airfield refuses to build the Supersonic Bomber");
    Ref plane;
    for (int t = 0; t < 20 * 90 && !plane.valid(); t++) { g_sim.step(); g_sim.events.clear(); for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit() && e.type == sbt) plane = g_sim.refOf(e); }
    CHECK8(plane.valid(), "the Supersonic Bomber was never produced");
    Entity* P = g_sim.get(plane);
    float hp0 = g_sim.get(target)->hp;
    Ref shield = NOREF;   // a second factory far enough away that only a full silver warhead's radius would touch it
    for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.owner == 1 && e.type == firstBuildOf(me == F_CYBER ? F_CLANKER : F_CYBER) + BR_FACTORY && dist(e.pos, g_sim.get(target)->pos) > 1.0f) shield = g_sim.refOf(e);
    CHECK8(shield.valid(), "no second factory for the blast-size check");
    Ref victim = g_sim.spawnUnit(me == F_CYBER ? U_K_TANK : U_C_TANK, 1, g_map.nearestFree(g_sim.get(target)->pos + Vec2(0, 40), 20));
    g_sim.cmdAttack({ plane }, target);
    // fly the attack: record its top speed with the bomb aboard, the bomb itself, the blast and its speed afterwards
    float topBefore = g_sim.airTopSpeed(*P), maxSpeed = 0, speedAfter = 0; int bombsSeen = 0; bool detonated = false, planeSurvivedBlast = false;
    float dropAt = -1;
    Vec2 prev = P->pos;
    for (int t = 0; t < 20 * 90; t++) {
        g_sim.step(); g_sim.events.clear();
        Entity* q = g_sim.get(plane);
        if (!q) return fail8("the Supersonic Bomber was lost with nothing shooting at it");
        float sp = dist(q->pos, prev) / SIM_DT; prev = q->pos;
        if (q->ammo > 0) maxSpeed = std::max(maxSpeed, sp);
        for (auto& p : g_sim.projs) if (p.weapon == W_NUKE_BOMB) bombsSeen = std::max(bombsSeen, 1);
        if (dropAt < 0 && q->ammo <= 0) dropAt = g_sim.time;
        if (!detonated && !g_sim.fallouts.empty()) { detonated = true; planeSurvivedBlast = g_sim.get(plane) != nullptr; }
        if (detonated && g_sim.time > dropAt + 3.0f) { speedAfter = std::max(speedAfter, sp); }
        if (detonated && g_sim.time > dropAt + 5.0f) break;
    }
    CHECK8(bombsSeen == 1, "the Supersonic Bomber did not drop exactly one nuclear bomb");
    CHECK8(detonated, "the compact nuclear bomb never went off");
    CHECK8(topBefore == SBOMBER_DASH, "the bomber is not dashing while its bomb is aboard");
    CHECK8(maxSpeed > 800.0f, "the bomber did not fly very fast before the drop");
    CHECK8(planeSurvivedBlast && g_sim.get(plane), "the Supersonic Bomber did not survive its own fireball");
    CHECK8(speedAfter > 0.0f && speedAfter < SBOMBER_SLOW + 120.0f, "the bomber did not slow down after dropping the payload");
    CHECK8(g_sim.airTopSpeed(*g_sim.get(plane)) == SBOMBER_SLOW, "the bomber's top speed did not drop to its slow cruise after the payload");
    CHECK8(!g_sim.get(target), "the compact nuclear bomb did not destroy the structure it hit (an Underground Nuke's damage)");
    CHECK8(!g_sim.get(victim), "a tank at ground zero survived the compact nuclear bomb");
    CHECK8(g_sim.get(shield) && g_sim.get(shield)->hp >= g_sim.get(shield)->maxHp - 0.5f, "the compact bomb's fireball is not much smaller than a silver warhead's");
    CHECK8(!g_sim.fallouts.empty() && std::fabs(g_sim.fallouts.back().life - Sim::FALLOUT_LIFE * UNUKE_FALLOUT) < 0.01f && g_sim.fallouts.back().power >= UNUKE_FALLOUT - 0.01f, "the compact bomb does not carry a silver warhead's radiation");
    // it goes home, rearms its one bomb and is fast again once the payload is back aboard
    bool rearmed = false;
    for (int t = 0; t < 20 * 60 && !rearmed; t++) { g_sim.step(); g_sim.events.clear(); if (Entity* q = g_sim.get(plane)) if (q->ammo >= 1) rearmed = true; }
    CHECK8(rearmed, "the Supersonic Bomber never rearmed its nuclear bomb at its pad");
    CHECK8(g_sim.airTopSpeed(*g_sim.get(plane)) == SBOMBER_DASH, "the rearmed bomber does not get its dash back");
    (void)hp0;
    return true;
}

// ---- the actual dice: a lone Anti-Nuke Laser stops about 18% of warheads (60% of its old 30%) and a lone Anti-Rocket wall about 11.4% (30% of 38%)
static int trialsShotDown8(Vec2 gz, Vec2 defence, int trials) {
    int down = 0;
    for (int k = 0; k < trials; k++) {
        for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && (e.bt().role == BR_ANTINUKE || e.bt().role == BR_WALL)) e.siloTimer = 0;
        g_sim.nukes.clear(); g_sim.interceptors.clear(); g_sim.fallouts.clear();
        Sim::Nuke n{ g_sim.players[1].basePos, gz, 1, Sim::NUKE_FLIGHT - 4.2f, false, 1.0f };
        g_sim.nukes.push_back(n);
        for (int t = 0; t < 20 * 6 && !g_sim.nukes.empty(); t++) {
            g_sim.step(); g_sim.events.clear();
            if (!g_sim.nukes.empty() && g_sim.nukes[0].broken) { down++; break; }
        }
        (void)defence;
    }
    return down;
}
static bool rolledChanceTest(Faction me, u64 seed) {
    Faction foe = me == F_CYBER ? F_CLANKER : F_CYBER;
    scene8(me, foe, seed);
    int b0 = firstBuildOf(me), anT = me == F_CYBER ? B_C_ANTINUKE : B_K_ANTINUKE, wall = me == F_CYBER ? B_C_WALL : B_K_WALL;
    Vec2 gz = openGroundNear(Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f), 16);
    flattenArena(gz, 20, 20);   // retain the intended defence distance on any map scale
    for (int k = 0; k < 4; k++) place8(b0 + BR_POWER, 0, gz + Vec2((-10 + k * 3) * TILE, -16 * TILE));
    Ref an = place8(anT, 0, gz + Vec2(0, 15 * TILE));
    CHECK8(an.valid(), "no spot for the sampling defence");
    g_sim.updatePowerPublic();
    CHECK8(g_sim.antiNukeWorking(*g_sim.get(an)), "the sampling defence is not working");
    CHECK8(dist(g_sim.get(an)->pos, gz) < g_sim.antiNukeCover(*g_sim.get(an), 1), "sampling defence is out of coverage");
    float p = g_sim.antiNukeWarheadShot(*g_sim.get(an));
    Vec2 defenceAt = g_sim.get(an)->pos;
    int trials = 300, down = trialsShotDown8(gz, defenceAt, trials);
    float rate = down / (float)trials, sigma = std::sqrt(p * (1.0f - p) / trials);
    if (std::fabs(rate - p) >= 4.0f * sigma + 0.03f)
        fprintf(stderr, "sampling defence: rate %.3f expected %.3f, distance %.1f tiles, alive %d working %d time %.1f gameOver %d\n", rate, p, dist(defenceAt, gz)/TILE, (int)(g_sim.get(an)!=nullptr), g_sim.get(an) ? (int)g_sim.antiNukeWorking(*g_sim.get(an)) : 0, g_sim.time, (int)g_sim.gameOver);
    CHECK8(std::fabs(rate - p) < 4.0f * sigma + 0.03f, "the warhead interception that is actually rolled is not the nerfed chance");
    printf("supersonictest %s: the anti-nuke defence stopped %.1f%% of %d warheads (stated %.1f%%)\n", FACTION_NAME[me], rate * 100.0f, trials, p * 100.0f);
    // the wall's chance against warheads is the 30% one (its anti-rocket chance stays as it was)
    scene8(me, foe, seed + 1);
    gz = openGroundNear(Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f), 16);
    flattenArena(gz, 20, 20);
    g_sim.players[0].upg[UPG_ANTIROCKET] = true;
    Ref w = place8(wall, 0, gz + Vec2(0, 15 * TILE));
    CHECK8(w.valid() && g_sim.isAntiRocketWall(*g_sim.get(w)), "no spot for the sampling Anti-Rocket wall");
    p = g_sim.antiNukeWarheadShot(*g_sim.get(w));
    down = trialsShotDown8(gz, g_sim.get(w)->pos, trials);
    rate = down / (float)trials; sigma = std::sqrt(p * (1.0f - p) / trials);
    CHECK8(std::fabs(rate - p) < 4.0f * sigma + 0.03f, "the wall's warhead interception that is actually rolled is not the 70% weaker chance");
    printf("supersonictest %s: an Anti-Rocket wall stopped %.1f%% of %d warheads (stated %.1f%%)\n", FACTION_NAME[me], rate * 100.0f, trials, p * 100.0f);
    return true;
}

bool supersonicTest(u64 seed) {
    g_ok8 = true;
    for (int fi = 0; fi < 2; fi++) {
        Faction me = (Faction)fi;
        if (!constantsTest(me, seed + fi)) return false;
        if (!antiNukeNerfTest(me, seed + 20 + fi)) return false;
        if (!bomberTest(me, seed + 40 + fi)) return false;
        printf("supersonictest %s: balance numbers, bunkers, anti-nuke nerfs and the Supersonic Bomber ok\n", FACTION_NAME[me]);
    }
    if (!unukeDamageTest(F_CYBER, seed + 60)) return false;
    if (!rolledChanceTest(F_CYBER, seed + 80)) return false;
    printf("supersonictest: ok\n");
    return g_ok8;
}
