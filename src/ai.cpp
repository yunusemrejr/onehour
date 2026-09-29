#include "ai.h"
// Evaluation showed the learned wave gate loses to the plain heuristic (17-33 over 64 games) while the learned
// unit-choice model wins (31-21), so wave gating is opt-in: ONEHOUR_WAVEMODEL=1. ONEHOUR_ABLATE=u disables the unit model.
static int ablate() { static int v = -1; if (v < 0) { const char* e = getenv("ONEHOUR_ABLATE"); v = (e && e[0] == 'u') ? 2 : (getenv("ONEHOUR_WAVEMODEL") ? 0 : 1); } return v; }   // 1 = no wave model, 2 = no unit model
static bool aiDebug() { static int v = -1; if (v < 0) v = getenv("ONEHOUR_DEBUG") ? 1 : 0; return v == 1; }

AiManager g_ai;

static float unitValue(const Entity& e) { return e.isUnit() ? UNITS[e.type].cost * (0.5f + 0.5f * e.hp / e.maxHp) : 0; }

void AiManager::init(u64 seed) {
    for (int p = 0; p < MAX_PLAYERS; p++) {
        bool ub = ais[p].useBrain;
        ais[p] = AiPlayer();
        ais[p].useBrain = ub;
        if (g_sim.players[p].active && g_sim.players[p].isAI) { ais[p].useBrain = brainEnabled[p]; ais[p].init(p, seed + p * 7919); }
    }
}
void AiManager::update() {
    for (int p = 0; p < MAX_PLAYERS; p++) if (ais[p].player >= 0 && g_sim.players[p].alive) ais[p].think();
}

void AiPlayer::init(int p, u64 seed) {
    player = p; rng = Rng(seed);
    nextThink = 2.0f + p * 0.37f;
    // The AI knows where the start positions are (like a human reading the map)
    Player& pl = g_sim.players[p];
    for (auto& s : g_map.starts) {
        for (int dy = -9; dy <= 9; dy++) for (int dx = -9; dx <= 9; dx++) {
            int x = s.tx + dx, y = s.ty + dy;
            if (inMap(x, y) && dx * dx + dy * dy <= 81) pl.explored[y * MAP_W + x] = 1;
        }
    }
    // explore own quadrant generously so base building has room
    int qx = pl.basePos.x < WORLD_W / 2 ? 0 : MAP_W / 2, qy = pl.basePos.y < WORLD_H / 2 ? 0 : MAP_H / 2;
    for (int y = qy; y < qy + MAP_H / 2; y++) for (int x = qx; x < qx + MAP_W / 2; x++) pl.explored[y * MAP_W + x] = 1;
}

Entity* AiPlayer::idleDozer() {
    for (auto& e : g_sim.ents)
        if (e.alive && e.isUnit() && e.owner == player && e.ut().role == UR_DOZER && (e.order == O_IDLE || e.order == O_MOVE)) return &e;
    return nullptr;
}

bool AiPlayer::findSpot(int buildType, Vec2 preferNear, int maxRing, int& tx, int& ty) {
    const BuildType& b = BUILDS[buildType];
    Vec2 base = g_sim.players[player].basePos;
    int cx = tileOf(base.x) - b.w / 2, cy = tileOf(base.y) - b.h / 2;
    float bestScore = 1e18f; int bx = -1, by = -1;
    for (int ring = 3; ring <= maxRing; ring++) {
        for (int dy = -ring; dy <= ring; dy++) for (int dx = -ring; dx <= ring; dx++) {
            if (std::abs(dx) != ring && std::abs(dy) != ring) continue;
            int x = cx + dx, y = cy + dy;
            if (!g_sim.canPlace(player, buildType, x, y)) continue;
            // keep a one-tile corridor around every structure so units can always get through
            bool gapOk = true;
            for (int j = y - 1; j <= y + b.h && gapOk; j++) for (int i = x - 1; i <= x + b.w; i++) {
                if (!inMap(i, j)) { gapOk = false; break; }
                if (g_map.blocked[j * MAP_W + i] & 2) { gapOk = false; break; }
            }
            if (!gapOk) continue;
            // don't build on roads leading out (keeps lanes open) unless it's a turret
            Vec2 c = g_sim.buildingCenter(buildType, x, y);
            float score = dist(c, preferNear) + ring * 6.0f;
            if (score < bestScore) { bestScore = score; bx = x; by = y; }
        }
        if (bx >= 0 && ring >= 6 && bestScore < ring * TILE) break;  // good enough, stop expanding
    }
    if (bx < 0) return false;
    tx = bx; ty = by; return true;
}

bool AiPlayer::tryBuild(int buildType, Vec2 preferNear, int maxRing) {
    Entity* d = idleDozer();
    if (!d) return false;
    if (!g_sim.buildAvailable(player, buildType)) return false;
    if (!g_sim.canAfford(player, BUILDS[buildType].cost)) return false;
    int tx, ty;
    if (!findSpot(buildType, preferNear, maxRing, tx, ty)) return false;
    return g_sim.cmdBuild(g_sim.refOf(*d), buildType, tx, ty);
}

