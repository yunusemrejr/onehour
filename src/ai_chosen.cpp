// One Hour - the commander and The Chosen (F2_CHOSEN), helicopters against an enemy's Chosen (F2_HELIHUNT), and waves that are harder to read
// (F2_GUILE).
//  * The Chosen fights a war of its own. Nothing but a helicopter sees it, so it walks into the enemy base and takes what keeps the enemy going: the
//    factory, the airfield, the tech structure, the nuke ramps, the Command structure. Far from home a taken structure is sold on the spot (the enemy
//    loses it for good, the commander gets half its price); a turret or a bunker is kept and fights for its new owner where it stands. On the way it
//    turns whatever enemy fighters come into reach. When it is in the thick of it, it calls its Shadow Guard, who wreck everything round its goal.
//    The moment an enemy helicopter comes near - or it is found at all - it slips away, and it comes home to turn a raid on its own base.
//  * Against an enemy that could have one, a helicopter keeps watch over the base; where something unseen turns, takes or shoots the commander's
//    own things, helicopters go and sweep the spot so the guns can see what they are shooting at.
//  * Feints: as a big wave sets off, a fast detachment goes for the enemy's economy somewhere else, to pull the defence apart.
#include "ai.h"

static bool aiDebugCh() { static int v = -1; if (v < 0) v = getenv("ONEHOUR_DEBUG") ? 1 : 0; return v == 1; }

// what a structure is worth to take (0: not worth the walk)
static float takeValue(const Entity& b) {
    switch (b.bt().role) {
    case BR_FACTORY: case BR_AIRFIELD: case BR_TECH: case BR_NUKE: case BR_UNUKE: return 6.0f;
    case BR_HQ: return 5.5f;
    case BR_NAVAL: case BR_BARRACKS: case BR_BUNKER: return 4.5f;
    case BR_SUPPLY: case BR_INCOME: case BR_ANTINUKE: return 3.5f;
    case BR_POWER: return 3.0f;
    case BR_TURRET: case BR_AATURRET: return 2.0f;
    case BR_ISLANDNUKE: return 5.0f;
    default: return 0.0f;
    }
}

// the nearest enemy helicopter the commander can see within r of p (nullptr: none)
static Entity* heliNear(int player, Vec2 p, float r) {
    Entity* best = nullptr; float bd = r;
    g_sim.forEachNear(p, r, [&](Entity& e) {
        if (!e.isUnit() || !helicopterType(e.ut()) || !g_sim.enemies(player, e.owner) || !g_sim.visibleTo(e, player)) return;
        float d = dist(e.pos, p); if (d < bd) { bd = d; best = &e; }
    });
    return best;
}

