// One Hour - the canal's bridges, the abandoned relic bases, the secret islands and surrender (see BRIDGE_*, RELIC_*, SURRENDER_* in data.h)
#include "sim.h"

// ------------------------------------------------------------ bridges
// A bridge is nobody's structure lying over the canal. Its deck tiles are water with a road over them: passable while it stands (Map::blocked bit 0
// cleared), sailable always (ships pass under). When enough fire lands on it the span drops: the deck tiles become water again for anything on the
// ground, everyone standing on it falls in (an infiltrator swims), and it waits as a wreck until a dozer of any army raises it again.
static void deckTiles(const Entity& b, bool open) {
    for (int j = 0; j < BRIDGE_L; j++) for (int i = 0; i < BRIDGE_W; i++) {
        int x = b.tx + i, y = b.ty + j;
        if (!inMap(x, y)) continue;
        int k = y * MAP_W + x;
        if (g_map.tiles[k] != T_WATER) continue;   // (the abutments on the banks are ground either way)
        if (open) g_map.blocked[k] &= (u8)~1; else g_map.blocked[k] |= 1;
    }
    g_map.regionDirty = true;   // (the ground's regions are worked out again; ships sail under the deck either way: the naval flags stand)
}

void Sim::breakBridge(Entity& b, int by) {
    if (!b.constructed) return;
    b.constructed = false; b.progress = 0; b.hp = b.maxHp * 0.1f;
    deckTiles(b, false);
    // whoever stands on the deck goes into the water: an infiltrator swims for it, anything else is lost
    const float x0 = b.tx * (float)TILE, y0 = b.ty * (float)TILE, x1 = x0 + BRIDGE_W * TILE, y1 = y0 + BRIDGE_L * TILE;
    for (auto& u : ents) {
        if (!u.alive || !u.isUnit() || u.isAir() || u.ut().naval || u.carrier.valid() || u.fall > 0) continue;
        if (u.pos.x < x0 || u.pos.x >= x1 || u.pos.y < y0 || u.pos.y >= y1) continue;
        int tx = tileOf(u.pos.x), ty = tileOf(u.pos.y);
        if (!inMap(tx, ty) || g_map.tiles[ty * MAP_W + tx] != T_WATER) continue;   // (on an abutment: still on the bank)
        if (swimmer(u)) continue;
        fx.push_back({FX_RING, u.pos, u.pos, 0, 0.6f, rgb(220, 240, 255), 18.0f});
        destroy(u, true);
    }
    // and every ground route that ran over it is planned again
    for (auto& u : ents) {
        if (!u.alive || !u.isUnit() || u.isAir() || u.ut().naval || u.path.empty()) continue;
        for (size_t k = u.pathIdx; k < u.path.size(); k++) {
            const Vec2& w = u.path[k];
            if (w.x >= x0 - TILE && w.x < x1 + TILE && w.y >= y0 && w.y < y1) { u.path.clear(); u.pathIdx = 0; u.repathTimer = 0; break; }
        }
    }
    for (int k = 0; k < 14; k++) {
        Vec2 p(x0 + rng.f(4, BRIDGE_W * TILE - 4), y0 + TILE * (2.0f + rng.f(0, BRIDGE_L - 4.0f)));
        fx.push_back({FX_EXPLODE, p, p, -rng.f(0, 0.5f), 0.8f, rgb(255, 170, 60), rng.f(14, 26)});
        fx.push_back({FX_SMOKE, p, p, -rng.f(0, 0.6f), 2.8f, rgb(66, 60, 54), rng.f(12, 22), Vec2(rng.f(-8, 8), rng.f(-30, -12))});
    }
    for (int k = 0; k < 6; k++) { Vec2 p(x0 + BRIDGE_W * TILE * 0.5f, y0 + TILE * (3.5f + k)); fx.push_back({FX_RING, p, p, -k * 0.08f, 0.9f, rgb(226, 240, 250), 26.0f}); }
    emit(EV_SOUND, -1, SND_COLLAPSE, b.pos);
    emit(EV_SOUND, -1, SND_SPLASH, b.pos);
    for (int p = 0; p < numPlayers; p++) if (exploredRaw(p, tileOf(b.pos.x), tileOf(b.pos.y))) emit(EV_MSG, p, SND_NONE, b.pos, p == by ? "We brought a bridge down" : "A bridge over the canal is down");
}

void Sim::raiseBridge(Entity& b) {
    b.constructed = true; b.progress = 1; b.hp = b.maxHp;
    deckTiles(b, true);
    for (int p = 0; p < numPlayers; p++) if (exploredRaw(p, tileOf(b.pos.x), tileOf(b.pos.y))) emit(EV_MSG, p, SND_NONE, b.pos, "A bridge over the canal is open again");
    emit(EV_SOUND, -1, SND_BUILD_DONE, b.pos);
}