int AiPlayer::chooseUnit(BuildRole role, int enemyInf, int enemyVeh, int enemyAir) {
    Player& pl = g_sim.players[player];
    int base = firstUnitOf(pl.faction);
    struct C { int type; float w; };
    std::vector<C> cands;
    float total = std::max(1, enemyInf + enemyVeh + enemyAir);
    float fi = enemyInf / total, fv = enemyVeh / total, fa = enemyAir / total;
    bool early = g_sim.time < 240;
    if (role == BR_BARRACKS) {
        cands.push_back({base + 2, 1.0f + fi * 1.5f});                    // rifle
        cands.push_back({base + 3, 0.9f + fv * 2.0f + fa * 2.5f});        // anti-armor / AA
        cands.push_back({base + 4, 0.7f + fi * 2.0f});                    // tech infantry
        cands.push_back({base + 9, 0.8f + fi * 1.0f + fv * 1.6f});        // elite (needs the Advanced Program)
    } else if (role == BR_FACTORY) {
        cands.push_back({base + 5, 1.4f + fv * 1.0f});                    // main tank
        cands.push_back({base + 6, 0.9f + fi * 2.0f + fa * 2.0f});        // anti-inf / AA vehicle
        cands.push_back({base + 7, (early ? 0.3f : 0.9f) + fv * 1.2f});  // artillery / railgun
        cands.push_back({base + 10, 1.0f + fv * 1.5f});                   // super-heavy (needs the Advanced Program)
    } else if (role == BR_AIRFIELD) {
        cands.push_back({base + 8, 1.0f});
    }
    // weight by how many we already have (diminishing returns), availability, and predicted efficiency:
    // the brain's per-type regression (value destroyed per credit given the enemy mix) replaces the old
    // running average when learning is on
    float x[UNIT_F]; Brain::unitFeatures(fi, fv, fa, x);
    float avgEff = 0; int nEff = 0;
    for (auto& c : cands) {
        if (useBrain && ablate() != 2) { avgEff += g_brain.unitEff(c.type, x); nEff++; }
        else if (pl.spentOn[c.type] > 0) { avgEff += pl.valueDealt[c.type] / pl.spentOn[c.type]; nEff++; }
    }
    avgEff = nEff ? avgEff / nEff : 0;
    float best = -1; int pick = -1;
    for (auto& c : cands) {
        if (!g_sim.unitAvailable(player, c.type)) continue;
        int have = g_sim.countUnits(player, c.type);
        float w = c.w / (1.0f + have * 0.25f) * rng.f(0.85f, 1.15f);
        if (useBrain && ablate() != 2) {
            if (avgEff > 0) w *= clampf(0.65f + 0.7f * (g_brain.unitEff(c.type, x) / avgEff), 0.5f, 1.8f);
        } else if (pl.spentOn[c.type] >= 1500 && avgEff > 0) {
            float eff = pl.valueDealt[c.type] / pl.spentOn[c.type];
            w *= clampf(0.7f + 0.6f * (eff / avgEff), 0.5f, 1.8f);
        }
        if (pl.money < UNITS[c.type].cost) continue;
        if (w > best) { best = w; pick = c.type; }
    }
    return pick;
}

float AiPlayer::enemyStrengthNear(Vec2 p, float radiusTiles) {
    float v = 0;
    for (auto& e : g_sim.ents) {
        if (!e.alive || e.kind == EK_RESOURCE || !g_sim.enemies(player, e.owner)) continue;
        if (dist(e.pos, p) > radiusTiles * TILE) continue;
        float hpf = e.hp / e.maxHp;
        if (e.isUnit()) { if (e.ut().role == UR_COMBAT) v += UNITS[e.type].cost * hpf; }
        else if (e.constructed && e.bt().weapon >= 0) v += BUILDS[e.type].cost * 1.6f * hpf;
    }
    return v;
}

Entity* AiPlayer::pickAttackTarget() {
    // Prefer economy and production of the main enemy; fall back to anything known
    Entity* best = nullptr; float bs = 1e18f;
    Vec2 base = g_sim.players[player].basePos;
    for (auto& e : g_sim.ents) {
        if (!e.alive || e.kind == EK_RESOURCE || !g_sim.enemies(player, e.owner)) continue;
        if (!g_sim.explored(player, clampi(tileOf(e.pos.x), 0, MAP_W - 1), clampi(tileOf(e.pos.y), 0, MAP_H - 1))) continue;
        float s = dist(e.pos, base) / TILE;
        if (e.isBuilding()) {
            BuildRole r = e.bt().role;
            if (r == BR_SUPPLY) s -= 14; else if (r == BR_POWER) s -= 12; else if (r == BR_FACTORY || r == BR_BARRACKS || r == BR_AIRFIELD) s -= 8;
            else if (r == BR_TURRET || r == BR_AATURRET) s += 4; else if (r == BR_HQ) s += 6;
            // defended targets cost more: count turrets around it
            for (auto& t : g_sim.ents) if (t.alive && t.isBuilding() && t.owner == e.owner && t.constructed && (t.bt().role == BR_TURRET || t.bt().role == BR_AATURRET) && dist(t.pos, e.pos) < 8 * TILE) s += 3;
            if (g_sim.refOf(e) == failedTarget && g_sim.time - failedAt < 240) s += 18;
            s += enemyStrengthNear(e.pos, 9) / 500.0f;   // prefer the softest worthwhile target
        } else {
            if (e.ut().role == UR_HARVESTER) s -= 10; else continue;   // armies are met on the way, not hunted
        }
        if (s < bs) { bs = s; best = &e; }
    }
    return best;
}

