// One Hour - three calls of the human's command structures: the tech structure's Gunship Call, every bunker's own Satellite Scan and the
// Command Center's Find and Destroy. The rest of the simulation is in sim.cpp and its sister files; they all share the Sim class.
#include "sim.h"
#include <map>

static inline float distSq(Vec2 a, Vec2 b) { Vec2 d = a - b; return d.x * d.x + d.y * d.y; }

// ------------------------------------------------------------ the satellite pass
// The whole map is visible to the army for 'duration' seconds; a human's map is charted for good.
void Sim::satellitePass(int player, Vec2 from, float duration, const char* name) {
    Player& pl = players[player];
    pl.revealUntil = std::max(pl.revealUntil, time + duration);
    if (!pl.isAI) std::fill(pl.explored.begin(), pl.explored.end(), 1);
    fx.push_back({FX_EMP, from, from, 0, 1.6f, pl.faction == F_CYBER ? rgb(110, 230, 255) : rgb(222, 178, 60), (float)WORLD_W * 0.6f});
    emit(EV_SOUND, player, SND_AIR, from);
    emit(EV_MSG, player, SND_NONE, from, (std::string(name) + ": the whole map is visible").c_str());
}

// ------------------------------------------------------------ Satellite Scan (bunkers)
// Every finished bunker has its own uplink: it calls the same satellite pass as the tech structure's scan, once per BUNKER_SCAN_COOLDOWN.
// Any ready bunker of the army answers, so the key calls the next one.
static bool scanBunker(const Entity& e, int player, float time) {
    return e.alive && e.isBuilding() && e.owner == player && e.constructed && e.bt().role == BR_BUNKER && e.disabledUntil <= time;
}

int Sim::bunkerScansReady(int player) const {
    if (player < 0 || player >= numPlayers || !players[player].alive) return 0;
    int n = 0;
    for (auto& e : ents) if (scanBunker(e, player, time) && time >= e.scanTimer) n++;
    return n;
}

float Sim::bunkerScanWait(int player) const {
    if (player < 0 || player >= numPlayers) return -1;
    float best = -1;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.bt().role == BR_BUNKER) {
        float w = std::max(0.0f, e.scanTimer - time);
        if (best < 0 || w < best) best = w;
    }
    return best;
}

bool Sim::cmdBunkerScan(int player, Ref prefer) {
    if (player < 0 || player >= numPlayers || !players[player].alive) return false;
    Entity* b = get(prefer);
    if (!b || !scanBunker(*b, player, time) || time < b->scanTimer) {
        b = nullptr;
        for (auto& e : ents) if (scanBunker(e, player, time) && time >= e.scanTimer) { b = &e; break; }
    }
    if (!b) return false;
    b->scanTimer = time + BUNKER_SCAN_COOLDOWN;
    satellitePass(player, b->pos, SCANS[players[player].faction].duration, "Satellite Scan");
    return true;
}

// ------------------------------------------------------------ Gunship Call (tech structures)
// Like Fleet Call for the air: HELI_CALL_COUNT of the army's gunships come in from the stretch of map border nearest the spot, five abreast in
// five waves, and attack-move to a square formation round it. From then on they are the army's own.
static int gunshipType(Faction f) { return f == F_CYBER ? U_C_HELI : U_K_AIR; }

static bool heliTech(const Entity& e, int player, float time) {
    return e.alive && e.isBuilding() && e.owner == player && e.constructed && e.disabledUntil <= time && e.bt().role == BR_TECH;
}

int Sim::heliCallsReady(int player) const {
    if (player < 0 || player >= numPlayers || !players[player].alive || time < players[player].dropReady) return 0;
    int n = 0;
    for (auto& e : ents) if (heliTech(e, player, time) && time >= e.heliTimer) n++;
    return n;
}

float Sim::heliCallWait(int player) const {
    if (player < 0 || player >= numPlayers) return -1;
    float best = -1;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.bt().role == BR_TECH) {
        float w = std::max(0.0f, std::max(e.heliTimer, players[player].dropReady) - time);
        if (best < 0 || w < best) best = w;
    }
    return best;
}