void AiPlayer::chosenOps(Ref chosen, Entity* threat) {
    Sim& S = g_sim;
    Entity* c = S.get(chosen);
    if (!c || c->carrier.valid() || c->fall > 0) return;
    Player& pl = S.players[player];
    bool found = false;
    for (int q = 0; q < S.numPlayers; q++) if (S.enemies(player, q) && c->detUntil[q] > S.time) found = true;
    Entity* heli = heliNear(player, c->pos, 8.0f * TILE);   // (it sees a helicopter coming a little before the helicopter can see it)
    // seen, or about to be: slip away from the helicopter (home if it is hurt), the Shadow Guard covering it
    if (found || heli) {
        if (S.time > chosenFleeUntil - 2.0f) {
            Vec2 away = heli ? (c->pos - heli->pos).norm() : (pl.basePos - c->pos).norm();
            Vec2 to = c->hp < c->maxHp * 0.5f ? pl.basePos : c->pos + away * (11.0f * TILE) + (pl.basePos - c->pos).norm() * (4.0f * TILE);
            to = g_map.nearestFree(Vec2(clampf(to.x, 2 * TILE, WORLD_W - 2 * TILE), clampf(to.y, 2 * TILE, WORLD_H - 2 * TILE)), 8);
            S.cmdMove({chosen}, to, false);
            chosenFleeUntil = S.time + 6.0f; chosenGoal = NOREF; chosenGoalAt = -100;
            if (aiDebugCh()) fprintf(stderr, "[ai%d t=%.0f] the Chosen slips away (%s)\n", player, S.time, found ? "found" : "a helicopter near");
        }
        if (S.shadowWait(player) == 0.0f && (found || c->hp < c->maxHp * 0.8f)) S.cmdShadowCall(player, chosen);
        return;
    }
    if (S.time < chosenFleeUntil) return;
    // a raid on the base: the Chosen turns the raiders (a few seconds' walk from wherever it is near home)
    if (threat && dist(c->pos, pl.basePos) < 34.0f * TILE && dist(threat->pos, pl.basePos) < 24.0f * TILE) {
        if (c->targetEnt != S.refOf(*threat) || c->order != O_ATTACK) { S.cmdAttack({chosen}, S.refOf(*threat)); chosenGoal = NOREF; }
        return;
    }
    if (S.time < nextChosenLook) return;
    nextChosenLook = S.time + 1.5f;
    // the goal still stands and is still the enemy's: keep at it (a goal held too long is given up: something was in the way)
    Entity* g = S.get(chosenGoal);
    if (g && S.time - chosenGoalAt >= 75.0f && S.spyMayTake(player, *g)) { chosenBadGoal = chosenGoal; chosenBadAt = S.time; }   // (75 s and not there: it cannot get in)
    bool goalOk = g && g->isBuilding() && S.spyMayTake(player, *g) && S.time - chosenGoalAt < 75.0f && !heliNear(player, g->pos, 9.0f * TILE);
    if (goalOk) {
        if (c->order == O_IDLE || (c->order == O_ATTACKMOVE && dist(c->target, g->pos) > 3 * TILE)) S.cmdMove({chosen}, g->pos, true);
        return;
    }
    // a new goal: the most valuable structure it knows of, weighed against the walk, the helicopters round it, and a little chance (never the same
    // order twice); towards the end a Command structure or production ends the war fastest
    Entity* best = nullptr; float bs = 0;
    Entity* waveAt = attacking ? S.get(attackTarget) : nullptr;   // a wave under way: the defences round its objective first (taken, they fire on their old owner)
    for (auto& e : S.ents) {
        if (!e.alive || !e.isBuilding() || (!e.constructed && e.district < 0) || e.owner < 0 || !S.enemies(player, e.owner)) continue;
        if (!S.explored(player, clampi(tileOf(e.pos.x), 0, MAP_W - 1), clampi(tileOf(e.pos.y), 0, MAP_H - 1))) continue;
        float v = takeValue(e);
        if (v <= 0 || (S.refOf(e) == chosenBadGoal && S.time - chosenBadAt < 150.0f)) continue;
        if (e.owner == mainEnemy) v *= 1.3f;
        if (waveAt && dist(e.pos, waveAt->pos) < 14.0f * TILE) {
            BuildRole r = e.bt().role;
            v *= (r == BR_TURRET || r == BR_AATURRET || r == BR_BUNKER) ? 4.0f : 2.0f;
        }
        if (e.bt().role == BR_BUNKER && !e.passengers.empty()) v += 0.05f * e.passengers.size();   // (its garrison changes sides with it)
        if (heliNear(player, e.pos, 11.0f * TILE)) v *= 0.15f;
        float d = dist(c->pos, e.pos) / TILE;
        float sc = v / (1.0f + d / 45.0f) * rng.f(0.7f, 1.3f);
        if (sc > bs) { bs = sc; best = &e; }
    }
    if (best) {
        chosenGoal = S.refOf(*best); chosenGoalPos = best->pos; chosenGoalAt = S.time;
        S.cmdMove({chosen}, best->pos, true);   // (attack-move: it turns and takes whatever it meets on the way, and the goal when it gets there)
        if (aiDebugCh()) fprintf(stderr, "[ai%d t=%.0f] the Chosen goes for the %s at %d,%d\n", player, S.time, best->bt().name, tileOf(best->pos.x), tileOf(best->pos.y));
    } else if (c->order == O_IDLE && mainEnemy >= 0) {
        S.cmdMove({chosen}, g_map.nearestFree(S.players[mainEnemy].basePos, 10), true);   // nothing known yet: into the enemy base, it finds its own goals there
    }
}