// ------------------------------------------------------------ the relic bases
// A relic structure nobody holds is dormant and open: an army's infantry walk in and claim it (slower than a spy, RELIC_CLAIM), and from then on it
// is theirs like any structure they built - until an enemy spy takes it or it is destroyed.
bool Sim::relicClaimable(const Entity& u, const Entity& b) const {
    return relicEnt(b) && b.owner < 0 && b.constructed && u.isUnit() && u.ut().kind == UK_INF && u.owner >= 0 && u.owner < numPlayers && u.ut().weapon >= 0 && !u.ut().sniper && !u.ut().temp;
}

bool Sim::relicBuildable(int player) const {
    if (player < 0 || player >= numPlayers) return false;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.type == B_R_COMMAND) return true;
    return false;
}

float Sim::captureTime(const Entity& b, const Entity& by) const {
    float t = captureTime(b);
    if (by.isUnit() && by.ut().role != UR_SPY && !chosenType(by.type)) t *= RELIC_CLAIM;   // (plain infantry claiming a relic structure nobody holds)
    t *= 1.0f + BUNKER_SNIPER_CAPTURE * (float)bunkerSnipers(b);   // a bunker's snipers hold the shaft against an intruder (each one adds)
    return t;
}

int Sim::bunkerSnipers(const Entity& b) const {
    if (!b.isBuilding() || b.bt().role != BR_BUNKER || b.owner < 0 || b.passengers.empty()) return 0;
    int n = 0;
    for (Ref r : b.passengers) if (const Entity* u = get(r)) if (u->isUnit() && u->ut().sniper) n++;
    return std::min(n, BUNKER_SNIPER_CAP);
}

void Sim::updateRelic(Entity& b) {
    if (b.type == B_R_COMMAND) {   // the stipend of a held Relic Command
        if (b.relicPay <= 0) b.relicPay = time + RELIC_INCOME_INTERVAL;
        if (time >= b.relicPay) {
            b.relicPay = time + RELIC_INCOME_INTERVAL;
            int amt = (int)(RELIC_COMMAND_INCOME * players[b.owner].econMul());
            players[b.owner].money += amt; players[b.owner].mined += amt;
            fx.push_back({FX_SPARK, b.pos, b.pos, 0, 0.6f, RELIC_GLOW, 9});
        }
    }
}

// The Hydra: one missile from the silo that comes down as RELIC_SILO_WARHEADS warheads, one on the aim point and the rest round it
bool Sim::cmdRelicNuke(int player, Vec2 pos, bool force, Ref from) {
    if (player < 0 || player >= numPlayers) return false;
    force = force && !players[player].isAI;
    auto ready = [&](const Entity& e) { return e.alive && e.isBuilding() && e.owner == player && e.constructed && e.bt().role == BR_RELICSILO && time >= e.siloTimer && e.disabledUntil <= time; };
    Entity* l = get(from);
    if (!l || !ready(*l)) {
        l = nullptr;
        for (auto& e : ents) if (ready(e)) { l = &e; break; }
    }
    if (!l) return false;
    l->siloTimer = time + RELIC_SILO_COOLDOWN;
    const float spread = RELIC_SILO_SPREAD * TILE;
    const float a0 = rng.f(0, 6.2831853f);
    for (int k = 0; k < RELIC_SILO_WARHEADS; k++) {
        Nuke n; n.from = l->pos; n.owner = player; n.t = -0.06f * k; n.force = force; n.scale = RELIC_SILO_SCALE; n.hydra = true;
        if (k == 0) n.pos = pos;
        else { float a = a0 + 6.2831853f * (k - 1) / (RELIC_SILO_WARHEADS - 1); n.pos = pos + Vec2(std::cos(a), std::sin(a)) * spread; }
        n.pos.x = clampf(n.pos.x, TILE, WORLD_W - TILE); n.pos.y = clampf(n.pos.y, TILE, WORLD_H - TILE);
        n.id = ++nukeSeq;
        nukes.push_back(n);
    }
    emit(EV_SOUND, -1, SND_ROCKET, l->pos);
    emit(EV_SOUND, -1, SND_EXPLODE_S, l->pos);
    for (int k = 0; k < 14; k++) fx.push_back({FX_SMOKE, l->pos + Vec2(rng.f(-24, 24), rng.f(-16, 16)), Vec2(), -rng.f(0, 0.3f), 3.0f, rgb(210, 222, 206), rng.f(12, 24), Vec2(rng.f(-26, 26), rng.f(-34, -8))});
    fx.push_back({FX_FLASH, l->pos, l->pos, 0, 0.35f, RELIC_GLOW, 46});
    emit(EV_MSG, player, SND_NONE, l->pos, "Hydra away: five warheads on the target");
    for (int p = 0; p < numPlayers; p++) if (enemies(player, p)) {
        emit(EV_MSG, p, SND_ATTACKED, pos, "HYDRA LAUNCH DETECTED"); emit(EV_UNDER_ATTACK, p, SND_ATTACKED, pos, "A Hydra missile from an old silo is coming down as five warheads!");
    }
    return true;
}
int Sim::relicNukesReady(int player) const {
    int n = 0;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.bt().role == BR_RELICSILO && time >= e.siloTimer && e.disabledUntil <= time) n++;
    return n;
}
float Sim::relicNukeWait(int player) const {
    float best = -1;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.bt().role == BR_RELICSILO) {
        float w = std::max(0.0f, e.siloTimer - time);
        if (best < 0 || w < best) best = w;
    }
    return best;
}

