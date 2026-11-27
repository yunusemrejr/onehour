// The Ghosts: the submarine leads, and Ghost2 sightings are the only way to find either half.
#include "sim.h"
#include <limits>

namespace {
constexpr float SPAWN_DISTANCE = 60 * TILE, SLIP_DISTANCE = 40 * TILE, MIN_SLIP_DISTANCE = 20 * TILE;
constexpr float CLOAK_MIN = 10.0f, CLOAK_MAX = 60.0f;

// Reservoir-sample the eligible open sea; when the map cannot fit the requested distance, take its farthest tile, provided that tile is at
// least fallbackMin away (the spawn takes the farthest tile whatever it is; a relocation refuses a short hop).
bool farSea(Rng& rng, const Vec2* avoid, int count, float minDistance, Vec2& out, float fallbackMin = 0) {
    int eligible = 0;
    float farthest = -1;
    for (int y = 1; y < MAP_H - 1; y++) for (int x = 1; x < MAP_W - 1; x++) {
        if (!g_map.openSea(x, y) || !g_map.navOpen(x, y)) continue;
        const Vec2 at = tileCenter(x, y);
        float gap = std::numeric_limits<float>::infinity();
        for (int i = 0; i < count; i++) gap = std::min(gap, dist2(at, avoid[i]));
        if (gap >= minDistance * minDistance) {
            if (rng.range(1, ++eligible) == 1) out = at;
        } else if (!eligible && gap > farthest) {
            farthest = gap; out = at;
        }
    }
    return eligible != 0 || (farthest >= 0 && farthest >= fallbackMin * fallbackMin);
}

void resetPosition(Entity& e, Vec2 at) {
    e.pos = e.prevPos = e.lastPos = e.guardPos = e.target = at;
    e.order = e.postOrder = e.resumeOrder = O_IDLE;
    e.engaged = e.targetEnt = e.forceTarget = NOREF;
    e.path.clear(); e.pathIdx = 0; e.repathTimer = e.stuckTimer = 0;
}
}

bool Sim::ghostsInProduction(int player) const {
    if (player < 0 || player >= numPlayers) return false;
    for (const auto& e : ents) {
        if (!e.alive || e.owner != player) continue;
        if (e.isUnit() && ghostType(e.type)) return true;
        if (e.isBuilding()) for (int t : e.queue) if (ghostType(t)) return true;
    }
    for (const auto& a : airlifts) if (a.owner == player)
        for (size_t k = a.next; k < a.load.size(); k++) if (ghostType(a.load[k])) return true;
    return false;
}

const char* Sim::ghostTrainBlockReason(int player) const {
    if (player < 0 || player >= numPlayers) return "Invalid army";
    if (!hasRole(player, BR_AIRFIELD)) return "Requires a finished Airfield";
    if (!hasRole(player, BR_NAVAL)) return "Requires a finished Naval Yard";
    if (ghostsInProduction(player)) return "Only one Ghosts pair per army";
    return nullptr;
}

void Sim::spawnGhostPair(int owner, int subTypeId, Vec2 airfield) {
    Vec2 bases[MAX_PLAYERS], at;
    for (int p = 0; p < numPlayers; p++) bases[p] = players[p].basePos;
    if (!farSea(rng, bases, numPlayers, SPAWN_DISTANCE, at)) {
        players[owner].money += UNITS[subTypeId].cost;
        players[owner].spentOn[subTypeId] -= UNITS[subTypeId].cost;
        emit(EV_MSG, owner, SND_CANT, airfield, "The Ghosts need navigable open sea");
        return;
    }
    const Ref sub = spawnUnit(subTypeId, owner, at);
    const Ref jet = spawnUnit(subTypeId == U_C_GHOSTSUB ? U_C_GHOSTJET : U_K_GHOSTJET, owner, at);
    get(sub)->partner = jet;
    get(jet)->partner = sub;
    get(jet)->angle = get(sub)->angle;
    ghosts[owner] = {sub, jet, time + GHOST_FIRST_STRIKE, 0, {}};
    emit(EV_UNIT_READY, owner, SND_UNIT_READY, at, "The Ghosts");
}