// The Shadow Guard: called when the Chosen is in the thick of it (near its goal or among enemies), or when it has been ready a while; led to the
// Chosen's goal (or onto raiders at home), and once the Chosen is gone, onto the main enemy
void AiPlayer::shadowOps(Ref chosen, Entity* threat, const std::vector<Ref>& shadowNow) {
    Sim& S = g_sim;
    Player& pl = S.players[player];
    for (Ref r : shadowNow) if (std::find(shadows.begin(), shadows.end(), r) == shadows.end()) shadows.push_back(r);
    shadows.erase(std::remove_if(shadows.begin(), shadows.end(), [&](Ref r) { Entity* e = S.get(r); return !e || e->owner != player; }), shadows.end());
    Entity* c = S.get(chosen);
    if (c && S.shadowWait(player) == 0.0f && !c->carrier.valid()) {
        bool hot = false;
        if (Entity* g = S.get(chosenGoal)) hot = dist(c->pos, g->pos) < 14.0f * TILE;
        if (!hot) S.forEachNearMask(c->pos, 9.0f * TILE, S.enemyMask(player), [&](Entity& e) { if (e.isUnit() && e.ut().role == UR_COMBAT && S.enemies(player, e.owner)) hot = true; });
        if (threat && dist(c->pos, threat->pos) < 14.0f * TILE) hot = true;
        if (hot || S.time - pl.shadowReady > 45.0f) {
            if (S.cmdShadowCall(player, chosen) && aiDebugCh()) fprintf(stderr, "[ai%d t=%.0f] the Chosen calls its Shadow Guard\n", player, S.time);
        }
    }
    if (shadows.empty() || S.time < nextShadowOrder) return;
    nextShadowOrder = S.time + 4.0f;
    Vec2 to; bool have = false;
    if (threat && dist(threat->pos, pl.basePos) < 22.0f * TILE) {
        // the base is raided: the guard nearest home (half of it at most) goes for the raiders
        std::vector<Ref> home;
        for (Ref r : shadows) if (Entity* e = S.get(r)) if (dist(e->pos, threat->pos) < 30.0f * TILE && home.size() < shadows.size() / 2 + 1) home.push_back(r);
        if (!home.empty()) groupMove(home, threat->pos, true);
    }
    if (Entity* g = S.get(chosenGoal)) { to = g->pos; have = true; }
    else if (c && c->order != O_IDLE) { to = c->target; have = true; }
    else if (Entity* t = pickAttackTarget()) { to = t->pos; have = true; }
    if (!have) return;
    std::vector<Ref> go;
    for (Ref r : shadows) if (Entity* e = S.get(r)) if ((e->order == O_IDLE || dist(e->target, to) > 6 * TILE) && !(threat && dist(e->pos, threat->pos) < 12.0f * TILE)) go.push_back(r);
    if (!go.empty()) groupMove(go, g_map.nearestFree(to, 6), true);
}

// The structures the Chosen took far out in enemy ground: production, power and the like are sold where they stand (they would only be taken back,
// and the enemy is down one for good); a turret, a battery or a bunker is kept: it fights for us right in the enemy base
void AiPlayer::sellTaken() {
    Sim& S = g_sim;
    Player& pl = S.players[player];
    for (auto& e : S.ents) {
        if (!e.alive || !e.isBuilding() || e.owner != player || e.takenFrom < 0 || !e.takenByChosen || e.district >= 0 || neutralRole(e.bt().role)) continue;
        BuildRole r = e.bt().role;
        if (r == BR_TURRET || r == BR_AATURRET || r == BR_BUNKER || r == BR_WALL || r == BR_FWALL || r == BR_ANTINUKE) continue;
        if (dist(e.pos, pl.basePos) < 26.0f * TILE) continue;   // (near home it serves the base)
        S.cmdSell(S.refOf(e));
        chosenSold++;
        if (aiDebugCh()) fprintf(stderr, "[ai%d t=%.0f] sold the taken %s\n", player, S.time, e.bt().name);
    }
}

// Helicopters against an enemy's Chosen: one keeps watch over the base once an enemy could have one (Hard and Brutal from the middle of the match),
// more on alarm - something unseen turned, took or shot our things in the last half minute - and they sweep the spot it happened. While the enemy
// has nothing but hidden things left they go with the army's sweep.
void AiPlayer::heliHunt(const std::vector<Ref>& aircraft, const std::vector<Entity*>& airfields, int reserve, bool hunting) {
    Sim& S = g_sim;
    Player& pl = S.players[player];
    const bool alarm = S.time - pl.chosenAlarmT < 30.0f;
    const bool watch = pl.difficulty >= 2 && S.time > 9.0f * 60.0f;
    if (!alarm && !watch && !hunting) return;
    const int heliType = pl.faction == F_CYBER ? U_C_HELI : U_K_AIR;
    std::vector<Ref> helis;
    for (Ref r : aircraft) if (Entity* e = S.get(r)) if (helicopterType(e->ut())) helis.push_back(r);
    int want = alarm ? 2 + pl.difficulty : (hunting ? 3 : 1);
    int queued = 0;
    for (auto* a : airfields) for (int t : a->queue) if (t == heliType) queued++;
    if ((int)helis.size() + queued < want && pl.money - reserve > UNITS[heliType].cost + 300)
        for (auto* a : airfields) if (a->constructed && a->queue.size() < 2 && S.cmdTrain(S.refOf(*a), heliType)) { queued++; break; }
    if (helis.empty() || S.time < nextHeliOrder) return;
    nextHeliOrder = S.time + 5.0f;
    // the enemy's Chosen in sight (found just now): every helicopter stays over it so the guns can finish it
    Entity* seen = nullptr;
    for (auto& e : S.ents) if (e.alive && e.isUnit() && chosenType(e.type) && S.enemies(player, e.owner) && S.visibleTo(e, player)) { seen = &e; break; }
    Vec2 spot; float r = 5.0f * TILE; int n = 0;
    if (seen) { spot = seen->pos; r = 2.5f * TILE; n = (int)helis.size(); }
    else if (alarm) { spot = pl.chosenAlarmAt; n = std::max(1, (int)helis.size() - (watch ? 1 : 0)); heliHunts++; }
    else if (hunting && huntAt > 0) { spot = huntPoint; n = (int)helis.size(); r = 8.0f * TILE; }
    std::vector<Ref> go, stay;
    for (size_t k = 0; k < helis.size(); k++) ((int)k < n ? go : stay).push_back(helis[k]);
    if (!go.empty()) S.cmdGuardArea(go, spot, r);
    if (!stay.empty()) S.cmdGuardArea(stay, pl.basePos, 9.0f * TILE);   // the watch over the base
    if (seen) {   // and the nearest fighters go for it too
        std::vector<Ref> near;
        S.forEachNear(seen->pos, 14.0f * TILE, [&](Entity& e) { if (e.isUnit() && e.owner == player && e.ut().role == UR_COMBAT && !e.isAir() && !chosenType(e.type) && near.size() < 12) near.push_back(S.refOf(e)); });
        if (!near.empty()) S.cmdAttack(near, S.refOf(*seen));
    }
}

