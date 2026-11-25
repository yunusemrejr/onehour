// One Hour - a computer commander's Phantom Jeep (F2_PHANTOM)
//  * The army's one Phantom is driven apart from every wave, group and defence (the survey leaves it out of them): it mends at home when badly hurt or
//    when three aircraft are on it, meets the enemy's soldiers round its own base (snipers first), and otherwise raids what keeps the enemy going -
//    power, the tech structure, airfields, nuke silos, factories, the naval yard - planting TNT while it has charges and shelling the structure after.
//    It picks a new goal when the last one falls, and keeps away from the enemy's aircraft, the only things that find and badly hurt it.
#include "ai.h"

static bool aiDebugPh() { static int v = -1; if (v < 0) v = getenv("ONEHOUR_DEBUG") ? 1 : 0; return v == 1; }

// what a structure is worth to the Phantom (0: not one of its targets)
static float phantomWorth(const Entity& b) {
    switch (b.bt().role) {
    case BR_POWER: return 3.0f;
    case BR_TECH: case BR_AIRFIELD: case BR_NUKE: case BR_UNUKE: case BR_FACTORY: return 6.0f;
    case BR_NAVAL: return 5.5f;
    default: return 0.0f;
    }
}

// the enemy aircraft the commander can see within r of p (they are what finds a Phantom and what hurts it most)
static int visibleAirNear(int player, Vec2 p, float r) {
    int n = 0;
    g_sim.forEachNear(p, r, [&](Entity& e) {
        if (!e.alive || !e.isUnit() || !e.isAir() || !g_sim.enemies(player, e.owner) || !g_sim.visibleTo(e, player)) return;
        if (dist(e.pos, p) <= r) n++;
    });
    return n;
}

void AiPlayer::phantomOps(Ref phantomRef, const std::vector<Entity*>& enemyUnits) {
    Sim& S = g_sim;
    Entity* c = S.get(phantomRef);
    if (!c || c->carrier.valid() || c->fall > 0) return;
    Player& pl = S.players[player];
    const Vec2 home = pl.basePos;

    // (a) hurt, or three aircraft on it: home to mend, and it stays there until it is well again
    const bool hunted = visibleAirNear(player, c->pos, 8.0f * TILE) >= 3;
    if (!phantomHome && (c->hp < c->maxHp * 0.35f || hunted)) {
        phantomHome = true;
        if (aiDebugPh()) fprintf(stderr, "[ai%d t=%.0f] the Phantom comes home to mend (%s)\n", player, S.time, hunted ? "aircraft on it" : "badly hurt");
    }
    if (phantomHome && c->hp > c->maxHp * 0.8f) phantomHome = false;
    if (phantomHome) {
        if (dist(c->pos, home) > 9.0f * TILE && c->order != O_MOVE) S.cmdMove({phantomRef}, g_map.nearestFree(home, 8), false);
        phantomGoal = NOREF;
        return;
    }

    // (b) the enemy's soldiers round the base, snipers first (the Phantom drives through the base to them)
    Entity* foe = nullptr; float bs = 0;
    for (Entity* u : enemyUnits) {
        if (!u->alive || !u->isUnit() || u->ut().kind != UK_INF || !S.enemies(player, u->owner) || !S.visibleTo(*u, player)) continue;
        const float d = dist(u->pos, home);
        if (d > 14.0f * TILE) continue;
        const float sc = (u->ut().sniper ? 2.0f : 1.0f) * (14.0f * TILE - d);
        if (sc > bs) { bs = sc; foe = u; }
    }
    if (foe) {
        const Ref fr = S.refOf(*foe);
        if (c->order != O_ATTACK || c->targetEnt != fr) S.cmdAttack({phantomRef}, fr);
        phantomGoal = NOREF;
        return;
    }

    // (c) a raid: the most valuable enemy structure it knows of (visible or remembered), far from the enemy's aircraft
    Entity* g = S.get(phantomGoal);
    const bool goalOk = g && g->alive && g->isBuilding() && g->owner >= 0 && S.enemies(player, g->owner) && g->bt().role != BR_FWALL
                        && S.explored(player, clampi(tileOf(g->pos.x), 0, MAP_W - 1), clampi(tileOf(g->pos.y), 0, MAP_H - 1))
                        && S.time - phantomGoalAt < 90.0f;
    if (!goalOk) {
        if (g && S.time - phantomGoalAt >= 90.0f) { phantomBad = phantomGoal; phantomBadAt = S.time; }   // (it could not get in)
        Entity* best = nullptr; float bs2 = 0;
        for (auto& e : S.ents) {
            if (!e.alive || !e.isBuilding() || e.owner < 0 || !S.enemies(player, e.owner)) continue;
            if (!e.constructed && e.district < 0) continue;
            float v = phantomWorth(e);
            if (v <= 0) continue;
            if (!S.explored(player, clampi(tileOf(e.pos.x), 0, MAP_W - 1), clampi(tileOf(e.pos.y), 0, MAP_H - 1))) continue;   // (what the army has found: visible or remembered)
            if (S.refOf(e) == phantomBad && S.time - phantomBadAt < 120.0f) continue;
            const int air = visibleAirNear(player, e.pos, 12.0f * TILE);
            if (air > 0) v /= (1.0f + 1.5f * (float)air);   // aircraft round it: it would be found and hurt
            if (e.owner == mainEnemy) v *= 1.25f;
            const float d = dist(c->pos, e.pos) / TILE;
            const float sc = v / (1.0f + d / 55.0f) * rng.f(0.8f, 1.2f);
            if (sc > bs2) { bs2 = sc; best = &e; }
        }
        if (!best) {   // nothing worth the drive: home, and it looks again later
            if (dist(c->pos, home) > 12.0f * TILE) S.cmdMove({phantomRef}, g_map.nearestFree(home, 8), false);
            return;
        }
        phantomGoal = S.refOf(*best); phantomGoalAt = S.time;
        g = best;
        if (aiDebugPh()) fprintf(stderr, "[ai%d t=%.0f] the Phantom raids the %s at %d,%d\n", player, S.time, best->bt().name, tileOf(best->pos.x), tileOf(best->pos.y));
    }
    if (!g) return;
    if (c->tntLeft > 0 && S.plantable(player, *g)) {   // charges first (a Bastion Wall is never plantable, so it is never sent at one)
        if (c->order != O_PLANT || c->targetEnt != phantomGoal) S.cmdPlant({phantomRef}, phantomGoal);
        return;
    }
    if (c->tntLeft == 0 && c->hp < c->maxHp * 0.5f) {   // out of charges and hurt: home instead of trading shots with the structure
        if (dist(c->pos, home) > 9.0f * TILE && c->order != O_MOVE) S.cmdMove({phantomRef}, g_map.nearestFree(home, 8), false);
        return;
    }
    if (c->order != O_ATTACK || c->targetEnt != phantomGoal) S.cmdAttack({phantomRef}, phantomGoal);
}