bool Sim::cmdHeliCall(int player, Vec2 pos) {
    if (player < 0 || player >= numPlayers) return false;
    Player& pl = players[player];
    if (!pl.alive || time < pl.dropReady) return false;
    Entity* tech = nullptr;
    for (auto& e : ents) if (heliTech(e, player, time) && time >= e.heliTimer) { tech = &e; break; }
    if (!tech) return false;
    const float m = 1.5f * TILE;
    pos = Vec2(clampf(pos.x, 3.0f * TILE, WORLD_W - 3.0f * TILE), clampf(pos.y, 3.0f * TILE, WORLD_H - 3.0f * TILE));
    tech->heliTimer = time + HELI_CALL_COOLDOWN;
    // the border nearest the spot; the column comes in level with it
    struct Side { float d; Vec2 n; } sides[4] = { { pos.x, Vec2(1, 0) }, { WORLD_W - pos.x, Vec2(-1, 0) }, { pos.y, Vec2(0, 1) }, { WORLD_H - pos.y, Vec2(0, -1) } };
    std::sort(sides, sides + 4, [](const Side& a, const Side& b) { return a.d < b.d; });
    const Vec2 n = sides[0].n, along(-n.y, n.x);
    const Vec2 entry = n.x > 0 ? Vec2(m, pos.y) : n.x < 0 ? Vec2(WORLD_W - m, pos.y) : n.y > 0 ? Vec2(pos.x, m) : Vec2(pos.x, WORLD_H - m);
    const int type = gunshipType(pl.faction), ABREAST = 5;
    for (int k = 0; k < HELI_CALL_COUNT; k++) {
        int row = k / ABREAST, col = k % ABREAST - ABREAST / 2;
        FleetShip f; f.owner = player; f.type = type; f.dir = n; f.air = true;
        Vec2 from = entry + along * (col * 1.8f * TILE);
        f.from = Vec2(clampf(from.x, m, WORLD_W - m), clampf(from.y, m, WORLD_H - m));
        Vec2 to = pos + along * (col * 2.4f * TILE) + n * ((ABREAST / 2 - row) * 2.4f * TILE);   // a 5 x 5 square on the spot, the first wave furthest in
        f.to = Vec2(clampf(to.x, m, WORLD_W - m), clampf(to.y, m, WORLD_H - m));
        f.at = time + 0.3f + row * 1.1f + 0.12f * std::abs(col);
        fleetArrivals.push_back(f);
    }
    const char* edge = n.x > 0 ? "west" : n.x < 0 ? "east" : n.y > 0 ? "north" : "south";
    char msg[160];
    snprintf(msg, sizeof msg, "Gunship Call: %d %ss inbound from the %s edge of the map", HELI_CALL_COUNT, UNITS[type].name, edge);
    emit(EV_MSG, player, SND_NONE, pos, msg);
    for (int p = 0; p < numPlayers; p++) if (enemies(player, p)) emit(EV_MSG, p, SND_ATTACKED, entry, "ENEMY GUNSHIPS INBOUND");
    return true;
}

// ------------------------------------------------------------ bunker garrison and drop escorts
// A bunker is finished with its own garrison already in the shaft: snipers (each multiplies an infiltrator's work, see captureTime) and riflemen.
void Sim::bunkerStartGarrison(Entity& b) {
    if (b.owner < 0 || b.owner >= numPlayers || !b.isBuilding() || b.bt().role != BR_BUNKER) return;
    const bool cyber = players[b.owner].faction == F_CYBER;
    const Ref bunker = refOf(b);
    for (int k = 0; k < BUNKER_START_SNIPERS + BUNKER_START_RIFLES; k++) {
        Ref u = spawnUnit(k < BUNKER_START_SNIPERS ? (cyber ? U_C_SNIPER : U_K_SNIPER) : (cyber ? U_C_INF1 : U_K_INF1), b.owner, b.pos);
        Entity* c = get(bunker); Entity* s = get(u);   // (spawning may move ents: look both up again)
        if (c && s) boardQuiet(*s, *c);
    }
}

// Every Para Backup and Tank Drop of a bunker brings DROP_ESCORT_HELIS gunships along the plane's track; they fight round the drop zone.
void Sim::dropEscort(int player, Vec2 from, Vec2 dir, Vec2 pos) {
    if (player < 0 || player >= numPlayers) return;
    const float m = 1.5f * TILE;
    const Vec2 side(-dir.y, dir.x);
    for (int k = 0; k < DROP_ESCORT_HELIS; k++) {
        const float o = (k - (DROP_ESCORT_HELIS - 1) * 0.5f) * 3.0f * TILE;
        FleetShip f; f.owner = player; f.type = gunshipType(players[player].faction); f.dir = dir; f.air = true; f.escort = true;
        Vec2 a = from + side * o, b = pos + side * o;
        f.from = Vec2(clampf(a.x, m, WORLD_W - m), clampf(a.y, m, WORLD_H - m));
        f.to = Vec2(clampf(b.x, m, WORLD_W - m), clampf(b.y, m, WORLD_H - m));
        f.at = time + 0.4f + 0.3f * k;
        fleetArrivals.push_back(f);
    }
}

