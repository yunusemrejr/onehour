#include "ai.h"
// Evaluation showed the learned wave gate loses to the plain heuristic (17-33 over 64 games) while the learned
// unit-choice model wins (31-21), so wave gating is opt-in: ONEHOUR_WAVEMODEL=1. ONEHOUR_ABLATE=u disables the unit model.
static int ablate() { static int v = -1; if (v < 0) { const char* e = getenv("ONEHOUR_ABLATE"); v = (e && e[0] == 'u') ? 2 : (getenv("ONEHOUR_WAVEMODEL") ? 0 : 1); } return v; }   // 1 = no wave model, 2 = no unit model
static bool aiDebug() { static int v = -1; if (v < 0) v = getenv("ONEHOUR_DEBUG") ? 1 : 0; return v == 1; }

AiManager g_ai;

// ---- combat estimate: the Lanchester square law over armour classes. A side is its total health, how that health splits across the
// armour classes, and the damage per second it deals to each class (weapon table: damage x armour multiplier x burst / cooldown, zero where
// the weapon cannot reach). A beats B by the ratio of (A's damage on B's armour mix x A's health) to (B's damage on A's mix x B's health).
struct Force {
    float hp = 0, hpBy[AR_COUNT] = {}, dps[AR_COUNT] = {};
    void add(const Entity& e, float weight) {
        int wi = e.weapon();
        if (wi < 0) return;
        const Weapon& w = WEAPONS[wi];
        float hp0 = e.hp, dmgMul = g_sim.players[e.owner].damageMul();
        if (e.isBuilding()) {
            if (!e.constructed || (g_sim.players[e.owner].lowPower() && e.bt().power < 0)) return;
            if (g_sim.players[e.owner].upg[UPG_RUGGED]) hp0 *= RUGGED_HP / RUGGED_ARMOR;   // plating halves the damage and adds half again to the health
        } else if (!e.isUnit() || e.ut().role != UR_COMBAT || e.fall > 0) return;
        hp += hp0 * weight; hpBy[e.armor()] += hp0 * weight;
        bool sniper = e.isUnit() && e.ut().sniper;
        for (int c = 0; c < AR_COUNT; c++) {
            if (c == AR_AIR ? !w.air : !w.ground) continue;
            if (sniper && c != AR_INF) continue;
            dps[c] += w.dmg * w.mult[c] * std::max(1, w.burst) / std::max(0.05f, w.cooldown) * (1.0f + 0.25f * w.splash) * dmgMul * weight;
        }
    }
    void addType(int type, float n, int owner) {   // n units of a type that does not exist yet
        const UnitType& u = UNITS[type];
        if (u.weapon < 0) return;
        const Weapon& w = WEAPONS[u.weapon];
        float dmgMul = g_sim.players[owner].damageMul();
        hp += u.hp * n; hpBy[u.armor] += u.hp * n;
        for (int c = 0; c < AR_COUNT; c++) {
            if (c == AR_AIR ? !w.air : !w.ground) continue;
            if (u.sniper && c != AR_INF) continue;
            dps[c] += w.dmg * w.mult[c] * std::max(1, w.burst) / std::max(0.05f, w.cooldown) * (1.0f + 0.25f * w.splash) * dmgMul * n;
        }
    }
    static float ratio(const Force& a, const Force& b) {   // > 1: a beats b
        if (a.hp <= 1.0f) return 0.0f;
        if (b.hp <= 1.0f) return 10.0f;
        float ad = 0, bd = 0;
        for (int c = 0; c < AR_COUNT; c++) { ad += b.hpBy[c] / b.hp * a.dps[c]; bd += a.hpBy[c] / a.hp * b.dps[c]; }
        if (bd <= 1e-3f) return 10.0f;
        if (ad <= 1e-3f) return 0.0f;
        return clampf((ad * a.hp) / (bd * b.hp), 0.02f, 10.0f);
    }
};

static float unitValue(const Entity& e) { return e.isUnit() ? UNITS[e.type].cost * (0.5f + 0.5f * e.hp / e.maxHp) : 0; }

// ONEHOUR_FEATOFF=bits: switch these pro features off for every computer army (headless balance runs: what does a feature do to the games?)
static int featOff() { static const int off = getenv("ONEHOUR_FEATOFF") ? atoi(getenv("ONEHOUR_FEATOFF")) : 0; return off; }

void AiManager::init(u64 seed) {
    for (int p = 0; p < MAX_PLAYERS; p++) {
        bool ub = ais[p].useBrain;
        ais[p] = AiPlayer();
        ais[p].useBrain = ub;
        if (g_sim.players[p].active && g_sim.players[p].isAI) { ais[p].useBrain = brainEnabled[p]; ais[p].smart = smartEnabled[p]; ais[p].pro = proEnabled[p] && smartEnabled[p]; ais[p].brain = brainOverride[p] ? brainOverride[p] : &g_brain; ais[p].feat = featMask[p] & ~featOff(); ais[p].init(p, seed + p * 7919); }
    }
}
void AiManager::update() {
    for (int p = 0; p < MAX_PLAYERS; p++) if (ais[p].player >= 0 && g_sim.players[p].alive) { ais[p].dodgeNukes(); ais[p].think(); }
}
// A finished match is the reward signal for the doctrine each commander opened with (a win counts 1, a loss a little for lasting long).
void AiManager::finish(int winnerTeam) {
    for (int p = 0; p < MAX_PLAYERS; p++) {
        AiPlayer& a = ais[p];
        if (a.player < 0 || !a.smart || !a.useBrain || !a.brain->learning || g_sim.players[p].difficulty < 1) continue;
        bool won = g_sim.players[p].team == winnerTeam;
        float reward = won ? 1.0f : 0.1f * std::min(1.0f, g_sim.time / 900.0f);
        a.brain->learnDoctrine(a.docCtx, a.doctrine, reward);
        if (aiDebug()) fprintf(stderr, "[ai%d] doctrine %s (context %d) %s -> reward %.2f (q now %.2f over %d games)\n", p, DOCTRINE_NAME[a.doctrine], a.docCtx, won ? "won" : "lost", reward, a.brain->ctxQ[a.docCtx][a.doctrine], a.brain->ctxN[a.docCtx][a.doctrine]);
    }
}

void AiPlayer::init(int p, u64 seed) {
    player = p; rng = Rng(seed);
    nextThink = 2.0f + p * 0.37f;
    if (smart) {
        // opening doctrine: learned from earlier matches (easy commanders just play it straight)
        int d = g_sim.players[p].difficulty;
        // who it faces: the nearest army of the other teams (a person or a computer, and its faction)
        int foe = -1; float fd = 1e18f;
        for (int q = 0; q < MAX_PLAYERS; q++) if (q != p && g_sim.players[q].active && g_sim.enemies(p, q)) { float dd = dist2(g_sim.players[q].basePos, g_sim.players[p].basePos); if (dd < fd) { fd = dd; foe = q; } }
        docCtx = foe >= 0 ? doctrineContext(!g_sim.players[foe].isAI, (int)g_sim.players[foe].faction) : 0;
        doctrine = (useBrain && d >= 1) ? brain->pickDoctrine(rng, docCtx) : (int)DOC_BALANCED;
        Style& st = style;
        st.incomes = d <= 0 ? 2 : (d == 1 ? 3 : 4);
        st.incomeFrom = d >= 2 ? 1.2f : (d == 1 ? 1.8f : 3.0f);
        switch (doctrine) {
        case DOC_RUSH:   st.first = 0.62f; st.thr = 0.70f; st.def = 0.5f; st.tech = 1.35f; st.nuke = 1.4f; st.army = 1.15f; st.incomes = std::max(2, st.incomes - 1); break;
        case DOC_TURTLE: st.first = 1.30f; st.thr = 1.30f; st.def = 1.8f; st.tech = 0.75f; st.nuke = 0.75f; st.army = 0.9f; break;
        case DOC_AIR:    st.air = 0.55f; st.airCap = 10; st.def = 1.1f; st.tech = 0.8f; break;
        case DOC_BOOM:   st.incomes = std::min(INCOME_MAX, st.incomes + 1); st.incomeFrom -= 0.4f; st.first = 1.15f; st.thr = 1.1f; st.army = 1.1f; break;
        default: break;
        }
    }
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

// A dozer free for a new structure: an idle one first, then one on the move, then one mending a finished structure or helping on a site on its own
// (it goes back to that site afterwards). A dozer aboard a bunker is not free: it cannot drive to a site from there.
Entity* AiPlayer::idleDozer(bool civic) {
    Entity* best = nullptr; int bestRank = 99;
    for (auto& e : g_sim.ents) {
        if (!e.alive || !e.isUnit() || e.owner != player || e.ut().role != UR_DOZER || e.carrier.valid() || civicDozerType(e.type) != civic) continue;
        const Entity* t = e.order == O_BUILD ? g_sim.get(e.targetEnt) : nullptr;
        int rank = e.order == O_IDLE ? 0 : e.order == O_MOVE ? 1 : (e.order == O_BUILD && t && t->constructed) ? 2 : (e.order == O_BUILD && e.autoTask) ? 3 : 99;
        if (rank < bestRank) { bestRank = rank; best = &e; if (rank == 0) break; }
    }
    return best;
}

// Recovery: the core of a base is a Command structure (the only source of dozers), a supply hub (income), power, a barracks and a factory. A commander that has
// lost most of it puts every free dozer on the next missing piece it can pay for, in that order of need; a hub comes first while a Command structure is
// out of reach of the treasury, since it is what brings money in.
bool AiPlayer::recoverBase(int dozers, bool hasHq) {
    Sim& S = g_sim;
    Player& pl = S.players[player];
    int base = firstBuildOf(pl.faction);
    bool placed = false;
    for (int k = 0; k < 3; k++) {
        if (!idleDozer()) break;
        int hqs = S.countRole(player, BR_HQ, false), hubs = S.countRole(player, BR_SUPPLY, false), pow = S.countRole(player, BR_POWER, false);
        int bars = S.countRole(player, BR_BARRACKS, false), facs = S.countRole(player, BR_FACTORY, false);
        int pending = 0;
        for (auto& e : S.ents) if (e.alive && e.isBuilding() && e.owner == player && !e.constructed && e.bt().power > 0) pending += e.bt().power;
        bool powerShort = pl.powerMade + pending < pl.powerUsed + 2;
        int want = -1;
        Vec2 at = pl.basePos;
        if (hqs == 0 && S.canAfford(player, BUILDS[base + BR_HQ].cost)) want = BR_HQ;
        else if (hubs == 0 && S.canAfford(player, BUILDS[base + BR_SUPPLY].cost)) {
            want = BR_SUPPLY;
            float bd = 1e18f;
            for (auto& e : S.ents) if (e.alive && e.kind == EK_RESOURCE && e.amount > 0) { float d = dist2(e.pos, pl.basePos); if (d < bd) { bd = d; at = e.pos; } }
        }
        else if (hqs == 0) break;   // bank for the Command structure
        else if (pow == 0 || powerShort) want = BR_POWER;
        else if (bars == 0) want = BR_BARRACKS;
        else if (facs == 0) want = BR_FACTORY;
        if (want < 0 || !S.canAfford(player, BUILDS[base + want].cost)) break;
        if (!tryBuild(base + want, at, want == BR_SUPPLY ? 26 : 22)) break;
        placed = true;
        if (aiDebug()) fprintf(stderr, "[ai%d t=%.0f] RECOVER: %s placed (dozers %d, hq %d)\n", player, S.time, BUILDS[base + want].name, dozers, (int)hasHq);
    }
    return placed;
}

bool AiPlayer::findSpot(int buildType, Vec2 preferNear, int maxRing, int& tx, int& ty) {
    const BuildType& b = BUILDS[buildType];
    if (b.role == BR_BUNKER) return findBunkerSpot(buildType, tx, ty);
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
            if (b.role != BR_WALL) for (int j = y - 1; j <= y + b.h && gapOk; j++) for (int i = x - 1; i <= x + b.w; i++) {
                if (!inMap(i, j)) { gapOk = false; break; }
                if (g_map.blocked[j * MAP_W + i] & 2) { gapOk = false; break; }
            }
            if (!gapOk) continue;
            Vec2 c = g_sim.buildingCenter(buildType, x, y);
            // never place bot structures or turrets right next to another player's bunker
            bool nearBunker = false;
            for (auto& o : g_sim.ents) {
                if (!o.alive || !o.isBuilding() || o.bt().role != BR_BUNKER) continue;
                float minD = (o.owner == player ? 8.0f : 16.0f) * TILE;
                if (dist(c, o.pos) < minD) { nearBunker = true; break; }
            }
            if (nearBunker) continue;
            float score = dist(c, preferNear) + ring * 6.0f;
            if (b.role == BR_TURRET || b.role == BR_AATURRET) {
                for (auto& o : g_sim.ents) if (o.alive && o.isBuilding() && (o.bt().role == BR_TURRET || o.bt().role == BR_AATURRET) && dist(c, o.pos) < 5.5f * TILE) score += 140.0f;
            }
            if (score < bestScore) { bestScore = score; bx = x; by = y; }
        }
        if (bx >= 0 && ring >= 6 && bestScore < ring * TILE) break;  // good enough, stop expanding
    }
    if (bx < 0) return false;
    tx = bx; ty = by; return true;
}