// A feint: as a big wave sets off, its fastest fifth goes for the enemy's economy somewhere else (a hub, an income structure, the haulers' piles)
// first, to pull the defence away from the wave's real objective; once there it fights on its own for a while and then rejoins the army
bool AiPlayer::launchFeint(std::vector<Ref>& waveUnits, Entity* mainTarget) {
    Sim& S = g_sim;
    static const bool off = getenv("ONEHOUR_NOFEINT") != nullptr;   // (evaluation: the timing alone, no feints)
    if (off || !mainTarget || waveUnits.size() < 10 || mainEnemy < 0) return false;
    float chance = 0.35f * (on(FEAT_PERSONA) ? pers().raid : 1.0f);
    if (rng.f() > chance) return false;
    Entity* best = nullptr; float bs = 0;
    for (auto& e : S.ents) {
        if (!e.alive || !e.isBuilding() || !e.constructed || e.owner != mainEnemy) continue;
        BuildRole r = e.bt().role;
        if (r != BR_SUPPLY && r != BR_INCOME && r != BR_POWER && r != BR_BARRACKS) continue;
        if (!S.explored(player, clampi(tileOf(e.pos.x), 0, MAP_W - 1), clampi(tileOf(e.pos.y), 0, MAP_H - 1))) continue;
        float d = dist(e.pos, mainTarget->pos) / TILE;
        if (d < 14.0f) continue;   // (it must pull the defence somewhere else)
        float sc = (r == BR_SUPPLY || r == BR_INCOME ? 2.0f : 1.0f) * std::min(d, 40.0f) / (1.0f + enemyStrengthNear(e.pos, 9) / 1500.0f) * rng.f(0.8f, 1.2f);
        if (sc > bs) { bs = sc; best = &e; }
    }
    if (!best) return false;
    std::vector<std::pair<float, Ref>> fast;
    for (Ref r : waveUnits) if (Entity* e = S.get(r)) if (!e->isAir() && !e->ut().naval) fast.push_back({e->ut().speed, r});
    std::sort(fast.begin(), fast.end(), [](const std::pair<float, Ref>& a, const std::pair<float, Ref>& b) { return a.first > b.first; });
    size_t n = std::max<size_t>(2, waveUnits.size() / 5);
    feint.clear();
    for (size_t k = 0; k < fast.size() && k < n; k++) feint.push_back(fast[k].second);
    if (feint.empty()) return false;
    waveUnits.erase(std::remove_if(waveUnits.begin(), waveUnits.end(), [&](Ref r) { return std::find(feint.begin(), feint.end(), r) != feint.end(); }), waveUnits.end());
    groupMove(feint, best->pos, true);
    feintUntil = S.time + 45.0f; feints++;
    if (aiDebugCh()) fprintf(stderr, "[ai%d t=%.0f] FEINT: %d units at the %s, the wave (%d) at the %s\n", player, S.time, (int)feint.size(), best->bt().name, (int)waveUnits.size(), mainTarget->isUnit() ? mainTarget->ut().name : mainTarget->bt().name);
    return true;
}