// ------------------------------------------------------------ Find and Destroy (human armies)
// Every armed unit of the army, wherever it is, goes after the nearest enemy thing it can reach and hit: units, ships, dived submarines, aircraft,
// structures and the ones that hide. Hunting units find hidden enemies near them (stealth units, dived boats, the structures of a secret island).
// A unit that finishes its prey takes the next; one sent elsewhere by its player rejoins once it is idle again. It ends when nothing is left.
void Sim::cmdFindDestroy(int player, bool on) {
    if (player < 0 || player >= numPlayers || players[player].isAI || !players[player].alive) return;
    Player& pl = players[player];
    pl.hunting = on; pl.huntFresh = on;
    for (auto& e : ents) if (e.alive && e.owner == player && e.isUnit()) { e.huntRef = NOREF; e.huntSkip = NOREF; }
    emit(EV_MSG, player, SND_NONE, pl.basePos, on ? "Find and Destroy: every unit is out to hunt down every enemy thing" : "Find and Destroy called off");
}

void Sim::updateHunts() {
    const int every = std::max(1, (int)(0.75f / SIM_DT));
    if (tick % every) return;
    for (int p = 0; p < numPlayers; p++) {
        Player& pl = players[p];
        if (!pl.hunting) continue;
        if (!pl.alive || pl.isAI) { pl.hunting = false; continue; }
        struct Prey { int idx; int land, sea; bool air, naval; };
        std::vector<Prey> prey;
        for (int i = 0; i < (int)ents.size(); i++) {
            const Entity& t = ents[i];
            if (!t.alive || t.kind == EK_RESOURCE || t.owner < 0 || !enemies(p, t.owner) || t.carrier.valid()) continue;
            if (invulnerable(t) || t.district >= 0 || (t.isBuilding() && prizeType(t.type))) continue;   // (what the defeat rule does not count as an army either)
            Prey q; q.idx = i; q.air = t.isAir(); q.naval = t.isUnit() && t.ut().naval;
            q.land = g_map.landNear(t.pos, 2);
            q.sea = q.naval ? g_map.compAt(t.pos) : g_map.compAt(g_map.nearestWater(t.pos, 6));
            prey.push_back(q);
        }
        if (prey.empty()) {
            pl.hunting = false;
            emit(EV_MSG, p, SND_UNIT_READY, pl.basePos, "Find and Destroy: nothing of the enemy is left");
            continue;
        }
        const bool fresh = pl.huntFresh;
        pl.huntFresh = false;
        std::vector<int> hunters;
        for (int i = 0; i < (int)ents.size(); i++) {
            const Entity& h = ents[i];
            if (!h.alive || h.owner != p || !h.isUnit() || h.weapon() < 0 || h.carrier.valid() || h.fall > 0 || h.ut().temp || h.lifeUntil >= 0) continue;
            hunters.push_back(i);
        }
        // the hunters find what hides near them
        const float sight2 = (HUNT_SIGHT * TILE) * (HUNT_SIGHT * TILE);
        for (int hi : hunters) {
            const Vec2 hp = ents[hi].pos;
            for (const Prey& q : prey) {
                Entity& t = ents[q.idx];
                if (distSq(hp, t.pos) > sight2) continue;
                t.detUntil[p] = std::max(t.detUntil[p], time + 2.5f);
                if (t.secret) t.discovered |= (u8)(1 << p);
            }
        }
        std::map<int, std::vector<Ref>> moves, attacks;   // (by prey: one order per group, so a crowd shares its path)
        for (int hi : hunters) {
            Entity& h = ents[hi];
            const Ref hr = refOf(h);
            Entity* cur = get(h.huntRef);
            if (cur && (!enemies(p, cur->owner) || cur->carrier.valid())) cur = nullptr;
            if (cur && time - h.huntSince > HUNT_GIVEUP) { h.huntSkip = h.huntRef; cur = nullptr; }
            const bool ours = h.order == O_ATTACK || h.order == O_ATTACKMOVE;
            const bool idle = h.order == O_IDLE || h.order == O_GUARDPOS || h.order == O_GUARDAREA;
            if (cur) {
                // still on it: once it is in sight and in reach of a direct attack, switch from the attack-move to the attack itself
                if (h.order == O_ATTACKMOVE && visibleTo(*cur, p) && canTarget(h, *cur, -1) && dist(h.pos, cur->pos) < 16.0f * TILE) attacks[(int)(cur - &ents[0])].push_back(hr);
                else if (!ours && idle) moves[(int)(cur - &ents[0])].push_back(hr);
                continue;
            }
            h.huntRef = NOREF;
            if (!(idle || (fresh && (h.order == O_MOVE || ours)) || (ours && h.huntSince > 0))) continue;   // (its player sent it somewhere: it rejoins once idle)
            const int w = h.weapon();
            const Weapon& wp = WEAPONS[w];
            const bool torpedo = w == W_TORPEDO_C || w == W_TORPEDO_K, air = h.isAir(), naval = h.ut().naval;
            const int hl = air || naval ? 0 : g_map.landNear(h.pos, 1), hs = naval ? g_map.compAt(h.pos) : 0;
            int best = -1; float bestD = 1e30f;
            for (const Prey& q : prey) {
                const Entity& t = ents[q.idx];
                if (h.huntSkip.valid() && h.huntSkip.idx == q.idx && h.huntSkip.gen == t.gen) continue;
                if (q.air ? (!wp.air || torpedo) : torpedo ? !q.naval : (!wp.ground && w != W_CONVERT)) continue;   // what its gun can hit
                if (!air && (naval ? (q.sea == 0 || q.sea != hs) : (hl == 0 || q.land != hl))) continue;               // where it can go
                float d = distSq(h.pos, t.pos);
                if (d < bestD) { bestD = d; best = q.idx; }
            }
            if (best < 0) continue;   // nothing left that this one can reach
            Entity& t = ents[best];
            h.huntRef = refOf(t); h.huntSince = time;
            if (visibleTo(t, p) && canTarget(h, t, -1)) attacks[best].push_back(hr);
            else moves[best].push_back(hr);
        }
        for (auto& kv : attacks) cmdAttack(kv.second, refOf(ents[kv.first]));
        for (auto& kv : moves) cmdMove(kv.second, ents[kv.first].pos, true);
        // the submarines join with their missiles: each one whose missile is ready launches it at the best enemy spot that spares the army
        for (auto& e : ents) {
            if (!e.alive || e.owner != p || !e.isUnit() || !subType(e.type) || time < e.siloTimer || e.nukeSalvo != 0 || e.disabledUntil > time) continue;
            Vec2 at;
            if (!huntNukeSpot(p, NUKE_RADIUS * TILE * std::sqrt(SUB_NUKE_SCALE), at)) break;   // (nothing worth a warhead: none of the others finds one either)
            cmdSubNuke(p, at, false, refOf(e));
        }
    }
}