// The secret islands' structures are found only by looking: a unit or structure of the army that has one within its own sight (a scan, or the whole
// map being revealed, finds nothing). Once found it stays found for that army.
void Sim::updateDiscovery() {
    for (auto& s : ents) {
        if (!s.alive || !s.secret) continue;
        u8 all = 0; for (int p = 0; p < numPlayers; p++) all |= (u8)(1u << p);
        if ((s.discovered & all) == all) continue;
        float reach = 12.0f * TILE;
        forEachNear(s.pos, reach, [&](Entity& u) {
            if (u.owner < 0 || u.owner >= numPlayers || (s.discovered >> u.owner & 1) || u.kind == EK_RESOURCE || u.carrier.valid()) return;
            if (u.isBuilding() && !u.constructed) return;
            float d = distToEntity(u.pos, s);
            if (d <= u.sight() * TILE) {
                for (int q = 0; q < numPlayers; q++) if (players[q].team == players[u.owner].team) s.discovered |= (u8)(1u << q);
                emit(EV_MSG, u.owner, SND_NONE, s.pos, s.bt().role == BR_RELICSILO ? "Found a hidden relic silo on the island" : "Found a secret relic bunker on the island");
            }
        });
    }
}

// ------------------------------------------------------------ surrender
// A computer army that has nothing left to fight on with: not one structure (relic ones and its share of anything included; a Bastion Wall alone is
// no army), no dozer to found a base with, no tank, super-heavy or warship, no heavy aircraft, and its few remaining light units worth less than
// SURRENDER_VALUE. Its cash does not save it: without a structure or a dozer it cannot spend it. A submarine keeps it in the fight only together
// with a structure or a dozer: on its own (or with light units) it is no army, since a boat out of its yard cannot build or repair anything, and
// its price does not count against the light units either.
bool Sim::armyBeaten(int p) const {
    float light = 0;
    for (auto& e : ents) {
        if (!e.alive || e.owner != p || e.kind == EK_RESOURCE) continue;
        if (e.isBuilding()) { if (e.district >= 0 || invulnerable(e) || prizeType(e.type)) continue; return false; }   // (a prize landmark it holds is no base: it cannot build or repair anything)
        const UnitType& ut = e.ut();
        if (ut.temp || civicDozerType(e.type)) continue;
        if (subType(e.type)) continue;   // (a submarine alone does not keep an army in the fight: see above)
        if (ut.role == UR_DOZER || chosenType(e.type)) return false;
        if (ut.naval && e.type != U_C_PATROL && e.type != U_K_PATROL) return false;
        if (ut.kind == UK_VEH && (ut.armor == AR_HEAVY || ut.cost >= 1500)) return false;
        if (ut.kind == UK_AIR && ut.cost >= 2000) return false;
        light += (float)ut.cost;
        for (Ref r : e.passengers) if (const Entity* q = get(r)) { if (q->ut().role == UR_DOZER) return false; light += (float)q->ut().cost; }
    }
    return light < SURRENDER_VALUE;
}

void Sim::surrender(int p) {
    Player& pl = players[p];
    char msg[96];
    snprintf(msg, sizeof msg, "The %s has surrendered", FACTION_NAME[pl.faction]);
    for (int q = 0; q < numPlayers; q++) emit(EV_MSG, q, SND_NONE, pl.basePos, q == p ? "We have nothing left to fight with: we surrender" : msg);
    for (int pass = 0; pass < 4; pass++) {   // (a transport that goes lets its passengers out: they lay down their arms on the next pass)
        bool left = false;
        for (size_t i = 0; i < ents.size(); i++) {
            Entity& e = ents[i];
            if (!e.alive || e.owner != p || e.kind == EK_RESOURCE || e.isBuilding()) continue;   // (only a Bastion Wall or a town can be left: checkVictory hands them back to nobody)
            fx.push_back({FX_RING, e.pos, e.pos, 0, 0.9f, rgb(250, 250, 250), 16.0f});
            destroy(e, false);
            left = true;
        }
        if (!left) break;
    }
}