// Creative bunker placement for bots: instead of clumping near the player's bunkers or sitting in a predictable ring,
// pick tactical flanking corridors, tree-lined ridges, expansion overlooks or side approaches far from any existing bunker.
bool AiPlayer::findBunkerSpot(int buildType, int& tx, int& ty) {
    const BuildType& b = BUILDS[buildType];
    Vec2 base = g_sim.players[player].basePos;
    Vec2 mid(WORLD_W * 0.5f, WORLD_H * 0.5f);
    Vec2 toMid = (mid - base).norm();
    Vec2 side(-toMid.y, toMid.x);
    std::vector<Vec2> anchors;
    // 1. Flanking corridors left and right of the main axis at varied depths
    for (int s = -1; s <= 1; s += 2) {
        anchors.push_back(base + toMid * (14.0f * TILE) + side * (s * 11.0f * TILE));
        anchors.push_back(base + toMid * (22.0f * TILE) + side * (s * 16.0f * TILE));
        anchors.push_back(base - toMid * (7.0f * TILE) + side * (s * 12.0f * TILE));
    }
    // 2. Unclaimed resource piles / expansion overlooks on our half of the map
    for (auto& e : g_sim.ents) {
        if (e.alive && e.kind == EK_RESOURCE && dist(e.pos, base) > 12.0f * TILE && dist(e.pos, base) < 42.0f * TILE)
            anchors.push_back(e.pos + side * ((rng.range(0, 1) ? 1.0f : -1.0f) * 6.0f * TILE));
    }
    float bestScore = -1e18f; int bx = -1, by = -1;
    for (size_t ai = 0; ai < anchors.size(); ai++) {
        int ax = clampi(tileOf(anchors[ai].x), 4, MAP_W - 6), ay = clampi(tileOf(anchors[ai].y), 4, MAP_H - 6);
        for (int dy = -7; dy <= 7; dy += 2) for (int dx = -7; dx <= 7; dx += 2) {
            int x = ax + dx, y = ay + dy;
            if (!g_sim.canPlace(player, buildType, x, y)) continue;
            bool gapOk = true;
            for (int j = y - 1; j <= y + b.h && gapOk; j++) for (int i = x - 1; i <= x + b.w; i++)
                if (!inMap(i, j) || (g_map.blocked[j * MAP_W + i] & 2)) gapOk = false;
            if (!gapOk) continue;
            Vec2 c = g_sim.buildingCenter(buildType, x, y);
            bool clash = false;
            for (auto& o : g_sim.ents) {
                if (!o.alive || !o.isBuilding()) continue;
                // Strictly keep far away from any player's or bot's bunkers (22 tiles from other players' bunkers, 15 tiles from own)
                if (o.bt().role == BR_BUNKER && dist(c, o.pos) < (o.owner == player ? 15.0f : 22.0f) * TILE) { clash = true; break; }
                if (o.owner != player && dist(c, o.pos) < 14.0f * TILE) { clash = true; break; }
            }
            if (clash) continue;
            // Reward natural cover (trees/rocks/ridges nearby), flanking angle diversity, and distance from other bunkers
            int cover = 0;
            for (int j = y - 3; j <= y + b.h + 2; j++) for (int i = x - 3; i <= x + b.w + 2; i++)
                if (inMap(i, j) && (g_map.tile(i, j) == T_TREE || g_map.tile(i, j) == T_ROCK)) cover++;
            float sc = cover * 18.0f - dist(c, anchors[ai]) / TILE * 4.0f + rng.f(-25.0f, 25.0f);
            if (sc > bestScore) { bestScore = sc; bx = x; by = y; }
        }
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
        cands.push_back({pl.faction == F_CYBER ? (int)U_C_SNIPER : (int)U_K_SNIPER, 0.25f + fi * 2.2f});   // sniper: only worth it against infantry (vehicles and aircraft are the ones that can answer it)
    } else if (role == BR_FACTORY) {
        cands.push_back({base + 5, 1.4f + fv * 1.0f});                    // main tank
        cands.push_back({base + 6, 0.9f + fi * 2.0f + fa * 2.0f});        // anti-inf / AA vehicle
        cands.push_back({base + 7, (early ? 0.3f : 0.9f) + fv * 1.2f});  // artillery / railgun
        cands.push_back({base + 10, 1.0f + fv * 1.5f});                   // super-heavy (needs the Advanced Program)
        if (on(FEAT_NAVY)) { int jt = pl.faction == F_CYBER ? (int)U_C_JEEP : (int)U_K_JEEP; if (g_sim.countUnits(player, jt) < 3 + pl.difficulty * 2) cands.push_back({jt, 0.5f + fi * 0.9f + fa * 0.4f}); }   // a jeep rides soldiers to the fight, and they shoot from it
    } else if (role == BR_AIRFIELD) {
        cands.push_back({base + 8, smart ? 1.4f + (doctrine == DOC_AIR ? 0.9f : 0.0f) : 1.0f});   // bomber drone / gunship (a smart commander knows what they do to structures)
        cands.push_back({base + 11, 0.8f + fa * 2.2f + fv * 0.9f});       // supersonic jet (needs the tech structure): best against aircraft and vehicles
        { int sb = pl.faction == F_CYBER ? (int)U_C_SBOMBER : (int)U_K_SBOMBER; cands.push_back({sb, 0.45f + (doctrine == DOC_AIR ? 0.35f : 0.0f)}); }   // Supersonic Bomber: one nuclear bomb a sortie, and slow prey after the drop
        if (pl.faction == F_CYBER) cands.push_back({(int)U_C_HELI, 0.8f + fv * 1.2f + fa * 1.0f});   // attack helicopter: all-round, never rearms
    }
    // weight by how many we already have (diminishing returns), availability, and predicted efficiency:
    // the brain's per-type regression (value destroyed per credit given the enemy mix) replaces the old
    // running average when learning is on
    float x[UNIT_F]; Brain::unitFeatures(fi, fv, fa, x);
    float avgEff = 0; int nEff = 0;
    for (auto& c : cands) {
        if (useBrain && ablate() != 2) { avgEff += brain->unitEff(c.type, x); nEff++; }
        else if (pl.spentOn[c.type] > 0) { avgEff += pl.valueDealt[c.type] / pl.spentOn[c.type]; nEff++; }
    }
    avgEff = nEff ? avgEff / nEff : 0;
    // the estimate picks the counter: what a few thousand credits of each candidate would add to the army's strength against the enemy
    // force actually seen (its armour mix decides whose damage counts, its weapons whose health counts)
    float estGain[16] = {}, estMax = 0;
    if (on(FEAT_COMP) && cands.size() <= 16) {
        Force A, B;
        for (auto& e : g_sim.ents) {
            if (!e.alive || e.kind == EK_RESOURCE || e.owner < 0 || (!e.isUnit() && !e.isBuilding())) continue;
            if (e.owner == player) { if (e.isUnit() && e.ut().role == UR_COMBAT) A.add(e, 1.0f); }
            else if (g_sim.enemies(player, e.owner) && (g_sim.explored(player, clampi(tileOf(e.pos.x), 0, MAP_W - 1), clampi(tileOf(e.pos.y), 0, MAP_H - 1)) && g_sim.visibleTo(e, player))) {
                if (e.isBuilding() && e.weapon() < 0) { float h = e.hp * 0.15f; B.hp += h; B.hpBy[AR_STRUCT] += h; }   // the objective: a little of every structure
                else B.add(e, e.isBuilding() ? 0.6f : 1.0f);
            }
        }
        if (B.hp > 1.0f) {
            float base0 = std::log(Force::ratio(A, B));
            for (size_t k = 0; k < cands.size(); k++) {
                Force A2 = A; A2.addType(cands[k].type, 3000.0f / UNITS[cands[k].type].cost, player);
                estGain[k] = std::max(0.0f, std::log(Force::ratio(A2, B)) - base0);
                estMax = std::max(estMax, estGain[k]);
            }
        }
    }
    float best = -1; int pick = -1;
    for (size_t k = 0; k < cands.size(); k++) {
        auto& c = cands[k];
        if (!g_sim.unitAvailable(player, c.type)) continue;
        int have = g_sim.countUnits(player, c.type);
        float w = c.w / (1.0f + have * 0.25f) * rng.f(0.85f, 1.15f);
        if (estMax > 0.02f) w *= 0.4f + 1.2f * estGain[k] / estMax;
        if (useBrain && ablate() != 2) {
            if (avgEff > 0) w *= clampf(0.65f + 0.7f * (brain->unitEff(c.type, x) / avgEff), 0.5f, 1.8f);
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

// Some enemy still stands and every living thing of it is a hidden kind (a sniper, a spy drone, a bunker, or a unit aboard a bunker or transport)
static bool enemyOnlyHidden(int player) {
    bool any = false;
    for (auto& e : g_sim.ents) {
        if (!e.alive || e.kind == EK_RESOURCE || e.owner < 0 || !g_sim.enemies(player, e.owner)) continue;
        if (!Sim::stealthOf(e) && !e.carrier.valid()) return false;
        any = true;
    }
    return any;
}

Entity* AiPlayer::pickAttackTarget() {
    // Prefer economy and production of the main enemy; fall back to anything known
    Entity* best = nullptr; float bs = 1e18f;
    Entity* lastResort = nullptr; float lastResortScore = 1e18f;
    Vec2 base = g_sim.players[player].basePos;
    for (auto& e : g_sim.ents) {
        if (!e.alive || e.kind == EK_RESOURCE || !g_sim.enemies(player, e.owner)) continue;
        if (!(g_sim.explored(player, clampi(tileOf(e.pos.x), 0, MAP_W - 1), clampi(tileOf(e.pos.y), 0, MAP_H - 1)) && g_sim.visibleTo(e, player))) continue;
        float s = dist(e.pos, hasFront ? humanFront : base) / TILE;   // (with a teammate's push under way, near their front rather than near home)
        if (e.isBuilding()) {
            BuildRole r = e.bt().role;
            if (smart && r == BR_NUKE) s -= 18; else if (smart && (r == BR_INCOME || r == BR_CIVIC)) s -= 12;   // (a town keeps paying for its army's rebuilding: raze it like an income structure)
            if (r == BR_SUPPLY) s -= 14; else if (r == BR_POWER) s -= 12; else if (r == BR_FACTORY || r == BR_BARRACKS || r == BR_AIRFIELD) s -= 8;
            else if (r == BR_TURRET || r == BR_AATURRET) s += 4; else if (r == BR_HQ) s += 6;
            // defended targets cost more: count turrets around it
            for (auto& t : g_sim.ents) if (t.alive && t.isBuilding() && t.owner == e.owner && t.constructed && (t.bt().role == BR_TURRET || t.bt().role == BR_AATURRET) && dist(t.pos, e.pos) < 8 * TILE) s += 3;
            if (g_sim.refOf(e) == failedTarget && g_sim.time - failedAt < 240) s += 18;
            s += enemyStrengthNear(e.pos, 9) / 500.0f;   // prefer the softest worthwhile target
            if (smart) for (auto& f : g_sim.fallouts) if (f.t < f.life - 15.0f && dist(e.pos, f.pos) < f.r) s += 30;   // radiation: come back when it has faded
        } else {
            if (e.ut().role == UR_HARVESTER) s -= 10;
            else {   // armies are met on the way, not hunted - unless nothing else of the enemy is left to find (a stranded fleet, a last tank, a last dozer): then they are the objective
                float lr = s + (e.ut().role == UR_COMBAT ? 40.0f : 60.0f);   // (fighting units first)
                if (!e.ut().temp && !civicDozerType(e.type) && (!lastResort || lr < lastResortScore)) { lastResort = &e; lastResortScore = lr; }
                continue;
            }
        }
        if (s < bs) { bs = s; best = &e; }
    }
    return best ? best : lastResort;
}

// An assault by 'attackers' on the spot 'at': everything that will meet them there. Armed structures count within 'radiusTiles' of the spot;
// the defender's fighters count in full near it and less the farther they stand (they come running), or all of them when the whole
// enemy army is what matters. Defenders also fight at home, repaired and reinforced: they get a small edge.
float AiPlayer::assaultRatio(const std::vector<Ref>& attackers, Vec2 at, float radiusTiles, bool wholeEnemy) {
    Sim& S = g_sim;
    Force A, B;
    for (auto r : attackers) { Entity* e = S.get(r); if (e) A.add(*e, 1.0f); }
    for (auto& e : S.ents) {
        if (!e.alive || e.kind == EK_RESOURCE || !S.enemies(player, e.owner)) continue;
        if (!(S.explored(player, clampi(tileOf(e.pos.x), 0, MAP_W - 1), clampi(tileOf(e.pos.y), 0, MAP_H - 1)) && S.visibleTo(e, player))) continue;
        float d = dist(e.pos, at) / TILE;
        if (e.isBuilding()) { if (d <= radiusTiles) B.add(e, 1.0f); }
        else if (e.isUnit()) B.add(e, wholeEnemy ? 1.0f : (d <= radiusTiles ? 1.0f : (d <= radiusTiles * 2.5f ? 0.65f : 0.3f)));
    }
    return Force::ratio(A, B) / 1.08f;
}

// Feed the outcome of a finished wave back into the model: did it destroy at least as much as it lost?
void AiPlayer::endWave(float remainingValue) {
    if (!waveHasSample) return;
    waveHasSample = false;
    Player& pl = g_sim.players[player];
    float dealt = -waveDealt0; for (int u = 0; u < U_COUNT; u++) dealt += pl.valueDealt[u];
    float lost = std::max(0.0f, waveValue - remainingValue);
    bool success = dealt >= lost * 0.9f && dealt > 300;
    if (useBrain) brain->learnWave(waveX, success);
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

// An enemy nuke takes seven seconds to arrive and its circle is public. A smart commander pulls everything it can out of the circle;
// the difficulty sets how long it takes to notice the launch (easy commanders never do).
void AiPlayer::dodgeNukes() {
    Sim& S = g_sim;
    if (!smart || S.nukes.empty()) return;
    Player& pl = S.players[player];
    if (pl.difficulty < 1 || (S.tick & 3) != (u32)(player & 3)) return;
    float notice = pl.difficulty >= 3 ? 0.3f : (pl.difficulty == 2 ? 2.0f : 4.5f);   // seconds after the launch before it reacts: slow soldiers on Normal are mostly caught
    for (auto& n : S.nukes) {
        bool hurts = S.nukeHurts(n, player);   // (a human ally's nuke hurts us too: get out of the way, never hit back)
        if (!hurts || n.t < notice || n.t > Sim::NUKE_FLIGHT - 0.5f) continue;
        float safe = Sim::nukeRadius(n) + 2.0f * TILE;
        for (auto& e : S.ents) {
            if (!e.alive || e.owner != player || !e.isUnit() || e.isAir()) continue;   // aircraft cannot outrun a warhead (see Sim::dodgeDanger)
            if (dist(e.pos, n.pos) > safe) continue;
            if (e.order == O_MOVE && dist(e.target, n.pos) > safe) continue;   // already on its way out
            Vec2 away = e.pos - n.pos; float l = away.len();
            if (l < 4) { away = pl.basePos - n.pos; l = away.len(); }
            away = away * (1.0f / std::max(l, 1.0f));
            Vec2 dest = n.pos + away * (safe + 3.0f * TILE);
            dest = Vec2(clampf(dest.x, TILE, WORLD_W - TILE), clampf(dest.y, TILE, WORLD_H - TILE));
            S.cmdMove({S.refOf(e)}, dest, false);
            dodges++;
        }
    }
}

void AiPlayer::think() {
    Sim& S = g_sim;
    Player& pl = S.players[player];
    if (S.time < nextThink) return;
    float cadence = pl.difficulty >= 3 ? 0.35f : (pl.difficulty == 2 ? 0.5f : (pl.difficulty == 1 ? 0.8f : 1.3f));
    nextThink = S.time + cadence;
    int base = firstBuildOf(pl.faction);
    int ubase = firstUnitOf(pl.faction);
    float minutes = S.time / 60.0f;
    // a commander with money in the bank does not wait for the clock: the timed gates for tech, airfields, upgrades and nukes run on 'effective' minutes
    float tm = minutes;
    const float ease = pl.difficulty >= 2 ? 1.0f : 0.5f;   // Normal uses the spending engine at half strength: the full engine is Hard and Brutal's
    if (on(FEAT_MACRO) && pl.difficulty >= 1) tm += ease * clampf((pl.money - 4000.0f) / 2500.0f, 0.0f, 6.0f);
    if (on(FEAT_MACRO) && S.time - incomeT >= 10.0f) {   // credits per second coming in (hauled supplies and income structures)
        float inc = (float)(pl.harvested + pl.mined), r = (inc - incomeAt) / (S.time - incomeT);
        incomeRate = incomeRate <= 0 ? r : 0.6f * incomeRate + 0.4f * r;
        incomeAt = inc; incomeT = S.time;
    }

    // ---------- survey
    int dozers = 0, civDozers = 0, harvesters = 0, armyCount = 0, shipsOrJeeps = 0; float armyValue = 0, fleetValue = 0;
    std::vector<Ref> army, idleArmy, aircraft, haulers, medics, spies, scouts, fleet, jeeps, decon;
    Entity* hq = nullptr;
    std::vector<Entity*> supplyHubs, barracks, factories, airfields, damaged, yards;
    int turrets = 0, aaTurrets = 0, powerPlants = 0, techs = 0, incomes = 0, ramps = 0, antiNukes = 0, bunkers = 0, civics = 0, silos = 0, walls = 0;
    for (auto& e : S.ents) {
        if (!e.alive || e.owner != player) continue;
        if (e.isUnit()) {
            const UnitType& ut = e.ut();
            if (ut.role == UR_DOZER) { if (civicDozerType(e.type)) civDozers++; else dozers++; }   // (a civilian dozer raises only the town: it is counted apart)
            else if (ut.role == UR_HARVESTER) { harvesters++; haulers.push_back(S.refOf(e)); }
            else if (ut.role == UR_SPY) spies.push_back(S.refOf(e));         // spies capture structures: never part of a wave
            else if (ut.role == UR_SCOUT) scouts.push_back(S.refOf(e));      // spy drones see what is hidden and coordinate as a swarm
            else if (ut.role == UR_TRANSPORT) {}                             // (the commander has no use for cargo choppers)
            else if (ut.role == UR_DECON) decon.push_back(S.refOf(e));       // anti-radiation trucks scrub fallout off the base: never part of a wave
            else if (ut.kind == UK_AIR) { if (!ut.temp) aircraft.push_back(S.refOf(e)); }   // (a Hotline Strike fighter is on loan: it flies its own run)
            else if (smart && ut.role == UR_HEALER) medics.push_back(S.refOf(e));   // medics trail the army instead of leading the charge
            else if (ut.naval) { fleet.push_back(S.refOf(e)); fleetValue += unitValue(e); shipsOrJeeps++; }   // (the fleet is handled apart from the land army)
            else {
                army.push_back(S.refOf(e)); armyCount++; armyValue += unitValue(e);
                if (e.order == O_IDLE && !e.carrier.valid()) idleArmy.push_back(S.refOf(e));
                if (ut.cargoCap > 0 && ut.kind == UK_VEH) { jeeps.push_back(S.refOf(e)); shipsOrJeeps++; }
            }
        } else {
            if (e.constructed && e.hp < e.maxHp * 0.6f && !(smart && S.inFallout(e.pos))) damaged.push_back(&e);
            switch (e.bt().role) {
            case BR_HQ: hq = &e; break;
            case BR_SUPPLY: supplyHubs.push_back(&e); break;
            case BR_BARRACKS: barracks.push_back(&e); break;
            case BR_FACTORY: factories.push_back(&e); break;
            case BR_AIRFIELD: airfields.push_back(&e); break;
            case BR_TURRET: case BR_AATURRET: turrets++; if (e.bt().role == BR_AATURRET) aaTurrets++; break;
            case BR_POWER: powerPlants++; break;
            case BR_TECH: techs++; break;
            case BR_INCOME: incomes++; break;
            case BR_NUKE: ramps++; break;
            case BR_NAVAL: yards.push_back(&e); break;
            case BR_ANTINUKE: antiNukes++; break;
            case BR_BUNKER: bunkers++; break;
            case BR_CIVIC: civics++; break;
            case BR_UNUKE: silos++; break;
            case BR_WALL: walls++; break;
            case BR_STRONGHOLD: break;   // neutral map landmarks are not the army's production
            case BR_NONE: break;
            }
        }
    }
    // enemy intel (only what we've explored)
    int enemyInf = 0, enemyVeh = 0, enemyAir = 0, enemyAirfields = 0, enemyShips = 0, enemyRamps = 0; float enemyArmyValue = 0;
    Entity* threat = nullptr; float threatDist = 1e18f; int threatCount = 0;
    std::vector<Entity*> enemyUnits;
    std::vector<Entity*> ownBuildings;
    for (auto& e : S.ents) if (e.alive && e.isBuilding() && e.owner == player) ownBuildings.push_back(&e);
    for (auto& e : S.ents) {
        if (!e.alive || e.kind == EK_RESOURCE || !S.enemies(player, e.owner)) continue;
        int tx = clampi(tileOf(e.pos.x), 0, MAP_W - 1), ty = clampi(tileOf(e.pos.y), 0, MAP_H - 1);
        if (!S.explored(player, tx, ty) || !S.visibleTo(e, player)) continue;   // (stealthy enemies stay unknown until a spy drone finds them)
        if (e.isUnit()) {
            const UnitType& ut = e.ut();
            if (ut.role == UR_COMBAT) {
                if (ut.kind == UK_INF) enemyInf++; else if (ut.kind == UK_AIR) enemyAir++; else { enemyVeh++; if (ut.naval) enemyShips++; }
                enemyArmyValue += unitValue(e);
                enemyUnits.push_back(&e);
                // threats: enemy combat units close to any of our structures
                float d = 1e18f;
                for (auto* b : ownBuildings) d = std::min(d, S.distToEntity(e.pos, *b));
                if (d < 12 * TILE) { threatCount++; float db = dist(e.pos, pl.basePos); if (db < threatDist) { threatDist = db; threat = &e; } }
            }
        } else if (e.isBuilding() && e.bt().role == BR_AIRFIELD) enemyAirfields++;
        else if (e.isBuilding() && e.bt().role == BR_NUKE) enemyRamps++;
    }
    for (auto& n : S.nukes) if (n.id > seenNukeIds) { seenNukeIds = n.id; if (S.enemies(player, n.owner)) nukesSeen++; }   // warheads launched at anybody on our side's account count: a commander that has seen a nuke builds defences
    if (!threat) {
        // also react to harvesters being attacked near home
        for (auto& e : S.ents) {
            if (!e.alive || e.owner != player || !e.isUnit() || e.ut().role != UR_HARVESTER || S.time - e.lastDamaged > 3.0f) continue;
            Entity* a = S.get(e.attacker);
            if (a && a->isUnit() && S.enemies(player, a->owner) && dist(a->pos, pl.basePos) < 24 * TILE) { threat = a; threatCount = 1; break; }
        }
    }

    // allies: enemy fighters closing on a teammate's structures call for help (a computer ally defends the human's base too)
    Entity* allyThreat = nullptr;
    if (smart && !threat && pl.difficulty >= 1 && S.time > 120.0f) {
        std::vector<Entity*> allyBuildings;
        for (auto& e : S.ents) if (e.alive && e.isBuilding() && e.owner >= 0 && e.owner != player && S.players[e.owner].team == pl.team) allyBuildings.push_back(&e);
        float best = 1e18f;
        if (!allyBuildings.empty()) for (auto* u : enemyUnits) {
            float d = 1e18f; for (auto* b : allyBuildings) d = std::min(d, S.distToEntity(u->pos, *b));
            if (d > 10 * TILE) continue;
            float db = dist(u->pos, pl.basePos);
            if (db < 70 * TILE && db < best) { best = db; allyThreat = u; }
        }
    }

    // a human teammate's push: where their fighters are out in the field and what they are up against
    hasFront = false;
    Entity* frontTarget = nullptr; int frontHuman = -1;
    if (on(FEAT_ALLY) && pl.difficulty >= 1) {
        Vec2 sum; int n = 0;
        for (auto& e : S.ents) {
            if (!e.alive || !e.isUnit() || e.isAir() || e.owner < 0 || e.owner == player || S.players[e.owner].isAI || S.players[e.owner].team != pl.team || e.ut().role != UR_COMBAT || e.weapon() < 0) continue;
            sum += e.pos; n++; frontHuman = e.owner;
        }
        if (n >= 5) {
            Vec2 c = sum * (1.0f / n);
            if (dist(c, S.players[frontHuman].basePos) > 16 * TILE) {   // out in the field, not parked at home
                float bd = 28 * TILE;
                for (auto& e : S.ents) {
                    if (!e.alive || !e.isBuilding() || !S.enemies(player, e.owner) || !(S.explored(player, clampi(tileOf(e.pos.x), 0, MAP_W - 1), clampi(tileOf(e.pos.y), 0, MAP_H - 1)) && S.visibleTo(e, player))) continue;
                    float d = dist(e.pos, c); if (d < bd) { bd = d; frontTarget = &e; }
                }
                if (frontTarget) { humanFront = c; hasFront = true; }
            }
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
                if (ds >= 400 && useBrain) brain->learnUnit(u, x, dd / ds);
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
    if (smart) for (auto& f : S.fallouts) if (f.t < f.life - 10.0f && dist(rally, f.pos) < f.r + 2 * TILE) rally = g_map.nearestFree(pl.basePos + enemyDir * (3 * TILE), 6);   // never muster inside radiation
    // radiation: whatever is not part of an attack wave walks out of a fallout zone and stays out of it
    if (smart && pl.difficulty >= 1 && !S.fallouts.empty()) {
        for (auto& f : S.fallouts) {
            if (f.t > f.life - 8.0f) continue;
            for (auto& e : S.ents) {
                if (!e.alive || e.owner != player || !e.isUnit() || e.isAir() || e.ut().role == UR_DECON || (e.ut().role == UR_HARVESTER && e.order == O_RETURN)) continue;   // (a decon truck is in the cloud on purpose)
                bool shielded = false;
                for (Ref tr : decon) { const Entity* t = S.get(tr); if (t && !t->carrier.valid() && t->disabledUntil <= S.time && dist(e.pos, t->pos) <= RADTRUCK_RADIUS * TILE) { shielded = true; break; } }
                if (shielded) continue;   // the decon truck's field is scrubbing this air: stay put and be safe
                if (dist(e.pos, f.pos) > f.r) continue;
                if (attacking && std::find(wave.begin(), wave.end(), S.refOf(e)) != wave.end()) continue;
                if (e.order == O_MOVE && dist(e.target, f.pos) > f.r + TILE) continue;
                Vec2 away = e.pos - f.pos; float l = away.len();
                if (l < 4) { away = pl.basePos - f.pos; l = away.len(); }
                S.cmdMove({S.refOf(e)}, f.pos + away * (1.0f / std::max(l, 1.0f)) * (f.r + 2.5f * TILE), false);
            }
        }
    }

    // decontamination: an idle anti-radiation truck heads for the nearest fallout within 70 tiles and sits in it until it is scrubbed away
    if (smart && !decon.empty() && !S.fallouts.empty()) {
        for (Ref dr : decon) {
            Entity* t = S.get(dr);
            if (!t || t->order != O_IDLE || t->carrier.valid()) continue;
            const Sim::Fallout* best = nullptr; float bd = 1e18f;
            for (auto& f : S.fallouts) {
                if (f.power <= 0.03f) continue;
                if (dist(t->pos, f.pos) <= f.r + RADTRUCK_RADIUS * TILE) { best = nullptr; bd = -1.0f; break; }   // already scrubbing this one
                float d = dist(t->pos, f.pos);
                if (d < 70.0f * TILE && d < bd) { bd = d; best = &f; }
            }
            if (best) S.cmdMove({dr}, best->pos, false);
        }
    }

    // ---------- economy
    int hubs = (int)supplyHubs.size();
    int wantHarv = pl.brutal() ? std::min(8, hubs * 4) : std::min(6, hubs * 3);
    // a hauler pays for itself in about half a minute and the piles are vast: a smart commander keeps a bigger fleet on the road
    if (on(FEAT_ECON) && pl.difficulty >= 1) wantHarv = pl.difficulty == 1 ? std::min(8, hubs * 4) : std::min(pl.brutal() ? 14 : 12, hubs * (pl.brutal() ? 7 : 6));
    int queuedHarv = 0;
    for (auto* h : supplyHubs) for (int t : h->queue) if (UNITS[t].role == UR_HARVESTER) queuedHarv++;
    if (harvesters + queuedHarv < wantHarv) {
        for (auto* h : supplyHubs) if (h->constructed && h->queue.empty()) { S.cmdTrain(S.refOf(*h), ubase + 1); break; }
    }
    int wantDozers = pl.difficulty >= 3 ? 4 : (pl.difficulty == 2 ? 3 : 2);
    if (on(FEAT_MACRO) && pl.difficulty >= 1) wantDozers += (pl.money > 6000 ? 1 : 0) + (pl.money > 14000 && pl.difficulty >= 2 ? 1 : 0);   // a rich army builds on several fronts at once
    int queuedDozer = 0;
    if (hq) for (int t : hq->queue) if (UNITS[t].role == UR_DOZER && !civicDozerType(t)) queuedDozer++;
    // ---------- recovery: most of the base is gone (a Command structure and at least one more core piece, or two pieces): rebuilding comes before
    // everything else as soon as there is a dozer and money for it (a relief drop, the bunker's trickle, what haulers still bring in)
    {
        int missing = (hq ? 0 : 2) + (hubs == 0 ? 1 : 0) + (powerPlants == 0 ? 1 : 0) + (barracks.empty() ? 1 : 0) + (factories.empty() ? 1 : 0);
        bool was = recovering;
        recovering = on(FEAT_RECOVER) && S.time > 150.0f && missing >= 2;
        if (aiDebug() && recovering != was) fprintf(stderr, "[ai%d t=%.0f] recovery mode %s (missing %d, dozers %d, $%d)\n", player, S.time, recovering ? "ON" : "off", missing, dozers, pl.money);
    }
    if (on(FEAT_RECOVER) && dozers > 0 && !idleDozer()) {   // every dozer sits in a bunker (or is busy): a stowed one is let out, it is no use underground
        bool outside = false;
        for (auto& e : S.ents) if (e.alive && e.isUnit() && e.owner == player && e.ut().role == UR_DOZER && !civicDozerType(e.type) && !e.carrier.valid()) outside = true;
        if (!outside) for (auto& e : S.ents) if (e.alive && e.isBuilding() && e.owner == player && e.bt().role == BR_BUNKER && !e.unloading && S.garrisonDozers(e) > 0) { S.cmdUnload(S.refOf(e), false, e.pos); break; }
    }
    if (recovering) wantDozers = std::max(wantDozers, 3);
    if (hq && hq->constructed && dozers + queuedDozer < wantDozers && hq->queue.size() < 2 && (dozers == 0 || pl.money > 2200 || (recovering && pl.money >= UNITS[ubase].cost))) S.cmdTrain(S.refOf(*hq), ubase + 0);

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

    // ---------- construction (one decision per think, with a reserve for economy; in recovery every free dozer gets the next core piece first)
    if (recovering) recoverBase(dozers, hq != nullptr);
    Entity* dz = idleDozer();
    if (dz) {
        bool built = false;
        // 1. power
        if (pl.powerMade < pl.powerUsed) { managePower(); built = idleDozer() != dz; }
        // 2. a lost Command Core is rebuilt as soon as the treasury allows: it is the only source of dozers
        if (!built && !hq && S.countRole(player, BR_HQ, false) == 0) built = tryBuild(base + BR_HQ, pl.basePos, 14);
        // 3. supply
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
        // income structures pay for themselves in well under a minute: a smart commander raises them as soon as it has power and a first army
        if (!built && smart && incomes < style.incomes && tm > style.incomeFrom && powerPlants > 0 && !barracks.empty() && !factories.empty()) built = tryBuild(base + BR_INCOME, pl.basePos - enemyDir * 80);
        // defenses scale with time and difficulty
        int wantTurrets = std::min(pl.difficulty >= 3 ? 9 : (pl.difficulty == 2 ? 5 : 3), (int)(minutes / (pl.difficulty >= 3 ? 1.5f : (pl.difficulty == 2 ? 2.0f : 3.0f))));
        if (smart) wantTurrets = (int)(wantTurrets * style.def + 0.5f);
        // anti-air follows the enemy's air power (bombers are no joke): batteries go beside the structures most worth protecting
        if (!built && smart && minutes > 3.0f && pl.difficulty >= 1) {
            int airThreat = enemyAir + 2 * enemyAirfields;
            int wantAA = std::min(7, (airThreat + 1) / 2 + (doctrine == DOC_AIR ? 1 : 0));
            if (aaTurrets < wantAA) {
                Entity* key = nullptr; int bestCover = 1 << 30;
                for (auto* b : ownBuildings) {
                    BuildRole r = b->bt().role;
                    if (!b->constructed || !(r == BR_FACTORY || r == BR_AIRFIELD || r == BR_HQ || r == BR_TECH || r == BR_NUKE || r == BR_SUPPLY || r == BR_INCOME || r == BR_CIVIC || r == BR_UNUKE)) continue;
                    int cover = 0; for (auto* t : ownBuildings) if (t->bt().role == BR_AATURRET && dist(t->pos, b->pos) < 9 * TILE) cover++;
                    if (cover < bestCover || (cover == bestCover && key && dist(b->pos, pl.basePos) < dist(key->pos, pl.basePos))) { bestCover = cover; key = b; }
                }
                if (key && bestCover < 2) { built = tryBuild(base + BR_AATURRET, key->pos, 10); if (built) aaTurrets++; }
            }
        }
        if (!built && turrets < wantTurrets && minutes > 2.5f) {
            int kind = (enemyAir > 0 || nextTurretKind % 2 == 1) ? BR_AATURRET : BR_TURRET;
            float baseAng = std::atan2(enemyDir.y, enemyDir.x) + (nextTurretKind * 2.39996f - 1.2f) * 0.55f;
            float rad = (6.5f + (nextTurretKind % 4) * 2.2f) * TILE;
            Vec2 spot = pl.basePos + Vec2(std::cos(baseAng), std::sin(baseAng)) * rad;
            built = tryBuild(base + kind, spot, 14);
            if (built) nextTurretKind++;
        }
        if (!built && techs == 0 && tm > (pl.difficulty >= 2 ? 4.0f : 6.0f) * style.tech && pl.money > 2600) built = tryBuild(base + BR_TECH, pl.basePos);
        if (!built && airfields.empty() && tm > (pl.difficulty >= 2 ? 6.0f : 9.0f) * style.air && pl.money > 2000) built = tryBuild(base + BR_AIRFIELD, pl.basePos);
        if (!built && factories.size() < 2 && tm > (pl.brutal() ? 4 : 7) && pl.money > (pl.brutal() ? 3000 : 4500)) built = tryBuild(base + BR_FACTORY, pl.basePos);
        // a smart commander with money piling up spends it on more production and a second airfield for the air doctrine
        if (!built && smart && !on(FEAT_MACRO) && !threat && pl.money > (pl.brutal() ? 4500 : 6500) && minutes > 5) {
            if (factories.size() < (pl.difficulty >= 3 ? 4u : (pl.difficulty == 2 ? 3u : 2u))) built = tryBuild(base + BR_FACTORY, pl.basePos);
            else if (barracks.size() < 3) built = tryBuild(base + BR_BARRACKS, pl.basePos);
            else if (doctrine == DOC_AIR && airfields.size() < 2 && techs > 0) built = tryBuild(base + BR_AIRFIELD, pl.basePos);
        }
        // production capacity follows the income: a bank that keeps growing means too few factories and barracks to turn credits into fighters
        // (and it is not put off by a fight at the door: the new buildings go up while the defenders hold)
        if (!built && on(FEAT_MACRO) && pl.difficulty >= 1 && minutes > 1.2f && powerPlants > 0) {
            float prodRate = 0;
            for (auto* f : factories) prodRate += f->constructed ? 75.0f : 40.0f;
            for (auto* b : barracks) prodRate += b->constructed ? 43.0f : 20.0f;
            prodRate *= pl.buildMul();
            static const unsigned facCap[4] = { 2, 3, 5, 7 }, barCap[4] = { 2, 3, 4, 5 };
            bool starved = pl.money > 7000 || (pl.money > 3000 && prodRate < incomeRate * 0.9f);
            if (starved) {
                bool facFirst = factories.size() * 2 <= barracks.size() * 3 + 2;
                if (factories.size() < facCap[pl.difficulty] && (facFirst || barracks.size() >= barCap[pl.difficulty])) built = tryBuild(base + BR_FACTORY, pl.basePos - enemyDir * 40);
                if (!built && barracks.size() < barCap[pl.difficulty]) built = tryBuild(base + BR_BARRACKS, pl.basePos - enemyDir * 40);
                if (!built && doctrine == DOC_AIR && airfields.size() < 2 && techs > 0 && pl.money > 5000) built = tryBuild(base + BR_AIRFIELD, pl.basePos);
            }
        }
        // expansion: a second hub near a farther pile once the closest piles thin out
        if (!built && hubs < 2 && tm > 8 && pl.money > 3000) {
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
        if (!built && incomes < (pl.difficulty >= 3 ? 4 : (pl.difficulty == 2 ? 3 : 2)) && tm > (pl.difficulty >= 2 ? 3.0f : 5.0f) && pl.money > 2200 && powerPlants > 0) built = tryBuild(base + BR_INCOME, pl.basePos - enemyDir * 80);
        if (!built && techs > 0 && ramps < (pl.difficulty >= 3 ? 2 : 1) && pl.difficulty >= 1 && tm > (pl.difficulty >= 3 ? 7.0f : (pl.difficulty == 2 ? 9.0f : 13.0f)) * style.nuke && pl.money > 5300) built = tryBuild(base + BR_NUKE, pl.basePos - enemyDir * 100);
        if (!built && barracks.size() < 2 && tm > 10 && pl.money > 3500) built = tryBuild(base + BR_BARRACKS, pl.basePos);
        // creative underground bunkers: placed in flanking corridors, ridges and expansion overlooks far away from the player's bunkers
        int bunkerType = pl.faction == F_CYBER ? B_C_BUNKER : B_K_BUNKER;
        int wantBunkers = pl.difficulty >= 3 ? 2 : (pl.difficulty >= 2 ? 1 : 0);
        if (!built && smart && bunkers < wantBunkers && !barracks.empty() && !threat && tm > (pl.difficulty >= 3 ? 9.5f : 12.5f) && pl.money > BUILDS[bunkerType].cost + 2500 && S.time >= nextBunkerTry) {
            nextBunkerTry = S.time + 25.0f;
            int tx, ty;
            if (S.buildAvailable(player, bunkerType) && findBunkerSpot(bunkerType, tx, ty)) built = S.cmdBuild(S.refOf(*dz), bunkerType, tx, ty);
        }
        // anti-nuke defences: once the enemy has a nuke ramp (or has launched a warhead) a commander with a tech structure rings its base centre with them. Every one has its
        // own chance at a warhead, so a few overlapping ones stop most of them; the more warheads have been seen, the more of them it builds
        if (!built && on(FEAT_AEGIS) && pl.difficulty >= 1 && techs > 0 && powerPlants > 0 && !threat) {
            int wantAN = 0;
            if (enemyRamps > 0 || nukesSeen > 0 || tm > (pl.difficulty >= 3 ? 10.0f : 15.0f))
                wantAN = std::min(ANTINUKE_MAX, (pl.difficulty >= 3 ? 3 : (pl.difficulty == 2 ? 2 : 1)) + (nukesSeen > 0 ? 1 : 0) + std::min(2, nukesSeen / 2) + (enemyRamps > 1 ? 1 : 0) + (pl.money > 9000 ? 1 : 0));
            int anType = pl.faction == F_CYBER ? B_C_ANTINUKE : B_K_ANTINUKE;
            if (antiNukes < wantAN && pl.money > BUILDS[anType].cost + 1500) {
                float ang = antiNukes * 1.2566f + 0.4f;
                built = tryBuild(anType, pl.basePos + Vec2(std::cos(ang), std::sin(ang)) * (5.5f * TILE), 14);
            }
        }
        // walls: a thick, unlimited belt around the base. The AI builds one segment per think toward a slowly turning ring, then
        // researches Anti-Rocket so the finished line becomes overlapping point-defence lasers.
        int wallType = pl.faction == F_CYBER ? B_C_WALL : B_K_WALL;
        int wantWalls = pl.difficulty >= 3 ? 44 : (pl.difficulty == 2 ? 28 : (pl.difficulty == 1 ? 14 : 0));
        if (!built && smart && walls < wantWalls && minutes > 1.5f && pl.money > 650 && !threat) {
            float ang = walls * 0.62f + 0.35f;
            float rad = (7.5f + 0.55f * (walls % 14)) * TILE;
            Vec2 spot = pl.basePos + Vec2(std::cos(ang), std::sin(ang) * 0.85f) * rad;
            built = tryBuild(wallType, spot, 12);
        }
        // a Naval Yard on the lake shore nearest to the base, once money is piling up (the dozer drives out to it)
        if (!built && on(FEAT_NAVY) && pl.difficulty >= 1 && yards.empty() && !factories.empty() && powerPlants > 0 && !threat && tm > (pl.difficulty >= 2 ? 7.5f : 10.5f) && pl.money > 5200 && S.time >= nextYardTry && S.countRole(player, BR_NAVAL, false) == 0) {
            nextYardTry = S.time + 20.0f;
            int yardType = pl.faction == F_CYBER ? B_C_NAVAL : B_K_NAVAL, tx, ty;
            Vec2 lake(WORLD_W * 0.5f, WORLD_H * 0.5f);
            if (S.buildAvailable(player, yardType) && S.shoreSite(player, yardType, lake + (pl.basePos - lake) * 0.5f, tx, ty)) built = S.cmdBuild(S.refOf(*dz), yardType, tx, ty);
        }
        // repair
        if (!built && !damaged.empty() && dz->order != O_BUILD) {   // (a dozer already mending something keeps at it; the job picker spreads the others)
            Entity* job = S.repairJob(*dz);
            if (job) S.cmdAssist({S.refOf(*dz)}, S.refOf(*job));
        }
        // continue unfinished structures whose dozer died
        if (!built) for (auto& e : S.ents)
            if (e.alive && e.isBuilding() && e.owner == player && !e.constructed && Sim::canRaise(*dz, e.type)) {   // (a civilian site waits for a civilian dozer)
                bool someone = false;
                for (auto& u : S.ents) if (u.alive && u.owner == player && u.isUnit() && u.order == O_BUILD && u.targetEnt == S.refOf(e)) someone = true;
                if (!someone) { S.cmdAssist({S.refOf(*dz)}, S.refOf(e)); break; }
            }
    }

    // ---------- the town: once the base runs (hub, power, barracks, factory) a civilian dozer raises civilian structures behind it, the best
    // income for the money first, while the treasury can spare it. They pay on their own and send out free dozers; the base's defences cover them.
    if (on(FEAT_CIVIC) && pl.difficulty >= 1 && !recovering && hq && hq->constructed && hubs > 0 && powerPlants > 0 && !barracks.empty() && !factories.empty() && tm > (pl.difficulty >= 2 ? 9.0f : 11.0f)) {
        const int wantTown = pl.difficulty >= 2 ? 3 : 2;   // (a bigger, earlier town made bases rebuild faster than waves could flatten them: far more games stalled)
        int civType = pl.faction == F_CYBER ? U_C_CIVDOZER : U_K_CIVDOZER, queuedCiv = 0;
        for (int t : hq->queue) if (t == civType) queuedCiv++;
        if (civics < wantTown && civDozers + queuedCiv == 0 && hq->queue.size() < 2 && pl.money > 2600 && !threat) S.cmdTrain(S.refOf(*hq), civType);
        Entity* cz = idleDozer(true);
        if (cz && cz->order == O_IDLE && civics < wantTown && !threat && S.time >= nextCivicTry) {
            nextCivicTry = S.time + 4.0f;
            int cb = pl.faction == F_CYBER ? B_C_HOUSE : B_K_HOUSE, pick = -1; float best = 0;
            for (int k = 0; k < CV_COUNT; k++) {
                int t = cb + k;
                if (!S.buildAvailable(player, t) || pl.money < BUILDS[t].cost + (pl.difficulty >= 2 ? 3000 : 4000)) continue;   // (spare money only: the army comes first)
                float v = CIVICS[k].income / (float)BUILDS[t].cost + 0.02f * (k == CV_TOWNHALL);   // pay per credit (the Town Hall opens the big ones)
                if (v > best) { best = v; pick = t; }
            }
            int tx, ty;
            Vec2 behind = pl.basePos - enemyDir * (5.0f + 1.2f * std::min(civics, 8)) * TILE + Vec2(-enemyDir.y, enemyDir.x) * ((civics & 1 ? 1.0f : -1.0f) * (2.0f + civics) * TILE);
            if (pick >= 0 && findSpot(pick, behind, 18, tx, ty)) S.cmdBuild(S.refOf(*cz), pick, tx, ty);
        }
    }
    // ---------- the Underground Nuke: a commander rolling in money that already runs a Nuke Ramp sinks a silo far behind its base
    if (on(FEAT_DEEPNUKE) && pl.difficulty >= 1 && ramps > 0 && !threat && silos < (pl.difficulty >= 3 ? 2 : 1)) {
        int siloType = pl.faction == F_CYBER ? B_C_UNUKE : B_K_UNUKE;
        if (pl.money > BUILDS[siloType].cost + 3000 && idleDozer()) tryBuild(siloType, pl.basePos - enemyDir * 120, 20);
    }

    // ---------- production
    int reserve = (hubs == 0 || pl.powerMade < pl.powerUsed) ? 1600 : 0;
    if (!hq && dozers > 0 && S.countRole(player, BR_HQ, false) == 0) reserve = std::max(reserve, BUILDS[base + BR_HQ].cost + 200);   // bank the price of a new Command Core
    if (harvesters + queuedHarv < 2 && hubs > 0) reserve = std::max(reserve, 900);
    if (recovering) {   // bank for the missing core before any soldier: the Command structure (with a dozer to raise it), a hub, power and production
        int need = (S.countRole(player, BR_HQ, false) == 0 && dozers > 0 ? BUILDS[base + BR_HQ].cost : 0) + (hubs == 0 ? BUILDS[base + BR_SUPPLY].cost : 0)
                 + (powerPlants == 0 ? BUILDS[base + BR_POWER].cost : 0) + (barracks.empty() ? BUILDS[base + BR_BARRACKS].cost : 0) + (factories.empty() ? BUILDS[base + BR_FACTORY].cost : 0);
        if (hq && dozers < 2) need += UNITS[ubase].cost;
        reserve = std::max(reserve, need + 200);
    }
    // save up for a nuke ramp once the tech structure stands
    if (techs > 0 && ramps < (pl.difficulty >= 3 ? 2 : 1) && pl.difficulty >= 1 && tm > (pl.difficulty >= 3 ? 6.5f : (pl.difficulty == 2 ? 8.0f : 12.0f)) && !threat) reserve = std::max(reserve, 5300);
    // a smart commander banks the price of the next income structure before spending on units
    if (smart && !threat && incomes < style.incomes && minutes > style.incomeFrom && powerPlants > 0 && !barracks.empty() && !factories.empty() && dz) reserve = std::max(reserve, BUILDS[base + BR_INCOME].cost + 250);
    // early game: economy and structures first, a modest guard force, then ramp
    float rampStart = pl.difficulty >= 3 ? 1.0f : (pl.difficulty == 2 ? 2.0f : (pl.difficulty == 1 ? 3.0f : 4.0f));
    int armyCap;
    if (pl.difficulty >= 3) armyCap = minutes < rampStart ? 8 : (minutes < rampStart + 2.5f ? 18 : (minutes < rampStart + 5 ? 34 : 90));
    else if (pl.difficulty >= 2) armyCap = minutes < rampStart ? 6 : (minutes < rampStart + 2.5f ? 14 : (minutes < rampStart + 5 ? 26 : 60));
    else armyCap = minutes < rampStart ? 4 : (minutes < rampStart + 2.5f ? 9 : (minutes < rampStart + 5 ? 16 : (pl.difficulty == 0 ? 24 : 40)));
    if (smart) armyCap = (int)(armyCap * style.army + 0.5f);
    if (on(FEAT_MACRO) && pl.difficulty >= 1) armyCap += (int)(ease * clampf((pl.money - 3000.0f) / 450.0f, 0.0f, 60.0f));   // surplus credits become soldiers, not a bank balance
    int queuedArmy = 0;
    for (auto* b : barracks) queuedArmy += (int)b->queue.size();
    for (auto* f : factories) queuedArmy += (int)f->queue.size();
    bool wantArmy = armyCount + queuedArmy < armyCap || threat;
    if (pl.money - reserve > 0 && wantArmy) {
        for (auto* b : barracks) if (b->constructed && b->queue.size() < 2 && pl.money - reserve > 400) { int t = chooseUnit(BR_BARRACKS, enemyInf, enemyVeh, enemyAir); if (t >= 0) S.cmdTrain(S.refOf(*b), t); }
        for (auto* f : factories) if (f->constructed && f->queue.size() < 2 && pl.money - reserve > 900) { int t = chooseUnit(BR_FACTORY, enemyInf, enemyVeh, enemyAir); if (t >= 0) S.cmdTrain(S.refOf(*f), t); }
    }
    // medics: one per dozen fighters or so, trained once there is an army worth healing
    if (smart && pl.difficulty >= 1 && armyCount >= 8 && pl.money - reserve > 1500) {
        int medicType = pl.faction == F_CYBER ? U_C_MEDIC : U_K_MEDIC, queuedMed = 0;
        for (auto* f : factories) for (int t : f->queue) if (t == medicType) queuedMed++;
        if ((int)medics.size() + queuedMed < std::min(3, 1 + armyCount / 14))
            for (auto* f : factories) if (f->constructed && f->queue.empty() && S.cmdTrain(S.refOf(*f), medicType)) { medicsBuilt++; break; }
    }
    // anti-radiation trucks: once enemy warheads are falling (or a cloud is already down), keep a couple to scrub the base clean
    if (smart && pl.difficulty >= 1 && (nukesSeen > 0 || !S.fallouts.empty()) && pl.money - reserve > 1400) {
        int rt = pl.faction == F_CYBER ? U_C_RADTRUCK : U_K_RADTRUCK, queuedDec = 0;
        for (auto* f : factories) for (int t : f->queue) if (t == rt) queuedDec++;
        int wantDec = 1 + (nukesSeen > 2 ? 1 : 0) + (pl.difficulty >= 3 ? 1 : 0);
        if ((int)decon.size() + queuedDec < wantDec)
            for (auto* f : factories) if (f->constructed && f->queue.size() < 2 && S.cmdTrain(S.refOf(*f), rt)) break;
    }
    // spy drones: autonomous swarm reconnaissance that finds hidden snipers and enemy spy drones. A smart commander fields a coordinated swarm.
    // When the enemy has nothing else left, every level keeps a swarm hunting its hidden things
    bool hunting = enemyOnlyHidden(player);
    if (on(FEAT_SPECIAL) && (pl.difficulty >= 1 || hunting) && techs > 0 && tm > 5.0f && (armyCount >= 6 || hunting) && pl.money - reserve > 1200) {
        int droneType = pl.faction == F_CYBER ? U_C_SDRONE : U_K_SDRONE, queued = 0;
        for (auto* a : airfields) for (int t : a->queue) if (t == droneType) queued++;
        int wantDrones = hunting ? 3 : (pl.difficulty >= 3 ? 3 : (pl.difficulty >= 2 ? 2 : 1));
        if (pl.money > 8000 && pl.difficulty >= 2) wantDrones++;
        if ((int)scouts.size() + queued < wantDrones)
            for (auto* a : airfields) if (a->constructed && a->queue.empty() && S.cmdTrain(S.refOf(*a), droneType)) break;
    }
    // spies: two of them on Hard and Brutal, to take structures nobody is guarding
    if (on(FEAT_SPECIAL) && pl.difficulty >= 2 && techs > 0 && tm > 6.5f && armyCount >= 8 && pl.money - reserve > 1500) {
        int spyType = pl.faction == F_CYBER ? U_C_SPY : U_K_SPY, queued = 0;
        for (auto* b : barracks) for (int t : b->queue) if (t == spyType) queued++;
        if ((int)spies.size() + queued < 2)
            for (auto* b : barracks) if (b->constructed && b->queue.empty() && S.cmdTrain(S.refOf(*b), spyType)) break;
    }
    for (auto* a : airfields) if (a->constructed && a->queue.empty() && pl.money - reserve > 2500 && (int)aircraft.size() < style.airCap) { int t = chooseUnit(BR_AIRFIELD, enemyInf, enemyVeh, enemyAir); if (t >= 0) S.cmdTrain(S.refOf(*a), t); }
    // ships: a yard keeps its slipway busy while there is money to spare - destroyers for the fight on the lake, a few missile cruisers for the shore
    if (on(FEAT_NAVY) && !yards.empty()) {
        int cruiserT = pl.faction == F_CYBER ? U_C_CRUISER : U_K_CRUISER, destroyerT = pl.faction == F_CYBER ? U_C_DESTROYER : U_K_DESTROYER;
        int queuedShips = 0, destroyers = 0, cruisers = 0;
        for (auto* y : yards) queuedShips += (int)y->queue.size();
        for (auto r : fleet) if (Entity* e = S.get(r)) (e->type == cruiserT ? cruisers : destroyers)++;
        int fleetCap = 3 + pl.difficulty + (enemyShips > 0 ? 2 : 0) + (pl.money > 9000 ? 1 : 0);
        for (auto* y : yards) if (y->constructed && y->queue.empty() && (int)fleet.size() + queuedShips < fleetCap && pl.money - reserve > 2600) {
            int want = (cruisers * 2 < destroyers && S.unitAvailable(player, cruiserT)) ? cruiserT : destroyerT;
            if (enemyShips > 0 && cruisers >= 1) want = destroyerT;   // an enemy navy first needs guns that answer it
            if (S.cmdTrain(S.refOf(*y), want)) queuedShips++;
        }
        for (auto* y : yards) if (y->constructed) S.cmdSetRally(S.refOf(*y), g_map.nearestWater(Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f) + (pl.basePos - Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f)).norm() * (6.0f * TILE), 12, true));
    }
    // jeeps take soldiers aboard: idle infantry near a jeep that is standing still climb in (the anti-armour troopers first), so the army rolls out with its infantry riding
    if (on(FEAT_NAVY) && !jeeps.empty() && S.time - lastJeepLoad > 2.5f) {
        lastJeepLoad = S.time;
        for (auto jr : jeeps) {
            Entity* j = S.get(jr);
            if (!j || j->unloading || dist(j->pos, j->prevPos) > 1.2f || j->fall > 0) continue;
            int entering = 0; for (auto& u : S.ents) if (u.alive && u.isUnit() && u.owner == player && u.order == O_ENTER && u.targetEnt == jr) entering++;
            int room = JEEP_SEATS - (int)j->passengers.size() - entering;
            if (room <= 0) continue;
            std::vector<std::pair<float, Ref>> cand;
            for (auto ir : idleArmy) {
                Entity* u = S.get(ir);
                if (!u || u->ut().kind != UK_INF || u->carrier.valid() || !S.canBoard(*u, *j) || dist(u->pos, j->pos) > 14.0f * TILE) continue;
                bool rocket = u->weapon() >= 0 && (WEAPONS[u->weapon()].proj == PJ_ROCKET || (WEAPONS[u->weapon()].mult[AR_HEAVY] >= 1.0f && WEAPONS[u->weapon()].range > 5.5f));
                cand.push_back({dist(u->pos, j->pos) - (rocket ? 3.0f * TILE : 0.0f), ir});
            }
            std::sort(cand.begin(), cand.end(), [](const std::pair<float, Ref>& a, const std::pair<float, Ref>& b) { return a.first < b.first; });
            std::vector<Ref> riders; for (auto& c : cand) { if ((int)riders.size() >= room) break; riders.push_back(c.second); }
            if (!riders.empty()) S.cmdEnter(riders, jr);
        }
    }
    // rally new units toward the front
    for (auto* b : barracks) S.cmdSetRally(S.refOf(*b), rally);
    for (auto* f : factories) S.cmdSetRally(S.refOf(*f), rally);

    // ---------- tech structure: the Advanced Program, a map scan when nothing is known about the enemy, and the strike
    if (techs > 0 && S.programAvailable(player) && tm > (pl.difficulty >= 2 ? 6.0f : 9.0f) && pl.money > PROGRAMS[pl.faction].cost + 1200) S.cmdResearch(player);
    // structure upgrades once the economy carries them: self-repair first, then the roof guns, then the rugged armour (easy skips the last two)
    if (techs > 0) {
        static const int order[UPG_COUNT] = { UPG_REPAIR, UPG_GUNS, UPG_RUGGED, UPG_DRONES, UPG_HYPER, UPG_ANTIROCKET };
        static const float from[UPG_COUNT] = { 13.0f, 11.0f, 8.0f, 14.0f, 12.0f, 10.0f };   // minutes, by UpgradeId
        float pace = pl.difficulty >= 3 ? 0.55f : (pl.difficulty == 2 ? 0.75f : (pl.difficulty == 1 ? 1.0f : 1.6f));
        for (int k = 0; k < UPG_COUNT; k++) {
            int u = order[k];
            if (pl.difficulty == 0 && u != UPG_REPAIR) continue;
            if (u == UPG_DRONES && shipsOrJeeps < 2) continue;   // the screen only pays for an army that has ships or jeeps
            if (u == UPG_ANTIROCKET && walls < 4) continue;       // the wall laser only matters when there is a line of walls to mount it on
            static const int upgMask = getenv("ONEHOUR_UPGMASK") ? atoi(getenv("ONEHOUR_UPGMASK")) : -1;   // (debug: bit u allows upgrade u)
            if (!(upgMask & (1 << u))) continue;
            if (!S.upgradeAvailable(player, u) || tm < from[u] * pace) continue;
            if (pl.money > UPGRADES[pl.faction][u].cost + (threat ? 3000 : 1500)) S.cmdUpgrade(player, u);
            break;   // one at a time, in order
        }
    }
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

    // ---------- nukes
    // a warhead kills ground units and small structures in the blast and wrecks the big ones: aim at whatever it would cost the enemy most
    auto nukeSpot = [&](float R, float& bestScore, float* rawOut = nullptr, bool deep = false) {
        std::vector<Entity*> cand;
        for (auto& e : S.ents) {
            if (!e.alive || e.kind == EK_RESOURCE || !S.enemies(player, e.owner) || e.isAir()) continue;
            if (!(S.explored(player, clampi(tileOf(e.pos.x), 0, MAP_W - 1), clampi(tileOf(e.pos.y), 0, MAP_H - 1)) && S.visibleTo(e, player))) continue;
            cand.push_back(&e);
        }
        Vec2 bestPos; bestScore = 0; float bestRaw = 0;
        for (auto* c : cand) {
            float score = 0;
            for (auto* o : cand) {
                float d = dist(c->pos, o->pos); if (d > R) continue;
                float f = d < R * 0.35f ? 1.0f : 1.0f - 0.75f * ((d - R * 0.35f) / (R * 0.65f));
                if (o->isUnit()) score += UNITS[o->type].cost * (UNITS[o->type].hp > 1200 && f < 0.5f ? 0.3f : 1.0f);
                else { float cost = (float)BUILDS[o->type].cost * (BUILDS[o->type].role == BR_CIVIC ? 0.4f : 1.0f); bool small = BUILDS[o->type].w * BUILDS[o->type].h <= 6; score += small ? cost * (f >= 0.62f ? 0.9f : 0.3f) : cost * 0.5f * f; }   // (a town is worth less to a warhead than an army)
            }
            for (auto& o : S.ents) if (o.alive && o.isUnit() && o.owner >= 0 && o.owner != player && !S.enemies(player, o.owner) && !o.isAir() && dist(c->pos, o.pos) < R * 0.95f) score -= UNITS[o.type].cost * 1.2f;   // fallout hurts friends too
            for (auto& o : S.ents) if (o.alive && o.isUnit() && o.owner == player && !o.isAir() && dist(c->pos, o.pos) < R * 0.95f) score -= UNITS[o.type].cost * 1.2f;
            for (auto& o : S.ents) if (o.alive && o.isAir() && o.owner >= 0 && !S.enemies(player, o.owner) && dist(c->pos, o.pos) < R) score -= UNITS[o.type].cost * 1.2f;   // and every aircraft in the blast falls, ours too
            float raw = score;
            if (on(FEAT_AEGIS)) score *= 1.0f - 0.8f * S.antiNukeChance(c->pos, player, true, deep ? UNUKE_SCALE : 1.0f, deep);   // the enemy's anti-nuke cover makes a spot worth less: its defences may shoot the warhead down
            if (score > bestScore) { bestScore = score; bestPos = c->pos; bestRaw = raw; }
        }
        if (rawOut) *rawOut = bestRaw;
        return bestPos;
    };
    float nukeBar = pl.difficulty >= 3 ? 2600.0f : (pl.difficulty == 2 ? 3200.0f : 4000.0f);
    if (smart && ramps > 0 && S.nukesReady(player) > 0) {
        float bestScore, rawScore = 0; Vec2 bestPos = nukeSpot(NUKE_RADIUS * TILE, bestScore, &rawScore);
        int cover = on(FEAT_AEGIS) ? S.antiNukeCovering(bestPos, player, true) : 0;
        if (bestScore >= nukeBar) {
            // a covered target: with a second ramp ready a salvo goes - the defences that open fire on the first warhead are reloading when the second arrives,
            // so one of the two gets through whatever the cover; with one ramp the commander waits a little for the second
            if (cover > 0 && S.nukesReady(player) >= 2) { S.cmdNuke(player, bestPos); S.cmdNuke(player, bestPos); nukeHoldSince = -1.0f; }
            else if (cover > 0 && ramps >= 2) {
                if (nukeHoldSince < 0) nukeHoldSince = S.time;                        // start waiting for the second ramp ...
                if (S.time - nukeHoldSince >= 40.0f) { S.cmdNuke(player, bestPos); nukeHoldSince = -1.0f; }   // ... and go alone if it takes too long
            }
            else { S.cmdNuke(player, bestPos); nukeHoldSince = -1.0f; }
        } else if (cover > 0 && S.nukesReady(player) >= 2 && rawScore >= nukeBar * 1.3f) { S.cmdNuke(player, bestPos); S.cmdNuke(player, bestPos); }   // worth a salvo although the cover costs a warhead
    } else if (!smart && ramps > 0 && S.nukesReady(player) > 0) {
        // legacy: fire at the densest cluster of enemy structures we know about
        Vec2 bestPos; float bestScore = 0;
        for (auto& e : S.ents) {
            if (!e.alive || !e.isBuilding() || !S.enemies(player, e.owner) || !(S.explored(player, clampi(tileOf(e.pos.x), 0, MAP_W - 1), clampi(tileOf(e.pos.y), 0, MAP_H - 1)) && S.visibleTo(e, player))) continue;
            float score = 0;
            for (auto& o : S.ents) if (o.alive && S.enemies(player, o.owner) && o.kind != EK_RESOURCE && dist(e.pos, o.pos) < NUKE_RADIUS * TILE * 0.8f) score += o.isUnit() ? UNITS[o.type].cost : BUILDS[o.type].cost * 0.6f;
            for (auto& o : S.ents) if (o.alive && o.owner == player && o.kind != EK_RESOURCE && dist(e.pos, o.pos) < NUKE_RADIUS * TILE * 1.1f) score -= 4000;   // never nuke our own people
            if (score > bestScore) { bestScore = score; bestPos = e.pos; }
        }
        if (bestScore >= 3500.0f) S.cmdNuke(player, bestPos);
    }
    // the silver warhead of an Underground Nuke: three times the circle, so it waits for a target worth it (only enemy lasers can stop it)
    if (smart && silos > 0 && S.deepNukesReady(player) > 0 && (S.tick & 7) == (u32)((player + 3) & 7)) {
        float bestScore; Vec2 bestPos = nukeSpot(NUKE_RADIUS * TILE * std::sqrt(UNUKE_SCALE), bestScore, nullptr, true);
        if (bestScore >= nukeBar * 2.0f) S.cmdDeepNuke(player, bestPos);
    }
    // a bunker's small missile (a computer army only has one by capturing a bunker): a smaller circle, so a lower bar
    if (smart && S.miniNukesReady(player) > 0 && (S.tick & 7) == (u32)(player & 7)) {
        float bestScore; Vec2 bestPos = nukeSpot(NUKE_RADIUS * TILE * std::sqrt(MINI_NUKE_SCALE), bestScore);
        if (bestScore >= nukeBar * 0.4f) S.cmdMiniNuke(player, bestPos);
    }

    // ---------- paradrop: reinforce a base under attack, or land on the objective of a wave that is under way (never into a wall of flak)
    if (techs > 0 && pl.difficulty >= 1 && S.dropsReady(player) > 0 && S.time > 150.0f) {
        Vec2 at; bool go = false;
        if (threat && threatCount >= 3 && dist(threat->pos, pl.basePos) < 26 * TILE) {
            Vec2 d = threat->pos - pl.basePos; float l = std::max(1.0f, d.len());
            at = pl.basePos + d * (std::min(l, 7.0f * TILE) / l); go = true;
        } else if (attacking && !threat) {
            Entity* tgt = S.get(attackTarget);
            if (tgt && S.aaCover(*tgt, player) <= 3.0f) {
                Vec2 d = pl.basePos - tgt->pos; Vec2 n = d.norm();
                at = tgt->pos + n * (5.0f * TILE); go = true;
            }
        }
        if (go && S.cmdParadrop(player, at, true) && aiDebug()) fprintf(stderr, "[ai%d t=%.0f] PARADROP at %d,%d\n", player, S.time, tileOf(at.x), tileOf(at.y));
    }

    // ---------- phone strike: the strike power's louder cousin - instead of resolving on the spot it puts a swarm of aircraft over it
    if (techs > 0 && pl.difficulty >= 1 && S.time > 150.0f && S.strikesReady(player) > 0 && !enemyUnits.empty()) {
        const float R = 6.0f * TILE;   // the package works a spot about this wide
        Vec2 at; float best = 0;
        for (auto* e : enemyUnits) {
            if (!S.visibleTo(*e, player)) continue;
            float score = 0;
            for (auto& o : S.ents) {
                if (!o.alive || o.kind == EK_RESOURCE || !S.enemies(player, o.owner) || o.isAir()) continue;
                if (o.isBuilding() && (BUILDS[o.type].role == BR_BUNKER || BUILDS[o.type].role == BR_UNUKE)) continue;   // aircraft can never take a bunker (or a silo) below its floor: not a strike target
                if (dist(e->pos, o.pos) > R) continue;
                float v = o.isUnit() ? (float)UNITS[o.type].cost : (float)BUILDS[o.type].cost * 0.6f;
                if (!o.isUnit() && BUILDS[o.type].w * BUILDS[o.type].h > 6) v *= 0.5f;   // a big structure shrugs a strike off
                score += v;
            }
            if (score > best) { best = score; at = e->pos; }
        }
        // never right on top of our own people: the package shoots whatever it finds over the spot
        for (auto& o : S.ents) if (o.alive && o.kind != EK_RESOURCE && o.owner == player && dist(at, o.pos) < R * 0.75f) best = 0;
        if (best >= (pl.faction == F_CYBER ? 1800.0f : 1500.0f) && S.cmdPhoneStrike(player, at) && aiDebug()) fprintf(stderr, "[ai%d t=%.0f] PHONE STRIKE at %d,%d\n", player, S.time, tileOf(at.x), tileOf(at.y));
    }

    // ---------- army
    float waveThreshold = 3200.0f + minutes * 320.0f;
    if (pl.difficulty == 0) waveThreshold *= 1.7f; else if (pl.difficulty == 2) waveThreshold *= 0.85f; else if (pl.difficulty == 3) waveThreshold *= 0.6f;
    waveThreshold = std::min(waveThreshold * style.thr, 11000.0f);
    float firstWaveAt = (pl.difficulty >= 3 ? 165.0f : (pl.difficulty == 2 ? 270.0f : (pl.difficulty == 1 ? 360.0f : 480.0f))) * style.first;

    // medics keep up with the army (the healer's own AI handles the last few tiles: it drifts to wounded friends)
    if (smart && !medics.empty()) {
        Vec2 cen; int n = 0;
        for (auto r : (attacking && !wave.empty() ? wave : army)) { Entity* e = S.get(r); if (e && e->weapon() >= 0 && !e->isAir()) { cen += e->pos; n++; } }
        Vec2 want = n ? cen * (1.0f / n) : rally;
        for (auto r : medics) { Entity* m = S.get(r); if (m && m->order == O_IDLE && dist(m->pos, want) > 9 * TILE) S.cmdMove({r}, want, false); }
    }

    // a teammate's structures under attack: send most of the army that is not already committed to a wave
    bool helpingAlly = false;
    if (smart && !threat && allyThreat && armyCount >= 4) {
        helpingAlly = true;
        if (S.time - lastAllyHelp > 8.0f) {
            lastAllyHelp = S.time;
            std::vector<Ref> help;
            for (auto r : army) if (!attacking || std::find(wave.begin(), wave.end(), r) == wave.end()) help.push_back(r);
            if (help.size() >= 3) {
                if (on(FEAT_ALLY) && S.time - lastAllyNote > 25.0f && S.humanPlayer >= 0 && !S.players[S.humanPlayer].isAI && S.players[S.humanPlayer].team == pl.team) { lastAllyNote = S.time; S.emit(EV_MSG, S.humanPlayer, SND_NONE, allyThreat->pos, "Ally: sending units to defend your base"); }
                if (aiDebug()) fprintf(stderr, "[ai%d t=%.0f] ALLY under attack near %d,%d: sending %d units\n", player, S.time, tileOf(allyThreat->pos.x), tileOf(allyThreat->pos.y), (int)help.size());
                S.cmdMove(help, allyThreat->pos, true);
            }
        }
    }

    // a teammate is pushing out: the army moves up to the same front and fights beside theirs
    if (frontTarget && !threat && !attacking && armyCount >= 6 && armyValue >= waveThreshold * 0.5f && S.time > regroupUntil && S.time - lastJoin > 30.0f) {
        lastJoin = S.time; attacking = true; waveValue = armyValue; attackStarted = S.time; attackTarget = S.refOf(*frontTarget); wave = army; waveHasSample = false;
        S.cmdMove(army, frontTarget->pos, true);
        nextOrderTime = S.time + 6;
        if (aiDebug()) fprintf(stderr, "[ai%d t=%.0f] JOIN the teammate's push at %d,%d with %d units\n", player, S.time, tileOf(frontTarget->pos.x), tileOf(frontTarget->pos.y), armyCount);
        if (S.time - lastAllyNote > 20.0f) { lastAllyNote = S.time; S.emit(EV_MSG, frontHuman, SND_NONE, frontTarget->pos, "Ally: joining your attack"); }
    }

    // defense has priority
    if (threat && threatCount > 0 && S.time - lastDefend > 3.0f) {
        lastDefend = S.time;
        if (aiDebug()) fprintf(stderr, "[ai%d t=%.0f] DEFEND vs %s at %d,%d (count %d) attacking=%d\n", player, S.time, threat->ut().name, tileOf(threat->pos.x), tileOf(threat->pos.y), threatCount, (int)attacking);
        std::vector<Ref> defenders;
        for (auto r : army) { Entity* e = S.get(r); if (!e) continue; if (!attacking || dist(e->pos, pl.basePos) < 22 * TILE) defenders.push_back(r); }
        // a base under real attack outranks a wave in progress (the wave resumes once the threat is gone)
        if (attacking && threatCount >= 4) defenders = army;
        if (!defenders.empty()) S.cmdMove(defenders, threat->pos, true);
        for (auto r : aircraft) { Entity* a = S.get(r); if (a && (a->ammo > 0 || a->ut().ammo <= 0) && a->order == O_IDLE) S.cmdAttack({r}, S.refOf(*threat)); }   // (helicopters that never run dry answer too)
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
        if (smart && pl.difficulty >= 1 && !exhausted) {
            // outgunned? compare what is fighting around the wave with what stands around it; two bad readings in a row break the attack off
            Vec2 cen; int n = 0; for (auto r : wave) { Entity* e = S.get(r); if (e) { cen += e->pos; n++; } }
            cen = cen * (1.0f / std::max(1, n));
            float mine = 0; for (auto r : wave) { Entity* e = S.get(r); if (e && dist(e->pos, cen) < 10 * TILE) mine += unitValue(*e); }
            float foe = enemyStrengthNear(cen, 10);
            bool bad = n >= 4 && foe > mine * 1.9f + 1200 && mine < waveValue * 0.85f;
            retreatVotes = bad ? retreatVotes + 1 : std::max(0.0f, retreatVotes - 1);
            if (retreatVotes >= 3) { exhausted = true; retreatVotes = 0; }
        }
        // a target that radiation has made deadly is not worth marching into
        if (smart && tgt) for (auto& f : S.fallouts) if (f.t < f.life - 15.0f && dist(tgt->pos, f.pos) < f.r) { attackTarget = NOREF; break; }
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
        if (!idleArmy.empty() && !helpingAlly) {
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
        bool eligible = (armyValue >= waveThreshold * (on(FEAT_LAUNCH) ? 0.7f : 1.0f) || (overwhelming && minutes > 6)) && !recovering;   // (a recovering army keeps its last units home)
        bool waveModel = useBrain && ablate() != 1;
        if (S.time > regroupUntil && S.time > firstWaveAt && eligible && mainEnemy >= 0) {
            Entity* tgt = pickAttackTarget();
            float defense = tgt ? enemyStrengthNear(tgt->pos, 11) : 0;
            float x[WAVE_F]; Brain::waveFeatures(armyValue, defense, enemyArmyValue, minutes, armyCount, x);
            bool go = true;
            float Rt = 0;
            if (tgt && on(FEAT_LAUNCH)) {
                // attack when the estimate says the army outguns what waits for it (the learned model calibrates how much margin that takes)
                Rt = assaultRatio(army, tgt->pos, 11, false);
                float Ra = assaultRatio(army, tgt->pos, 11, true);
                Brain::waveFeaturesR(Rt, Ra, minutes, armyCount, x);
                float p = brain->waveProb(x);
                bool explore = useBrain && brain->learning && Rt > 0.8f && rng.f() < 0.12f;
                go = explore || (useBrain ? p >= 0.5f : Rt >= 1.35f) || (armyCount >= armyCap && Rt >= 1.0f);
                if (aiDebug() && !go && (int)S.time % 30 == 0) fprintf(stderr, "[ai%d t=%.0f] holding: R=%.2f (all %.2f) p=%.2f\n", player, S.time, Rt, Ra, p);
            } else if (tgt) {
                if (waveModel) {
                    float p = brain->waveProb(x);
                    float thr = 0.45f;   // the model can only hold a wave back, never launch a smaller one
                    // exploration: now and then launch (or hold) against the model's advice so it keeps seeing both outcomes
                    bool explore = brain->learning && rng.f() < 0.15f;   // launch against its advice sometimes so it keeps seeing both outcomes
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
        // only the enemy's hidden things are left (a bunker, snipers, spy drones): nothing is in sight to attack, so the army sweeps the ground around its
        // base, where the spy drones trailing it find what hides there. Without this the match would stall with the army idle at home
        if (hunting && !helpingAlly && armyCount > 0 && mainEnemy >= 0 && S.time > huntAt) {
            huntAt = S.time + 20.0f;
            Vec2 c = S.players[mainEnemy].basePos + Vec2(rng.f(-1, 1), rng.f(-1, 1)) * (rng.f(3.0f, 15.0f) * TILE);
            huntPoint = g_map.nearestFree(c, 20);
            if (aiDebug()) fprintf(stderr, "[ai%d t=%.0f] HUNT the enemy's hidden forces at %d,%d with %d units\n", player, S.time, tileOf(huntPoint.x), tileOf(huntPoint.y), armyCount);
            S.cmdMove(army, huntPoint, true);
        }
        // harassment: hard AIs send a small fast squad after enemy harvesters
        if (pl.difficulty >= 2 && minutes > (pl.brutal() ? 3 : 4) && S.time - lastHarass > (pl.brutal() ? 55 : 90) && armyCount >= 5) {
            Entity* h = nullptr; float bd = 1e18f;
            for (auto& e : S.ents) if (e.alive && e.isUnit() && S.enemies(player, e.owner) && e.ut().role == UR_HARVESTER && (S.explored(player, clampi(tileOf(e.pos.x), 0, MAP_W - 1), clampi(tileOf(e.pos.y), 0, MAP_H - 1)) && S.visibleTo(e, player))) { float d = dist2(e.pos, pl.basePos); if (d < bd) { bd = d; h = &e; } }
            if (h) {
                lastHarass = S.time;
                std::vector<Ref> squad;
                for (auto r : army) { Entity* e = S.get(r); if (e && e->ut().kind == UK_VEH && e->ut().speed >= 70) { squad.push_back(r); if ((int)squad.size() >= (pl.brutal() ? 4 : 2)) break; } }
                if (!squad.empty()) S.cmdAttack(squad, S.refOf(*h));
            }
        }
    }
    // spy drones coordinate autonomously as an intelligent swarm (updateScoutSwarm in Sim);
    // only pull a badly damaged drone back over a friendly medic if one exists
    for (auto r : scouts) {
        Entity* d = S.get(r);
        if (!d || d->hp > d->maxHp * 0.45f || medics.empty()) continue;
        if (Entity* m = S.get(medics[0])) if (dist(d->pos, m->pos) > 4.0f * TILE) S.cmdMove({r}, m->pos, false);
    }
    // spies go for the structure that is worth most and guarded least (nothing armed within six tiles), and the second one tags along
    if (!spies.empty() && S.time - lastSpyOrder > 5.0f) {
        lastSpyOrder = S.time;
        Entity* best = nullptr; float bs = -1e18f;
        for (auto& e : S.ents) {
            if (!e.alive || !e.isBuilding() || !e.constructed || !S.enemies(player, e.owner) || !(S.explored(player, clampi(tileOf(e.pos.x), 0, MAP_W - 1), clampi(tileOf(e.pos.y), 0, MAP_H - 1)) && S.visibleTo(e, player))) continue;
            float w = 1.0f;
            switch (e.bt().role) { case BR_NUKE: case BR_UNUKE: w = 2.2f; break; case BR_INCOME: w = 1.7f; break; case BR_CIVIC: w = 0.6f; break; case BR_TECH: w = 1.4f; break; case BR_FACTORY: case BR_AIRFIELD: w = 1.3f; break; case BR_SUPPLY: w = 1.2f; break; case BR_POWER: w = 1.1f; break; case BR_HQ: w = 0.8f; break; default: break; }
            float armed = 0;
            S.forEachNear(e.pos, 6.5f * TILE, [&](Entity& g) {
                if (g.kind == EK_RESOURCE || g.owner < 0 || !(g.owner == e.owner || S.players[g.owner].team == S.players[e.owner].team)) return;
                if (g.weapon() >= 0 && (g.isBuilding() ? g.constructed : g.ut().role == UR_COMBAT)) armed += g.isBuilding() ? 1.5f : 1.0f;
            });
            Entity* sp = S.get(spies[0]);
            float sc = BUILDS[e.type].cost * w * 0.4f - 900.0f * armed - (sp ? dist(e.pos, sp->pos) / TILE * 12.0f : 0.0f);
            if (sc > bs) { bs = sc; best = &e; }
        }
        if (best && bs > -400.0f) {
            std::vector<Ref> idle;
            for (auto r : spies) { Entity* sp = S.get(r); if (sp && sp->order != O_CAPTURE && !sp->carrier.valid()) idle.push_back(r); }
            if (!idle.empty()) S.cmdCapture(idle, S.refOf(*best));
        }
    }
    // the fleet keeps the lake: it fights enemy ships first, shells what stands near the shore that it can beat, and falls back to a post on its own side of the lake
    if (on(FEAT_NAVY) && !fleet.empty()) {
        Vec2 lake(WORLD_W * 0.5f, WORLD_H * 0.5f);
        Vec2 post = g_map.nearestWater(lake + (pl.basePos - lake).norm() * (5.0f * TILE), 12, true);
        Vec2 cen; int nShips = 0;
        for (auto r : fleet) if (Entity* e = S.get(r)) { cen += e->pos; nShips++; }
        cen = cen * (1.0f / std::max(1, nShips));
        Entity* ftgt = nullptr; float fbd = 1e18f;
        for (auto& e : S.ents) {
            if (!e.alive || e.kind == EK_RESOURCE || !S.enemies(player, e.owner) || e.carrier.valid()) continue;
            if (!(S.explored(player, clampi(tileOf(e.pos.x), 0, MAP_W - 1), clampi(tileOf(e.pos.y), 0, MAP_H - 1)) && S.visibleTo(e, player))) continue;
            bool ship = e.isUnit() && e.ut().naval;
            if (!ship) {   // a land target must be within reach of the lake
                if (e.isAir()) continue;
                Vec2 w = g_map.nearestWater(e.pos, 13, true);
                if (!g_map.navTile(tileOf(w.x), tileOf(w.y)) || dist(w, e.pos) > 12.0f * TILE) continue;
                if (enemyStrengthNear(e.pos, 8) > fleetValue * 1.3f + 600.0f) continue;   // not into a wall of guns
                if (e.isBuilding() && e.bt().role == BR_AATURRET) continue;
            }
            float d = dist(e.pos, cen) / TILE;
            if (ship) d -= 7.0f;
            else if (e.isUnit() && e.ut().role == UR_COMBAT) d -= 2.0f;
            else if (e.isBuilding() && (e.bt().role == BR_NAVAL || e.bt().role == BR_NUKE || e.bt().role == BR_SUPPLY)) d -= 4.0f;
            if (d < fbd) { fbd = d; ftgt = &e; }
        }
        Entity* yardHurt = nullptr;
        for (auto* y : yards) if (S.time - y->lastDamaged < 6.0f) yardHurt = y;
        if (S.time - lastFleetOrder > 5.0f) {
            lastFleetOrder = S.time;
            if (yardHurt) S.cmdMove(fleet, yardHurt->pos, true);
            else if (ftgt && (nShips >= 2 || ftgt->isBuilding() == false)) S.cmdMove(fleet, ftgt->pos, true);
            else if (dist(cen, post) > 7.0f * TILE) { std::vector<Ref> idleShips; for (auto r : fleet) { Entity* e = S.get(r); if (e && (e->order == O_IDLE || e->order == O_GUARDPOS)) idleShips.push_back(r); } if (!idleShips.empty()) S.cmdMove(idleShips, post, true); }
        }
    }
    // bombers (smart): wait until a flight is loaded, then go together for what hurts most and sits under the least flak:
    // a structure the enemy cannot do without (nuke ramps, income, supply, power), worth a bombing run, or a cluster of units a stick of bombs will catch
    if (smart) {
        std::vector<Ref> ready; int bombers = 0;
        for (auto r : aircraft) {
            Entity* a = S.get(r);
            if (!a || !a->ut().bomber) continue;
            bombers++;
            if (a->order == O_IDLE && a->weapon() >= 0) S.cmdGuardArea({r}, pl.basePos, 9 * TILE);   // between raids they patrol over the base
            if ((a->order == O_IDLE || a->order == O_GUARDAREA) && a->ammo >= a->ut().ammo) ready.push_back(r);
        }
        if (!ready.empty() && ((int)ready.size() >= std::min(3, bombers) || S.time - lastBombRun > 30.0f)) {
            Entity* lead = S.get(ready[0]);
            Entity* best = nullptr; float bestScore = 300.0f;
            for (auto& e : S.ents) {
                if (!e.alive || e.kind == EK_RESOURCE || e.isAir() || !S.enemies(player, e.owner)) continue;
                if (!(S.explored(player, clampi(tileOf(e.pos.x), 0, MAP_W - 1), clampi(tileOf(e.pos.y), 0, MAP_H - 1)) && S.visibleTo(e, player))) continue;
                float v = 0;
                if (e.isBuilding()) {
                    float w = 1.0f;
                    switch (e.bt().role) {
                    case BR_NUKE: case BR_UNUKE: w = 1.9f; break; case BR_INCOME: w = 1.5f; break; case BR_CIVIC: w = 0.45f; break; case BR_SUPPLY: w = 1.3f; break; case BR_POWER: w = 1.25f; break;   // (a town is expensive but fights nobody: bombing it instead of the army stalled games)
                    case BR_TECH: w = 1.1f; break; case BR_HQ: w = 0.55f; break; case BR_TURRET: w = 0.35f; break; case BR_AATURRET: w = 0.2f; break; default: break;
                    }
                    v = BUILDS[e.type].cost * w * (e.constructed ? 1.0f : 0.5f);
                } else if (e.ut().role == UR_HARVESTER) v = 900;
                else {
                    for (auto* o : enemyUnits) if (!o->isAir() && dist(o->pos, e.pos) < 2.2f * TILE) v += UNITS[o->type].cost * (0.5f + 0.5f * o->hp / o->maxHp);
                    if (e.ut().role != UR_COMBAT) v = 250;
                }
                float flak = S.aaCover(e, player);
                float sc = v - 650.0f * flak - (flak >= 3 ? 1500.0f : 0.0f) - dist(e.pos, lead->pos) / TILE * 14.0f;
                if (sc > bestScore) { bestScore = sc; best = &e; }
            }
            if (best) {
                if (aiDebug()) fprintf(stderr, "[ai%d t=%.0f] BOMB RUN: %d bombers -> %s score %.0f\n", player, S.time, (int)ready.size(), best->isUnit() ? best->ut().name : best->bt().name, bestScore);
                S.cmdAttack(ready, S.refOf(*best)); lastBombRun = S.time;
            }
        }
    }
    // aircraft strike on their own cadence: hit the juiciest known target
    for (auto r : aircraft) {
        Entity* a = S.get(r);
        if (!a) continue;
        if (smart && a->ut().bomber) continue;
        // between strikes drones patrol a circle over the base
        if (a->order == O_IDLE && a->weapon() >= 0) S.cmdGuardArea({r}, pl.basePos, 9 * TILE);
        if ((a->order != O_IDLE && a->order != O_GUARDAREA) || a->ammo < a->ut().ammo) continue;
        Entity* best = nullptr; float bs = 1e18f;
        for (auto& e : S.ents) {
            if (!e.alive || !S.enemies(player, e.owner) || e.kind == EK_RESOURCE) continue;
            if (!(S.explored(player, clampi(tileOf(e.pos.x), 0, MAP_W - 1), clampi(tileOf(e.pos.y), 0, MAP_H - 1)) && S.visibleTo(e, player))) continue;
            if (!S.canTarget(*a, e)) continue;   // (a bunker at its floor, or a target the craft cannot shoot: it would only fly there and idle)
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