// Where a hunting army's warhead of blast radius R does the most: enemy structures (a Command structure counts most) and big groups of enemy
// units, and never where the blast would reach a structure of the army or its allies, or more than HUNT_NUKE_OWN of their units.
bool Sim::huntNukeSpot(int p, float R, Vec2& out) const {
    const float R2 = R * R;
    std::vector<Vec2> cand;
    int u = 0;
    for (auto& t : ents) {
        if (!t.alive || t.kind == EK_RESOURCE || t.owner < 0 || !enemies(p, t.owner) || t.carrier.valid() || invulnerable(t) || t.district >= 0) continue;
        if (t.isBuilding() ? !prizeType(t.type) : (visibleTo(t, p) && (u++ % 4) == 0)) cand.push_back(t.pos);
        if (cand.size() >= 240) break;
    }
    float best = 0;
    for (Vec2 c : cand) {
        float score = 0; int own = 0; bool spare = true;
        for (auto& t : ents) {
            if (!t.alive || t.kind == EK_RESOURCE || t.owner < 0 || t.carrier.valid() || distSq(t.pos, c) > R2) continue;
            if (!enemies(p, t.owner)) {   // its own or an ally's
                if (t.isBuilding() || ++own > HUNT_NUKE_OWN) { spare = false; break; }
                continue;
            }
            if (invulnerable(t) || t.district >= 0) continue;
            score += t.isBuilding() ? (t.bt().role == BR_HQ ? 12.0f : 3.0f) : 1.0f;
        }
        if (spare && score > best) { best = score; out = c; }
    }
    return best >= HUNT_NUKE_MIN;
}