// True for the pair's own team, and for the team that holds a live Ghost2 sighting. Nobody else ever sees either half.
bool Sim::ghostSightedBy(const Entity& e, int player) const {
    if (player < 0 || player >= numPlayers || e.owner < 0 || e.owner >= numPlayers) return false;
    if (players[player].team == players[e.owner].team) return true;
    const GhostPair& g = ghosts[e.owner];
    const GhostSighting& s = g.sighting;
    return s.until > time && players[player].team == s.team && (&e == get(g.sub) || &e == get(g.jet));
}

bool Sim::cmdGhostStrike(int player, Vec2 at) {
    if (player < 0 || player >= numPlayers || !std::isfinite(at.x) || !std::isfinite(at.y)
        || at.x < 0 || at.y < 0 || at.x >= WORLD_W || at.y >= WORLD_H) return false;
    GhostPair& g = ghosts[player];
    const Entity* sub = get(g.sub);
    const Entity* jet = get(g.jet);
    if (!sub || !jet || time < g.strikeReadyAt || sub->disabledUntil > time || jet->disabledUntil > time) return false;
    Nuke n; n.pos = at; n.owner = player; n.t = 0;
    n.from = sub->pos; n.scale = SUB_NUKE_SCALE * 4; n.dmgMul = 2; n.sub = n.deep = true;
    nukes.push_back(n);
    n.from = jet->pos; n.scale = NUKEBOMBER_SCALE * 9; n.dmgMul = 5; n.sub = n.deep = false;
    nukes.push_back(n);
    g.strikeReadyAt = time + GHOST_STRIKE_COOLDOWN;
    return true;
}

Entity* Sim::ghostOrderLeader(Entity* e) {
    if (e && e->isUnit() && ghostJetType(e->type)) return get(e->partner);
    return e;
}

void Sim::moveGhostSub(Entity& e, Vec2 dest, bool attackMove) {
    if (!std::isfinite(dest.x) || !std::isfinite(dest.y)) return;
    const Vec2 from = g_map.nearestWater(e.pos, 6);
    const int sx = tileOf(from.x), sy = tileOf(from.y);
    if (!g_map.navOpen(sx, sy)) return;
    if (g_map.navFlagGen != g_map.navGen) {
        const bool changed = g_map.buildNavFlags();
        if (!changed && g_map.navLabelGen == g_map.navFlagGen) g_map.navLabelGen = g_map.navGen;
        g_map.navFlagGen = g_map.navGen;
    }
    if (g_map.navLabelGen != g_map.navFlagGen) g_map.buildNavLabels();
    const u32 stretch = g_map.navLabel[sy * MAP_W + sx];
    if (!stretch) return;
    dest = Vec2(clampf(dest.x, 0, WORLD_W - 1), clampf(dest.y, 0, WORLD_H - 1));
    if (g_map.navLabel[tileOf(dest.y) * MAP_W + tileOf(dest.x)] != stretch) {
        float best = std::numeric_limits<float>::infinity();
        Vec2 closest = from;
        for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
            if (g_map.navLabel[y * MAP_W + x] != stretch) continue;
            const Vec2 at = tileCenter(x, y);
            const float d = dist2(at, dest);
            if (d < best) { best = d; closest = at; }
        }
        dest = closest;
    }
    e.zoneR = 0; e.leashed = false;
    e.order = attackMove ? O_ATTACKMOVE : O_MOVE; e.postOrder = O_IDLE;
    e.target = dest; e.targetEnt = e.engaged = e.forceTarget = NOREF; e.actionTimer = 0;
    requestPath(e, dest);
}