// Feed the outcome of a finished wave back into the model: did it destroy at least as much as it lost?
void AiPlayer::endWave(float remainingValue) {
    if (!waveHasSample) return;
    waveHasSample = false;
    Player& pl = g_sim.players[player];
    float dealt = -waveDealt0; for (int u = 0; u < U_COUNT; u++) dealt += pl.valueDealt[u];
    float lost = std::max(0.0f, waveValue - remainingValue);
    bool success = dealt >= lost * 0.9f && dealt > 300;
    if (useBrain) g_brain.learnWave(waveX, success);
    if (aiDebug()) fprintf(stderr, "[ai%d t=%.0f] wave result: dealt %.0f lost %.0f -> %s\n", player, g_sim.time, dealt, lost, success ? "success" : "failure");
}

void AiPlayer::managePower() {
    Player& pl = g_sim.players[player];
    int base = firstBuildOf(pl.faction);
    // count power that is under construction too
    int pending = 0;
    for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.owner == player && !e.constructed && e.bt().power > 0) pending += e.bt().power;
    if (pl.powerMade + pending < pl.powerUsed + 4) tryBuild(base + BR_POWER, pl.basePos);
}

void AiPlayer::think() {
    Sim& S = g_sim;
    Player& pl = S.players[player];
    if (S.time < nextThink) return;
    float cadence = pl.difficulty >= 2 ? 0.5f : (pl.difficulty == 1 ? 0.8f : 1.3f);
    nextThink = S.time + cadence;
    int base = firstBuildOf(pl.faction);
    int ubase = firstUnitOf(pl.faction);
    float minutes = S.time / 60.0f;

    // ---------- survey
    int dozers = 0, harvesters = 0, armyCount = 0; float armyValue = 0;
    std::vector<Ref> army, idleArmy, aircraft, haulers;
    Entity* hq = nullptr;
    std::vector<Entity*> supplyHubs, barracks, factories, airfields, damaged;
    int turrets = 0, powerPlants = 0, techs = 0, incomes = 0, ramps = 0;
    for (auto& e : S.ents) {
        if (!e.alive || e.owner != player) continue;
        if (e.isUnit()) {
            const UnitType& ut = e.ut();
            if (ut.role == UR_DOZER) dozers++;
            else if (ut.role == UR_HARVESTER) { harvesters++; haulers.push_back(S.refOf(e)); }
            else if (ut.kind == UK_AIR) aircraft.push_back(S.refOf(e));
            else { army.push_back(S.refOf(e)); armyCount++; armyValue += unitValue(e); if (e.order == O_IDLE) idleArmy.push_back(S.refOf(e)); }
        } else {
            if (e.constructed && e.hp < e.maxHp * 0.6f) damaged.push_back(&e);
            switch (e.bt().role) {
            case BR_HQ: hq = &e; break;
            case BR_SUPPLY: supplyHubs.push_back(&e); break;
            case BR_BARRACKS: barracks.push_back(&e); break;
            case BR_FACTORY: factories.push_back(&e); break;
            case BR_AIRFIELD: airfields.push_back(&e); break;
            case BR_TURRET: case BR_AATURRET: turrets++; break;
            case BR_POWER: powerPlants++; break;
            case BR_TECH: techs++; break;
            case BR_INCOME: incomes++; break;
            case BR_NUKE: ramps++; break;
            }
        }
    }
    // enemy intel (only what we've explored)
    int enemyInf = 0, enemyVeh = 0, enemyAir = 0; float enemyArmyValue = 0;
    Entity* threat = nullptr; float threatDist = 1e18f; int threatCount = 0;
    std::vector<Entity*> enemyUnits;
    std::vector<Entity*> ownBuildings;
    for (auto& e : S.ents) if (e.alive && e.isBuilding() && e.owner == player) ownBuildings.push_back(&e);
    for (auto& e : S.ents) {
        if (!e.alive || e.kind == EK_RESOURCE || !S.enemies(player, e.owner)) continue;
        int tx = clampi(tileOf(e.pos.x), 0, MAP_W - 1), ty = clampi(tileOf(e.pos.y), 0, MAP_H - 1);
        if (!S.explored(player, tx, ty)) continue;
        if (e.isUnit()) {
            const UnitType& ut = e.ut();
            if (ut.role == UR_COMBAT) {
                if (ut.kind == UK_INF) enemyInf++; else if (ut.kind == UK_AIR) enemyAir++; else enemyVeh++;
                enemyArmyValue += unitValue(e);
                enemyUnits.push_back(&e);
                // threats: enemy combat units close to any of our structures
                float d = 1e18f;
                for (auto* b : ownBuildings) d = std::min(d, S.distToEntity(e.pos, *b));
                if (d < 12 * TILE) { threatCount++; float db = dist(e.pos, pl.basePos); if (db < threatDist) { threatDist = db; threat = &e; } }
            }
        }
    }
    if (!threat) {
        // also react to harvesters being attacked near home
        for (auto& e : S.ents) {
            if (!e.alive || e.owner != player || !e.isUnit() || e.ut().role != UR_HARVESTER || S.time - e.lastDamaged > 3.0f) continue;
            Entity* a = S.get(e.attacker);
            if (a && a->isUnit() && S.enemies(player, a->owner) && dist(a->pos, pl.basePos) < 24 * TILE) { threat = a; threatCount = 1; break; }
        }
    }

    // enemy mix, and periodic learning of per-type efficiency from the last interval
    {
        float tot = std::max(1, enemyInf + enemyVeh + enemyAir);
        if (enemyInf + enemyVeh + enemyAir > 0) { mixFi = enemyInf / tot; mixFv = enemyVeh / tot; mixFa = enemyAir / tot; }
        if (S.time >= nextLearn) {
            nextLearn = S.time + 30;
            float x[UNIT_F]; Brain::unitFeatures(mixFi, mixFv, mixFa, x);
            for (int u = 0; u < U_COUNT; u++) {
                float ds = pl.spentOn[u] - prevSpent[u], dd = pl.valueDealt[u] - prevDealt[u];
                if (ds >= 400 && useBrain) g_brain.learnUnit(u, x, dd / ds);
                if (ds >= 400) { prevSpent[u] = pl.spentOn[u]; prevDealt[u] = pl.valueDealt[u]; }
            }
        }
    }

    // main enemy = nearest living enemy base
    if (mainEnemy < 0 || !S.players[mainEnemy].alive) {
        float bd = 1e18f; mainEnemy = -1;
        for (int p = 0; p < S.numPlayers; p++) if (S.players[p].alive && S.enemies(player, p)) { float d = dist2(S.players[p].basePos, pl.basePos); if (d < bd) { bd = d; mainEnemy = p; } }
        if (mainEnemy >= 0) enemyDir = (S.players[mainEnemy].basePos - pl.basePos).norm();
        else enemyDir = (Vec2(WORLD_W / 2, WORLD_H / 2) - pl.basePos).norm();
    }
    rally = g_map.nearestFree(pl.basePos + enemyDir * (9 * TILE), 6);

    // ---------- economy
    int hubs = (int)supplyHubs.size();
    int wantHarv = std::min(6, hubs * 3);
    int queuedHarv = 0;
    for (auto* h : supplyHubs) for (int t : h->queue) if (UNITS[t].role == UR_HARVESTER) queuedHarv++;
    if (harvesters + queuedHarv < wantHarv) {
        for (auto* h : supplyHubs) if (h->constructed && h->queue.empty()) { S.cmdTrain(S.refOf(*h), ubase + 1); break; }
    }
    int wantDozers = pl.difficulty >= 2 ? 3 : 2;
    int queuedDozer = 0;
    if (hq) for (int t : hq->queue) if (UNITS[t].role == UR_DOZER) queuedDozer++;
    if (hq && hq->constructed && dozers + queuedDozer < wantDozers && hq->queue.size() < 2 && (dozers == 0 || pl.money > 2200)) S.cmdTrain(S.refOf(*hq), ubase + 0);

    // ---------- supply areas: every hub gets a gather circle around the piles near it; haulers are spread across them
    struct Mine { Vec2 c; float r; int workers; };
    std::vector<Mine> mines;
    for (auto* h : supplyHubs) {
        if (!h->constructed) continue;
        Vec2 sum; float wsum = 0; int n = 0;
        for (auto& p : S.ents) if (p.alive && p.kind == EK_RESOURCE && p.amount > 0 && dist(p.pos, h->pos) < 16 * TILE) { sum += p.pos * (float)p.amount; wsum += p.amount; n++; }
        if (n == 0) continue;
        Vec2 c = sum * (1.0f / wsum);
        float r = 0; for (auto& p : S.ents) if (p.alive && p.kind == EK_RESOURCE && p.amount > 0 && dist(p.pos, h->pos) < 16 * TILE) r = std::max(r, dist(p.pos, c));
        mines.push_back({c, clampf(r + 2.0f * TILE, 4.0f * TILE, 14.0f * TILE), 0});
    }
    for (auto r : haulers) { Entity* e = S.get(r); if (!e || e->zoneR <= 0) continue; for (auto& m : mines) if (dist(m.c, e->zone) < 5 * TILE) m.workers++; }
    for (auto r : haulers) {
        Entity* e = S.get(r);
        if (!e || e->zoneR > 0 || e->cargo > 0 || (e->order != O_IDLE && e->order != O_HARVEST)) continue;
        Mine* best = nullptr;
        for (auto& m : mines) if (!best || m.workers < best->workers || (m.workers == best->workers && dist(m.c, e->pos) < dist(best->c, e->pos))) best = &m;
        if (best) { S.cmdGatherArea({r}, best->c, best->r); best->workers++; }
        else if (e->order == O_IDLE) { Entity* pile = S.findPile(*e, 60 * TILE); if (pile) S.cmdHarvest({r}, S.refOf(*pile)); }   // nothing near a hub: fall back to the nearest pile anywhere
    }

    // ---------- construction (one decision per think, with a reserve for economy)
    Entity* dz = idleDozer();
    if (dz) {
        bool built = false;
        // 1. power
        if (pl.powerMade < pl.powerUsed) { managePower(); built = idleDozer() != dz; }
        // 2. supply
        if (!built && hubs == 0) {
            // nearest pile to base
            Entity* pile = nullptr; float bd = 1e18f;
            for (auto& e : S.ents) if (e.alive && e.kind == EK_RESOURCE) { float d = dist2(e.pos, pl.basePos); if (d < bd) { bd = d; pile = &e; } }
            built = tryBuild(base + BR_SUPPLY, pile ? pile->pos : pl.basePos, 26);
        }
        if (!built && powerPlants == 0) built = tryBuild(base + BR_POWER, pl.basePos);
        if (!built && barracks.empty()) built = tryBuild(base + BR_BARRACKS, pl.basePos + enemyDir * 100);
        if (!built && factories.empty()) built = tryBuild(base + BR_FACTORY, pl.basePos);
        if (!built) { int before = S.countRole(player, BR_POWER, false); managePower(); built = S.countRole(player, BR_POWER, false) != before; }
        // defenses scale with time and difficulty
        int wantTurrets = std::min(pl.difficulty >= 3 ? 7 : (pl.difficulty == 2 ? 5 : 3), (int)(minutes / (pl.difficulty >= 2 ? 2.0f : 3.0f)));
        if (!built && turrets < wantTurrets && minutes > 2.5f) {
            int kind = (enemyAir > 0 || nextTurretKind % 2 == 1) ? BR_AATURRET : BR_TURRET;
            Vec2 spot = pl.basePos + enemyDir * (7 * TILE) + Vec2(-enemyDir.y, enemyDir.x) * ((nextTurretKind % 3 - 1) * 3 * TILE);
            built = tryBuild(base + kind, spot, 12);
            if (built) nextTurretKind++;
        }
        if (!built && techs == 0 && minutes > (pl.difficulty >= 2 ? 4.0f : 6.0f) && pl.money > 2600) built = tryBuild(base + BR_TECH, pl.basePos);
        if (!built && airfields.empty() && minutes > (pl.difficulty >= 2 ? 6.0f : 9.0f) && pl.money > 2000) built = tryBuild(base + BR_AIRFIELD, pl.basePos);
        if (!built && factories.size() < 2 && minutes > 7 && pl.money > 4500) built = tryBuild(base + BR_FACTORY, pl.basePos);
        // expansion: a second hub near a farther pile once the closest piles thin out
        if (!built && hubs < 2 && minutes > 8 && pl.money > 3000) {
            Entity* pile = nullptr; float bd = 1e18f;
            for (auto& e : S.ents) {
                if (!e.alive || e.kind != EK_RESOURCE) continue;
                float d = dist(e.pos, pl.basePos);
                bool covered = false;
                for (auto* h : supplyHubs) if (dist(h->pos, e.pos) < 14 * TILE) covered = true;
                if (!covered && d < bd && d < 40 * TILE) { bd = d; pile = &e; }
            }
            if (pile) {
                // build near the pile: temporarily search around the pile rather than the base
                Vec2 saved = pl.basePos; pl.basePos = pile->pos;
                built = tryBuild(base + BR_SUPPLY, pile->pos, 8);
                pl.basePos = saved;
            }
        }
        // steady income: oil wells / bitcoin datacenters, then nuke ramps once the economy is comfortable
        if (!built && incomes < (pl.difficulty >= 2 ? 3 : 2) && minutes > (pl.difficulty >= 2 ? 3.0f : 5.0f) && pl.money > 2200 && powerPlants > 0) built = tryBuild(base + BR_INCOME, pl.basePos - enemyDir * 80);
        if (!built && techs > 0 && ramps < (pl.difficulty >= 3 ? 2 : 1) && pl.difficulty >= 1 && minutes > (pl.difficulty >= 2 ? 9.0f : 13.0f) && pl.money > 5300) built = tryBuild(base + BR_NUKE, pl.basePos - enemyDir * 100);
        if (!built && barracks.size() < 2 && minutes > 10 && pl.money > 3500) built = tryBuild(base + BR_BARRACKS, pl.basePos);
        // repair
        if (!built && !damaged.empty()) S.cmdAssist({S.refOf(*dz)}, S.refOf(*damaged[0]));
        // continue unfinished structures whose dozer died
        if (!built) for (auto& e : S.ents)
            if (e.alive && e.isBuilding() && e.owner == player && !e.constructed) {
                bool someone = false;
                for (auto& u : S.ents) if (u.alive && u.owner == player && u.isUnit() && u.order == O_BUILD && u.targetEnt == S.refOf(e)) someone = true;
                if (!someone) { S.cmdAssist({S.refOf(*dz)}, S.refOf(e)); break; }
            }
    }

    // ---------- production
    int reserve = (hubs == 0 || pl.powerMade < pl.powerUsed) ? 1600 : 0;
    if (harvesters + queuedHarv < 2 && hubs > 0) reserve = std::max(reserve, 900);
    // save up for a nuke ramp once the tech structure stands
    if (techs > 0 && ramps < (pl.difficulty >= 3 ? 2 : 1) && pl.difficulty >= 1 && minutes > (pl.difficulty >= 2 ? 8.0f : 12.0f) && !threat) reserve = std::max(reserve, 5300);
    // early game: economy and structures first, a modest guard force, then ramp
    float rampStart = pl.difficulty >= 3 ? 1.5f : (pl.difficulty == 2 ? 2.0f : (pl.difficulty == 1 ? 3.0f : 4.0f));
    int armyCap;
    if (pl.difficulty >= 2) armyCap = minutes < rampStart ? 6 : (minutes < rampStart + 2.5f ? 14 : (minutes < rampStart + 5 ? 26 : 60));
    else armyCap = minutes < rampStart ? 4 : (minutes < rampStart + 2.5f ? 9 : (minutes < rampStart + 5 ? 16 : (pl.difficulty == 0 ? 24 : 40)));
    int queuedArmy = 0;
    for (auto* b : barracks) queuedArmy += (int)b->queue.size();
    for (auto* f : factories) queuedArmy += (int)f->queue.size();
    bool wantArmy = armyCount + queuedArmy < armyCap || threat;
    if (pl.money - reserve > 0 && wantArmy) {
        for (auto* b : barracks) if (b->constructed && b->queue.size() < 2 && pl.money - reserve > 400) { int t = chooseUnit(BR_BARRACKS, enemyInf, enemyVeh, enemyAir); if (t >= 0) S.cmdTrain(S.refOf(*b), t); }
        for (auto* f : factories) if (f->constructed && f->queue.size() < 2 && pl.money - reserve > 900) { int t = chooseUnit(BR_FACTORY, enemyInf, enemyVeh, enemyAir); if (t >= 0) S.cmdTrain(S.refOf(*f), t); }
    }
    for (auto* a : airfields) if (a->constructed && a->queue.empty() && pl.money - reserve > 2500 && aircraft.size() < 4) { int t = chooseUnit(BR_AIRFIELD, enemyInf, enemyVeh, enemyAir); if (t >= 0) S.cmdTrain(S.refOf(*a), t); }
    // rally new units toward the front
    for (auto* b : barracks) S.cmdSetRally(S.refOf(*b), rally);
    for (auto* f : factories) S.cmdSetRally(S.refOf(*f), rally);

    // ---------- tech structure: the Advanced Program, a map scan when nothing is known about the enemy, and the strike
    if (techs > 0 && S.programAvailable(player) && minutes > (pl.difficulty >= 2 ? 6.0f : 9.0f) && pl.money > PROGRAMS[pl.faction].cost + 1200) S.cmdResearch(player);
    if (techs > 0 && enemyUnits.empty() && minutes > 4 && S.time >= pl.scanReady) S.cmdScan(player);
    if (techs > 0 && S.time >= pl.powerReady && !enemyUnits.empty()) {
        const PowerType& pw = POWERS[pl.faction];
        Vec2 bestPos; float bestScore = 0;
        for (auto* e : enemyUnits) {
            float score = 0;
            for (auto* o : enemyUnits) if (dist(e->pos, o->pos) < pw.radius * TILE) score += (pl.faction == F_CYBER ? (o->ut().kind == UK_VEH ? UNITS[o->type].cost : 0) : UNITS[o->type].cost);
            // shell storm also loves supply hubs; EMP loves turrets
            for (auto& s : S.ents) if (s.alive && s.isBuilding() && S.enemies(player, s.owner) && dist(e->pos, s.pos) < pw.radius * TILE) score += 400;
            if (score > bestScore) { bestScore = score; bestPos = e->pos; }
        }
        if (bestScore >= (pl.faction == F_CYBER ? 2500.0f : 1800.0f)) S.cmdPower(player, bestPos);
    }

    // ---------- nukes: fire at the densest cluster of enemy structures we know about
    if (ramps > 0 && S.nukesReady(player) > 0) {
        Vec2 bestPos; float bestScore = 0;
        for (auto& e : S.ents) {
            if (!e.alive || !e.isBuilding() || !S.enemies(player, e.owner) || !S.explored(player, clampi(tileOf(e.pos.x), 0, MAP_W - 1), clampi(tileOf(e.pos.y), 0, MAP_H - 1))) continue;
            float score = 0;
            for (auto& o : S.ents) if (o.alive && S.enemies(player, o.owner) && o.kind != EK_RESOURCE && dist(e.pos, o.pos) < NUKE_RADIUS * TILE * 0.8f) score += o.isUnit() ? UNITS[o.type].cost : BUILDS[o.type].cost * 0.6f;
            for (auto& o : S.ents) if (o.alive && o.owner == player && o.kind != EK_RESOURCE && dist(e.pos, o.pos) < NUKE_RADIUS * TILE * 1.1f) score -= 4000;   // never nuke our own people
            if (score > bestScore) { bestScore = score; bestPos = e.pos; }
        }
        if (bestScore >= 3500.0f) S.cmdNuke(player, bestPos);
    }

    // ---------- army
    float waveThreshold = 3200.0f + minutes * 320.0f;
    if (pl.difficulty == 0) waveThreshold *= 1.7f; else if (pl.difficulty == 2) waveThreshold *= 0.85f; else if (pl.difficulty == 3) waveThreshold *= 0.75f;
    waveThreshold = std::min(waveThreshold, 11000.0f);
    float firstWaveAt = pl.difficulty >= 3 ? 210.0f : (pl.difficulty == 2 ? 270.0f : (pl.difficulty == 1 ? 360.0f : 480.0f));

    // defense has priority
    if (threat && threatCount > 0 && S.time - lastDefend > 3.0f) {
        lastDefend = S.time;
        if (aiDebug()) fprintf(stderr, "[ai%d t=%.0f] DEFEND vs %s at %d,%d (count %d) attacking=%d\n", player, S.time, threat->ut().name, tileOf(threat->pos.x), tileOf(threat->pos.y), threatCount, (int)attacking);
        std::vector<Ref> defenders;
        for (auto r : army) { Entity* e = S.get(r); if (!e) continue; if (!attacking || dist(e->pos, pl.basePos) < 22 * TILE) defenders.push_back(r); }
        // a base under real attack outranks a wave in progress (the wave resumes once the threat is gone)
        if (attacking && threatCount >= 4) defenders = army;
        if (!defenders.empty()) S.cmdMove(defenders, threat->pos, true);
        for (auto r : aircraft) { Entity* a = S.get(r); if (a && a->ammo > 0 && a->order == O_IDLE) S.cmdAttack({r}, S.refOf(*threat)); }
        if (attacking && armyValue < waveValue * 0.5f) { endWave(armyValue); attacking = false; wave.clear(); }
    } else if (attacking) {
        // prune the wave and measure what is left of it
        wave.erase(std::remove_if(wave.begin(), wave.end(), [&](Ref r) { return S.get(r) == nullptr; }), wave.end());
        float waveNow = 0;
        for (auto r : wave) waveNow += unitValue(*S.get(r));
        Entity* tgt = S.get(attackTarget);
        if (!tgt) { tgt = pickAttackTarget(); attackTarget = tgt ? S.refOf(*tgt) : NOREF; if (tgt) S.cmdMove(wave, tgt->pos, true); }
        bool exhausted = waveNow < waveValue * 0.5f || wave.empty();
        bool stale = S.time - attackStarted > 240.0f;
        if (exhausted || stale || !tgt) {
            if (aiDebug()) fprintf(stderr, "[ai%d t=%.0f] %s value=%.0f/%.0f units=%d\n", player, S.time, exhausted ? "RETREAT" : "REGROUP", waveNow, waveValue, (int)wave.size());
            endWave(waveNow);
            attacking = false; regroupUntil = S.time + (exhausted ? 40.0f : 10.0f);
            if (exhausted && tgt) { failedTarget = attackTarget; failedAt = S.time; }
            S.cmdMove(wave, rally, true);
            wave.clear();
        } else {
            if (S.time > nextOrderTime) {
                nextOrderTime = S.time + 6;
                std::vector<Ref> idle;
                for (auto r : wave) { Entity* e = S.get(r); if (e && e->order == O_IDLE) idle.push_back(r); }
                if (!idle.empty()) S.cmdMove(idle, tgt->pos, true);
            }
            // reinforcements mass at the rally and join as a second wave once they are worth it
            std::vector<Ref> reserveUnits; float reserveValue = 0;
            for (auto r : army) if (std::find(wave.begin(), wave.end(), r) == wave.end()) { reserveUnits.push_back(r); reserveValue += unitValue(*S.get(r)); }
            std::vector<Ref> farIdle;
            for (auto r : reserveUnits) { Entity* e = S.get(r); if (e && e->order == O_IDLE && dist(e->pos, rally) > 4 * TILE) farIdle.push_back(r); }
            if (!farIdle.empty()) S.cmdGuardArea(farIdle, rally, 6 * TILE);
            if (reserveValue >= waveThreshold * 0.6f && reserveUnits.size() >= 5) {
                if (aiDebug()) fprintf(stderr, "[ai%d t=%.0f] REINFORCE value=%.0f units=%d\n", player, S.time, reserveValue, (int)reserveUnits.size());
                S.cmdMove(reserveUnits, tgt->pos, true);
                wave.insert(wave.end(), reserveUnits.begin(), reserveUnits.end());
                waveValue += reserveValue;
            }
        }
    } else {
        // idle fighters take up a guard circle at the rally point; a few also protect the mining area
        if (!idleArmy.empty()) {
            std::vector<Ref> far;
            for (auto r : idleArmy) { Entity* e = S.get(r); if (e && dist(e->pos, rally) > 4 * TILE) far.push_back(r); }
            if (!far.empty()) S.cmdGuardArea(far, rally, clampf((4.0f + armyCount * 0.15f) * TILE, 5.0f * TILE, 8.0f * TILE));
        }
        if (!mines.empty() && minutes > 3.0f) {
            const Mine& m = mines[0];
            int want = pl.difficulty >= 2 ? 3 : 2, have = 0;
            for (auto r : army) { Entity* e = S.get(r); if (e && e->zoneR > 0 && dist(e->zone, m.c) < 3 * TILE) have++; }
            if (have < want) {
                std::vector<Ref> pick;
                for (auto r : army) {
                    Entity* e = S.get(r);
                    if (!e || (int)pick.size() >= want - have) break;
                    bool atRally = e->zoneR > 0 && dist(e->zone, rally) < 3 * TILE;
                    if ((e->order == O_IDLE || atRally) && e->weapon() >= 0) pick.push_back(r);
                }
                if (!pick.empty()) S.cmdGuardArea(pick, m.c, std::max(m.r + 2 * TILE, 6.0f * TILE));
            }
        }
        bool overwhelming = armyValue > enemyArmyValue * 1.6f + 1500 && armyCount >= 6;
        bool eligible = armyValue >= waveThreshold || (overwhelming && minutes > 6);
        bool waveModel = useBrain && ablate() != 1;
        if (S.time > regroupUntil && S.time > firstWaveAt && eligible && mainEnemy >= 0) {
            Entity* tgt = pickAttackTarget();
            float defense = tgt ? enemyStrengthNear(tgt->pos, 11) : 0;
            float x[WAVE_F]; Brain::waveFeatures(armyValue, defense, enemyArmyValue, minutes, armyCount, x);
            bool go = true;
            if (tgt) {
                if (waveModel) {
                    float p = g_brain.waveProb(x);
                    float thr = 0.45f;   // the model can only hold a wave back, never launch a smaller one
                    // exploration: now and then launch (or hold) against the model's advice so it keeps seeing both outcomes
                    bool explore = g_brain.learning && rng.f() < 0.15f;   // launch against its advice sometimes so it keeps seeing both outcomes
                    go = explore || p >= thr || armyCount >= armyCap;
                    if (aiDebug() && !go && (int)S.time % 30 == 0) fprintf(stderr, "[ai%d t=%.0f] holding: p=%.2f army %.0f vs defense %.0f\n", player, S.time, p, armyValue, defense);
                } else {
                    // heuristic: commit only when the wave outweighs what is waiting for it
                    float need = defense * (minutes < 15 ? 1.3f : 1.1f);
                    go = !(armyValue < need && armyCount < armyCap);
                }
            }
            if (tgt && go) {
                attacking = true; waveValue = armyValue; attackStarted = S.time; attackTarget = S.refOf(*tgt);
                wave = army;
                memcpy(waveX, x, sizeof x); waveHasSample = true; waveDealt0 = 0; for (int u = 0; u < U_COUNT; u++) waveDealt0 += pl.valueDealt[u];
                if (aiDebug()) fprintf(stderr, "[ai%d t=%.0f] WAVE value=%.0f units=%d -> %s at %d,%d\n", player, S.time, armyValue, armyCount, tgt->isUnit() ? tgt->ut().name : tgt->bt().name, tileOf(tgt->pos.x), tileOf(tgt->pos.y));
                S.cmdMove(army, tgt->pos, true);
                nextOrderTime = S.time + 6;
            }
        }
        // harassment: hard AIs send a small fast squad after enemy harvesters
        if (pl.difficulty >= 2 && minutes > 4 && S.time - lastHarass > 90 && armyCount >= 5) {
            Entity* h = nullptr; float bd = 1e18f;
            for (auto& e : S.ents) if (e.alive && e.isUnit() && S.enemies(player, e.owner) && e.ut().role == UR_HARVESTER && S.explored(player, clampi(tileOf(e.pos.x), 0, MAP_W - 1), clampi(tileOf(e.pos.y), 0, MAP_H - 1))) { float d = dist2(e.pos, pl.basePos); if (d < bd) { bd = d; h = &e; } }
            if (h) {
                lastHarass = S.time;
                std::vector<Ref> squad;
                for (auto r : army) { Entity* e = S.get(r); if (e && e->ut().kind == UK_VEH && e->ut().speed >= 70) { squad.push_back(r); if (squad.size() >= 2) break; } }
                if (!squad.empty()) S.cmdAttack(squad, S.refOf(*h));
            }
        }
    }
    // aircraft strike on their own cadence: hit the juiciest known target
    for (auto r : aircraft) {
        Entity* a = S.get(r);
        if (!a) continue;
        // between strikes drones patrol a circle over the base
        if (a->order == O_IDLE && a->weapon() >= 0) S.cmdGuardArea({r}, pl.basePos, 9 * TILE);
        if ((a->order != O_IDLE && a->order != O_GUARDAREA) || a->ammo < a->ut().ammo) continue;
        Entity* best = nullptr; float bs = 1e18f;
        for (auto& e : S.ents) {
            if (!e.alive || !S.enemies(player, e.owner) || e.kind == EK_RESOURCE) continue;
            if (!S.explored(player, clampi(tileOf(e.pos.x), 0, MAP_W - 1), clampi(tileOf(e.pos.y), 0, MAP_H - 1))) continue;
            float s = dist(e.pos, a->pos) / TILE;
            if (e.isUnit()) { if (e.ut().role == UR_HARVESTER) s -= 12; else if (e.ut().kind == UK_VEH) s -= 6; else if (e.ut().kind == UK_AIR) s -= 8; }
            else { if (e.bt().role == BR_AATURRET) s += 25; else if (e.bt().role == BR_POWER) s -= 6; else if (e.bt().role == BR_TURRET) s += 6; }
            // avoid flying into AA nests
            for (auto& t : S.ents) if (t.alive && t.isBuilding() && t.owner == e.owner && t.bt().role == BR_AATURRET && t.constructed && dist(t.pos, e.pos) < 9 * TILE) s += 18;
            if (s < bs) { bs = s; best = &e; }
        }
        if (best && (attacking || threat || bs < 30)) S.cmdAttack({r}, S.refOf(*best));
    }
    // artillery micro: keep long-range units from leading the charge when idle at rally
    for (auto r : army) {
        Entity* e = S.get(r);
        if (!e || e->weapon() < 0) continue;
        if (e->hp < e->maxHp * 0.22f && e->ut().kind == UK_VEH && !attacking && dist(e->pos, pl.basePos) > 12 * TILE && e->order != O_MOVE) S.cmdMove({r}, pl.basePos, false);
    }
}