void Sim::huntGhost2(Entity& e) {
    // Compare dispersed sea samples with the other drones' destinations, so two idle drones do not pick the same patrol.
    std::vector<Vec2> patrols;
    for (const auto& other : ents) if (&other != &e && other.alive && other.isUnit() && other.owner == e.owner && ghost2Type(other.type))
        patrols.push_back(other.order == O_MOVE || other.order == O_ATTACKMOVE ? other.target : other.pos);
    Vec2 best;
    float score = -1;
    for (int i = 0; i < 24; i++) {
        const int x = rng.range(1, MAP_W - 2), y = rng.range(1, MAP_H - 2);
        if (!g_map.openSea(x, y) || !g_map.navOpen(x, y)) continue;
        const Vec2 at = tileCenter(x, y);
        if (dist2(at, e.pos) < (6 * TILE) * (6 * TILE)) continue;
        float gap = (40 * TILE) * (40 * TILE);
        for (Vec2 p : patrols) gap = std::min(gap, dist2(at, p));
        if (gap > score) { best = at; score = gap; }
    }
    if (score < 0 && !farSea(rng, &e.pos, 1, 6 * TILE, best)) return;
    if (dist2(e.pos, best) < TILE * TILE) return;
    e.order = O_MOVE; e.postOrder = O_IDLE; e.target = best;
    e.targetEnt = e.engaged = e.forceTarget = NOREF; e.autoTask = true; e.loiterUntil = -1;
}

void Sim::updateGhosts() {
    for (int owner = 0; owner < numPlayers; owner++) {
        GhostPair& g = ghosts[owner];
        if (!g.sub.valid() && !g.jet.valid()) continue;
        Entity* sub = get(g.sub);
        Entity* jet = get(g.jet);
        if (!sub || !jet) {   // the pair lives and dies together: the survivor ditches or scuttles, and a new pair can be trained
            Entity* survivor = sub ? sub : jet;
            if (survivor) { emit(EV_MSG, owner, SND_ATTACKED, survivor->pos, "The Ghosts are lost"); destroy(*survivor, true); }
            g = {};
            continue;
        }
        Entity* lead = sub;
        jet->pos = sub->pos; jet->angle = sub->angle;
        GhostSighting& s = g.sighting;
        if (s.until > 0) {
            s.at = lead->pos;
            if (time >= s.until) {
                Vec2 at;
                // A relocation of 40+ tiles (20+ at the least when the map has no such stretch of sea) ends in a cloak; with no sea that far
                // the pair stays put and is not cloaked, so a Ghost2 beside it sights it again.
                if (farSea(rng, &lead->pos, 1, SLIP_DISTANCE, at, MIN_SLIP_DISTANCE)) {
                    resetPosition(*sub, at);
                    resetPosition(*jet, at);
                    g.cloakUntil = time + rng.f(CLOAK_MIN, CLOAK_MAX);
                }
                s = {};
            }
        }
    }
}

void Sim::detectGhosts() {
    constexpr float range = GHOST_DETECT * TILE;
    for (int owner = 0; owner < numPlayers; owner++) {
        GhostPair& g = ghosts[owner];
        if ((!g.sub.valid() && !g.jet.valid()) || g.cloakUntil > time || g.sighting.until > 0) continue;
        const Entity* lead = get(g.sub);
        if (!lead) lead = get(g.jet);
        if (!lead) continue;
        int detector = numPlayers;
        forEachNearMask(lead->pos, range, enemyMask(owner), [&](Entity& d) {
            if (!d.isUnit() || !ghost2Type(d.type) || !(d.ut().detectMask & DET_GHOST) || d.disabledUntil > time || d.fall > 0
                || d.carrier.valid() || !enemies(owner, d.owner) || dist2(lead->pos, d.pos) > range * range) return;
            detector = std::min(detector, d.owner);
        });
        if (detector == numPlayers) continue;
        const int team = players[detector].team;
        g.sighting = {team, time + GHOST_SIGHT_TIME, lead->pos};
        for (int p = 0; p < numPlayers; p++) if (players[p].team == team)
            emit(EV_MSG, p, SND_ATTACKED, lead->pos, "Enemy Ghosts sighted - 10 s to strike");
        emit(EV_MSG, owner, SND_NONE, lead->pos, "Our Ghosts are spotted - slipping away in 10 s");
    }
}
