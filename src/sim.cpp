#include "sim.h"

Sim g_sim;

static const float BUILD_REACH = 26.0f;     // px from structure edge for dozers
static const float HARVEST_REACH = 34.0f;   // px from the pile's edge: a hauler parked on a diagonal neighbour tile is about 29 px away
static const float PATH_INTERVAL = 0.6f;
static const float EMP_DURATION = 8.0f;

// ------------------------------------------------------------ init
void Sim::init(int nPlayers, const Faction* factions, const bool* isAI, const int* difficulties, const int* teams, u64 seed) {
    ents.clear(); freeList.clear(); projs.clear(); fx.clear(); events.clear(); storms.clear(); nukes.clear(); airlifts.clear(); aidDrops.clear(); fleetArrivals.clear(); fleetMarks.clear(); fallouts.clear(); interceptors.clear(); strikeMarks.clear(); strikeWaves.clear(); nukeSeq = 0; anyBunkerLasers = false;   // (strikeWaves: an Air Strike still coming in when a match ended used to fly into the next one)
    districts.clear(); riots.clear(); riotSeq = 0; riotsOn = riotsDefault && !getenv("ONEHOUR_NORIOTS");   // (ONEHOUR_NORIOTS=1: a match without riots)
    for (int p = 0; p < MAX_PLAYERS; p++) { unrest[p] = 0; control[p] = 1; riotsRaised[p] = 0; lossHeat[p] = 0; basePeak[p] = 0; riotNoteAt[p] = -100; }
    for (auto& c : grid) c.clear();
    ents.reserve(1024);
    time = 0; tick = 0; gameOver = false; winnerTeam = -1;
    rng = Rng(seed);
    g_map.generate();
    numPlayers = clampi(nPlayers, 1, MAX_ARMIES);
    // Assign start spots: human gets a random corner, enemies spread to the others
    int order[4] = {0, 1, 2, 3};
    for (int i = 3; i > 0; i--) std::swap(order[i], order[rng.range(0, i)]);
    for (int p = 0; p < MAX_PLAYERS; p++) players[p] = Player();
    for (int p = 0; p < numPlayers; p++) {
        Player& pl = players[p];
        pl.active = true; pl.alive = true; pl.isAI = isAI[p]; pl.difficulty = difficulties[p];
        pl.team = teams[p]; pl.faction = factions[p]; pl.money = START_CASH;
        if (pl.brutal()) pl.money += 6000;
        pl.startIdx = order[p];
        pl.explored.assign(MAP_W * MAP_H, 0);
        StartSpot s = g_map.starts[pl.startIdx];
        int hqType = firstBuildOf(pl.faction) + BR_HQ;
        int tx = s.tx - 2, ty = s.ty - 2;
        pl.basePos = buildingCenter(hqType, tx, ty);
        placeBuilding(hqType, p, tx, ty, true);
        // one dozer, ready to work
        Vec2 dz = pl.basePos + Vec2(0, BUILDS[hqType].h * TILE * 0.5f + 30);
        spawnUnit(firstUnitOf(pl.faction) + 0, p, g_map.nearestFree(dz));
    }
    {   // the riot crowd: a slot after the armies (not counted in numPlayers), on no army's team, so it fights them all and never wins or loses
        Player& rp = players[RIOT_PLAYER];
        rp.active = false; rp.alive = true; rp.isAI = true; rp.difficulty = 1; rp.team = 99; rp.faction = F_CLANKER; rp.money = 0;
        rp.explored.assign(MAP_W * MAP_H, 0); rp.basePos = Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f);
    }
    for (auto& s : g_map.supplies) spawnResource(s.tx, s.ty, s.amount);
    spawnMapLandmarks();   // the map's neutral civilian strongholds: empty until somebody garrisons them
    for (int p = 0; p < numPlayers; p++) { players[p].powerReady = 120.0f; players[p].scanReady = 90.0f; players[p].dropReady = 150.0f; } // powers unlock a couple of minutes in (and need the tech structure)
    rebuildGrid();
    updateVision(); updateSight();
    updatePower();
}

// ------------------------------------------------------------ entity creation
static int allocEnt(Sim& s) {
    if (!s.freeList.empty()) { int i = s.freeList.back(); s.freeList.pop_back(); return i; }
    s.ents.push_back(Entity());
    return (int)s.ents.size() - 1;
}

Ref Sim::spawnUnit(int type, int owner, Vec2 pos) {
    int i = allocEnt(*this);
    Entity& e = ents[i];
    u32 gen = e.gen + 1;
    e = Entity();
    e.gen = gen; e.alive = true; e.kind = EK_UNIT; e.type = type; e.owner = owner;
    const UnitType& t = UNITS[type];
    e.pos = t.kind == UK_AIR ? pos : (t.naval ? g_map.nearestWater(pos, 60, true) : g_map.nearestFree(pos));   // (a ship is always put on the lake or the ocean, however far it is from where it was asked for)
    e.lastPos = e.pos; e.prevPos = e.pos;
    e.hp = e.maxHp = t.hp;
    e.ammo = t.ammo;
    e.guardPos = e.pos;
    if (t.role == UR_SCOUT) e.autoHoldUntil = time + 18.0f;   // wait briefly at spawn for player orders before autonomous swarm search (spawnFromQueue sets 2.5s)
    e.angle = rng.f(0, 6.28f);
    e.turret = e.angle;
    if (type == U_C_CARRIER || type == U_K_CARRIER) e.deck.assign(CARRIER_WING, time);   // a new carrier comes with its whole wing on deck
    if (owner >= 0 && !t.temp) players[owner].unitsBuilt++;   // (the loans and a carrier's jets taking off again are not the army's production)
    return refOf(i);
}

Ref Sim::placeBuilding(int type, int owner, int tx, int ty, bool instant) {
    int i = allocEnt(*this);
    Entity& e = ents[i];
    u32 gen = e.gen + 1;
    e = Entity();
    e.gen = gen; e.alive = true; e.kind = EK_BUILDING; e.type = type; e.owner = owner;
    e.tx = tx; e.ty = ty;
    const BuildType& b = BUILDS[type];
    e.pos = buildingCenter(type, tx, ty);
    float hpMul = (!civilianRole(b.role) && hasUpgrade(owner, UPG_RUGGED) ? RUGGED_HP : 1.0f);
    if (!civilianRole(b.role) && hasUpgrade(owner, UPG_HYPER)) hpMul *= HYPER_HP;
    e.maxHp = b.hp * hpMul;   // (the civilian town gets no plating)
    e.constructed = instant;
    e.progress = instant ? 1.0f : 0.0f;
    e.hp = instant ? e.maxHp : e.maxHp * 0.1f;
    e.rally = e.pos + Vec2(0, b.h * TILE * 0.5f + 40);
    if (b.role == BR_BUNKER && instant) { e.hatchMax = e.hatch = BUNKER_HATCH_HP; e.siloTimer = time + MINI_NUKE_ARM; }
    if (b.role == BR_UNUKE && instant) { e.hatchMax = e.hatch = UNUKE_DOOR_HP; e.actionTimer = time + UNUKE_ARM; }
    if (b.role == BR_CIVIC && instant) e.dozerAt = time + CIVICS[civicKind(type)].dozerEvery;
    e.angle = -1.5708f;
    e.turret2 = 2.4f;   // (the roof gun idles looking out over a corner)
    if (b.role == BR_WAYMAKER) return refOf(i);   // a Waymaker is no obstacle: it may stand partly over a structure or water and clears only what is clearable (updateWaymaker)
    g_map.clearForStructure(tx, ty, b.w, b.h);   // trees, bushes and small rocks on the footprint are cleared away
    g_map.setStructure(tx, ty, b.w, b.h, true);
    // shove units off the foundation
    Ref self = refOf(i);
    forEachNear(e.pos, std::max(b.w, b.h) * TILE, [&](Entity& u) {
        if (!u.isUnit() || u.isAir()) return;
        int ux = tileOf(u.pos.x), uy = tileOf(u.pos.y);
        if (ux >= tx && ux < tx + b.w && uy >= ty && uy < ty + b.h) u.pos = g_map.nearestFree(u.pos, 8);
    });
    (void)self;
    return refOf(i);
}

Ref Sim::spawnResource(int tx, int ty, int amount) {
    int i = allocEnt(*this);
    Entity& e = ents[i];
    u32 gen = e.gen + 1;
    e = Entity();
    e.gen = gen; e.alive = true; e.kind = EK_RESOURCE; e.owner = -1;
    e.tx = tx; e.ty = ty; e.pos = tileCenter(tx, ty); e.amount = amount;
    e.hp = e.maxHp = 1;
    g_map.setResource(tx, ty, true);
    return refOf(i);
}

// The map's neutral civilian strongholds. They are ordinary buildings with owner -1: nobody may sell them, they have no production, and no
// army upgrade ever applies to them. The first infantryman to climb into one claims it (see Sim::board), and its windows become firing ports.
void Sim::spawnMapLandmarks() {
    static const int TYPES[7] = { B_C_CASTLE, B_C_CIVBUNKER, B_C_WATCHTOWER, B_C_COTTAGE, B_C_WORKSHOP, B_C_FORTRESS, B_C_ISLANDNUKE };
    for (auto& L : g_map.landmarks) {
        int type = TYPES[clampi(L.kind, 0, 6)];
        Ref r = placeBuilding(type, -1, L.tx, L.ty, true);
        if (Entity* e = get(r)) { e->constructed = true; e->progress = 1.0f; e->hp = e->maxHp; if (BUILDS[type].role == BR_ISLANDNUKE) e->siloTimer = time + ISLAND_NUKE_ARM; }
    }
    spawnDistricts();   // and the towns and cities round the lake
}

void Sim::deathFx(Entity& e) {
    float r = e.radius();
    if (e.isBuilding()) {
        for (int k = 0; k < 6 + (int)r / 8; k++) {
            Vec2 p = e.pos + Vec2(rng.f(-r * 0.8f, r * 0.8f), rng.f(-r * 0.6f, r * 0.6f));
            fx.push_back({FX_EXPLODE, p, p, -rng.f(0, 0.5f), 0.7f, rgb(255, 170, 60), rng.f(14, 30)});
            fx.push_back({FX_SMOKE, p, p, -rng.f(0, 0.6f), 2.5f, rgb(60, 55, 50), rng.f(10, 22), Vec2(rng.f(-8, 8), rng.f(-30, -12))});
        }
        for (int k = 0; k < 14; k++)
            fx.push_back({FX_DEBRIS, e.pos, e.pos, 0, rng.f(0.6f, 1.3f), rgb(70, 70, 70), rng.f(2, 5), Vec2(rng.f(-120, 120), rng.f(-160, -40))});
        fx.push_back({FX_RUBBLE, e.pos, Vec2((float)e.bt().w, (float)e.bt().h), 0, 50.0f, rgb(255, 255, 255), (float)e.gen});   // scorched ruins stay behind
        emit(EV_SOUND, -1, SND_EXPLODE_L, e.pos);
    } else if (e.isUnit() && e.ut().kind == UK_INF) {
        // a fallen soldier lies where they dropped for a few seconds (vel.y = 1 marks a corpse rather than a burning wreck)
        fx.push_back({FX_WRECK, e.pos, Vec2((float)e.type, (float)std::max(0, e.owner)), 0, 7.0f, rgb(255, 255, 255), e.turret, Vec2(e.angle + rng.f(-0.6f, 0.6f), 1.0f)});
        fx.push_back({FX_SPARK, e.pos, e.pos, 0, 0.3f, rgb(255, 200, 120), 4});
        emit(EV_SOUND, -1, SND_HIT, e.pos);
    } else if (e.isUnit()) {
        if (e.ut().naval) {   // a ship breaks up in a string of blasts along its length and sinks
            Vec2 fw(std::cos(e.angle), std::sin(e.angle));
            for (int k = -2; k <= 2; k++) {
                Vec2 q = e.pos + fw * (k * r * 0.7f);
                fx.push_back({FX_EXPLODE, q, q, -0.12f * (k + 2), 0.9f, rgb(255, 176, 76), r * 1.5f});
                fx.push_back({FX_SMOKE, q, q, -0.12f * (k + 2), 3.4f, rgb(46, 44, 42), r * 1.1f, Vec2(rng.f(-8, 8), -rng.f(18, 34))});
            }
            fx.push_back({FX_RING, e.pos, e.pos, 0, 0.9f, rgb(235, 245, 255), r * 3.2f});
            fx.push_back({FX_WRECK, e.pos, Vec2((float)e.type, (float)std::max(0, e.owner)), 0, 12.0f, rgb(255, 255, 255), e.turret, Vec2(e.angle, 2.0f)});
        } else if (e.ut().kind == UK_VEH) fx.push_back({FX_WRECK, e.pos, Vec2((float)e.type, (float)std::max(0, e.owner)), 0, 16.0f, rgb(255, 255, 255), e.turret, Vec2(e.angle, 0)});
        if (e.isAir()) {   // a downed aircraft hits the ground a moment later, short of where it was shot
            Vec2 g = e.pos + Vec2(std::cos(e.angle), std::sin(e.angle)) * 26.0f + Vec2(0, 20);
            fx.push_back({FX_EXPLODE, g, g, -0.3f, 0.8f, rgb(255, 170, 70), r * 2.6f});
            fx.push_back({FX_SMOKE, g, g, -0.3f, 3.0f, rgb(46, 44, 42), r * 1.6f, Vec2(0, -22)});
            for (int k = 0; k < 8; k++) fx.push_back({FX_DEBRIS, g, g, -0.3f, rng.f(0.5f, 1.1f), rgb(80, 80, 82), rng.f(2, 4), Vec2(rng.f(-130, 130), rng.f(-150, -30))});
        }
        fx.push_back({FX_EXPLODE, e.pos, e.pos, 0, 0.6f, rgb(255, 180, 70), r * 1.6f});
        fx.push_back({FX_SMOKE, e.pos, e.pos, 0, 2.0f, rgb(50, 50, 50), r, Vec2(0, -25)});
        for (int k = 0; k < 6; k++)
            fx.push_back({FX_DEBRIS, e.pos, e.pos, 0, rng.f(0.4f, 0.9f), rgb(90, 90, 90), rng.f(2, 4), Vec2(rng.f(-90, 90), rng.f(-140, -30))});
        emit(EV_SOUND, -1, SND_EXPLODE_S, e.pos);
    }
}

void Sim::destroy(Entity& e, bool violent) {
    if (!e.alive) return;
    if (violent) finishUnitExperience(e);
    if (violent) deathFx(e);
    if (e.isBuilding()) {
        if (e.bt().role != BR_WAYMAKER) g_map.setStructure(e.tx, e.ty, e.bt().w, e.bt().h, false);   // (a Waymaker never marked it)
        if (!e.passengers.empty()) ejectAll(e, violent);   // a bunker that falls drops its garrison onto the ground, exposed
        // refund queued units
        if (e.owner >= 0) for (int t : e.queue) players[e.owner].money += UNITS[t].cost;
        if (e.owner >= 0 && violent) players[e.owner].structuresLost++;
    } else if (e.kind == EK_RESOURCE) {
        g_map.setResource(e.tx, e.ty, false);
    } else if (e.owner >= 0 && violent) {
        players[e.owner].unitsLost++;
    }
    if (violent && e.owner >= 0 && e.owner < numPlayers && e.kind != EK_RESOURCE && e.district < 0)   // what an army loses shakes its hold on its own people (see updateRiots)
        lossHeat[e.owner] += e.isUnit() ? (float)UNITS[e.type].cost : (float)BUILDS[e.type].cost;
    if (e.isUnit() && !e.passengers.empty()) ejectAll(e, violent);   // a cargo chopper shot down in the air takes everyone aboard with it
    if (e.isUnit() && e.carrier.valid()) {   // (a soldier that dies aboard, e.g. through a rule change, leaves its host's list)
        if (Entity* c = get(e.carrier)) c->passengers.erase(std::remove(c->passengers.begin(), c->passengers.end(), refOf(e)), c->passengers.end());
        e.carrier = NOREF;
    }
    e.alive = false;
    freeList.push_back((int)(&e - &ents[0]));
}

// ------------------------------------------------------------ helpers
void Sim::emit(EventType t, int player, Sound s, Vec2 pos, const char* msg) {
    Event ev; ev.type = t; ev.player = player; ev.sound = s; ev.pos = pos; ev.msg = msg;
    events.push_back(ev);
}

void Sim::rebuildGrid() {
    for (auto& c : grid) c.clear();
    for (int i = 0; i < (int)ents.size(); i++) {
        Entity& e = ents[i];
        if (!e.alive || e.carrier.valid()) continue;   // (soldiers aboard a transport or in a bunker are not on the map)
        int cx = clampi((int)(e.pos.x / GRID_CELL), 0, GRID_W - 1), cy = clampi((int)(e.pos.y / GRID_CELL), 0, GRID_H - 1);
        grid[cy * GRID_W + cx].push_back(i);
    }
    heliList.clear();
    for (int i = 0; i < (int)ents.size(); i++) if (ents[i].alive && ents[i].isUnit() && ents[i].ut().heli) heliList.push_back(i);
    incoming.assign(ents.size(), 0.0f);
    for (auto& p : projs) if (p.alive && p.target.idx >= 0 && p.target.idx < (int)ents.size() && ents[p.target.idx].gen == p.target.gen && p.weapon >= 0) {
        const Entity& t = ents[p.target.idx];
        if (t.alive && t.kind != EK_RESOURCE) incoming[p.target.idx] += WEAPONS[p.weapon].dmg * WEAPONS[p.weapon].mult[t.armor()];
    }
}

Entity* Sim::nearestEntity(Vec2 p, float maxDist, bool (*pred)(const Entity&, void*), void* ctx) {
    Entity* best = nullptr; float bd = maxDist;
    for (auto& e : ents) {
        if (!e.alive || !pred(e, ctx)) continue;
        float d = distToEntity(p, e);
        if (d < bd) { bd = d; best = &e; }
    }
    return best;
}

bool Sim::hasBuilding(int player, int buildType) const {   // (a block of a town the army holds is not its own: no Town Hall prerequisite from it)
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.type == buildType && e.constructed && e.district < 0) return true;
    return false;
}
bool Sim::techOnline(int player) const {   // a standing tech structure that is not switched off by an EMP strike
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.bt().role == BR_TECH && e.constructed && e.disabledUntil <= time) return true;
    return false;
}
bool Sim::hasRole(int player, BuildRole role) const {
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.bt().role == role && e.constructed) return true;
    return false;
}
bool Sim::unitAvailable(int player, int unitType) const {
    const UnitType& t = UNITS[unitType];
    if (t.faction != players[player].faction) return false;
    if (t.program && !players[player].advTech) return false;
    return prereqMet(player, t.requires_);
}
bool Sim::buildAvailable(int player, int buildType) const {
    const BuildType& b = BUILDS[buildType];
    if (neutralRole(b.role)) return false;   // map landmarks and island launchers: never raised by a dozer or sold
    if (b.faction != players[player].faction) return false;
    if (atBuildLimit(player, buildType)) return false;
    return prereqMet(player, b.requires_);
}
bool Sim::atIncomeLimit(int player, int buildType) const {
    return BUILDS[buildType].role == BR_INCOME && countRole(player, BR_INCOME, false) >= INCOME_MAX;
}
// a dozer may found or rebuild a Command Core / Post any time it can pay for one; up to HQ_MAX stand (or rise) at once
bool Sim::atBuildLimit(int player, int buildType) const {
    if (atIncomeLimit(player, buildType)) return true;
    BuildRole r = BUILDS[buildType].role;
    if (r == BR_NAVAL) return countRole(player, BR_NAVAL, false) >= 3;
    if (r == BR_ANTINUKE) return countRole(player, BR_ANTINUKE, false) >= ANTINUKE_MAX;
    if (r == BR_CIVIC) return countRole(player, BR_CIVIC, false) >= CIVIC_MAX || (civicKind(buildType) == CV_TOWNHALL && countBuildings(player, buildType) >= 1);   // (one Town Hall)
    return r == BR_HQ && countRole(player, BR_HQ, false) >= HQ_MAX;
}
int Sim::countUnits(int player, int type) const {
    int n = 0;
    for (auto& e : ents) if (e.alive && e.isUnit() && e.owner == player && (type < 0 || e.type == type)) n++;
    return n;
}
int Sim::countBuildings(int player, int type, bool onlyConstructed) const {
    int n = 0;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.district < 0 && (type < 0 || e.type == type) && (!onlyConstructed || e.constructed)) n++;   // (a town the army holds is not its own base)
    return n;
}
int Sim::countRole(int player, BuildRole role, bool onlyConstructed) const {
    int n = 0;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.district < 0 && e.bt().role == role && (!onlyConstructed || e.constructed)) n++;
    return n;
}

bool Sim::canPlace(int player, int buildType, int tx, int ty) const {
    const BuildType& b = BUILDS[buildType];
    if (b.role == BR_WAYMAKER) return waymakerFits(player, tx, ty, b.w, b.h);   // (trees, rock and ridges are its job; it never plugs anything, it opens)
    for (int j = ty; j < ty + b.h; j++) for (int i = tx; i < tx + b.w; i++) {
        if (!g_map.buildable(i, j)) return false;
        if (!exploredRaw(player, i, j)) return false;
    }
    // keep a one tile gap around resource piles and other structures' edges so paths stay open
    // (walls are the exception: they are meant to be chained, and they are exactly what closes a line)
    if (!wallRole(b.role)) for (int j = ty - 1; j <= ty + b.h; j++) for (int i = tx - 1; i <= tx + b.w; i++) {
        if (!inMap(i, j)) continue;
        if (g_map.blocked[j * MAP_W + i] & 4) return false;
    }
    if (b.role == BR_NAVAL) {   // a Naval Yard stands on the shore: its edge must touch the open lake (not a pond or a river cut off by a bridge)
        bool touch = false;
        for (int j = ty - 1; j <= ty + b.h && !touch; j++) for (int i = tx - 1; i <= tx + b.w; i++) {
            bool inside = i >= tx && i < tx + b.w && j >= ty && j < ty + b.h;
            bool corner = (i < tx || i >= tx + b.w) && (j < ty || j >= ty + b.h);
            if (inside || corner) continue;
            if (g_map.bigWater(i, j) && g_map.navTile(i, j)) { touch = true; break; }
        }
        if (!touch) return false;
        Vec2 out = g_map.nearestWater(buildingCenter(buildType, tx, ty), 7, true);   // and ships must have open lake to sail out into
        if (!g_map.navOpen(tileOf(out.x), tileOf(out.y))) return false;
    }
    if (!wallRole(b.role) && g_map.wouldPlug(tx, ty, b.w, b.h)) return false;   // never plug a corridor or seal a pocket: units must still get through (a wall's job is to seal)
    // units standing on the footprint are pushed off when placed; no check needed
    return true;
}

Vec2 Sim::unitExit(const Entity& b) const {
    const BuildType& t = b.bt();
    Vec2 p = b.pos + Vec2(0, t.h * TILE * 0.5f + 18);
    return g_map.nearestFree(p, 14);
}

// ------------------------------------------------------------ commands
// any direct order (from the player or the computer commander) replaces whatever the unit was doing on its own initiative
static void takeOrder(Entity& e) { e.stuckTimer = 0; e.lastPos = e.pos; e.autoTask = false; e.resumeOrder = O_IDLE; e.evadeUntil = -1; e.jetBreak = 0; }

static void formationOffsets(const std::vector<Entity*>& units, Vec2 dest, std::vector<Vec2>& out) {
    int n = (int)units.size();
    out.assign(n, Vec2());
    if (n <= 1) return;
    float spacing = 0;
    for (auto* u : units) spacing = std::max(spacing, u->radius() * 2.6f);
    spacing = std::max(spacing, 24.0f);
    int cols = (int)std::ceil(std::sqrt((float)n));
    int rows = (n + cols - 1) / cols;
    std::vector<Vec2> slots;
    for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++) {
        if ((int)slots.size() >= n) break;
        slots.push_back(Vec2((c - (cols - 1) * 0.5f) * spacing, (r - (rows - 1) * 0.5f) * spacing));
    }
    // greedy assignment: each unit takes the free slot closest to its current relative position
    std::vector<bool> used(n, false);
    Vec2 centroid;
    for (auto* u : units) centroid += u->pos;
    centroid = centroid * (1.0f / n);
    for (int i = 0; i < n; i++) {
        Vec2 rel = units[i]->pos - centroid;
        int best = -1; float bd = 1e18f;
        for (int s = 0; s < n; s++) if (!used[s]) { float d = dist2(rel, slots[s]); if (d < bd) { bd = d; best = s; } }
        used[best] = true; out[i] = slots[best];
    }
    (void)dest;
}

void Sim::cmdMove(const std::vector<Ref>& sel, Vec2 dest, bool attackMove) {
    std::vector<Entity*> ground, air;
    for (auto r : sel) { Entity* e = get(r); if (e && e->isUnit() && !e->carrier.valid() && !e->ut().temp) (e->isAir() ? air : ground).push_back(e); }   // (a Hotline Strike fighter takes no orders)
    std::vector<Vec2> offs;
    formationOffsets(ground, dest, offs);
    // A big group shares routes: a unit standing near one that has just been routed to a slot near its own follows that route (straight on to
    // its first waypoint, off to its own slot at the end) instead of planning the same long path again. A mass order of hundreds of units used
    // to plan every path in one tick.
    // A route that could not reach its spot is shared too (the spot is walled off, an enemy base or a pocket behind rock): the units round it head for
    // the same closest point instead of each flooding the whole reachable map again in a search that is bound to fail.
    struct Shared { Vec2 from, to; std::vector<Vec2> path; bool reached; bool naval; int comp; };   // (ships share sea routes on their own sea the same way)
    std::vector<Shared> shared;
    for (size_t i = 0; i < ground.size(); i++) {
        Entity& e = *ground[i];
        if (e.ut().role == UR_DOZER) { keepSite(e); e.autoHoldUntil = time + DOZER_MANUAL_HOLD; }   // (it goes back to an unfinished site once there; no mending rounds for a while)
        e.zoneR = 0; e.leashed = false; takeOrder(e); e.forceTarget = NOREF;
        Vec2 dd(clampf(dest.x + offs[i].x, TILE, WORLD_W - TILE), clampf(dest.y + offs[i].y, TILE, WORLD_H - TILE));
        Vec2 d = e.ut().naval ? g_map.nearestWater(dd, 14, true, g_map.compAt(e.pos)) : g_map.nearestFree(dd, 6);   // ships sail to the nearest open water of their sea
        if (e.ut().naval && g_map.compAt(d) != g_map.compAt(e.pos)) d = g_map.nearestWater(dd, 90, true, g_map.compAt(e.pos));   // (sent onto the other sea, or far inland: as close as its own sea goes)
        e.order = attackMove && e.weapon() >= 0 ? O_ATTACKMOVE : O_MOVE;
        e.postOrder = O_IDLE;
        e.target = d; e.targetEnt = NOREF; e.engaged = NOREF; e.actionTimer = 0;
        bool reused = false;
        const bool naval = e.ut().naval;
        const int comp = naval ? g_map.compAt(e.pos) : 0;
        auto clear = [&](Vec2 a, Vec2 b) { return naval ? g_map.navLineClear(a, b) : g_map.lineClear(a, b); };
        // a ship of a fleet joins a route another ship has planned: at the nearest point of the route (within 30 tiles) it can sail straight to, which is
        // a waypoint or a point on one of its legs. The fleet sails one route between them instead of a search for each ship (that was most of a late-game
        // spike). Ground units still share only a route that starts beside them.
        auto joinAt = [&](const std::vector<Vec2>& route, size_t last, Vec2& at) {
            int best = -1; float bd = 1e18f;
            for (size_t k = 0; k <= last && k < route.size(); k++) {
                Vec2 p = route[k];
                if (k >= 1) {   // the closest point of the leg that ends at this waypoint
                    Vec2 a0 = route[k - 1], ab = route[k] - a0;
                    float t = std::max(0.0f, std::min(1.0f, ((e.pos.x - a0.x) * ab.x + (e.pos.y - a0.y) * ab.y) / std::max(1e-6f, ab.len2())));
                    p = a0 + ab * t;
                }
                float dk = dist(e.pos, p);
                bool whole = p.x == route[k].x && p.y == route[k].y;   // (a point on a leg must also see the waypoint it leads to)
                if (dk < bd && dk <= 30.0f * TILE && clear(e.pos, p) && (whole || clear(p, route[k]))) { bd = dk; best = (int)k; at = p; }
            }
            return best;
        };
        if (!swimmer(e)) for (auto& sh : shared) {
            if (sh.naval != naval || sh.comp != comp) continue;
            if (dist(sh.to, d) > (naval ? 6.0f : 4.0f) * TILE || (!naval && dist(sh.from, e.pos) > 2.5f * TILE)) continue;
            if (!sh.reached) {   // the spot cannot be reached from here: the same closest point
                if (sh.path.empty()) continue;
                Vec2 at = sh.path[0];
                int k = naval ? joinAt(sh.path, sh.path.size() - 1, at) : (clear(e.pos, sh.path[0]) ? 0 : -1);
                if (k < 0) continue;
                e.path.assign(sh.path.begin() + k, sh.path.end());
                if (at.x != sh.path[k].x || at.y != sh.path[k].y) e.path.insert(e.path.begin(), at);
                e.pathIdx = 0; e.repathTimer = PATH_INTERVAL;
                reused = true; break;
            }
            if (sh.path.size() < 3) continue;
            Vec2 at = sh.path[0];
            int k = naval ? joinAt(sh.path, sh.path.size() - 2, at) : (clear(e.pos, sh.path[0]) ? 0 : -1);
            if (k < 0 || !clear(sh.path[sh.path.size() - 2], d)) continue;
            e.path.assign(sh.path.begin() + k, sh.path.end());
            if (at.x != sh.path[k].x || at.y != sh.path[k].y) e.path.insert(e.path.begin(), at);
            e.path.back() = d; e.pathIdx = 0; e.repathTimer = PATH_INTERVAL;
            reused = true; break;
        }
        // A wider net for land units: a route planned for a unit up to nine tiles off (more on a long march) is joined at a point along it this one
        // can see (and that is not much farther from it than from that unit), so an army spread over a field plans a handful of routes, not dozens
        // (the farther the march, the less the first stretch matters: a wave sent 150 tiles shares routes among units up to 18 tiles apart)
        const float joinR = std::max(9.0f * TILE, std::min(18.0f * TILE, 0.12f * dist(e.pos, d)));
        if (!reused && !naval && !swimmer(e)) for (auto& sh : shared) {
            if (sh.naval || !sh.reached || sh.path.size() < 4 || dist(sh.to, d) > 4.0f * TILE || dist(sh.from, e.pos) > joinR) continue;
            if (!clear(sh.path[sh.path.size() - 2], d)) continue;
            for (size_t k = 0; k < std::min<size_t>(3, sh.path.size() - 2) && !reused; k++) {
                float dj = dist(e.pos, sh.path[k]);
                if (dj > joinR + 3.0f * TILE || dj > dist(sh.from, sh.path[k]) + 5.0f * TILE || !clear(e.pos, sh.path[k])) continue;
                e.path.assign(sh.path.begin() + k, sh.path.end()); e.path.back() = d; e.pathIdx = 0; e.repathTimer = PATH_INTERVAL;
                reused = true;
            }
            if (reused) break;
        }
        if (!reused) {
            bool reached = requestPath(e, d);
            if (!swimmer(e) && shared.size() < 64) shared.push_back({e.pos, d, e.path, reached, naval, comp});   // (an infiltrator's route may cross water)
        }
    }
    for (auto* a : air) {
        a->zoneR = 0; a->leashed = false; takeOrder(*a); a->forceTarget = NOREF; a->loiterUntil = -1;
        if (a->ut().role == UR_SCOUT) { a->guardPos = dest; a->autoHoldUntil = time + SCOUT_MANUAL_HOLD; }
        if (a->ammo <= 0 && a->ut().ammo > 0) { a->targetEnt = NOREF; a->order = O_REARM; continue; }
        a->order = attackMove ? O_ATTACKMOVE : O_MOVE; a->postOrder = O_IDLE;
        a->target = dest; a->targetEnt = NOREF; a->engaged = NOREF;
    }
}

// Attacking your own or an ally's units and structures on purpose ("force fire") is something only a human player can order: the
// computer armies (and the learned brain) never pass 'force', and even if they did it is dropped here.
void Sim::cmdAttack(const std::vector<Ref>& sel, Ref target, bool force) {
    Entity* t = get(target);
    if (!t) return;
    for (auto r : sel) {
        Entity* e = get(r);
        if (!e || e == t || e->carrier.valid() || (e->isUnit() && e->ut().temp)) continue;
        if (e->isUnit() && e->ut().role == UR_SPY) {   // a spy takes an enemy structure; against anything else it just walks over
            if (t->isBuilding() && (t->constructed || t->district >= 0) && spyMayTake(e->owner, *t)) cmdCapture({r}, target); else cmdMove({r}, t->pos, false);
            continue;
        }
        bool ff = force && !players[e->owner].isAI && t->owner >= 0 && t->kind != EK_RESOURCE && !enemies(e->owner, t->owner);
        // a player may pour fire on an indestructible structure (an enemy Bastion Wall, an island launcher): it only burns, it never breaks. Nothing
        // ever does that on its own initiative, and the computer armies never order it (see canTarget)
        if (invulnerable(*t) && t->constructed && !players[e->owner].isAI && (force || enemies(e->owner, t->owner))) ff = true;
        if (e->isBuilding()) {   // a turret or battery takes the target the player points at (a friendly one too, with force fire)
            if (!e->constructed || e->bt().weapon < 0) continue;
            e->forceTarget = ff ? target : NOREF;
            if (canTarget(*e, *t)) e->engaged = target;
            continue;
        }
        if (!e->isUnit()) continue;
        e->forceTarget = ff ? target : NOREF;
        takeOrder(*e); e->loiterUntil = -1;
        const UnitType& ut = e->ut();
        if (ut.role == UR_DOZER) {
            if (t->isBuilding() && dozerMayWork(*e, *t)) { cmdAssist({r}, target); continue; }   // (its own structure, or a town round the lake that is not an enemy's)
            cmdMove({r}, t->pos, false); continue;
        }
        if (ut.role == UR_HARVESTER) {
            if (t->kind == EK_RESOURCE) { cmdHarvest({r}, target); continue; }
            cmdMove({r}, t->pos, false); continue;
        }
        if (ut.weapon < 0 || !canTarget(*e, *t)) { cmdMove({r}, t->pos, false); continue; }
        if (e->isAir() && e->ammo <= 0 && ut.ammo > 0) { e->targetEnt = target; e->order = O_REARM; e->leashed = false; continue; }   // (helicopters like the Hornet never run dry: they attack at once)
        e->order = O_ATTACK; e->postOrder = e->zoneR > 0 ? O_GUARDAREA : O_IDLE; e->targetEnt = target; e->engaged = NOREF; e->leashed = false;
        e->repathTimer = 0;
    }
}

void Sim::cmdStop(const std::vector<Ref>& sel) {
    for (auto r : sel) {
        Entity* e = get(r);
        if (!e || !e->isUnit() || e->carrier.valid() || e->ut().temp) continue;
        e->zoneR = 0; e->leashed = false; takeOrder(*e); e->forceTarget = NOREF; e->loiterUntil = -1;
        if (e->ut().role == UR_DOZER) { e->siteQueue.clear(); e->autoHoldUntil = time + DOZER_MANUAL_HOLD; }   // stop means stop: the kept sites are dropped too
        if (e->ut().role == UR_SCOUT) e->autoHoldUntil = time + SCOUT_MANUAL_HOLD;
        if (e->isAir() && e->ammo <= 0 && e->ut().ammo > 0) { e->order = O_REARM; e->targetEnt = NOREF; continue; }
        e->order = O_IDLE; e->postOrder = O_IDLE; e->targetEnt = NOREF; e->engaged = NOREF; e->path.clear(); e->guardPos = e->pos;
    }
}

void Sim::cmdHarvest(const std::vector<Ref>& sel, Ref pile) {
    Entity* p = get(pile);
    if (!p || p->kind != EK_RESOURCE) return;
    for (auto r : sel) {
        Entity* e = get(r);
        if (!e || !e->isUnit() || e->ut().role != UR_HARVESTER) continue;
        e->zoneR = 0; takeOrder(*e);
        e->lastPile = pile; e->targetEnt = pile; e->actionTimer = 0;
        e->order = e->cargo > 0 ? O_RETURN : O_HARVEST;
        e->repathTimer = 0;
    }
}

// Fighters and aircraft protect a circle: they hold spread-out slots inside it, engage what enters it
// (chasing only while the target stays in the zone) and return to their slot afterwards.
void Sim::cmdGuardArea(const std::vector<Ref>& sel, Vec2 center, float radius) {
    radius = clampf(radius, 2.0f * TILE, 14.0f * TILE);
    center = Vec2(clampf(center.x, TILE, WORLD_W - TILE), clampf(center.y, TILE, WORLD_H - TILE));
    std::vector<Entity*> us;
    for (auto r : sel) { Entity* e = get(r); if (e && e->isUnit() && !e->carrier.valid() && !e->ut().temp && e->ut().role == UR_COMBAT && e->weapon() >= 0) us.push_back(e); }
    int n = (int)us.size();
    for (int k = 0; k < n; k++) {
        Entity& e = *us[k];
        e.zone = center; e.zoneR = radius; e.leashed = false; takeOrder(e); e.forceTarget = NOREF; e.loiterUntil = -1;
        e.engaged = NOREF; e.targetEnt = NOREF; e.postOrder = O_IDLE; e.actionTimer = 0;
        // sunflower spread so a group fills the circle instead of piling on the centre
        float a = k * 2.39996f, rr = radius * 0.6f * std::sqrt((k + 0.5f) / n);
        Vec2 slot = center + Vec2(std::cos(a), std::sin(a)) * (n == 1 ? 0.0f : rr);
        e.orbit = rng.f(0, 6.283f);
        if (e.isAir()) {
            e.guardPos = slot;
            e.order = (e.ammo <= 0 && e.ut().ammo > 0) ? O_REARM : O_GUARDAREA;
        } else {
            slot = e.ut().naval ? g_map.nearestWater(slot, 8, true, g_map.compAt(e.pos)) : g_map.nearestFree(slot, 6);   // a ship holds a slot on the water
            e.guardPos = slot; e.order = O_GUARDAREA;
            requestPath(e, slot);
        }
    }
}

// Haulers look for supply piles inside the circle (they go to its middle to find them) and work them until it is empty.
void Sim::cmdGatherArea(const std::vector<Ref>& sel, Vec2 center, float radius) {
    radius = clampf(radius, 3.0f * TILE, 16.0f * TILE);
    center = Vec2(clampf(center.x, TILE, WORLD_W - TILE), clampf(center.y, TILE, WORLD_H - TILE));
    for (auto r : sel) {
        Entity* e = get(r);
        if (!e || !e->isUnit() || e->ut().role != UR_HARVESTER) continue;
        takeOrder(*e);
        e->zone = center; e->zoneR = radius; e->targetEnt = NOREF; e->lastPile = NOREF; e->actionTimer = 0;
        e->order = e->cargo > 0 ? O_RETURN : O_HARVEST;
        e->path.clear(); e->repathTimer = 0;
    }
}

void Sim::cmdArea(const std::vector<Ref>& sel, Vec2 center, float radius) {
    cmdGuardArea(sel, center, radius);
    cmdGatherArea(sel, center, radius);
}

bool Sim::cmdBuild(Ref dozer, int buildType, int tx, int ty) {
    Entity* d = get(dozer);
    if (!d || !d->isUnit() || d->ut().role != UR_DOZER || d->carrier.valid()) return false;   // (a dozer aboard a bunker cannot drive to a site)
    int p = d->owner;
    if (buildType >= 0 && buildType < B_COUNT && BUILDS[buildType].role == BR_WAYMAKER) {   // the Waymaker clears its ground by itself: the dozer stays on its job
        if (civicDozerType(d->type) || !buildAvailable(p, buildType) || !canPlace(p, buildType, tx, ty)) return false;
        if (!canAfford(p, BUILDS[buildType].cost)) { emit(EV_NOFUNDS, p, SND_NOFUNDS, d->pos, "Insufficient funds"); return false; }
        players[p].money -= BUILDS[buildType].cost;
        placeBuilding(buildType, p, tx, ty, false);
        emit(EV_SOUND, p, SND_PLACE, tileCenter(tx + 1, ty + 1));
        return true;
    }
    if (!canRaise(*d, buildType)) return false;   // a civilian dozer raises the town, a dozer the base
    if (!buildAvailable(p, buildType) || !canPlace(p, buildType, tx, ty)) return false;
    const BuildType& b = BUILDS[buildType];
    if (!canAfford(p, b.cost)) { emit(EV_NOFUNDS, p, SND_NOFUNDS, d->pos, "Insufficient funds"); return false; }
    players[p].money -= b.cost;
    Ref site = placeBuilding(buildType, p, tx, ty, false);
    d = get(dozer);   // (placing may grow the entity array)
    if (!d) return true;
    keepSite(*d);   // (a site it leaves unfinished for this one is resumed afterwards)
    takeOrder(*d);
    d->order = O_BUILD; d->targetEnt = site; d->engaged = NOREF; d->repathTimer = 0; d->actionTimer = 0;
    emit(EV_SOUND, p, SND_PLACE, d->pos);
    return true;
}

// A whole line of one structure at once (the player's shift-drag): every spot that can be placed and paid for gets its foundation, in order,
// and the sites are shared out among the dozers in contiguous runs - each dozer drives to the first site of its run and keeps the rest in
// its site queue, so a long wall goes up from one end without anybody crossing the line. Waymakers need no dozer. Returns the sites placed.
int Sim::cmdBuildLine(const std::vector<Ref>& dozers, int buildType, const std::vector<std::pair<int, int>>& spots) {
    if (buildType < 0 || buildType >= B_COUNT || spots.empty()) return 0;
    const bool way = BUILDS[buildType].role == BR_WAYMAKER;
    std::vector<Ref> crew;
    for (Ref r : dozers) {
        Entity* d = get(r);
        if (d && d->isUnit() && d->ut().role == UR_DOZER && !d->carrier.valid() && (way ? !civicDozerType(d->type) : canRaise(*d, buildType))) crew.push_back(r);
    }
    if (crew.empty()) return 0;
    if (way) {   // (each Waymaker clears its own patch: nobody has to drive there)
        int n = 0;
        for (auto& s : spots) if (cmdBuild(crew[0], buildType, s.first, s.second)) n++;
        return n;
    }
    int p = get(crew[0])->owner;
    const BuildType& b = BUILDS[buildType];
    std::vector<Ref> sites;
    for (auto& s : spots) {
        if (!buildAvailable(p, buildType) || !canPlace(p, buildType, s.first, s.second)) continue;
        if (!canAfford(p, b.cost)) { emit(EV_NOFUNDS, p, SND_NOFUNDS, tileCenter(s.first, s.second), "Insufficient funds"); break; }
        players[p].money -= b.cost;
        sites.push_back(placeBuilding(buildType, p, s.first, s.second, false));
    }
    if (sites.empty()) return 0;
    int k = std::min((int)crew.size(), (int)sites.size());
    for (int i = 0; i < k; i++) {
        Entity* d = get(crew[i]);
        if (!d) continue;
        size_t a = sites.size() * i / k, z = sites.size() * (i + 1) / k;
        keepSite(*d);
        takeOrder(*d);
        d->order = O_BUILD; d->targetEnt = sites[a]; d->engaged = NOREF; d->repathTimer = 0; d->actionTimer = 0;
        d->siteQueue.insert(d->siteQueue.begin(), sites.begin() + a + 1, sites.begin() + z);   // (ahead of what it was already keeping)
    }
    emit(EV_SOUND, p, SND_PLACE, tileCenter(spots[0].first, spots[0].second));
    return (int)sites.size();
}

// Construction takes every dozer sent. A repair takes the closest few the structure has room for; the rest of a big group fan out to the
// other damaged structures around it (and only pile on when there is nothing else to mend nearby).
void Sim::cmdAssist(const std::vector<Ref>& sel, Ref building) {
    Entity* b = get(building);
    if (!b || !b->isBuilding()) return;
    std::vector<Entity*> dz;
    for (auto r : sel) {
        Entity* e = get(r);
        if (e && e->isUnit() && e->ut().role == UR_DOZER && !e->carrier.valid() && dozerMayWork(*e, *b)) dz.push_back(e);   // (only the right kind of dozer builds; a civilian one mends only the town; any helps a town round the lake that is not an enemy's)
    }
    Vec2 at = b->pos;
    std::stable_sort(dz.begin(), dz.end(), [&](const Entity* x, const Entity* y) { return dist2(x->pos, at) < dist2(y->pos, at); });
    int room = std::max(2, repairCrewCap(*b));
    for (auto* e : dz) {
        if (e->targetEnt != building) keepSite(*e);
        if (!players[e->owner].isAI) e->autoHoldUntil = time + DOZER_MANUAL_HOLD;   // the player picked its job: no wandering off on its own right after
        takeOrder(*e);
        Ref job = building;
        if (b->constructed && e->targetEnt != building && repairCrew(*b) >= room) {
            Vec2 keep = e->pos; e->pos = at;                 // look for work around the structure the player pointed at
            e->order = O_IDLE; e->targetEnt = NOREF;          // (so this dozer does not count itself in a crew)
            Entity* other = repairJob(*e, 14.0f * TILE);
            e->pos = keep;
            if (other) job = refOf(*other);
        }
        e->order = O_BUILD; e->targetEnt = job; e->repathTimer = 0; e->actionTimer = 0; e->path.clear();
        e->siteQueue.erase(std::remove(e->siteQueue.begin(), e->siteQueue.end(), job), e->siteQueue.end());
    }
}

bool Sim::cmdTrain(Ref building, int unitType) {
    Entity* b = get(building);
    if (!b || !b->isBuilding() || !b->constructed || b->owner < 0) return false;
    const UnitType& t = UNITS[unitType];
    if (t.builtBy != b->bt().role || !unitAvailable(b->owner, unitType)) return false;
    if (b->queue.size() >= 9) return false;
    if (!canAfford(b->owner, t.cost)) { emit(EV_NOFUNDS, b->owner, SND_NOFUNDS, b->pos, "Insufficient funds"); return false; }
    players[b->owner].money -= t.cost;
    players[b->owner].spentOn[unitType] += t.cost;
    b->queue.push_back(unitType);
    return true;
}

void Sim::cmdCancelTrain(Ref building, int queueIndex) {
    Entity* b = get(building);
    if (!b || !b->isBuilding() || queueIndex < 0 || queueIndex >= (int)b->queue.size()) return;
    players[b->owner].money += UNITS[b->queue[queueIndex]].cost;
    b->queue.erase(b->queue.begin() + queueIndex);
    if (queueIndex == 0) b->queueProgress = 0;
}

void Sim::cmdSetRally(Ref building, Vec2 p) {
    Entity* b = get(building);
    if (!b || !b->isBuilding()) return;
    b->rally = p; b->hasRally = true;
}

void Sim::cmdSell(Ref building) {
    Entity* b = get(building);
    if (!b || !b->isBuilding() || b->owner < 0 || neutralRole(b->bt().role) || b->district >= 0) return;   // (a captured island launcher, a claimed stronghold or a town is held, not sold)
    if (b->bt().role == BR_HQ && countBuildings(b->owner, -1, false) == 1 && countUnits(b->owner) == 0) return;
    float frac = b->constructed ? 0.5f : 0.5f * (0.3f + 0.7f * (1 - b->progress));
    players[b->owner].money += (int)(b->bt().cost * frac);
    // units queued are refunded by destroy()
    destroy(*b, false);
    emit(EV_SOUND, b->owner, SND_CLICK, b->pos);
}

bool Sim::cmdPower(int player, Vec2 pos, bool force) {
    Player& pl = players[player];
    force = force && !pl.isAI;   // hitting your own or an ally's units is a human's choice
    static int noPower = getenv("ONEHOUR_NOPOWER") ? 1 : 0;
    if (noPower) return false;
    if (time < pl.powerReady) return false;
    if (!techOnline(player)) return false;
    const PowerType& pw = POWERS[pl.faction];
    pl.powerReady = time + pw.cooldown;
    if (pl.faction == F_CYBER) {
        forEachNear(pos, pw.radius * TILE, [&](Entity& e) {
            if (e.kind == EK_RESOURCE || !(enemies(player, e.owner) || (force && e.owner >= 0))) return;
            if (e.isUnit() && e.ut().kind == UK_INF) return;
            e.disabledUntil = time + EMP_DURATION;
            if (e.isAir()) { e.hp = 0; applyDamage(e, 1, player, NOREF, nullptr); }
        });
        fx.push_back({FX_EMP, pos, pos, 0, 1.2f, rgb(160, 220, 255), pw.radius * TILE});
        emit(EV_SOUND, -1, SND_EMP, pos);
    } else {
        storms.push_back({pos, pw.radius * TILE, player, 0, 18, 0, force});
        emit(EV_SOUND, -1, SND_CANNON, pos);
    }
    return true;
}

bool Sim::inFallout(Vec2 p, float margin) const {
    for (auto& f : fallouts) if (f.t < f.life - 8.0f && dist(p, f.pos) < f.r + margin) return true;
    return false;
}

int Sim::dropsReady(int player) const {
    if (player < 0 || player >= numPlayers || time < players[player].dropReady) return 0;
    int n = 0;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.disabledUntil <= time && e.bt().role == BR_TECH && time >= e.dropTimer) n++;
    return n;
}
float Sim::dropWait(int player) const {
    float best = -1;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.bt().role == BR_TECH) {
        float w = std::max(0.0f, std::max(e.dropTimer, players[player].dropReady) - time);
        if (best < 0 || w < best) best = w;
    }
    return best;
}

// ------------------------------------------------------------ what a drop carries
// The single supply plane a tech structure can send never carries a fixed list. It carries what the army can field at the moment it is
// called in: DROP_*_POOL is that army's order of battle in the three slots, cheapest first, and how far the load reaches into those pools
// is the army's drop tier. A drop is therefore a picture of the army's progress rather than a constant - call the same Data Center ten
// minutes later, after the Advanced Program and a couple of structure upgrades, and a heavier and better force lands.
int Sim::dropTier(int player) const {
    if (player < 0 || player >= numPlayers) return 0;
    const Player& pl = players[player];
    int lvl = pl.advTech ? 1 : 0;                  // the Advanced Program is the first thing that deepens a drop
    for (int u = 0; u < UPG_STRUCTURE_COUNT; u++) if (pl.upg[u]) lvl++;   // and every finished structure upgrade deepens it further (the guard drones change no structure)
    if (lvl >= 3) return 2;                         // Spearhead: the arms race is finished
    if (lvl >= 1) return 1;                         // Battle Group: something beyond the tech structure has opened up
    return 0;                                       // Reinforced: what a fresh tech structure can already send
}

std::vector<int> Sim::dropManifest(int player) const {
    std::vector<int> load;
    if (player < 0 || player >= numPlayers) return load;
    Faction f = players[player].faction;
    int tier = dropTier(player);
    const DropTierType& dt = DROP_TIERS[tier];
    // How far into a pool this tier reaches. A fresh tech structure already reaches its own unlocks (snipers, a medic, the rail tank,
    // a gunship), which is what "drop what you have unlocked" means at the first tier; the Advanced Program and the structure upgrades
    // then push the reach further, and the budgets above grow with it.
    auto depthOf = [&](int poolSize) { return std::min(poolSize, 10 + 3 * tier); };
    // The best thing this army could put in a slot: walk down from the tier's reach to the cheapest it can field, so an army that has not
    // learned a step simply fields the previous one. Only if the whole cheap end is unavailable (a structure it needed was bombed away)
    // does it reach back up, and only if that is empty too does it give up the slot: a standing tech structure always lands somebody.
    auto pick = [&](const int* pool, int poolSize, int want) -> int {
        int dep = depthOf(poolSize);
        for (int i = std::min(want, dep - 1); i >= 0; i--) if (i < poolSize && unitAvailable(player, pool[i])) return pool[i];
        for (int i = 0; i < poolSize; i++) if (unitAvailable(player, pool[i])) return pool[i];
        return -1;
    };
    int ic = (int)dt.infBudget, vc = (int)dt.vehBudget, ac = (int)dt.airBudget;
    std::vector<int> inf, veh, air;
    int jeepType = f == F_CYBER ? U_C_JEEP : U_K_JEEP;
    for (int k = 0; k < ic; k++) { int t = pick(DROP_INF_POOL[f], DROP_INF, (k * depthOf(DROP_INF)) / std::max(1, ic)); if (t >= 0) inf.push_back(t); }
    for (int k = 0; k < vc; k++) { int t = pick(DROP_VEH_POOL[f], DROP_VEH, (k * depthOf(DROP_VEH)) / std::max(1, vc)); if (t >= 0) veh.push_back(t); }
    for (int k = 0; k < ac; k++) { int t = pick(DROP_AIR_POOL[f], DROP_AIR, (k * depthOf(DROP_AIR)) / std::max(1, ac)); if (t >= 0) air.push_back(t); }
    // jeeps come on top of the fighting slots, each with its seven soldiers already aboard (they are put in when the jeep leaves the plane, see releaseUnit):
    // DROP_JEEPS_MIN of them from the start and more as the army unlocks things (dropJeeps)
    if (unitAvailable(player, jeepType)) for (int k = 0, nj = dropJeeps(player); k < nj; k++) veh.push_back(jeepType);
    // Support rides on top of the slots instead of eating one: it is what the army has learned to bring besides its troops. A medic and a spy
    // do no damage at all and a sniper and a scout drone none against a structure, so letting them take a fighting slot would quietly make every
    // drop weaker than the one before. Each one still appears only if the army can field it - and the sniper's own tech structure is the very one
    // that calls the drop, which is what puts snipers in the load the moment they are unlocked.
    {
        const int* sup = DROP_SUPPORT[tier];
        int medic = f == F_CYBER ? U_C_MEDIC : U_K_MEDIC;
        int sniper = f == F_CYBER ? U_C_SNIPER : U_K_SNIPER;
        int spy = f == F_CYBER ? U_C_SPY : U_K_SPY;
        int scout = f == F_CYBER ? U_C_SDRONE : U_K_SDRONE;
        for (int k = 0; k < sup[0]; k++) if (unitAvailable(player, medic)) inf.push_back(medic);
        for (int k = 0; k < sup[1]; k++) if (unitAvailable(player, sniper)) inf.push_back(sniper);
        for (int k = 0; k < sup[2]; k++) if (unitAvailable(player, spy)) inf.push_back(spy);
        for (int k = 0; k < sup[3]; k++) if (unitAvailable(player, scout)) air.push_back(scout);
    }
    // release order: vehicles spread through the infantry, the aircraft leave last (they simply fly off toward the zone)
    size_t vi = 0, ii = 0;
    while (vi < veh.size() || ii < inf.size()) {
        if ((load.size() % 3 == 0 && vi < veh.size()) || ii >= inf.size()) load.push_back(veh[vi++]);
        else load.push_back(inf[ii++]);
    }
    for (int a : air) load.push_back(a);
    return load;
}
int Sim::nukesReady(int player) const {
    if (players[player].lowPower()) return 0;
    int n = 0;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.bt().role == BR_NUKE && time >= e.actionTimer && e.disabledUntil <= time) n++;
    return n;
}
float Sim::nukeWait(int player) const {
    float best = -1;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.bt().role == BR_NUKE) {
        float w = std::max(0.0f, e.actionTimer - time);
        if (best < 0 || w < best) best = w;
    }
    return best;
}

bool Sim::cmdNuke(int player, Vec2 pos, bool force) {
    force = force && !players[player].isAI;
    Entity* ramp = nullptr;
    if (players[player].lowPower()) return false;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.bt().role == BR_NUKE && time >= e.actionTimer && e.disabledUntil <= time) { ramp = &e; break; }
    if (!ramp) return false;
    ramp->actionTimer = time + NUKE_COOLDOWN;
    nukes.push_back({ramp->pos, pos, player, 0, force});
    emit(EV_SOUND, -1, SND_ROCKET, ramp->pos);
    for (int p = 0; p < numPlayers; p++) if (enemies(player, p)) { emit(EV_MSG, p, SND_ATTACKED, pos, "NUCLEAR LAUNCH DETECTED"); emit(EV_UNDER_ATTACK, p, SND_ATTACKED, pos, "Nuclear missile incoming!"); }
    return true;
}

// Underground Nuke: the same launch as a ramp's (power needed, NUKE_COOLDOWN per silo), the warhead silver and three times as strong.
static bool deepReady(const Sim& S, const Entity& e, int player) {
    return e.alive && e.isBuilding() && e.owner == player && e.constructed && e.bt().role == BR_UNUKE && S.time >= e.actionTimer && e.disabledUntil <= S.time;
}
int Sim::deepNukesReady(int player) const {
    if (player < 0 || player >= numPlayers || players[player].lowPower()) return 0;
    int n = 0;
    for (auto& e : ents) if (deepReady(*this, e, player)) n++;
    return n;
}
float Sim::deepNukeWait(int player) const {
    float best = -1;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.bt().role == BR_UNUKE) {
        float w = std::max(0.0f, std::max(e.actionTimer - time, e.disabledUntil - time));
        if (best < 0 || w < best) best = w;
    }
    return best;
}
bool Sim::cmdDeepNuke(int player, Vec2 pos, bool force, Ref from) {
    if (player < 0 || player >= numPlayers || players[player].lowPower()) return false;
    force = force && !players[player].isAI;
    Entity* silo = get(from);
    if (!silo || !deepReady(*this, *silo, player)) {
        silo = nullptr;
        for (auto& e : ents) if (deepReady(*this, e, player)) { silo = &e; break; }
    }
    if (!silo) return false;
    silo->actionTimer = time + NUKE_COOLDOWN;
    Nuke n; n.from = silo->pos; n.pos = pos; n.owner = player; n.t = 0; n.force = force; n.scale = UNUKE_SCALE; n.deep = true;
    nukes.push_back(n);
    emit(EV_SOUND, -1, SND_ROCKET, silo->pos);
    for (int p = 0; p < numPlayers; p++) if (enemies(player, p)) { emit(EV_MSG, p, SND_ATTACKED, pos, "UNDERGROUND NUKE LAUNCH DETECTED"); emit(EV_UNDER_ATTACK, p, SND_ATTACKED, pos, "Silver warhead incoming: only Anti-Nuke Lasers can stop it!"); }
    return true;
}

// A bunker's own missile: needs no power, no ramp and no other structure. The bunker stays hidden except for a few seconds after a launch.
int Sim::miniNukesReady(int player) const {
    int n = 0;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.bt().role == BR_BUNKER && time >= e.siloTimer && e.disabledUntil <= time) n++;
    return n;
}
float Sim::miniNukeWait(int player) const {
    float best = -1;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.bt().role == BR_BUNKER) {
        float w = std::max(0.0f, std::max(e.siloTimer - time, e.disabledUntil - time));
        if (best < 0 || w < best) best = w;
    }
    return best;
}

bool Sim::cmdMiniNuke(int player, Vec2 pos, bool force, Ref from) {
    if (player < 0 || player >= numPlayers) return false;
    force = force && !players[player].isAI;
    auto ready = [&](const Entity& e) { return e.alive && e.isBuilding() && e.owner == player && e.constructed && e.bt().role == BR_BUNKER && time >= e.siloTimer && e.disabledUntil <= time; };
    Entity* silo = get(from);
    if (!silo || !ready(*silo)) {
        silo = nullptr;
        for (auto& e : ents) if (ready(e)) { silo = &e; break; }
    }
    if (!silo) return false;
    silo->siloTimer = time + MINI_NUKE_COOLDOWN;
    const bool swift = bunkerHas(*silo, BU_SWIFT);
    if (bunkerHas(*silo, BU_TRIPLE)) {   // triple tubes: three missiles leave together (a beat apart), one on each corner of a triangle over the target
        const float kk = silo->bt().w * TILE / 96.0f, rr = BUNKER_TRIPLE_SPREAD * TILE * 0.57735f;
        for (int k = 0; k < BUNKER_TRIPLE_SALVO; k++) {
            float an = -1.5708f + k * 2.0944f;
            Nuke n{silo->pos + Vec2((k - 1) * BUNKER_SILO_X, BUNKER_SILO_Y) * kk, pos + Vec2(std::cos(an), std::sin(an)) * rr, player, -0.25f * k, force, MINI_NUKE_SCALE};
            n.swift = swift;
            nukes.push_back(n);
        }
        silo->nukeSalvo = 0;
    } else {
        nukes.push_back({silo->pos, pos, player, 0, force, MINI_NUKE_SCALE});
        nukes.back().swift = swift;
        silo->nukeSalvo = (u8)(MINI_NUKE_SALVO - 1); silo->nukeSalvoAt = time + MINI_NUKE_SALVO_GAP;   // the rest of the salvo follows (see updateBunkerSelf)
        silo->nukeSalvoPos = pos; silo->nukeSalvoForce = force;
    }
    emit(EV_SOUND, -1, SND_ROCKET, silo->pos);
    for (int p = 0; p < numPlayers; p++) if (enemies(player, p)) {
        silo->detUntil[p] = std::max(silo->detUntil[p], time + MINI_NUKE_REVEAL);   // the missile climbs out of the hatch: whoever sees the launch knows where it is
        emit(EV_MSG, p, SND_ATTACKED, pos, "TACTICAL NUCLEAR LAUNCH DETECTED"); emit(EV_UNDER_ATTACK, p, SND_ATTACKED, pos, "Small nuclear missile incoming!");
    }
    return true;
}

// ------------------------------------------------------------ paradrop
// A transport's track: the line from the sender's base through the target (straight across the map's middle when the target is next to
// the base), entering from beyond the map edge at least a few hundred pixels before the target. Returns the heading; 'start' the entry.
static Vec2 flightTrack(Vec2 base, Vec2 pos, Vec2& start) {
    Vec2 d = pos - base;
    Vec2 dir = d.len() > 6.0f * TILE ? d.norm() : (Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f) - pos).norm();
    if (dir.len2() < 0.5f) dir = Vec2(1, 0);
    float s = 1e9f;
    if (dir.x > 1e-4f) s = std::min(s, (pos.x + 160.0f) / dir.x); else if (dir.x < -1e-4f) s = std::min(s, (pos.x - WORLD_W - 160.0f) / dir.x);
    if (dir.y > 1e-4f) s = std::min(s, (pos.y + 160.0f) / dir.y); else if (dir.y < -1e-4f) s = std::min(s, (pos.y - WORLD_H - 160.0f) / dir.y);
    s = clampf(s, 560.0f, 3200.0f);
    start = pos - dir * s;
    return dir;
}

// A cargo plane crosses the map along the line from the owner's base through the target and releases its load while it flies over:
// infantry and vehicles hang under parachutes for a few seconds (they cannot be hit or act until they land), the aircraft simply
// leave the cargo bay and take up guard over the zone. Anti-air that covers the plane's track can shoot it down, and the load with it.
bool Sim::cmdParadrop(int player, Vec2 pos, bool attackOn) {
    if (player < 0 || player >= numPlayers) return false;
    Player& pl = players[player];
    if (!pl.alive || time < pl.dropReady) return false;
    Entity* tech = nullptr;   // like the nuke ramps: every Data Center / Arms Lab has its own cooldown, so more of them means more drops
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.disabledUntil <= time && e.bt().role == BR_TECH && time >= e.dropTimer) { tech = &e; break; }
    if (!tech) return false;
    const DropType& dt = DROPS[pl.faction];
    std::vector<int> manifest = dropManifest(player);   // what this army can field right now, not a fixed list
    if (manifest.empty()) return false;
    pos = Vec2(clampf(pos.x, 2.0f * TILE, WORLD_W - 2.0f * TILE), clampf(pos.y, 2.0f * TILE, WORLD_H - 2.0f * TILE));
    tech->dropTimer = time + DROP_TIERS[dropTier(player)].cooldown;   // a deeper drop takes a little longer to assemble
    Airlift a;
    a.owner = player; a.target = pos; a.attackOn = attackOn;
    a.hp = a.maxHp = AIRLIFT_HP;
    a.dir = flightTrack(pl.basePos, pos, a.pos);   // the plane comes in from beyond the map edge, at least a few hundred pixels before the target
    a.prevPos = a.pos;
    a.load = std::move(manifest);
    float span = 0;
    for (int t : a.load) span += (UNITS[t].kind == UK_AIR ? 0.12f : (UNITS[t].kind == UK_VEH ? 0.09f : 0.045f)) * AIRLIFT_SPEED;
    a.releaseAt = -span * 0.5f;   // the stick is centred on the target
    airlifts.push_back(a);
    emit(EV_SOUND, player, SND_AIR, a.pos);
    emit(EV_MSG, player, SND_NONE, pos, (std::string(dt.name) + " inbound").c_str());
    for (int p = 0; p < numPlayers; p++) if (enemies(player, p)) emit(EV_MSG, p, SND_ATTACKED, pos, "ENEMY AIRLIFT DETECTED");
    return true;
}

// A bunker's Para Backup (BU_PARA): a cargo plane comes in over the bunker's side of the map and drops BUNKER_PARA_TROOPS paratroopers on the
// spot (bunkerParaManifest). Like any airlift it can be shot down on the way. Once per BUNKER_PARA_COOLDOWN per bunker.
bool Sim::cmdBunkerPara(Ref bunker, Vec2 pos, bool attackOn) {
    Entity* b = get(bunker);
    if (!b || !b->alive || !b->isBuilding() || !b->constructed || b->bt().role != BR_BUNKER || b->owner < 0 || !bunkerHas(*b, BU_PARA) || time < b->paraReady || b->disabledUntil > time) return false;
    int player = b->owner;
    std::vector<int> load = bunkerParaManifest(player);
    if (load.empty()) return false;
    pos = Vec2(clampf(pos.x, 2.0f * TILE, WORLD_W - 2.0f * TILE), clampf(pos.y, 2.0f * TILE, WORLD_H - 2.0f * TILE));
    b->paraReady = time + BUNKER_PARA_COOLDOWN;
    Airlift a;
    a.owner = player; a.target = pos; a.attackOn = attackOn;
    a.hp = a.maxHp = AIRLIFT_HP;
    a.dir = flightTrack(b->pos, pos, a.pos);
    a.prevPos = a.pos;
    a.load = std::move(load);
    a.releaseAt = -(float)a.load.size() * 0.045f * AIRLIFT_SPEED * 0.5f;   // the stick is centred on the target
    airlifts.push_back(a);
    emit(EV_SOUND, player, SND_AIR, a.pos);
    char msg[64]; snprintf(msg, sizeof msg, "Para Backup inbound: %d paratroopers", BUNKER_PARA_TROOPS);
    emit(EV_MSG, player, SND_NONE, pos, msg);
    for (int p = 0; p < numPlayers; p++) if (enemies(player, p)) emit(EV_MSG, p, SND_ATTACKED, pos, "ENEMY AIRLIFT DETECTED");
    return true;
}

Vec2 Sim::landingSpot(Airlift& a, float spacing) {
    float R = DROPS[players[a.owner].faction].radius * TILE;
    Vec2 fallback; bool have = false;
    for (int k = 0; k < 32; k++) {
        float ang = rng.f(0, 6.2832f), r = R * std::sqrt(rng.f());
        Vec2 p = a.target + Vec2(std::cos(ang), std::sin(ang)) * r;
        if (!g_map.passable(tileOf(p.x), tileOf(p.y))) continue;
        if (!have) { fallback = p; have = true; }
        bool ok = true;
        for (auto& q : a.spots) if (dist(p, q) < spacing) { ok = false; break; }
        if (ok) { a.spots.push_back(p); return p; }
    }
    Vec2 p = have ? fallback : g_map.nearestFree(a.target, 10);
    a.spots.push_back(p);
    return p;
}

void Sim::releaseUnit(Airlift& a) {
    int type = a.load[a.next++];
    const UnitType& ut = UNITS[type];
    a.releaseAt += (ut.kind == UK_AIR ? 0.12f : (ut.kind == UK_VEH ? 0.09f : 0.045f)) * AIRLIFT_SPEED;
    Vec2 from(clampf(a.pos.x, 8, WORLD_W - 8), clampf(a.pos.y, 8, WORLD_H - 8));
    Vec2 dir = a.dir; int owner = a.owner; Vec2 target = a.target; bool attackOn = a.attackOn;
    Vec2 to = ut.kind == UK_AIR ? target : landingSpot(a, ut.kind == UK_VEH ? 34.0f : 20.0f);
    Ref r = spawnUnit(type, owner, from);
    if (ut.kind == UK_VEH && ut.cargoCap > 0) {   // a jeep leaves the plane with its crew already aboard (they ride down under the same canopy)
        for (int ct : jeepCrew(owner)) {
            Ref cr = spawnUnit(ct, owner, from);   // (may grow the entity array: fetch both again)
            Entity* c = get(cr); Entity* jp = get(r);
            if (c && jp) board(*c, *jp, true);
        }
    }
    Entity& e = ents[r.idx];
    e.pos = e.prevPos = e.lastPos = from;
    float face = std::atan2(dir.y, dir.x);
    e.angle = e.turret = face;
    if (ut.kind == UK_AIR) {
        e.alt = 1; e.airspeed = ut.speed * 0.6f;   // already flying when it leaves the cargo bay, a little inside the map, turned toward the drop zone
        const float m = 3.0f * TILE;
        e.pos = e.prevPos = e.lastPos = Vec2(clampf(from.x, m, WORLD_W - m), clampf(from.y, m, WORLD_H - m));
        Vec2 tt = target - e.pos;
        if (tt.len() > 2.0f * TILE) e.angle = e.turret = std::atan2(tt.y, tt.x);
        cmdGuardArea({r}, target, 9.0f * TILE);
    } else {
        e.dropFrom = from; e.dropTo = to;
        e.fall = e.fallTime = ut.kind == UK_VEH ? 3.4f : 2.6f;
        e.guardPos = to;
        if (attackOn) { e.dropGo = true; e.dropGoal = target; }
    }
}

void Sim::updateAirlifts() {
    for (size_t i = 0; i < airlifts.size();) {
        Airlift& a = airlifts[i];
        a.t += SIM_DT;
        a.prevPos = a.pos;
        a.pos += a.dir * (AIRLIFT_SPEED * SIM_DT);
        // anti-air on the track: every gun that can hit aircraft and reaches the plane damages it
        float dps = 0;
        forEachNear(a.pos, 12.0f * TILE, [&](Entity& g) {
            if (g.kind == EK_RESOURCE || !enemies(a.owner, g.owner)) return;
            if (g.isBuilding() && (!g.constructed || players[g.owner].lowPower())) return;
            if (g.isUnit() && (g.fall > 0 || (g.isAir() && g.ammo <= 0 && g.ut().ammo > 0))) return;
            if (g.disabledUntil > time) return;
            int w = g.weapon();
            if (w < 0 || !WEAPONS[w].air || WEAPONS[w].cooldown <= 0) return;
            const Weapon& wp = WEAPONS[w];
            if (dist(g.pos, a.pos) - g.radius() > wp.range * TILE) return;
            dps += wp.dmg * wp.mult[AR_AIR] * wp.burst / wp.cooldown;
            if ((tick + (u32)(&g - &ents[0])) % 6 == 0) fx.push_back({FX_BEAM, g.pos, a.pos, 0, 0.12f, wp.color, 2});
        });
        if (dps > 0) { a.hp -= dps * SIM_DT; a.lastHit = time; }
        if (a.hp <= 0) {   // shot down: the load goes with it
            Vec2 p = a.pos;
            fx.push_back({FX_EXPLODE, p, p, 0, 1.2f, rgb(255, 190, 90), 70.0f});
            fx.push_back({FX_EXPLODE, p + Vec2(24, 6), p + Vec2(24, 6), -0.15f, 0.9f, rgb(255, 150, 60), 44.0f});
            fx.push_back({FX_EXPLODE, p + Vec2(-26, -4), p + Vec2(-26, -4), -0.3f, 0.9f, rgb(255, 150, 60), 40.0f});
            fx.push_back({FX_RING, p, p, 0, 0.9f, rgb(255, 214, 150), 110.0f});
            fx.push_back({FX_SMOKE, p, p, 0, 4.0f, rgb(44, 42, 40), 40.0f, Vec2(rng.f(-10, 10), -20)});
            for (int k = 0; k < 14; k++) fx.push_back({FX_DEBRIS, p, p, 0, rng.f(0.7f, 1.6f), rgb(84, 84, 86), rng.f(2, 5), Vec2(rng.f(-200, 200), rng.f(-220, -30))});
            emit(EV_SOUND, -1, SND_EXPLODE_L, p);
            emit(EV_MSG, a.owner, SND_ATTACKED, p, "Paradrop plane shot down");
            for (int q = 0; q < numPlayers; q++) if (enemies(a.owner, q)) emit(EV_MSG, q, SND_NONE, p, "Enemy airlift shot down");
            airlifts.erase(airlifts.begin() + i);
            continue;
        }
        float rel = (a.pos.x - a.target.x) * a.dir.x + (a.pos.y - a.target.y) * a.dir.y;
        int guard = 0;
        while (a.next < a.load.size() && rel >= a.releaseAt && guard++ < 6) releaseUnit(a);
        bool gone = a.pos.x < -260 || a.pos.y < -260 || a.pos.x > WORLD_W + 260 || a.pos.y > WORLD_H + 260;
        if ((gone && a.t > 3.0f) || a.t > 60.0f) { airlifts.erase(airlifts.begin() + i); continue; }
        i++;
    }
}

// ------------------------------------------------------------ air strike
// The tech structure's strike power: one call and a flight of strike fighters comes in from the open sea behind the caller's base, wave after wave
// of V formations, each fighter making one bombing pass over the called spot before it breaks away and leaves the map for good (see STRIKE_* and
// strikeJetRun). They are real aircraft in the air, so anti-air can claw at them, but they are hard to hit and nobody commands them.
int Sim::strikesReady(int player) const {
    if (player < 0 || player >= numPlayers || time < players[player].dropReady) return 0;
    int n = 0;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.disabledUntil <= time && e.bt().role == BR_TECH && time >= e.strikeTimer) n++;
    return n;
}
float Sim::strikeWait(int player) const {
    if (player < 0 || player >= numPlayers) return -1;
    float best = -1;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.bt().role == BR_TECH) {
        float w = std::max(0.0f, std::max(e.strikeTimer, players[player].dropReady) - time);
        if (best < 0 || w < best) best = w;
    }
    return best;
}

// The flight that would answer a call now: the army's strike fighter, as many as its drop tier sends (the same Data Center / Arms Lab sends a bigger
// flight once the Advanced Program and the structure upgrades are in).
std::vector<int> Sim::strikeManifest(int player) const {
    std::vector<int> out;
    if (player < 0 || player >= numPlayers) return out;
    int jet = players[player].faction == F_CYBER ? U_C_STRIKEJET : U_K_STRIKEJET;
    out.assign(STRIKE_FLIGHT[dropTier(player)], jet);
    return out;
}

bool Sim::cmdPhoneStrike(int player, Vec2 pos) {
    if (player < 0 || player >= numPlayers) return false;
    Player& pl = players[player];
    if (!pl.alive || time < pl.dropReady) return false;
    Entity* tech = nullptr;   // like the drops and the nuke ramps: each Data Center / Arms Lab takes a call on its own cooldown
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.disabledUntil <= time && e.bt().role == BR_TECH && time >= e.strikeTimer) { tech = &e; break; }
    if (!tech) return false;
    std::vector<int> load = strikeManifest(player);
    if (load.empty()) return false;
    pos = Vec2(clampf(pos.x, 2.0f * TILE, WORLD_W - 2.0f * TILE), clampf(pos.y, 2.0f * TILE, WORLD_H - 2.0f * TILE));
    tech->strikeTimer = time + STRIKE_COOLDOWN;
    // the waves: the first straight in along the line from the caller's base, the next ones swung to either side of it, so the spot is crossed
    // on three headings
    Vec2 start; Vec2 dir = flightTrack(pl.basePos, pos, start);
    float base = std::atan2(dir.y, dir.x);
    int n = (int)load.size();
    for (int w = 0, k = 0; k < n; w++) {
        StrikeWave sw;
        float turn = w == 0 ? 0.0f : ((w & 1) ? 1.0f : -1.0f) * STRIKE_TURN * ((w + 1) / 2);
        sw.owner = player; sw.type = load[0]; sw.n = std::min(STRIKE_WAVE, n - k); sw.target = pos;
        sw.dir = Vec2(std::cos(base + turn), std::sin(base + turn)); sw.at = time + w * STRIKE_WAVE_GAP;
        strikeWaves.push_back(sw);
        k += sw.n;
    }
    strikeMarks.push_back({pos, player, time});
    emit(EV_SOUND, player, SND_JET, pos);
    emit(EV_MSG, player, SND_NONE, pos, (std::string(STRIKES[pl.faction].name) + " inbound: " + std::to_string(n) + " " + UNITS[load[0]].name + " strike fighters").c_str());
    for (int p = 0; p < numPlayers; p++) if (enemies(player, p)) emit(EV_MSG, p, SND_ATTACKED, pos, "ENEMY AIR STRIKE INBOUND");
    return true;
}

// ------------------------------------------------------------ aid drop
// The human player's own relief flight (computer armies never get it): one per tech structure per AID_COOLDOWN. The white plane flies
// the same kind of track as a paradrop and releases its crates over the spot; no enemy ever harms it (see nukeBlast for the one exception).
int Sim::aidsReady(int player) const {
    if (player < 0 || player >= numPlayers || players[player].isAI || !players[player].alive) return 0;
    int n = 0;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.disabledUntil <= time && e.bt().role == BR_TECH && time >= e.aidTimer) n++;
    return n;
}
float Sim::aidWait(int player) const {
    if (player < 0 || player >= numPlayers || players[player].isAI) return -1;
    float best = -1;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.bt().role == BR_TECH) {
        float w = std::max(0.0f, e.aidTimer - time);
        if (best < 0 || w < best) best = w;
    }
    return best;
}

int Sim::aidCandidates(int sender, Vec2 at, int* out) const {
    float best[MAX_PLAYERS];
    for (int p = 0; p < MAX_PLAYERS; p++) best[p] = -1;
    for (auto& e : ents) {
        if (!e.alive || e.kind == EK_RESOURCE || e.owner < 0 || e.owner >= numPlayers || e.owner == sender || !players[e.owner].alive || e.district >= 0) continue;   // (a town it holds is not the army)
        float d = distToEntity(at, e);
        if (best[e.owner] < 0 || d < best[e.owner]) best[e.owner] = d;
    }
    float m = -1;
    for (int p = 0; p < numPlayers; p++) if (best[p] >= 0 && (m < 0 || best[p] < m)) m = best[p];
    int n = 0;
    if (m >= 0) for (int p = 0; p < numPlayers; p++) if (best[p] == m) out[n++] = p;
    return n;
}

bool Sim::cmdAidDrop(int player, Vec2 pos) {
    if (player < 0 || player >= numPlayers) return false;
    Player& pl = players[player];
    if (pl.isAI || !pl.alive) return false;   // only a human player has it
    Entity* tech = nullptr;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.disabledUntil <= time && e.bt().role == BR_TECH && time >= e.aidTimer) { tech = &e; break; }
    if (!tech) return false;
    pos = Vec2(clampf(pos.x, 2.0f * TILE, WORLD_W - 2.0f * TILE), clampf(pos.y, 2.0f * TILE, WORLD_H - 2.0f * TILE));
    int who[MAX_PLAYERS];
    if (aidCandidates(player, pos, who) == 0) { emit(EV_MSG, player, SND_CANT, pos, "No other army is left to receive aid"); return false; }
    tech->aidTimer = time + AID_COOLDOWN;
    AidDrop a;
    a.owner = player; a.target = pos;
    a.dir = flightTrack(pl.basePos, pos, a.pos);
    a.prevPos = a.pos;
    a.releaseAt = -(AID_CRATES - 1) * 0.5f * 46.0f;   // a stick of crates 46 px apart, centred on the spot
    aidDrops.push_back(a);
    emit(EV_SOUND, player, SND_AIR, a.pos);
    emit(EV_MSG, player, SND_NONE, pos, "Aid flight inbound");
    return true;
}

void Sim::deliverAid(AidDrop& a) {
    a.delivered = true; a.deliveredAt = time;
    int who[MAX_PLAYERS];
    int n = aidCandidates(a.owner, a.target, who);   // whoever is nearest when the crates are down (never the sender)
    if (n == 0) { a.recipient = -1; emit(EV_MSG, a.owner, SND_NONE, a.target, "The aid found nobody to take it"); return; }
    int r = n == 1 ? who[0] : who[rng.range(0, n - 1)];
    a.recipient = r;
    Player& rp = players[r];
    rp.money += AID_MONEY;
    Vec2 at = a.crates.empty() ? a.target : a.crates[a.crates.size() / 2].to;
    Vec2 side(-a.dir.y, a.dir.x), home = rp.basePos - at;   // the dozer rolls out beside the crates, on the side facing its own base
    if (side.x * home.x + side.y * home.y < 0) side = side * -1.0f;
    Vec2 out = at + side * 52.0f;
    spawnUnit(rp.faction == F_CYBER ? U_C_DOZER : U_K_DOZER, r, Vec2(clampf(out.x, TILE, WORLD_W - TILE), clampf(out.y, TILE, WORLD_H - TILE)));
    emit(EV_SOUND, -1, SND_SUPPLY, at);
    events.push_back({EV_AID, a.owner, SND_SUPPLY, at, "", r});
    char msg[64]; snprintf(msg, sizeof msg, "Aid received: $%d and a dozer", AID_MONEY);
    emit(EV_MSG, r, SND_NONE, at, msg);
}

void Sim::updateAidDrops() {
    for (size_t i = 0; i < aidDrops.size();) {
        AidDrop& a = aidDrops[i];
        a.t += SIM_DT;
        a.prevPos = a.pos;
        if (!a.downed) a.pos += a.dir * (AID_SPEED * SIM_DT);
        float rel = (a.pos.x - a.target.x) * a.dir.x + (a.pos.y - a.target.y) * a.dir.y;
        while (!a.downed && (int)a.crates.size() < AID_CRATES && rel >= a.releaseAt) {
            AidCrate c;
            c.from = Vec2(clampf(a.pos.x, 8, WORLD_W - 8), clampf(a.pos.y, 8, WORLD_H - 8));
            Vec2 perp(-a.dir.y, a.dir.x);
            Vec2 to = c.from + a.dir * 22.0f + perp * rng.f(-0.4f, 0.4f) * AID_RADIUS * TILE;   // carried on a little by the plane's speed
            Vec2 off = to - a.target; float l = off.len();
            if (l > AID_RADIUS * TILE) to = a.target + off * (AID_RADIUS * TILE / l);
            c.to = Vec2(clampf(to.x, 16, WORLD_W - 16), clampf(to.y, 16, WORLD_H - 16));
            c.fall = c.fallTime = 4.2f + 0.3f * (a.crates.size() % 2);
            a.crates.push_back(c);
            a.releaseAt += 46.0f;
        }
        bool down = (int)a.crates.size() == AID_CRATES || a.downed;
        for (auto& c : a.crates) if (c.fall > 0) {
            c.fall -= SIM_DT;
            if (c.fall <= 0) { c.fall = 0; fx.push_back({FX_SMOKE, c.to, c.to, 0, 1.4f, rgb(170, 150, 120), 16.0f, Vec2(0, -8)}); }   // dust as it lands
            else down = false;
        }
        if (down && !a.delivered) {
            if (a.crates.empty()) { a.delivered = true; a.deliveredAt = time; a.recipient = -1; }   // shot down before a single crate got out
            else deliverAid(a);
        }
        bool gone = a.downed || a.pos.x < -300 || a.pos.y < -300 || a.pos.x > WORLD_W + 300 || a.pos.y > WORLD_H + 300;
        if ((a.delivered && gone && time - a.deliveredAt > AID_LINGER) || a.t > 120.0f) { aidDrops.erase(aidDrops.begin() + i); continue; }
        i++;
    }
}

// What a detonation does (all of it to whatever nukeHurts says it hurts; anything else is thrown but never hurt):
//  * ground units: a lethal core, then falling damage out to the rim (they collapse, as ever)
//  * aircraft: everything in the blast falls out of the sky (and none of them try to outrun it, see dodgeDanger)
//  * small structures (footprint of NUKE_SMALL_AREA tiles or less: turrets, batteries, reactors, barracks, income structures) collapse
//    in the inner blast; every large structure survives with heavy damage that tapers with the distance and is never lethal
static const int NUKE_SMALL_AREA = 6;

void Sim::updateNukes() {
    for (auto& n : nukes) { n.t += SIM_DT; if (n.id == 0) n.id = ++nukeSeq; }
    // anti-nuke defences and Anti-Rocket walls engage warheads closing in; all of them also take aircraft under fire
    for (auto& b : ents) if (b.alive && b.isBuilding() && b.constructed && (b.bt().role == BR_ANTINUKE || isAntiRocketWall(b))) {
        updateAntiNuke(b);
        if (time >= b.pdAt && (tick + b.gen) % 3 == 0) updatePointDefenseAir(b);   // (the sky is searched seven times a second, not every tick: a wall of them was a big share of a late tick)
    }
    for (auto& n : nukes) if (!n.broken && n.killT >= 0 && n.t >= n.killT) breakNuke(n);
    for (size_t i = 0; i < nukes.size();) {
        if (nukes[i].t < flightOf(nukes[i])) { i++; continue; }
        Nuke n = nukes[i];
        nukes.erase(nukes.begin() + i);
        nukeBlast(n);
    }
}

void Sim::nukeBlast(const Nuke& n) {
    float R = nukeRadius(n);
    const float P = n.power;   // 1 for a whole warhead; ANTINUKE_FALLOFF for the pieces of one an anti-nuke defence shot down
    const float D = n.super ? ISLAND_NUKE_DAMAGE : (n.deep ? UNUKE_DAMAGE : 1.0f);   // an Underground Nuke hits three times as hard (and three times as wide: see nukeRadius); a Super Nuke harder still
    const bool heavy = n.deep || n.super;   // (the two big warheads flatten large structures in their inner blast and throw things further)
    std::vector<Ref> hit;
    forEachNear(n.pos, R + 80, [&](Entity& e) { if (e.kind != EK_RESOURCE) hit.push_back(refOf(e)); });
    for (auto r : hit) {
        Entity* e = get(r); if (!e) continue;
        float d = std::max(0.0f, dist(e->pos, n.pos) - e->radius() * 0.5f);
        if (d > R) continue;
        float f = d < R * 0.35f ? 1.0f : 1.0f - 0.75f * ((d - R * 0.35f) / (R * 0.65f));   // flat lethal core, then a falloff to a quarter at the rim
        // the pieces of a warhead that was shot down do P of what it would have done to the target, and never more than the target had left to lose
        auto lessen = [&](float dmg, const Entity& t) { return P < 1.0f ? P * std::min(dmg, t.hp) : dmg; };
        bool foe = nukeHurts(n, e->owner);   // a human's nuke flattens allies and the human's own army as well
        if (e->isAir()) {   // nothing flying survives the fireball and the shock front, whoever owns it: torn apart, it falls out of the sky
            if (n.source.valid() && refOf(*e) == n.source) continue;   // ... except the bomber that dropped the compact bomb: it is already pulling out of its own blast
            fx.push_back({FX_EXPLODE, e->pos, e->pos, 0, 0.9f, rgb(255, 214, 140), 50.0f});
            fx.push_back({FX_SMOKE, e->pos, e->pos, 0, 3.2f, rgb(40, 38, 36), 26.0f, Vec2(rng.f(-14, 14), -26)});
            for (int k = 0; k < 6; k++) fx.push_back({FX_DEBRIS, e->pos, e->pos, 0, rng.f(0.7f, 1.4f), rgb(84, 84, 86), rng.f(2, 4), Vec2(rng.f(-170, 170), rng.f(-190, -40))});
            applyDamage(*e, lessen(e->maxHp * 2.0f * D, *e), n.owner, NOREF, nullptr);
            continue;
        }
        Vec2 away = (e->pos - n.pos); float l = away.len(); away = l > 1 ? away * (1.0f / l) : Vec2(1, 0);
        if (invulnerable(*e)) {   // an island launcher or a Bastion Wall takes no harm and is not knocked offline: the blast only sets it ablaze
            e->heat = std::max(e->heat, std::min(HEAT_MAX, (0.8f + 0.8f * f) * std::max(P, 0.5f)));
            e->lastDamaged = time;
            continue;
        }
        if (foe) {
            if (e->isBuilding() && e->constructed && e->bt().role == BR_BUNKER) {   // nuclear-hardened: a warhead only scrapes it (BUNKER_NUKE_*; the big ones count up to twice)
                bunkerNuked(*e, f * P * (1.0f + 0.5f * (D - 1.0f)), n.owner);
                e = get(r); if (!e) continue;
                e->disabledUntil = std::max(e->disabledUntil, time + BUNKER_NUKE_STUN * f * P);
                continue;
            }
            if (e->isBuilding()) {
                bool bunkerB = hardened(*e);
                bool hqB = e->bt().role == BR_HQ;
                bool hyperB = hasUpgrade(e->owner, UPG_HYPER) && !civilianRole(e->bt().role);
                bool protectedB = hqB || hyperB;      // walls are ordinary small structures now: a warhead flattens them like anything else small
                bool small = e->bt().w * e->bt().h <= NUKE_SMALL_AREA && !bunkerB && !protectedB;
                bool rugged = hasUpgrade(e->owner, UPG_RUGGED) && !civilianRole(e->bt().role);
                bool fortB = e->bt().role == BR_STRONGHOLD;
                float dmg, cap = -1.0f;
                // Hyper Protection and the Command keystone put a hard ceiling on one warhead (so neither is erased in a single hit, even by
                // the silver Underground Nuke); the ceiling is a share of max health, so enough direct hits always bring them down.
                if (hyperB) cap = e->maxHp * HYPER_NUKE;
                if (hqB) cap = cap < 0 ? e->maxHp * HQ_NUKE : std::min(cap, e->maxHp * HQ_NUKE);
                if (fortB) { float fc = e->maxHp * (e->type == B_C_FORTRESS ? FORTRESS_NUKE : STRONGHOLD_NUKE); cap = cap < 0 ? fc : std::min(cap, fc); }
                if (bunkerB) dmg = e->maxHp * 1.2f * f * D;                                       // wrecks the hatch and hurts the buried body; warheads ignore the floor, so repeated direct hits destroy it
                else if (protectedB) dmg = e->maxHp * 1.6f * f * D;                              // (capped below; the ceiling is what stops a one-shot erase)
                else if (rugged) dmg = e->maxHp * RUGGED_NUKE * f * D / RUGGED_ARMOR;               // (after the armour) a bit over half at ground zero: it takes two warheads (one silver one)
                else if (small) dmg = e->maxHp * 1.6f * f * D;                                    // f above ~0.63 (the inner blast) brings it down
                else {
                    dmg = e->maxHp * 0.80f * f * rng.f(0.9f, 1.1f) * D;                      // a large structure is left holding on: 80% lost at ground zero, a fifth at the rim
                    if (!heavy) dmg = std::min(dmg, std::max(0.0f, e->hp - e->maxHp * 0.06f));   // never lethal (an Underground Nuke's triple blast flattens the inner half)
                }
                if (dmg > 0) applyDamage(*e, lessen(dmg, *e), n.owner, NOREF, nullptr, cap > 0 ? lessen(cap, *e) : -1.0f, true);
                e = get(r); if (!e) continue;
                e->disabledUntil = std::max(e->disabledUntil, time + 12.0f * f * P * (rugged ? 0.5f : 1.0f) * (heavy ? 1.5f : 1.0f));   // the pulse knocks survivors offline for a while
                if (e->hp < e->maxHp * 0.5f) {                                                 // wrecked but standing: fire and smoke pour out of it
                    for (int k = 0; k < 4; k++) {
                        Vec2 p = e->pos + Vec2(rng.f(-e->radius() * 0.7f, e->radius() * 0.7f), rng.f(-e->radius() * 0.5f, e->radius() * 0.5f));
                        fx.push_back({FX_EXPLODE, p, p, -rng.f(0.0f, 1.2f), 0.7f, rgb(255, 160, 60), rng.f(14, 26)});
                        fx.push_back({FX_SMOKE, p, p, -rng.f(0.0f, 1.0f), 4.0f, rgb(52, 48, 44), rng.f(14, 24), Vec2(rng.f(-8, 8), rng.f(-32, -14))});
                    }
                }
            } else {
                float dmg = 6000.0f * f * D;
                if (e->ut().naval) dmg = std::max(dmg, e->maxHp * 1.6f);   // a warship is no match for a warhead anywhere in the blast: it goes down
                if (!e->passengers.empty() && lessen(dmg, *e) >= e->hp) {   // a vehicle the blast destroys takes its crew with it (nobody climbs out of this one)
                    std::vector<Ref> crew = e->passengers;
                    for (Ref pr : crew) if (Entity* pu = get(pr)) { pu->carrier = NOREF; pu->pos = e->pos; destroy(*pu, true); }
                    e = get(r); if (!e) continue;
                    e->passengers.clear();
                }
                applyDamage(*e, lessen(dmg, *e), n.owner, NOREF, nullptr);
                e = get(r); if (!e) continue;
                e->disabledUntil = std::max(e->disabledUntil, time + 10.0f * f * P);       // survivors are stunned by the pulse
            }
        }
        if (e->isUnit()) {   // the shockwave throws everything that survives, friend or foe
            Vec2 np = e->pos + away * (90.0f * f * P * (n.super ? 2.0f : n.deep ? 1.6f : 1.0f));
            if (e->ut().naval ? g_map.navTile(tileOf(np.x), tileOf(np.y)) : g_map.passable(tileOf(np.x), tileOf(np.y))) e->pos = np;
            e->path.clear();
        }
    }
    // paradrop cargo planes in the blast go down with their load, whoever sent them (updateAirlifts wrecks them on its next pass); an aid
    // plane only goes down in its own sender's nuke (nobody else ever harms it), with the crates still aboard
    for (auto& a : airlifts) if (dist(a.pos, n.pos) <= R) a.hp = 0;
    for (auto& a : aidDrops) if (!a.downed && a.owner == n.owner && dist(a.pos, n.pos) <= R) {
        a.downed = true;
        fx.push_back({FX_EXPLODE, a.pos, a.pos, 0, 1.2f, rgb(255, 190, 90), 70.0f});
        fx.push_back({FX_SMOKE, a.pos, a.pos, 0, 4.0f, rgb(44, 42, 40), 40.0f, Vec2(rng.f(-10, 10), -20)});
        for (int k = 0; k < 14; k++) fx.push_back({FX_DEBRIS, a.pos, a.pos, 0, rng.f(0.7f, 1.6f), rgb(220, 222, 226), rng.f(2, 5), Vec2(rng.f(-200, 200), rng.f(-220, -30))});
        emit(EV_MSG, a.owner, SND_NONE, a.pos, a.crates.empty() ? "Your nuke brought down the aid plane: the aid is lost" : "Your nuke brought down the aid plane: only the crates already out will land");
    }
    if (n.broken) {   // a warhead that was shot down: its pieces come down in a rain of scattered blasts and burning wreckage, with no fireball or mushroom
        fx.push_back({FX_EXPLODE, n.pos, n.pos, 0, 1.0f, rgb(255, 236, 190), R * 0.55f});
        fx.push_back({FX_RING, n.pos, n.pos, 0, 1.2f, rgb(255, 200, 130), R * 0.8f});
        for (int k = 0; k < 26; k++) {
            float a = rng.f(0, 6.283f), r = R * std::sqrt(rng.f(0.0f, 1.0f)) * 0.95f; Vec2 p = n.pos + Vec2(std::cos(a) * r, std::sin(a) * r);
            fx.push_back({FX_EXPLODE, p, p, -rng.f(0.0f, 1.6f), rng.f(0.5f, 0.9f), rgb(255, 160, 60), rng.f(14, 34)});
            fx.push_back({FX_SMOKE, p, p, -rng.f(0, 1.8f), 4.0f, rgb(60, 54, 50), rng.f(16, 34), Vec2(rng.f(-10, 10), rng.f(-34, -14))});
        }
        for (int k = 0; k < 24; k++) fx.push_back({FX_DEBRIS, n.pos, n.pos, -rng.f(0, 0.8f), rng.f(0.7f, 1.6f), rgb(80, 76, 72), rng.f(2, 5), Vec2(rng.f(-300, 300), rng.f(-380, -90))});
        const float fr = R * (n.super ? ISLAND_NUKE_FALLOUT_R : 0.95f);
        fx.push_back({FX_FALLOUT, n.pos, n.pos, 0, FALLOUT_LIFE * (n.deep ? UNUKE_FALLOUT : 1.0f), n.super ? ISLAND_NUKE_GLOW : n.deep ? UNUKE_GLOW : rgb(120, 255, 90), fr});
        fx.back().tag = ++fxTagSeq;
        fallouts.push_back({n.pos, fr, 0.0f, 0.0f, P * (n.deep ? UNUKE_FALLOUT : 1.0f), -1, false, FALLOUT_LIFE * (n.deep ? UNUKE_FALLOUT : 1.0f), fxTagSeq});
        emit(EV_SOUND, -1, SND_EXPLODE_L, n.pos);
        emit(EV_SOUND, -1, SND_EXPLODE_S, n.pos);
        return;
    }
    // the fireball, the shock rings, a rolling field of secondary blasts and the mushroom cloud
    // (an Underground Nuke's is silver: a white-silver fireball, grey-silver rings, cloud and radiation glow. Its shock ring runs out to the whole
    // triple radius, but the fireball and the cloud are drawn at most 1.7 times a ramp's: three times as wide would be nine times the overdraw)
    if (n.super) {   // a Super Nuke: a magenta-white flash and fireball, shock rings that run out over the whole huge circle, a field of blasts across it
        // (the fireball and the cloud are drawn at most 1.8 times a ramp's, the fireball's flash no bigger than an Underground Nuke's: the blast itself
        // reaches six times as far, and on the software renderer a fireball sprite that size cost over 100 ms a frame)
        const Color G = ISLAND_NUKE_GLOW;
        const float Rs = std::min(R, NUKE_RADIUS * TILE * 1.8f);
        fx.push_back({FX_EXPLODE, n.pos, n.pos, 0, 2.4f, rgb(255, 240, 252), std::min(Rs, NUKE_RADIUS * TILE * 1.7f) * 1.3f});
        fx.push_back({FX_EMP, n.pos, n.pos, 0, 2.2f, rgb(255, 200, 240), Rs * 1.5f});
        fx.push_back({FX_RING, n.pos, n.pos, 0, 1.6f, rgb(255, 230, 248), R * 0.45f});
        fx.push_back({FX_RING, n.pos, n.pos, -0.3f, 2.6f, G, R * 1.02f});
        fx.push_back({FX_RING, n.pos, n.pos, -0.8f, 3.2f, rgb(255, 170, 120), R * 1.15f});
        fx.push_back({FX_MUSHROOM, n.pos, n.pos, 0, 22.0f, rgb(255, 214, 240), Rs});
        for (int k = 0; k < 90; k++) {
            float a = rng.f(0, 6.283f), r = R * std::sqrt(rng.f(0.0f, 1.0f)); Vec2 p = n.pos + Vec2(std::cos(a) * r, std::sin(a) * r);
            fx.push_back({FX_EXPLODE, p, p, -rng.f(0.0f, 2.0f) - r / R * 2.2f, 1.0f, (k & 1) ? rgb(255, 150, 60) : rgb(255, 140, 220), rng.f(28, 76)});
            fx.push_back({FX_SMOKE, p, p, -rng.f(0, 3.5f) - r / R * 2.0f, 6.0f, rgb(70, 58, 66), rng.f(30, 60), Vec2(rng.f(-12, 12), rng.f(-44, -18))});
        }
        for (int k = 0; k < 60; k++) fx.push_back({FX_DEBRIS, n.pos, n.pos, -rng.f(0, 0.6f), rng.f(0.9f, 2.2f), rgb(70, 66, 62), rng.f(2, 7), Vec2(rng.f(-560, 560), rng.f(-680, -140))});
        const float fr = R * ISLAND_NUKE_FALLOUT_R;
        fx.push_back({FX_FALLOUT, n.pos, n.pos, 0, FALLOUT_LIFE, G, fr});
        fx.back().tag = ++fxTagSeq;
        fallouts.push_back({n.pos, fr, 0.0f, 0.0f, 1.5f, -1, false, FALLOUT_LIFE, fxTagSeq});
        for (int k = 0; k < 4; k++) emit(EV_SOUND, -1, SND_EXPLODE_L, n.pos);
        return;
    }
    const bool sv = n.deep;
    const float Rv = sv ? std::min(R, NUKE_RADIUS * TILE * 1.7f) : R;
    fx.push_back({FX_EXPLODE, n.pos, n.pos, 0, 2.0f, sv ? rgb(240, 242, 246) : rgb(255, 240, 200), Rv * 1.3f});
    fx.push_back({FX_RING, n.pos, n.pos, 0, 1.8f, sv ? rgb(214, 218, 224) : rgb(255, 210, 140), sv ? R * 1.05f : R * 1.2f});
    if (!sv) fx.push_back({FX_RING, n.pos, n.pos, -0.5f, 2.4f, rgb(255, 180, 100), R * 1.7f});
    fx.push_back({FX_EMP, n.pos, n.pos, 0, 2.0f, sv ? rgb(220, 224, 232) : rgb(255, 220, 160), Rv * 1.3f});
    fx.push_back({FX_MUSHROOM, n.pos, n.pos, 0, 18.0f, sv ? UNUKE_GLOW : rgb(255, 255, 255), Rv});
    for (int k = 0; k < 44; k++) {
        float a = rng.f(0, 6.283f), r = R * std::sqrt(rng.f(0.0f, 1.0f)) * 1.05f; Vec2 p = n.pos + Vec2(std::cos(a) * r, std::sin(a) * r);
        fx.push_back({FX_EXPLODE, p, p, -rng.f(0.0f, 3.0f) - r / R * 0.8f, 1.0f, sv ? rgb(214, 218, 226) : rgb(255, 150, 50), rng.f(26, 70)});
        fx.push_back({FX_SMOKE, p, p, -rng.f(0, 3.5f), 6.0f, sv ? rgb(120, 124, 130) : rgb(60, 54, 50), rng.f(28, 56), Vec2(rng.f(-12, 12), rng.f(-44, -18))});
    }
    for (int k = 0; k < 40; k++) fx.push_back({FX_DEBRIS, n.pos, n.pos, -rng.f(0, 0.6f), rng.f(0.9f, 2.0f), rgb(70, 66, 62), rng.f(2, 6), Vec2(rng.f(-420, 420), rng.f(-520, -120))});
    const float life = FALLOUT_LIFE * (sv ? UNUKE_FALLOUT : 1.0f);
    fx.push_back({FX_FALLOUT, n.pos, n.pos, 0, life, sv ? UNUKE_GLOW : rgb(120, 255, 90), R * 0.95f});
    fx.back().tag = ++fxTagSeq;
    fallouts.push_back({n.pos, R * 0.95f, 0.0f, 0.0f, sv ? UNUKE_FALLOUT : 1.0f, -1, false, life, fxTagSeq});
    emit(EV_SOUND, -1, SND_EXPLODE_L, n.pos);
    emit(EV_SOUND, -1, SND_EXPLODE_L, n.pos);
    emit(EV_SOUND, -1, SND_EXPLODE_L, n.pos);
}

// Radioactive fallout: for FALLOUT_LIFE seconds everything on the ground inside the zone is poisoned, friend and foe alike;
// during the first seconds fires and secondary blasts keep tearing through it.
// The active Anti-Radiation Trucks (their decontamination fields are on; an EMP-ed or stowed one is off).
static void activeDeconTrucks(const Sim& S, std::vector<Ref>& out) {
    out.clear();
    for (auto& u : S.ents) if (u.alive && u.isUnit() && u.ut().role == UR_DECON && !u.carrier.valid() && u.fall <= 0 && u.disabledUntil <= S.time) out.push_back(S.refOf(u));
}

void Sim::updateFallout() {
    if (fallouts.empty()) return;   // (the common case: no crater to clean and no field to run)
    // Anti-Radiation Trucks: a field protects everything in it from the radiation of the zones it touches, and scrubs those zones: their
    // strength and radius decay (so do their hazy visuals), while the truck itself absorbs a dose of what it neutralises. A truck can die
    // doing it, and a silver cloud's much heavier radiation hurts it more (RADTRUCK_DAMAGE scales with the square root of the zone's power).
    std::vector<Ref> trucks;
    activeDeconTrucks(*this, trucks);
    auto inField = [&](Vec2 p) {
        for (Ref tr : trucks) { const Entity* t = get(tr); if (t && dist(p, t->pos) <= RADTRUCK_RADIUS * TILE) return true; }
        return false;
    };
    // Healing Antennas scrub too, much more slowly, unharmed, and without shielding anyone (see HEALANT_SCRUB)
    static std::vector<Vec2> masts; masts.clear();
    // ... and so do bunkers, a little faster, with their scrubber stacks (never the radiation a bunker vents at its own attackers)
    static std::vector<Vec2> stacks; stacks.clear();
    static std::vector<int> stackOwner; stackOwner.clear();
    for (auto& a : ents) if (a.alive && a.isBuilding() && a.constructed && a.owner >= 0) {
        if (a.bt().role == BR_HEALANT && a.disabledUntil <= time) masts.push_back(a.pos);
        else if (a.bt().role == BR_BUNKER) { stacks.push_back(a.pos); stackOwner.push_back(a.owner); }
    }
    for (size_t i = 0; i < fallouts.size();) {
        Fallout& f = fallouts[i];
        if (!masts.empty() || !stacks.empty()) {
            int n = 0, nb = 0;
            for (auto& m : masts) if (dist(m, f.pos) <= f.r + HEALANT_RADIUS * TILE) n++;
            for (size_t s = 0; s < stacks.size(); s++) if (!(f.enemyOnly && f.owner >= 0 && !enemies(f.owner, stackOwner[s])) && dist(stacks[s], f.pos) <= f.r + BUNKER_SCRUB_RADIUS * TILE) nb++;
            if (n > 0 || nb > 0) {
                float k = std::exp(-(std::min(HEALANT_SCRUB * n, HEALANT_SCRUB_MAX) + std::min(BUNKER_SCRUB * nb, BUNKER_SCRUB * 3.0f)) * SIM_DT);
                f.power = std::max(0.0f, f.power * k);
                f.r *= k;
                if (f.visTag) for (auto& vf : fx) if (vf.type == FX_FALLOUT && vf.tag == f.visTag) { vf.size *= k; break; }
                if (f.power <= 0.02f || f.r < 6.0f) {
                    if (f.visTag) for (auto& vf : fx) if (vf.type == FX_FALLOUT && vf.tag == f.visTag) { vf.t = vf.life; break; }
                    fallouts.erase(fallouts.begin() + i);
                    continue;
                }
            }
        }
        if (!trucks.empty()) {
            int scrubbing = 0;
            for (Ref tr : trucks) { const Entity* t = get(tr); if (t && dist(t->pos, f.pos) <= f.r + RADTRUCK_RADIUS * TILE) scrubbing++; }
            if (scrubbing > 0) {
                float k = std::exp(-RADTRUCK_SCRUB * scrubbing * SIM_DT);   // exponential: fast at first, then the last wisps
                f.power = std::max(0.0f, f.power * k);
                f.r *= k;
                if (f.visTag) for (auto& vf : fx) if (vf.type == FX_FALLOUT && vf.tag == f.visTag) { vf.size *= k; break; }
                float dose = RADTRUCK_DAMAGE * std::sqrt(std::max(f.power, 0.05f)) * SIM_DT;   // (a silver cloud's radiation is heavier)
                for (Ref tr : trucks) {
                    Entity* t = get(tr);
                    if (!t || dist(t->pos, f.pos) > f.r + RADTRUCK_RADIUS * TILE) continue;
                    applyDamage(*t, t->maxHp * std::min(dose, RADTRUCK_DRAIN_CAP * SIM_DT), -1, NOREF, nullptr);
                }
                if (f.power <= 0.02f || f.r < 6.0f) {   // washed out: take the haze with it
                    if (f.visTag) for (auto& vf : fx) if (vf.type == FX_FALLOUT && vf.tag == f.visTag) { vf.t = vf.life; break; }
                    fallouts.erase(fallouts.begin() + i);
                    continue;
                }
            }
        }
        f.t += SIM_DT; f.tick += SIM_DT;
        if (f.t >= f.life) { fallouts.erase(fallouts.begin() + i); continue; }
        if (f.t < 12.0f && rng.f() < 0.06f) {   // chaos: random blasts and flame inside the crater
            float a = rng.f(0, 6.283f), r = f.r * std::sqrt(rng.f(0.0f, 1.0f)) * 0.9f; Vec2 p = f.pos + Vec2(std::cos(a) * r, std::sin(a) * r);
            fx.push_back({FX_EXPLODE, p, p, 0, 0.8f, rgb(255, 150, 60), rng.f(20, 44)});
            fx.push_back({FX_SMOKE, p, p, 0, 4.0f, rgb(52, 48, 44), rng.f(18, 36), Vec2(rng.f(-8, 8), rng.f(-34, -16))});
        }
        if (f.tick >= 0.5f) {
            f.tick -= 0.5f;
            float strength = (f.t < f.life - 15.0f ? 1.0f : (f.life - f.t) / 15.0f) * f.power;   // dies down at the end (and is weaker for the pieces of a warhead that was shot down)
            std::vector<Ref> in;
            forEachNear(f.pos, f.r, [&](Entity& e) { if (e.kind != EK_RESOURCE && !e.isAir() && dist(e.pos, f.pos) <= f.r) in.push_back(refOf(e)); });
            for (auto r : in) {
                Entity* e = get(r); if (!e) continue;
                if (f.enemyOnly && !enemies(f.owner, e->owner)) continue;   // bunker defense radiation vents only poison enemy forces
                if (inField(e->pos)) continue;   // inside a decontamination field: the air is being scrubbed, the truck absorbs its dose instead (see above)
                if (e->isBuilding()) {   // radiation only weakens structures: a slow, non-lethal drain that stops at a tenth of their health (a bunker's filters keep it out)
                    if (e->hp > e->maxHp * 0.12f && !(e->constructed && e->bt().role == BR_BUNKER)) applyDamage(*e, e->maxHp * 0.0012f * strength, f.enemyOnly ? f.owner : -1, NOREF, nullptr);
                    continue;
                }
                float baseHp = f.enemyOnly && e->isUnit() ? (float)UNITS[e->type].hp : e->maxHp;
                float dmg = std::max(7.0f, baseHp * 0.03f);
                applyDamage(*e, dmg * strength, f.enemyOnly ? f.owner : -1, NOREF, nullptr);
            }
        }
        i++;
    }
}

bool Sim::programAvailable(int player) const {
    const Player& pl = players[player];
    return !pl.advTech && !pl.researching && techOnline(player);
}

bool Sim::cmdResearch(int player) {
    Player& pl = players[player];
    if (!programAvailable(player)) return false;
    const ProgramType& pg = PROGRAMS[pl.faction];
    if (!canAfford(player, pg.cost)) { emit(EV_NOFUNDS, player, SND_NOFUNDS, pl.basePos, "Insufficient funds"); return false; }
    pl.money -= pg.cost;
    pl.researching = true; pl.researchProgress = 0;
    emit(EV_MSG, player, SND_CLICK, pl.basePos, (std::string(pg.name) + " started").c_str());
    return true;
}

bool Sim::cmdScan(int player) {
    Player& pl = players[player];
    if (time < pl.scanReady || !techOnline(player)) return false;
    const ScanType& sc = SCANS[pl.faction];
    pl.scanReady = time + sc.cooldown;
    pl.revealUntil = time + sc.duration;
    if (!pl.isAI) std::fill(pl.explored.begin(), pl.explored.end(), 1);   // the satellite pass charts the whole map for good (its live picture lasts SCANS[].duration)
    fx.push_back({FX_EMP, pl.basePos, pl.basePos, 0, 1.6f, pl.faction == F_CYBER ? rgb(110, 230, 255) : rgb(222, 178, 60), (float)WORLD_W * 0.6f});
    emit(EV_SOUND, player, SND_AIR, pl.basePos);
    emit(EV_MSG, player, SND_NONE, pl.basePos, (std::string(sc.name) + ": the whole map is visible").c_str());
    return true;
}

bool Sim::upgradeAvailable(int player, int upg) const {
    if (player < 0 || player >= numPlayers || upg < 0 || upg >= UPG_COUNT) return false;
    const Player& pl = players[player];
    return !pl.upg[upg] && !pl.upgBusy[upg] && techOnline(player);
}

bool Sim::cmdUpgrade(int player, int upg) {
    if (!upgradeAvailable(player, upg)) return false;
    Player& pl = players[player];
    const UpgradeType& u = UPGRADES[pl.faction][upg];
    if (!canAfford(player, u.cost)) { emit(EV_NOFUNDS, player, SND_NOFUNDS, pl.basePos, "Insufficient funds"); return false; }
    pl.money -= u.cost;
    pl.upgBusy[upg] = true; pl.upgProgress[upg] = 0;
    emit(EV_MSG, player, SND_CLICK, pl.basePos, (std::string(u.name) + " started").c_str());
    return true;
}

// an upgrade takes effect at once on every structure the army already has (new ones get it when they are placed)
void Sim::finishUpgrade(int p, int upg) {
    Player& pl = players[p];
    pl.upgBusy[upg] = false; pl.upg[upg] = true; pl.upgProgress[upg] = 1;
    for (auto& e : ents) {
        if (!e.alive || !e.isBuilding() || e.owner != p) continue;
        if (upg == UPG_RUGGED && e.bt().role != BR_CIVIC) { e.maxHp *= RUGGED_HP; e.hp *= RUGGED_HP; }
        if (upg == UPG_HYPER && e.bt().role != BR_CIVIC) { e.maxHp *= HYPER_HP; e.hp *= HYPER_HP; }
    }
    emit(EV_MSG, p, SND_BUILD_DONE, pl.basePos, (std::string(UPGRADES[pl.faction][upg].name) + " complete: " + UPGRADES[pl.faction][upg].desc).c_str());
}

void Sim::updateResearch() {
    for (int p = 0; p < numPlayers; p++) {
        Player& pl = players[p];
        if (!techOnline(p)) continue;                  // paused while the tech structure is down or switched off
        float rate = 5 * SIM_DT * (pl.lowPower() ? 0.5f : 1.0f) * pl.buildMul();   // called every 5th tick
        if (pl.researching) {
            pl.researchProgress += rate / PROGRAMS[pl.faction].time;
            if (pl.researchProgress >= 1.0f) {
                pl.researching = false; pl.advTech = true; pl.researchProgress = 1;
                emit(EV_MSG, p, SND_BUILD_DONE, pl.basePos, (std::string(PROGRAMS[pl.faction].name) + " complete: " + PROGRAMS[pl.faction].desc).c_str());
            }
        }
        for (int u = 0; u < UPG_COUNT; u++) {
            if (!pl.upgBusy[u]) continue;
            pl.upgProgress[u] += rate / UPGRADES[pl.faction][u].time;
            if (pl.upgProgress[u] >= 1.0f) finishUpgrade(p, u);
        }
    }
}

// ------------------------------------------------------------ paths & movement
// A ground unit heading for a structure (a dozer to build or mend it, a spy to take it): the shortest path to any side of it
bool Sim::requestSidePath(Entity& e, const Entity& b) {
    if (e.isAir() || e.ut().naval || !b.isBuilding()) return requestPath(e, b.pos);
    e.path.clear(); e.pathIdx = 0;
    e.repathTimer = PATH_INTERVAL;
    // (an infiltrator plans with the water open to it: one search, which still keeps to land wherever that is nearly as short)
    bool ok = g_map.findPathToRect(e.pos, b.tx, b.ty, b.bt().w, b.bt().h, e.path, MAP_W * MAP_H, swimmer(e));
    if (e.path.empty()) return requestPath(e, g_map.nearestFree(b.pos + Vec2(0, b.bt().h * TILE * 0.5f + 12), 6));   // (already beside it, or boxed in: the old doorstep)
    return ok;
}

bool Sim::requestPath(Entity& e, Vec2 dest) {
    e.path.clear(); e.pathIdx = 0;
    e.repathTimer = PATH_INTERVAL;
    // Ground progress is measured across replans; resetting here prevented the
    // 1.5-second stuck check from ever firing in a frequently replanned corridor.
    if (e.isAir()) { e.stuckTimer = 0; e.lastPos = e.pos; }
    if (e.isAir()) { e.path.push_back(dest); return true; }
    if (e.ut().naval) {   // ships plan over open water only
        bool ok = g_map.findNavalPath(e.pos, dest, e.path);
        if (e.path.empty()) e.path.push_back(g_map.nearestWater(e.pos, 20, true));   // (no way there: it only moves to the open water beside it, never along a straight line across the land)
        return ok;
    }
    bool ok = g_map.findPath(e.pos, dest, e.path, MAP_W * MAP_H, swimmer(e));   // (an infiltrator may cross water: see SWIM_COST)
    if (e.path.empty()) {   // (already next to a blocked spot: stop on the free tile beside it, never bump into it forever)
        Vec2 fb = g_map.passable(tileOf(dest.x), tileOf(dest.y)) ? dest : g_map.nearestFree(dest, 4);
        if (!g_map.passable(tileOf(fb.x), tileOf(fb.y))) { fb = g_map.nearestFree(dest, 18); ok = g_map.findPath(e.pos, fb, e.path); }   // (a spot out in the lake: walk to the shore)
        // (a far goal it cannot reach at all: stay put, never push against the terrain in a straight line; nor against a wall line for a spot just
        // beyond it - units of a wave used to stand pressed to an enemy wall for minutes, walking at a goal two tiles off on its other side)
        if (e.path.empty()) e.path.push_back(g_map.passable(tileOf(fb.x), tileOf(fb.y)) && dist(fb, e.pos) <= 3.0f * TILE && (swimmer(e) || g_map.sameGround(e.pos, fb)) ? fb : e.pos);
    }
    return ok;
}

static inline float wrapAngle(float a);
void Sim::moveAlong(Entity& e, float speed) {
    if (!e.isAir() && e.ut().naval) { moveShip(e, speed); return; }
    if (e.pathIdx >= e.path.size()) return;
    Vec2 wp = e.path[e.pathIdx];
    Vec2 d = wp - e.pos;
    float len = d.len();
    const float stepMax = speed * SIM_DT;
    // A vehicle has weight: it builds up speed over about a third of a second, slows for the last stretch of its path and while its hull is still swinging
    // round toward where it is going (the current speed is kept in 'airspeed', which a ground unit has no other use for)
    float v = speed;
    if (!e.isAir() && e.ut().kind == UK_VEH) {
        float diff0 = std::abs(wrapAngle(std::atan2(d.y, d.x) - e.angle));
        float vt = speed * clampf(1.3f - diff0 / 1.1f, 0.35f, 1.0f);
        if (e.pathIdx + 1 >= e.path.size()) vt = std::min(vt, std::max(speed * 0.3f, len * 2.2f));
        float a = speed * 3.2f * SIM_DT;
        e.airspeed = e.airspeed < vt ? std::min(vt, e.airspeed + a) : std::max(vt, e.airspeed - a * 1.6f);
        v = e.airspeed;
    }
    if (swimmer(e) && g_map.swimTile(tileOf(e.pos.x), tileOf(e.pos.y))) v *= SWIM_SPEED;   // paddling a raft is slow
    float step = v * SIM_DT;
    if (len <= stepMax + 0.5f) {
        if (!e.isAir() && !walkable(e, tileOf(wp.x), tileOf(wp.y))) { requestPath(e, e.path.back()); return; }
        e.pos = wp; e.pathIdx++;
        return;
    }
    Vec2 dir = d * (1.0f / len);
    Vec2 np = e.pos + dir * step;
    if (!e.isAir()) {
        int nx = tileOf(np.x), ny = tileOf(np.y);
        if (!walkable(e, nx, ny)) {
            // slide along axes if possible
            Vec2 alt1(e.pos.x + dir.x * step, e.pos.y), alt2(e.pos.x, e.pos.y + dir.y * step);
            if (std::abs(dir.x) > 0.01f && walkable(e, tileOf(alt1.x), tileOf(alt1.y))) np = alt1;
            else if (std::abs(dir.y) > 0.01f && walkable(e, tileOf(alt2.x), tileOf(alt2.y))) np = alt2;
            else { e.airspeed = 0; requestPath(e, e.path.back()); return; }
        }
    }
    e.pos = np;
    float want = std::atan2(dir.y, dir.x);
    float diff = want - e.angle;
    while (diff > 3.14159f) diff -= 6.28318f;
    while (diff < -3.14159f) diff += 6.28318f;
    float turn = (e.ut().kind == UK_INF ? 20.0f : 7.0f) * SIM_DT;
    e.angle += clampf(diff, -turn, turn);
}

// ------------------------------------------------------------ combat
bool Sim::canTarget(const Entity& e, const Entity& t, int wpn) const {
    if (!t.alive || t.kind == EK_RESOURCE) return false;
    if (t.fall > 0) return false;   // still under the parachute
    if (t.fade < 0.5f && t.isUnit() && t.ut().naval) return false;   // a called ship still mostly haze at the map border
    // an island launcher or a Bastion Wall: shooting it does nothing but set it burning (only a spy takes it), so only a player's own order aims at it
    const bool burnOrder = invulnerable(t) && t.constructed && e.forceTarget.valid() && !players[e.owner].isAI && get(e.forceTarget) == &t;
    if (invulnerable(t) && t.constructed && !burnOrder) return false;
    if (districtRuin(t) && t.progress < DISTRICT_RUIN_SAFE) return false;   // a razed town block is rubble until its people have raised it again a way
    if (t.carrier.valid() || !visibleTo(t, e.owner)) return false;   // aboard a carrier, or stealthy and not found by any detector (snipers, spy drones)
    // a bunker whose hatch is gone and whose body is at the floor can only be hurt by soldiers and vehicles (see applyDamage): aircraft and
    // structures would hover over it or shoot at it for nothing, so they do not pick it
    if (hardened(t) && t.hatch <= 0 && t.hp <= t.maxHp * (BUNKER_FLOOR + BUNKER_FLOOR_MARGIN) + 1.0f && !(e.isUnit() && e.ut().kind != UK_AIR)) return false;   // (the margin: a bunker mends a little under fire, and aircraft must not hover over it plinking at that trickle)
    if (e.isUnit() && e.ut().sniper && !(t.isUnit() && t.ut().kind == UK_INF && !t.ut().sniper)) return false;   // a sniper shoots infantry, never another sniper
    if (!enemies(e.owner, t.owner) && !burnOrder) {   // a friendly is only a legal target for the one the human ordered it to hit
        if (t.owner < 0 || !e.forceTarget.valid() || players[e.owner].isAI) return false;
        if (get(e.forceTarget) != &t) return false;
    }
    int w = wpn >= 0 ? wpn : e.weapon();
    if (w < 0) return false;
    const Weapon& wp = WEAPONS[w];
    if (t.isAir()) return wp.air;
    return wp.ground;
}


// how many enemy anti-air guns cover a target (jets avoid diving into flak when softer targets exist)
float Sim::aaCover(const Entity& t, int owner) {
    float n = 0;
    forEachNear(t.pos, 11.0f * TILE, [&](Entity& g) {
        if (g.kind == EK_RESOURCE || !enemies(owner, g.owner) || !g.alive) return;
        if (g.isBuilding() && !g.constructed) return;
        int w = g.weapon();
        if (w < 0 || !WEAPONS[w].air) return;
        if (distToEntity(t.pos, g) <= (WEAPONS[w].range + 1.0f) * TILE) n += WEAPONS[w].mult[AR_AIR] >= 1.5f ? 2.0f : 1.0f;
    });
    return n;
}

// Sharp fire (Player::sharpFire): no shot is wasted on a dead man walking. A unit picking its own target passes over one that the shells and rockets
// already on their way will kill (Sim::incoming), so a group spreads its fire instead of a dozen rounds landing on one wreck. Units of every army,
// the player's included, use it whenever they choose for themselves. (Measured in --duelmatrix: mirrored battles, 224 won to 208 over 432 fights,
// more value left on every seed set. Also tried and dropped there, because they lost: preferring wounded targets in reach, and preferring the enemy
// that can hurt the unit most.)
float Sim::sharpScore(const Entity& e, const Entity& t, float d, const Weapon& wp) const {
    (void)d; (void)wp;
    if (e.owner < 0 || e.owner >= MAX_PLAYERS || !players[e.owner].sharpFire || !t.isUnit()) return 0;
    size_t ti = (size_t)(&t - &ents[0]);
    return ti < incoming.size() && incoming[ti] >= t.hp ? 4.0f : 0.0f;
}

// The plain score is the distance against how well the weapon hurts the target's armour, with nudges (things that shoot back first, a Command
// structure last). The smart one (MICRO_THREAT, every shooter that is not an aircraft) weighs what a kill is worth: the damage per second the
// target deals - to this shooter, or for less to its friends - against the seconds this shooter needs to kill it, so a group shoots the tank
// destroyer before the wall it stands behind and the wounded rocket trooper before the fresh rifleman. A target it has to walk to costs extra.
// MICRO_FOCUS prefers a target already taking fire that is not yet doomed (the group finishes one enemy before starting the next).
float Sim::targetScore(const Entity& e, const Entity& t, const Weapon& wp, int wpn, float d) {
    const int micro = e.owner >= 0 && e.owner < MAX_PLAYERS ? players[e.owner].micro : 0;
    if ((micro & MICRO_THREAT) && !e.isAir()) {
        const float myDps = std::max(0.5f, wp.dmg * wp.mult[t.armor()] * std::max(1, wp.burst) / std::max(0.05f, wp.cooldown) * (wp.splash > 0 ? 1.15f : 1.0f));
        const float ttk = std::max(0.5f, t.hp / myDps);   // seconds this shooter alone needs
        float threat = 0;
        int tw = t.weapon();
        if (tw >= 0 && (t.isUnit() ? t.ut().role == UR_COMBAT || t.ut().cargoCap > 0 : (t.constructed && !(players[t.owner < 0 ? 0 : t.owner].lowPower() && t.bt().power < 0)))) {
            const Weapon& w2 = WEAPONS[tw];
            const float dps2 = w2.dmg * std::max(1, w2.burst) / std::max(0.05f, w2.cooldown);
            const bool hitsMe = (e.isAir() ? w2.air : w2.ground) && !(t.isUnit() && t.ut().sniper && !(e.isUnit() && e.ut().kind == UK_INF));
            threat = hitsMe ? dps2 * w2.mult[e.armor()] : dps2 * 0.3f;
        }
        float base = 0.6f;   // nothing to fear from it: what its loss costs the enemy
        if (t.isUnit()) base = t.ut().role == UR_HARVESTER ? 5.0f : (t.ut().role == UR_DOZER ? 4.0f : (t.ut().role == UR_HEALER ? 6.0f : 1.5f));
        else if (!t.constructed) base = 0.35f;
        else if (t.bt().role == BR_HQ) base = 0.3f;
        float value = (threat + base) / ttk;
        float reach = d > wp.range + 0.15f ? (d - wp.range) * 1.6f : 0.0f;   // it would have to walk there under fire
        float score = d * 0.35f + reach - 3.0f * std::log(value + 1e-4f);
        if (wpn >= 0 && t.isAir()) score -= 1.5f;   // a roof gun is there to keep aircraft off the base first
        size_t ti = (size_t)(&t - &ents[0]);
        if (ti < incoming.size() && incoming[ti] > 0) {
            if (players[e.owner].sharpFire && incoming[ti] >= t.hp) score += 6.0f;   // doomed already: no shot is wasted on it
            else if (micro & MICRO_FOCUS) score -= 2.0f * std::min(1.0f, incoming[ti] / std::max(1.0f, t.hp)) + 0.6f;
        }
        return score;
    }
    float score = d - 2.5f * wp.mult[t.armor()];
    if (t.isUnit() && t.weapon() >= 0) score -= 1.5f;          // shoot back at things that shoot
    if (t.isUnit() && t.ut().role == UR_HARVESTER) score -= 0.5f;
    if (t.isBuilding() && !t.constructed) score += 1.0f;
    if (t.isBuilding() && t.bt().role == BR_HQ) score += 2.0f;  // HQ is a slog; prefer softer targets
    if (wpn >= 0 && t.isAir()) score -= 1.0f;                  // a roof gun is there to keep aircraft off the base first
    score += sharpScore(e, t, d, wp);
    if (e.isAir() && !t.isAir()) {
        if (e.ut().jet || e.ut().heli) score += std::min(8.0f, aaCover(t, e.owner) * (e.ut().heli ? 1.2f : 1.6f));   // (a gunship hovers in the flak: it picks prey outside it too)
        if (e.ut().bomber) { score += std::min(7.0f, aaCover(t, e.owner) * 1.2f); if (t.isBuilding()) score -= 3.0f; }
        else if (t.isBuilding() && t.bt().role != BR_AATURRET && t.bt().weapon < 0) score += 1.5f;
    }
    return score;
}

Entity* Sim::acquireTarget(Entity& e, float rangeTiles, int wpn, float* scoreOut) {
    int w = wpn >= 0 ? wpn : e.weapon();
    if (w < 0) return nullptr;
    const Weapon& wp = WEAPONS[w];
    Entity* best = nullptr; float bs = 1e9f;
    const bool groundOnly = wpn < 0 && e.isUnit() && e.ut().nukeBomb;   // a nuclear bomber's business is the ground: it never goes after aircraft
    forEachNear(e.pos, rangeTiles * TILE, [&](Entity& t) {
        if (&t == &e || !enemies(e.owner, t.owner) || !canTarget(e, t, w)) return;   // (auto-acquisition never picks a friend)
        if (groundOnly && t.isAir()) return;
        float d = distToEntity(e.pos, t) / TILE;
        if (d < wp.minRange) return;
        float score = targetScore(e, t, wp, wpn, d);
        if (score < bs) { bs = score; best = &t; }
    });
    if (scoreOut) *scoreOut = bs;
    return best;
}

// MICRO_KITE: a unit that outranges the foe it is fighting by a tile and a half (and is not much slower) backs away while its gun reloads, as
// long as the foe is closing in: it keeps shooting from beyond the foe's reach instead of trading shots it does not have to.
bool Sim::kiteStep(Entity& e, const Entity& t, const Weapon& w, float d) {
    if (e.isAir() || e.ut().naval || e.ut().kind != UK_VEH || !t.isUnit() || t.isAir() || e.cooldown < 0.25f) return false;   // (vehicles: a squad of riflemen backing off only broke up its own line)
    int tw = t.weapon();
    if (tw < 0 || !canTarget(t, e, tw)) return false;
    const Weapon& w2 = WEAPONS[tw];
    if (w.range < w2.range + 1.5f || e.ut().speed < t.ut().speed * 0.85f || d > w2.range + 1.2f) return false;
    if (e.hp > e.maxHp * 0.9f && d > w2.range + 0.6f) return false;   // (healthy and still outside its reach: no need yet)
    Vec2 away = e.pos - t.pos; float l = away.len();
    if (l < 1) return false;
    away = away * (1.0f / l);
    const float step = e.ut().speed * SIM_DT * 0.8f;
    for (float side : { 0.0f, 0.6f, -0.6f }) {   // straight back, or slanted off a wall
        Vec2 dir(away.x * std::cos(side) - away.y * std::sin(side), away.x * std::sin(side) + away.y * std::cos(side));
        Vec2 np = e.pos + dir * step;
        if (!walkable(e, tileOf(np.x), tileOf(np.y)) || inFallout(np)) continue;
        e.pos = np; e.path.clear(); e.pathIdx = 0;
        return true;
    }
    return false;
}

// Best enemy for a unit guarding a zone: anything inside the circle, or anything it can already shoot from where it stands.
Entity* Sim::acquireZoneTarget(Entity& e) {
    int w = e.weapon();
    if (w < 0 || e.zoneR <= 0) return nullptr;
    const Weapon& wp = WEAPONS[w];
    Entity* best = nullptr; float bs = 1e9f;
    // A fighter guarding an area takes aircraft and ground forces alike; a Supersonic Bomber guards against the ground only (its bomb cannot
    // hit an aircraft), and once its bomb is gone it has nothing to guard with until it has rearmed.
    if (e.isUnit() && e.ut().nukeBomb && e.ammo <= 0) return nullptr;
    forEachNear(e.zone, e.zoneR + (wp.range + 1.0f) * TILE, [&](Entity& t) {
        if (&t == &e || !enemies(e.owner, t.owner) || !canTarget(e, t)) return;
        if (e.isUnit() && e.ut().nukeBomb && t.isAir()) return;
        float d = distToEntity(e.pos, t) / TILE;
        if (d < wp.minRange) return;
        bool inZone = distToEntity(e.zone, t) <= e.zoneR + (e.isAir() ? 4.0f : 2.0f) * TILE;   // a little beyond the circle counts too
        if (!inZone && d > wp.range + 0.3f) return;
        float score = d - 2.5f * wp.mult[t.armor()];
        if (t.isUnit() && t.weapon() >= 0) score -= 1.5f;
        if (t.isUnit() && t.ut().role == UR_HARVESTER) score -= 0.5f;
        score += sharpScore(e, t, d, wp);
        if (t.isBuilding() && !t.constructed) score += 1.0f;
        if (t.isBuilding() && t.bt().role == BR_HQ) score += 2.0f;
        if (e.isAir() && !t.isAir() && (e.ut().jet || e.ut().heli)) score += std::min(8.0f, aaCover(t, e.owner) * (e.ut().heli ? 1.2f : 1.6f));
        if (e.isAir() && !t.isAir() && e.ut().bomber) { score += std::min(7.0f, aaCover(t, e.owner) * 1.2f); if (t.isBuilding()) score -= 3.0f; }
        if (score < bs) { bs = score; best = &t; }
    });
    return best;
}

bool Sim::tryFire(Entity& e, Entity& tgt) {
    int w = e.weapon();
    if (w < 0) return false;
    const Weapon& wp = WEAPONS[w];
    float d = distToEntity(e.pos, tgt) / TILE;
    if (d > wp.range + 0.15f) return false;
    if (d < wp.minRange) return false;
    Vec2 dir = tgt.pos - e.pos;
    e.turret = std::atan2(dir.y, dir.x);
    if (e.isUnit() && e.ut().kind == UK_INF) e.angle = e.turret;
    if (e.isBuilding()) e.angle = e.turret;
    if (e.cooldown > 0) return true;  // in range, waiting on cooldown
    if (e.isAir() && e.ut().ammo > 0) {
        if (e.ammo <= 0) return false;
        e.ammo--;
    }
    fireWeapon(e, tgt, wp);
    e.cooldown = wp.cooldown;
    if (wp.burst > 1) { e.burstLeft = wp.burst - 1; e.burstTimer = e.isUnit() && e.ut().jet ? 0.07f : 0.14f; }
    return true;
}

void Sim::fireWeapon(Entity& e, Entity& tgt, const Weapon& w, const Vec2* muzzle) {
    Vec2 from = muzzle ? *muzzle : e.pos;
    if (e.isUnit() && e.ut().kind == UK_VEH) from = e.pos + Vec2(std::cos(e.turret), std::sin(e.turret)) * (e.radius() * 0.9f);
    else if (e.isUnit() && e.ut().kind == UK_INF) from = e.pos + Vec2(std::cos(e.turret), std::sin(e.turret)) * 12.0f;   // muzzle of the rifle, not the soldier's chest
    else if (e.isUnit() && e.ut().kind == UK_AIR) from = e.pos + Vec2(std::cos(e.angle), std::sin(e.angle)) * 9.0f;
    Ref tref = refOf(tgt);
    Ref self = refOf(e);
    bool forced = !enemies(e.owner, tgt.owner);   // only possible on a human's order: the shot is meant for a friend and its blast hurts everybody
    emit(EV_SOUND, e.owner, w.sound, from);
    fx.push_back({FX_FLASH, from, from, 0, 0.08f, w.color, 5});
    switch (w.proj) {
    case PJ_LASER:
        fx.push_back({FX_BEAM, from, tgt.pos, 0, 0.12f, w.color, 2});
        applyDamage(tgt, w.dmg, e.owner, self, &w);
        break;
    case PJ_ARC:
        fx.push_back({FX_ARC, from, tgt.pos, 0, 0.16f, w.color, 2});
        splashDamage(tgt.pos, w.splash, w.dmg, e.owner, self, w, tref, forced);
        break;
    case PJ_RAIL: {
        Vec2 dir = (tgt.pos - from).norm();
        Vec2 end = from + dir * (w.range * TILE);
        fx.push_back({FX_RAIL, from, end, 0, 0.25f, w.color, 3});
        // pierce: every enemy near the line takes the hit
        std::vector<Entity*> hits;
        forEachNear(from + dir * (w.range * TILE * 0.5f), w.range * TILE * 0.5f + 40, [&](Entity& t) {
            if (forced ? (&t == &e || !t.alive || t.owner < 0 || t.kind == EK_RESOURCE || t.fall > 0 || (t.isAir() ? !w.air : !w.ground)) : !canTarget(e, t, (int)(&w - WEAPONS))) return;
            Vec2 rel = t.pos - from;
            float along = rel.x * dir.x + rel.y * dir.y;
            if (along < 0 || along > w.range * TILE + t.radius()) return;   // (range is measured to a target's edge, not its centre)
            float perp = std::abs(rel.x * dir.y - rel.y * dir.x);
            if (perp <= t.radius() + 6) hits.push_back(&t);
        });
        if (std::find(hits.begin(), hits.end(), &tgt) == hits.end()) hits.push_back(&tgt);   // the aimed target is always hit: tryFire already checked the range
        for (auto* t : hits) applyDamage(*t, refOf(*t) == tref ? w.dmg : w.dmg * 0.5f, e.owner, self, &w);  // pierce: half damage to anything else on the line
        break;
    }
    case PJ_BULLET: case PJ_ROCKET: {
        Projectile p; p.pos = from; p.prevPos = from; p.target = tref; p.weapon = (int)(&w - WEAPONS); p.owner = e.owner; p.shooter = self; p.forced = forced;
        Vec2 dir = (tgt.pos - from).norm();
        if (w.proj == PJ_ROCKET) dir = (dir + Vec2(rng.f(-0.35f, 0.35f), rng.f(-0.35f, 0.35f))).norm();
        p.vel = dir * w.projSpeed;
        p.life = dist(from, tgt.pos) / w.projSpeed + (w.proj == PJ_ROCKET ? 2.5f : 0.4f);
        p.dest = tgt.pos;
        projs.push_back(p);
        break;
    }
    case PJ_SHELL: {
        Projectile p; p.pos = from; p.prevPos = from; p.target = tref; p.weapon = (int)(&w - WEAPONS); p.owner = e.owner; p.shooter = self; p.forced = forced;
        Vec2 lead = tgt.pos;
        p.dest = lead; p.vel = from;  // vel stores the launch point for the arc
        p.arcLen = std::max(0.25f, dist(from, lead) / w.projSpeed);
        p.arcT = 0; p.life = p.arcLen + 0.1f;
        projs.push_back(p);
        break;
    }
    case PJ_BOMB: break;   // bombs are released by bomberAttack / dropBomb, never fired at a target
    }
}

void Sim::noteCombat(Entity& e, const Entity& opponent) {
    if (!e.isUnit() || e.owner < 0 || !players[e.owner].isAI || e.ut().role != UR_COMBAT || e.ut().temp || e.combatSeen) return;
    e.combatSeen = true;
    int n = 0;
    forEachNear(e.pos, 12 * TILE, [&](Entity& foe) {
        if (!foe.isUnit() || !enemies(e.owner, foe.owner) || foe.ut().role != UR_COMBAT || foe.fall > 0 || !visibleTo(foe, e.owner)) return;
        int k = foe.isAir() ? 2 : foe.ut().kind == UK_INF ? 0 : 1;
        e.combatMix[k]++; n++;
    });
    // A new unit or its opponent may not have reached the spatial grid yet.
    if (!n && opponent.isUnit()) { e.combatMix[opponent.isAir() ? 2 : opponent.ut().kind == UK_INF ? 0 : 1] = 1; n = 1; }
    if (n) for (float& v : e.combatMix) v /= n;
}

void Sim::finishUnitExperience(Entity& e) {
    if (!e.isUnit() || e.owner < 0 || !players[e.owner].isAI || !e.combatSeen || e.combatRecorded) return;
    e.combatRecorded = true;
    auto& outcomes = players[e.owner].unitOutcomes;
    if (outcomes.size() < 1024) outcomes.push_back({e.type, e.combatValue, {e.combatMix[0], e.combatMix[1], e.combatMix[2]}});
}

void Sim::applyDamage(Entity& tgt, float dmg, int attackerOwner, Ref attacker, const Weapon* w, float cap, bool nuke) {
    if (!tgt.alive || tgt.kind == EK_RESOURCE || tgt.fall > 0 || tgt.carrier.valid()) return;
    if (w && tgt.isUnit() && tgt.ut().evade > 0 && rng.f() < tgt.ut().evade) {   // a Hotline Strike fighter: most of what is thrown at it misses (a flash of flares)
        if (tick % 3 == 0) { fx.push_back({FX_SPARK, tgt.pos, tgt.pos, 0, 0.3f, rgb(255, 236, 190), 5}); fx.push_back({FX_RING, tgt.pos, tgt.pos, 0, 0.25f, rgb(255, 255, 255), 16.0f}); }
        return;
    }
    float m = w ? w->mult[tgt.armor()] : 1.0f;
    float real = dmg * m;
    if (invulnerable(tgt) && tgt.constructed) {   // an island launcher or a Bastion Wall: nothing breaks it, it only burns (and keeps working)
        heatUp(tgt, std::max(real, dmg) * HEAT_PER_DAMAGE * (nuke ? 3.0f : 1.0f));   // (the firepower that hits it, before any armour: a tank gun sets it burning)
        tgt.lastDamaged = time;
        if (attacker.valid()) tgt.attacker = attacker;
        return;
    }
    if (tgt.isBuilding() && !tgt.constructed) real *= 1.5f;
    const bool hardNuke = nuke && tgt.isBuilding() && tgt.constructed && tgt.bt().role == BR_BUNKER;   // a warhead on a nuclear-hardened bunker: bunkerNuked already worked out what it takes
    if (tgt.isBuilding() && hasUpgrade(tgt.owner, UPG_RUGGED) && !civilianRole(tgt.bt().role) && !hardNuke) {   // Rugged: half damage, until so much lands at once that the plating is swamped (the town has none)
        float load = tgt.dmgLoad / tgt.maxHp;
        real *= RUGGED_ARMOR + (1.0f - RUGGED_ARMOR) * clampf((load - RUGGED_SWAMP_LO) / (RUGGED_SWAMP_HI - RUGGED_SWAMP_LO), 0, 1);
    }
    if (tgt.isBuilding()) {   // Hyper Protection / the Command keystone / thick walls shrug off conventional fire (a warhead's blast overwhelms a wall's plating)
        if (hasUpgrade(tgt.owner, UPG_HYPER) && !civilianRole(tgt.bt().role) && !hardNuke) real *= HYPER_ARMOR;
        if (tgt.bt().role == BR_BUNKER && bunkerHas(tgt, BU_SHELL) && !nuke) real *= BUNKER_SHELL_ARMOR;   // its own super-thick shell
        if (tgt.bt().role == BR_HQ) real *= HQ_ARMOR;
        if (tgt.bt().role == BR_WALL && !nuke) real *= WALL_ARMOR;
        if (tgt.bt().role == BR_STRONGHOLD) real *= tgt.type == B_C_FORTRESS ? FORTRESS_ARMOR : STRONGHOLD_ARMOR;   // heavy old masonry: no army upgrade applies to it
        tgt.dmgLoad += real;
        if (cap > 0) real = std::min(real, cap);   // a warhead's hard ceiling (absolute HP), applied after every multiplier
    }
    if (attackerOwner >= 0 && enemies(attackerOwner, tgt.owner) && !hardNuke) real *= players[attackerOwner].damageMul();
    const Entity* attU = get(attacker);
    if (attU && attU->isUnit() && attU->rank > 0) real *= 1.0f + 0.2f * attU->rank;   // veterans and elites hit harder
    // Underground bunker (and the Underground Nuke silo, its blast doors standing in for the hatch): the surface hatch takes the damage first; what gets
    // past it is stopped by the plating. Only soldiers and vehicles can ever take it below BUNKER_FLOOR of its health: aircraft, structures, shells
    // from the sky and fallout cannot. Nuclear warheads ignore the floor entirely - enough of them destroy even a buried bunker or silo.
    bool bunker = hardened(tgt) && !hardNuke;
    float hpBefore = tgt.hp;
    if (bunker) {
        if (tgt.hatch > 0) {
            float soaked = std::min(tgt.hatch, real);
            tgt.hatch -= soaked; real -= soaked;
            if (tgt.hatch <= 0) { tgt.hatch = 0; hatchBlown(tgt); }
        }
        real *= BUNKER_PLATING;
    }
    tgt.hp -= real;
    if (bunker && !nuke && !(attU && attU->isUnit() && attU->ut().kind != UK_AIR)) {
        float floorHp = tgt.maxHp * BUNKER_FLOOR;
        tgt.hp = std::max(tgt.hp, std::min(hpBefore, floorHp));
    }
    tgt.lastDamaged = time;
    if (attacker.valid()) tgt.attacker = attacker;
    // credit the attacking unit type with the value it chewed through (for adaptive AI composition; friendly fire earns nothing)
    bool foe = attackerOwner >= 0 && tgt.owner >= 0 && enemies(attackerOwner, tgt.owner);
    if (foe) {
        Entity* a = get(attacker);
        if (a && a->owner == attackerOwner) {
            noteCombat(*a, tgt); noteCombat(tgt, *a);
        }
        if (a && a->isUnit() && a->owner == attackerOwner) {
            float tv = tgt.isUnit() ? UNITS[tgt.type].cost : BUILDS[tgt.type].cost * 0.5f;
            // Credit actual health removed, including overkill/floor handling.
            // Damage absorbed by a hatch or stopped at the bunker floor is not a kill.
            float value = tv * std::max(0.0f, hpBefore - std::max(0.0f, tgt.hp)) / tgt.maxHp;
            players[attackerOwner].valueDealt[a->type] += value;
            a->combatValue += value;
            if (a->waveSerial && a->waveSerial == players[attackerOwner].waveSerial) players[attackerOwner].waveCredit += value;
        }
    }
    if (foe) {   // (a human's force fire on an ally raises no alarm)
        Player& pl = players[tgt.owner];
        // "base under attack" style notice, throttled per player (state lives in Player so a new game starts clean)
        if (time - pl.lastNotice > 10.0f) {
            pl.lastNotice = time;
            // shot by something hidden (a sniper, a garrisoned bunker) that none of our detectors has found: say so, it needs a spy drone
            bool hidden = false;
            if (attU) { const Entity* src = attU->carrier.valid() ? get(attU->carrier) : attU; hidden = src && stealthOf(*src) && !visibleTo(*src, tgt.owner); }
            char townMsg[96]; if (tgt.district >= 0) snprintf(townMsg, sizeof townMsg, "%s is under attack", districts[tgt.district].name);
            emit(EV_UNDER_ATTACK, tgt.owner, SND_ATTACKED, tgt.pos, tgt.district >= 0 ? townMsg : hidden ? (tgt.isBuilding() ? "Our base is under fire from a hidden enemy: a spy drone can find it" : "Our forces are under fire from a hidden enemy: a spy drone can find it") : (tgt.isBuilding() ? "Our base is under attack" : "Our forces are under attack"));
            for (int q = 0; q < numPlayers; q++)   // human teammates hear about it too (an AI ally being hit)
                if (q != tgt.owner && !players[q].isAI && players[q].team == pl.team) emit(EV_UNDER_ATTACK, q, SND_ATTACKED, tgt.pos, tgt.isBuilding() ? "Your ally's base is under attack" : "Your ally's forces are under attack");
        }
    }
    if (tgt.district >= 0) districts[tgt.district].lastHit = time;
    if (tgt.hp <= 0 && districtBlock(tgt)) {   // a town block is never wiped out: it falls into a ruin its people raise again (a ruin hit again starts over)
        if (tgt.constructed) collapseBlock(tgt, attackerOwner);
        else { tgt.progress = 0; tgt.hp = tgt.maxHp * 0.1f; }
        return;
    }
    if (tgt.hp <= 0) {
        if (foe) {
            if (tgt.isBuilding()) players[attackerOwner].structuresKilled++; else players[attackerOwner].unitsKilled++;
            if (tgt.isUnit()) awardKill(attacker, tgt);
        }
        destroy(tgt, true);
    } else if (tgt.isUnit() && w) {
        fx.push_back({FX_SPARK, tgt.pos, tgt.pos, 0, 0.15f, w->color, 3});
    }
}

void Sim::splashDamage(Vec2 at, float radiusTiles, float dmg, int owner, Ref attacker, const Weapon& w, Ref direct, bool forced) {
    float r = std::max(radiusTiles, 0.05f) * TILE;
    std::vector<Entity*> hits;
    forEachNear(at, r, [&](Entity& t) {
        if (t.kind == EK_RESOURCE || !(enemies(owner, t.owner) || (forced && t.owner >= 0)) || &t == get(attacker)) return;   // (a force-fired blast never hits the gun that fired it)
        if (t.isAir() && !w.air) return;
        if (!t.isAir() && !w.ground) return;
        if (t.carrier.valid()) return;
        hits.push_back(&t);
    });
    Entity* dt = get(direct);
    if (dt && std::find(hits.begin(), hits.end(), dt) == hits.end() && (enemies(owner, dt->owner) || (forced && dt->owner >= 0))) hits.push_back(dt);
    for (auto* t : hits) {
        float d = distToEntity(at, *t);
        float f = (refOf(*t) == direct) ? 1.0f : clampf(1.0f - d / r, 0.35f, 1.0f);
        applyDamage(*t, dmg * f, owner, attacker, &w);
    }
}

void Sim::updateProjectiles() {
    for (auto& p : projs) {
        if (!p.alive) continue;
        const Weapon& w = WEAPONS[p.weapon];
        p.life -= SIM_DT;
        if (interceptProjectile(p)) { p.alive = false; continue; }   // a protector drone got it
        if (w.proj == PJ_SHELL || w.proj == PJ_BOMB) {
            p.arcT += SIM_DT / p.arcLen;
            Vec2 start = p.vel;
            Entity* st = get(p.target);
            if (st && w.proj == PJ_SHELL) p.dest = st->pos;   // shells track their target: tank cannons don't miss in Zero Hour either (bombs just fall)
            p.pos = start + (p.dest - start) * clampf(p.arcT, 0, 1);
            if (w.proj == PJ_BOMB) {
                if (p.arcT >= 1.0f) { bombImpact(p); p.alive = false; }
                continue;
            }
            if (p.arcT >= 1.0f) {
                fx.push_back({FX_EXPLODE, p.dest, p.dest, 0, 0.35f, rgb(255, 190, 90), w.splash * TILE + 8});
                fx.push_back({FX_SMOKE, p.dest, p.dest, 0, 1.2f, rgb(70, 65, 60), 9, Vec2(0, -20)});
                splashDamage(p.dest, w.splash, w.dmg, p.owner, p.shooter, w, p.target, p.forced);
                p.alive = false;
            }
            continue;
        }
        Entity* t = get(p.target);
        Vec2 aim = t ? t->pos : p.dest;
        Vec2 toT = aim - p.pos;
        float d = toT.len();
        float step = w.projSpeed * SIM_DT;
        if (w.proj == PJ_ROCKET && t) {
            // homing with limited turn rate
            Vec2 want = toT.norm() * w.projSpeed;
            p.vel = (p.vel * 0.75f + want * 0.25f).norm() * w.projSpeed;
        } else if (t) {
            p.vel = toT.norm() * w.projSpeed;
        }
        float hitR = (t ? t->radius() : 6.0f) + step * 0.5f;
        if (d <= hitR || p.life <= 0) {
            if (w.splash > 0) {
                fx.push_back({FX_EXPLODE, aim, aim, 0, 0.3f, rgb(255, 190, 90), w.splash * TILE + 6});
                splashDamage(aim, w.splash, w.dmg, p.owner, p.shooter, w, p.target, p.forced);
            } else if (t && d <= hitR) {
                applyDamage(*t, w.dmg, p.owner, p.shooter, &w);
            }
            p.alive = false;
            continue;
        }
        p.pos += p.vel * SIM_DT;
        if (w.proj == PJ_ROCKET && (tick % 2 == 0))
            fx.push_back({FX_SMOKE, p.pos, p.pos, 0, 0.5f, rgb(200, 200, 200), 3, Vec2(0, -6)});
    }
    projs.erase(std::remove_if(projs.begin(), projs.end(), [](const Projectile& p) { return !p.alive; }), projs.end());
}

void Sim::updateFx() {
    // burning wrecks smoulder for a while (collected first: pushing while iterating would invalidate the loop)
    static std::vector<Fx> puffs; puffs.clear();
    for (auto& f : fx) if (f.type == FX_WRECK && f.vel.y < 0.5f && (tick % 6) == ((u32)(f.a.x * 7 + f.a.y * 13) % 6) && f.t < f.life * 0.7f)
        puffs.push_back({FX_SMOKE, f.a + Vec2((float)((int)(f.a.x + tick) % 9 - 4), (float)((int)(f.a.y + tick * 3) % 7 - 3)), Vec2(), 0, 2.4f, rgb(46, 44, 42), 6.0f + (float)(tick % 3), Vec2((float)((int)tick % 5 - 2), -14)});
    for (auto& p : puffs) fx.push_back(p);
    for (auto& f : fx) {
        f.t += SIM_DT;
        if (f.type == FX_SMOKE || f.type == FX_DEBRIS) {
            f.a += f.vel * SIM_DT;
            if (f.type == FX_DEBRIS) f.vel.y += 380 * SIM_DT;
            else f.vel = f.vel * 0.96f;
        }
    }
    fx.erase(std::remove_if(fx.begin(), fx.end(), [](const Fx& f) { return f.t >= f.life; }), fx.end());
}

void Sim::updateStorms() {
    for (auto& s : storms) {
        s.t += SIM_DT;
        s.nextShell -= SIM_DT;
        if (s.shellsLeft > 0 && s.nextShell <= 0) {
            s.nextShell = 0.25f;
            s.shellsLeft--;
            float a = rng.f(0, 6.283f), r = rng.f(0, s.radius);
            Vec2 at = s.pos + Vec2(std::cos(a) * r, std::sin(a) * r);
            Projectile p; p.pos = at + Vec2(0, -400); p.prevPos = p.pos; p.vel = p.pos; p.dest = at; p.weapon = 18; p.owner = s.owner; p.forced = s.force;
            p.arcLen = 0.7f; p.arcT = 0; p.life = 1.0f; p.target = NOREF;
            projs.push_back(p);
            emit(EV_SOUND, -1, SND_CANNON, at);
        }
    }
    storms.erase(std::remove_if(storms.begin(), storms.end(), [](const Storm& s) { return s.shellsLeft <= 0 && s.t > 2; }), storms.end());
}

// ------------------------------------------------------------ economy helpers
static bool predSupply(const Entity& e, void* ctx) { int owner = *(int*)ctx; return e.isBuilding() && e.owner == owner && e.constructed && e.bt().role == BR_SUPPLY; }
static bool predPile(const Entity& e, void* ctx) { return e.kind == EK_RESOURCE && e.amount > 0 && !(ctx && ((const Sim*)ctx)->inFallout(e.pos, TILE * 0.5f)); }   // (haulers do not drive into radiation, nor park where idle units would walk out of it)
static bool predAirfield(const Entity& e, void* ctx) { int owner = *(int*)ctx; return e.alive && e.isBuilding() && e.owner == owner && e.constructed && e.bt().role == BR_AIRFIELD; }   // (alive: never the ruins of a destroyed pad)

Entity* Sim::findSupplyBuilding(Entity& h) { int o = h.owner; return nearestEntity(h.pos, 1e9f, predSupply, &o); }
// The nearest pile a hauler can drive to: one with free ground beside it in the hauler's own region (the second island's piles are only worked by
// haulers shipped over there), clean ones first; only radioactive ones left is still better than standing idle.
Entity* Sim::findPile(Entity& h, float maxDist) {
    if (g_map.regionDirty) g_map.buildRegions();
    u16 rg = g_map.region[clampi(tileOf(h.pos.y), 0, MAP_H - 1) * MAP_W + clampi(tileOf(h.pos.x), 0, MAP_W - 1)];
    Entity* best = nullptr; float bd = maxDist * maxDist; bool bestHot = true;
    for (auto& e : ents) {
        if (!e.alive || !predPile(e, nullptr)) continue;
        float d = dist2(e.pos, h.pos);
        bool hot = inFallout(e.pos, TILE * 0.5f);
        if (d > maxDist * maxDist || (hot && !bestHot) || (hot == bestHot && d >= bd)) continue;
        bool reach = rg == 0;
        for (int j = -1; j <= 1 && !reach; j++) for (int i = -1; i <= 1; i++) { int x = e.tx + i, y = e.ty + j; if (inMap(x, y) && g_map.region[y * MAP_W + x] == rg) { reach = true; break; } }
        if (!reach) continue;
        best = &e; bd = d; bestHot = hot;
    }
    return best;
}
Entity* Sim::findZonePile(Entity& h) {
    Entity* best = nullptr; float bd = 1e18f;
    for (auto& e : ents) {
        if (!e.alive || e.kind != EK_RESOURCE || e.amount <= 0) continue;
        if (dist(e.pos, h.zone) > h.zoneR + TILE * 0.5f) continue;
        if (!exploredRaw(h.owner, clampi(e.tx, 0, MAP_W - 1), clampi(e.ty, 0, MAP_H - 1))) continue;   // haulers only work what their side has seen
        float d = dist2(h.pos, e.pos);
        if (inFallout(e.pos, TILE * 0.5f)) d += 1e12f;   // radioactive piles come last
        if (d < bd) { bd = d; best = &e; }
    }
    return best;
}
int Sim::padsUsed(const Entity& airfield, const Entity* except) const {
    int n = 0; Ref self = refOf(airfield);
    for (auto& u : ents) if (u.alive && &u != except && u.isUnit() && u.isAir() && !u.ut().heli && u.home == self) n++;
    return n;
}

Entity* Sim::findAirfield(Entity& a) {
    int o = a.owner;
    // a plane needs an airfield with a free pad; a helicopter can use any airfield (it lands on a helipad beside it)
    Entity* best = nullptr; float bd = 1e18f;
    for (auto& e : ents) {
        if (!predAirfield(e, &o)) continue;
        if (!a.ut().heli && padsUsed(e, &a) >= AIRFIELD_CAP) continue;
        float d = dist2(a.pos, e.pos);
        if (d < bd) { bd = d; best = &e; }
    }
    return best;
}

// ------------------------------------------------------------ aircraft
static float angDiff(float a, float b) {   // b - a wrapped to (-pi, pi]
    float d = b - a;
    while (d > 3.14159265f) d -= 6.2831853f;
    while (d < -3.14159265f) d += 6.2831853f;
    return d;
}

Vec2 Sim::padSlot(const Entity& h, const Entity& e) const {
    int idx = (int)(&e - &ents[0]);
    if (e.isUnit() && e.ut().heli) {   // helipads in two rings around the airfield (as many helicopters as you like: a crowded ring just packs tighter)
        int slot = idx % 20, ring = slot / 10;
        float a = (slot % 10) * 0.6283f + ring * 0.314f;
        float R = std::max(h.bt().w, h.bt().h) * TILE * 0.5f + 26.0f + ring * HELI_SPACE;
        Vec2 p = h.pos + Vec2(std::cos(a) * R, std::sin(a) * R * 0.8f);
        return Vec2(clampf(p.x, 12, WORLD_W - 12), clampf(p.y, 12, WORLD_H - 12));
    }
    // a plane takes the pad of its rank among the planes based here (at most AIRFIELD_CAP of them), so no two ever share a pad
    Ref hr = refOf(h);
    int slot = 0;
    for (int i = 0; i < idx; i++) { const Entity& o = ents[i]; if (o.alive && o.isUnit() && o.isAir() && !o.ut().heli && o.home == hr) slot++; }
    slot %= AIRFIELD_CAP;
    // 25 pads in a 5x5 grid: the oversized airfield has room for every one of them
    int col = slot % 5, row = slot / 5;
    return h.pos + Vec2((col - 2) * 42, (row - 2) * 34);
}

// Fixed-wing aircraft (the jets and the Wraith flying wing) fly like aeroplanes: they hold an airspeed between their stall speed and their
// top speed, speed up and brake at finite rates, and turn under a g-limit (the slower they fly, the tighter they turn). They never hover:
// an aircraft waiting in the air circles, and only the final approach to the pad is flown slower than the stall speed. The Vulture
// Gunship is a helicopter and hovers.
static inline bool fixedWing(const UnitType& ut) { return ut.kind == UK_AIR && !ut.heli; }
static inline float wrapAngle(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
static inline float baseTurn(const UnitType& ut) { return ut.jet ? JET_TURN : BOMBER_TURN; }   // turn rate at top speed

float Sim::airTopSpeed(const Entity& e) const {
    const UnitType& ut = e.ut();
    if (ut.sprintSpeed <= 0.0f) return ut.speed;
    return (e.ammo > 0 || (e.slowAt >= 0 && time < e.slowAt)) ? ut.sprintSpeed : ut.speed;   // (slowAt: the dash that clears its own fireball)
}

float Sim::airTurnRate(const Entity& e) const {
    const UnitType& ut = e.ut();
    float base = baseTurn(ut);
    return std::min(base * AIR_TURN_MAX, base * airTopSpeed(e) / std::max(1.0f, e.airspeed));
}

void Sim::airSteer(Entity& e, float want, float wantSpeed, bool keepOnMap) {
    const UnitType& ut = e.ut();
    const float top = airTopSpeed(e);
    float vmin = ut.heli ? 0.0f : top * AIR_STALL;
    if (keepOnMap) {   // about to leave the map within about one turn: pull round toward the middle, tightly
        float look = e.airspeed / airTurnRate(e) * 1.3f + 40.0f;
        Vec2 ahead = e.pos + Vec2(std::cos(e.angle), std::sin(e.angle)) * look;
        if (ahead.x < 8 || ahead.y < 8 || ahead.x > WORLD_W - 8 || ahead.y > WORLD_H - 8) {
            Vec2 c = Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f) - e.pos; want = std::atan2(c.y, c.x);
            wantSpeed = std::min(wantSpeed, top * 0.5f);
        }
    }
    wantSpeed = clampf(wantSpeed, vmin, top);
    if (ut.heli) e.airspeed = wantSpeed;
    else if (e.airspeed < wantSpeed) e.airspeed = std::min(wantSpeed, e.airspeed + top * 1.1f * SIM_DT);   // throttle up
    else e.airspeed = std::max(wantSpeed, e.airspeed - top * 1.6f * SIM_DT);                            // airbrakes
    float turn = airTurnRate(e) * SIM_DT;
    float da = clampf(angDiff(e.angle, want), -turn, turn);
    e.angle = wrapAngle(e.angle + da);
    if (!ut.heli) e.roll += (clampf(da / std::max(1e-4f, turn), -1.0f, 1.0f) * 0.9f - e.roll) * std::min(1.0f, 5.0f * SIM_DT);   // banks into its turns
    e.pos += Vec2(std::cos(e.angle), std::sin(e.angle)) * (e.airspeed * SIM_DT);
    e.pos.x = clampf(e.pos.x, 8, WORLD_W - 8); e.pos.y = clampf(e.pos.y, 8, WORLD_H - 8);
}

void Sim::flyTo(Entity& e, Vec2 dest, float speed) {
    const UnitType& ut = e.ut();
    Vec2 d = dest - e.pos;
    float l = d.len();
    if (!fixedWing(ut)) {
        if (l > 3) { e.pos += d.norm() * std::min(l, speed * SIM_DT); e.angle = std::atan2(d.y, d.x); }
        e.airspeed = l > 3 ? speed : 0;
        return;
    }
    // final approach: bleed speed in proportion to the distance left, which keeps the turn circle well under half the distance,
    // so the craft always spirals in onto the spot instead of orbiting it
    if (l < 3) { e.airspeed = 0; return; }
    float want = std::atan2(d.y, d.x);
    if (l > 260) { airSteer(e, want, speed); return; }
    float vt = std::max(30.0f, std::min(speed, l * 1.6f));
    const float top = airTopSpeed(e);
    if (e.airspeed > vt) e.airspeed = std::max(vt, e.airspeed - top * 1.6f * SIM_DT);
    else e.airspeed = std::min(vt, e.airspeed + top * 1.1f * SIM_DT);
    float turn = airTurnRate(e) * SIM_DT;
    e.angle = wrapAngle(e.angle + clampf(angDiff(e.angle, want), -turn, turn));
    if (l < 14) e.angle = wrapAngle(e.angle + clampf(angDiff(e.angle, want) * 0.5f, -turn, turn));
    Vec2 step(std::cos(e.angle) * e.airspeed * SIM_DT, std::sin(e.angle) * e.airspeed * SIM_DT);
    e.pos += step.len() >= l ? d : step;
}

// Helicopters hover on spots of their own. Where several are sent to the same place (one move order, a rally point, crowded guard slots,
// a shared helipad), they take spots in hexagonal rings around it a small gap apart (each the free spot of the innermost ring nearest to
// it) instead of piling up on one point. Every helicopter claims the spot it heads for; when two claims are closer than the spacing, the
// helicopter nearer to its own spot keeps it (ties: the lower index) and the other moves over, so one already hovering keeps its spot,
// one passing by never pushes anybody off, and the choice settles as they arrive.
Vec2 Sim::heliSpot(Entity& e, Vec2 want) {
    const float s = HELI_SPACE;
    const int self = (int)(&e - &ents[0]);
    struct Claim { Vec2 spot; float d2; int idx; };   // another helicopter's claimed spot and its own distance (squared) to it
    std::vector<Claim> claims;
    const float reach = s * 8.0f;
    for (int i : heliList) {
        const Entity& h = ents[i];
        if (i == self || !h.alive || !h.isUnit() || !h.ut().heli || h.hoverTick == 0 || h.hoverTick + 1 < tick) continue;
        if (dist2(h.hoverSpot, want) > reach * reach) continue;
        claims.push_back({h.hoverSpot, dist2(h.pos, h.hoverSpot), i});
    }
    auto taken = [&](Vec2 c) {
        float de = dist2(e.pos, c);
        for (auto& h : claims) if (dist2(h.spot, c) < s * s && (h.d2 < de || (h.d2 == de && h.idx < self))) return true;
        return false;
    };
    Vec2 spot = want;
    if (taken(want)) {
        bool found = false;
        for (int ring = 1; ring <= 7 && !found; ring++) {
            float bd = 1e18f;
            for (int side = 0; side < 6; side++) {
                Vec2 c0 = Vec2(std::cos(side * 1.0471976f), std::sin(side * 1.0471976f)) * (ring * s);
                Vec2 c1 = Vec2(std::cos((side + 1) * 1.0471976f), std::sin((side + 1) * 1.0471976f)) * (ring * s);
                for (int k = 0; k < ring; k++) {
                    Vec2 c = want + c0 + (c1 - c0) * ((float)k / ring);
                    c = Vec2(clampf(c.x, 12, WORLD_W - 12), clampf(c.y, 12, WORLD_H - 12));
                    float d = dist2(e.pos, c);
                    if (d < bd && !taken(c)) { bd = d; spot = c; found = true; }
                }
            }
        }
    }
    e.hoverSpot = spot; e.hoverTick = tick;
    return spot;
}

void Sim::heliHover(Entity& e, Vec2 spot, float speed) {
    Vec2 d = spot - e.pos; float l = d.len();
    if (l <= 1.5f) { e.airspeed = 0; return; }
    float v = l > 120.0f ? speed : std::max(speed * 0.25f, speed * l / 120.0f);   // eases in over the last few metres instead of stopping dead
    e.pos += d * (std::min(l, v * SIM_DT) / l);
    e.airspeed = v;
    if (l > 10.0f) { float tr = 5.0f * SIM_DT; e.angle = wrapAngle(e.angle + clampf(angDiff(e.angle, std::atan2(d.y, d.x)), -tr, tr)); }   // only a real transfer turns the nose
}

// Waiting in the air: a fixed-wing craft circles the spot at a relaxed speed (the circle is never tighter than it can fly), a helicopter
// hovers over it (on a spot of its own when others wait there too).
void Sim::airLoiter(Entity& e, Vec2 c, float R, float speedFrac) {
    const UnitType& ut = e.ut();
    if (!fixedWing(ut)) { heliHover(e, heliSpot(e, Vec2(clampf(c.x, 2.0f * TILE, WORLD_W - 2.0f * TILE), clampf(c.y, 2.0f * TILE, WORLD_H - 2.0f * TILE))), ut.speed * 0.6f); return; }
    const float top = airTopSpeed(e);
    float v = top * speedFrac;
    float rTurn = v / std::min(baseTurn(ut) * AIR_TURN_MAX, baseTurn(ut) * top / v);
    R = std::min(std::max(R, rTurn * 1.5f + 12.0f), WORLD_H * 0.3f);
    float m = R + rTurn * 1.3f + 48.0f;   // the whole circle stays far enough inside the map that the edge look-ahead never fires on it
    c = Vec2(clampf(c.x, m, WORLD_W - m), clampf(c.y, m, WORLD_H - m));
    Vec2 r = e.pos - c; float l = r.len();
    if (l > R * 2.5f) { airSteer(e, std::atan2(-r.y, -r.x), top * std::max(speedFrac, 0.85f)); return; }   // still on the way there
    float phi = l > 1 ? std::atan2(r.y, r.x) : e.angle;
    Vec2 aim = c + Vec2(std::cos(phi + 0.75f), std::sin(phi + 0.75f)) * R;   // a carrot on the circle a little ahead: converges onto the circle and flies it anticlockwise
    airSteer(e, std::atan2(aim.y - e.pos.y, aim.x - e.pos.x), v);
}

// Air-to-air with a fixed-wing fighter. It flies lead pursuit (aims where the target will be when the shot arrives), pulls round at its
// corner speed (the slowest safe speed, where it turns tightest) whenever the target is off its nose, slows to the target's pace when it
// is tucked in behind it, and extends straight out for a moment when the target sits inside its turn circle behind the wing, coming
// round again from further out. It fires whenever the target is in range inside its weapon cone (wide for homing missiles, narrow for
// guns and lasers) and stays on the target after a burst instead of breaking away: that is how it out-turns helicopters and drones.
void Sim::airDogfight(Entity& e, Entity& t) {
    const UnitType& ut = e.ut();
    const Weapon& w = WEAPONS[ut.weapon];
    Vec2 tv = (t.pos - t.prevPos) * (1.0f / SIM_DT);
    if (tv.len2() > 1500.0f * 1500.0f) tv = Vec2();
    float D = dist(e.pos, t.pos);
    float shot = w.projSpeed > 0 ? w.projSpeed : 1800.0f;
    Vec2 aim = t.pos + tv * clampf(D / shot, 0.0f, 0.45f);
    Vec2 to = aim - e.pos;
    float want = std::atan2(to.y, to.x);
    float off = std::abs(angDiff(e.angle, want));
    float d = distToEntity(e.pos, t) / TILE;
    float cone = w.proj == PJ_ROCKET ? 0.9f : 0.55f;
    const float top = airTopSpeed(e);
    float v = top;
    float rTurn = std::max(e.airspeed, 1.0f) / airTurnRate(e);
    if (e.jetBreak > 0) {
        e.jetBreak -= SIM_DT;
        want = e.angle;                                                       // extending: straight out at full power
    } else {
        if (off > 0.45f) v = top * 0.5f;                                       // corner speed: the tightest turn the airframe can pull
        else if (D < 2.5f * TILE) v = std::max(top * 0.5f, std::min(top, tv.len() * 1.1f));   // tucked in behind: do not overshoot
        if (off > 1.9f && D < rTurn * 1.3f) { e.jetBreak = 0.6f; e.jetMode = 1; }
        else if (d <= w.range + 0.1f && off < cone && e.cooldown <= 0 && tryFire(e, t)) { e.stuckTimer = 0; if (ut.jet) emit(EV_SOUND, e.owner, SND_JET, e.pos); }
    }
    airSteer(e, want, v);
}

// Ground attack with a jet: strafing passes. It dives on the target and fires when lined up, then peels away from the defences and
// comes round again; a target it cannot line up on (inside its turn circle) makes it extend straight out for a fresh run.
void Sim::jetAttack(Entity& e, Entity& t) {
    if (t.isAir()) { airDogfight(e, t); return; }
    const UnitType& ut = e.ut();
    const Weapon& w = WEAPONS[ut.weapon];
    Vec2 to = t.pos - e.pos;
    float d = distToEntity(e.pos, t) / TILE;
    float want = std::atan2(to.y, to.x);
    float off = std::abs(angDiff(e.angle, want));
    const float top = airTopSpeed(e);
    float v = top;
    if (e.jetBreak > 0) {
        e.jetBreak -= SIM_DT;
        want = e.jetMode == 1 ? e.angle : std::atan2(-to.y, -to.x);          // extend straight out, or peel away from the target's defences
    } else if (d <= w.range && off < 0.3f) {
        if (e.cooldown <= 0 && tryFire(e, t)) { e.stuckTimer = 0; e.jetBreak = 1.25f; e.jetMode = 0; emit(EV_SOUND, e.owner, SND_JET, e.pos); }
    } else if (d < 2.0f && e.cooldown > 0) {
        e.jetBreak = 0.9f; e.jetMode = 0;                                     // arrived with the guns still cooling: peel off and come round again
    } else if (d < w.range + 5.0f) {
        if (off > 0.5f) v = top * 0.6f;                                       // pull round hard onto the target
        e.stuckTimer += SIM_DT;
        if (e.stuckTimer > 1.1f) { e.jetBreak = 0.8f; e.jetMode = 1; e.stuckTimer = 0; }
    } else e.stuckTimer = 0;
    airSteer(e, want, v);
}

// A bombing run. The aircraft steers at the target (leading a moving one), and when the middle of a stick of bombs would land on it
// it releases them one after another along the flight line, then flies straight on for a second before swinging round for another pass.
void Sim::bomberAttack(Entity& e, Entity& t) {
    const UnitType& ut = e.ut();
    const float top = airTopSpeed(e);
    Vec2 tp = t.pos;
    if (t.isUnit()) tp += (t.pos - t.prevPos) * (1.0f / SIM_DT) * 0.9f;
    Vec2 to = tp - e.pos;
    float D = to.len();
    float want = std::atan2(to.y, to.x);
    float carry = top * 0.42f;                                        // a bomb keeps the aircraft's forward speed while it falls
    float release = carry + top * 0.11f * (BOMB_STICK - 1) * 0.5f;    // distance at which the middle of the stick lands on the target
    // A supersonic nuclear bomber has one aimed bomb: it can let it go over a wide window (its compact warhead is aimed straight at the
    // target, so the release distance is not what makes it hit) instead of the narrow stick window a slow bomber flies through.
    const float releaseAhead = ut.nukeBomb ? top * 0.05f + 8.0f : 8.0f;
    const float releaseBehind = ut.nukeBomb ? top * 0.25f : 40.0f;
    if (e.jetBreak > 0) {
        e.jetBreak -= SIM_DT;
        want = e.angle;                                               // straight on over and past the target
    } else if (e.bombsLeft <= 0 && e.ammo > 0 && D <= release + releaseAhead && D >= release - releaseBehind && std::abs(angDiff(e.angle, want)) < 0.2f) {
        e.bombsLeft = std::min(BOMB_STICK, e.ammo); e.bombTimer = 0; e.jetBreak = ut.nukeBomb ? 1.4f : 1.15f; e.stuckTimer = 0;
        emit(EV_SOUND, e.owner, SND_AIR, e.pos);
    } else if (D < release - releaseBehind && e.bombsLeft <= 0) {
        e.jetBreak = 0.8f;                                            // too close or badly lined up: fly through and come round again
    } else if (e.bombsLeft <= 0 && D < release + 160) {
        // near the target but never lining up (it sits inside the bomber's turn circle): straighten out and make a fresh run
        e.stuckTimer += SIM_DT;
        if (e.stuckTimer > 1.6f) { e.jetBreak = 1.0f; e.stuckTimer = 0; }
    } else e.stuckTimer = 0;
    airSteer(e, want, top);
}

void Sim::dropBomb(Entity& e, Vec2 at) {
    Projectile p;
    p.pos = e.pos; p.prevPos = e.pos; p.vel = e.pos;      // vel keeps the release point (the ground point under the aircraft), like a shell's launch point
    p.dest = at; p.target = NOREF; p.owner = e.owner; p.shooter = refOf(e);
    { const Entity* ft = get(e.targetEnt); p.forced = ft && e.order == O_ATTACK && !enemies(e.owner, ft->owner); }   // a human-ordered run on a friendly: the bombs hurt everything near it
    if (e.ut().nukeBomb) {
        // The compact nuclear bomb: it is aimed at the target itself (the casing is small, so the plane lets it go over a wide window) and
        // the bomber keeps its dash for a moment after the release to clear the fireball it is about to make (see e.slowAt).
        const Entity* tg = get(e.targetEnt);
        if (tg && !tg->isAir()) p.dest = tg->pos;
        p.weapon = W_NUKE_BOMB;
        p.arcLen = SBOMBER_BOMB_FALL;
        p.arcT = 0; p.life = p.arcLen + 0.3f;
        e.slowAt = time + std::max(SBOMBER_CLEAR, SBOMBER_BOMB_FALL + 0.15f);
    } else {
        p.weapon = e.order == O_STRIKERUN ? W_STRIKE_BOMB : (e.ut().faction == F_CYBER ? W_BOMB_CYBER : W_BOMB_CLANKER);
        p.arcLen = 0.62f; p.arcT = 0; p.life = p.arcLen + 0.3f;
    }
    projs.push_back(p);
}

// A bomb going off: a fireball with secondary bursts along the ground, a shockwave ring, a rising smoke column, flung debris and sparks.
void Sim::bombImpact(Projectile& p) {
    if (p.weapon == W_NUKE_BOMB) {
        // the small casing cracks open and the silver warhead goes off: the damage of an Underground Nuke (UNUKE_DAMAGE) but a Nuke Ramp's
        // blast area, so the fireball is much smaller than a silo warhead's. p.vel is the release point (the bomb's arc starts there).
        Nuke n;
        n.from = p.vel; n.pos = p.dest; n.owner = p.owner; n.t = NUKE_FLIGHT - 0.001f; n.force = p.forced;
        n.scale = SBOMBER_BOMB_SCALE; n.deep = true; n.source = p.shooter;
        if (p.nukeScale > 0) { n.scale = p.nukeScale; n.deep = false; }   // a Nuclear Bomber call-in: an ordinary mid-sized warhead
        nukes.push_back(n);
        fx.push_back({FX_FLASH, p.dest, p.dest, 0, 0.20f, rgb(240, 244, 250), 44.0f});
        emit(EV_SOUND, -1, SND_EXPLODE_L, p.dest);
        return;
    }
    const Weapon& w = WEAPONS[p.weapon];
    Vec2 at = p.dest;
    float R = w.splash * TILE;
    splashDamage(at, w.splash, w.dmg, p.owner, p.shooter, w, NOREF, p.forced);
    bool cy = p.weapon == W_BOMB_CYBER;
    const bool carpet = p.weapon == W_STRIKE_BOMB;   // an Air Strike's hundred bombs: lighter smoke each (Vec2(0, -1) marks it for the renderer), or the spot drowns in it
    Color fire = cy ? rgb(190, 235, 255) : rgb(255, 210, 140), hot = cy ? rgb(120, 220, 255) : rgb(255, 150, 60);
    fx.push_back({FX_EXPLODE, at, at, 0, 0.75f, fire, R * 1.25f, carpet ? Vec2(0, -1) : Vec2()});
    fx.push_back({FX_RING, at, at, 0, 0.8f, hot, R * 2.3f});
    fx.push_back({FX_RING, at, at, -0.1f, 0.6f, fire, R * 1.4f});
    for (int k = 0; k < (carpet ? 2 : 4); k++) {
        float a = rng.f(0, 6.283f), r = R * rng.f(0.35f, 0.95f); Vec2 q = at + Vec2(std::cos(a) * r, std::sin(a) * r * 0.8f);
        fx.push_back({FX_EXPLODE, q, q, -rng.f(0.05f, 0.4f), 0.65f, hot, R * rng.f(0.5f, 0.85f), carpet ? Vec2(0, -1) : Vec2()});
    }
    // (an Air Strike lays a hundred bombs on one spot: every bomb's own smoke column would pile up into a black blot, so each of those leaves one
    // thinner, paler wisp, and the wind carries the lot off together)
    for (int k = 0; k < (carpet ? (rng.f() < 0.3f ? 1 : 0) : 3); k++) fx.push_back({FX_SMOKE, at, at, -rng.f(0.0f, 0.5f), carpet ? 2.6f : 3.6f, carpet ? rgb(92, 86, 80) : rgb(54, 50, 48), R * rng.f(0.55f, 0.85f) * (carpet ? 0.7f : 1.0f), Vec2(rng.f(-12, 12) + (carpet ? 18.0f : 0.0f), -rng.f(26, 48))});
    for (int k = 0; k < (carpet ? 6 : 12); k++) fx.push_back({FX_DEBRIS, at, at, 0, rng.f(0.6f, 1.3f), rgb(80, 76, 72), rng.f(2, 5), Vec2(rng.f(-210, 210), rng.f(-300, -60))});
    for (int k = 0; k < 5; k++) fx.push_back({FX_SPARK, at, at, 0, rng.f(0.2f, 0.5f), fire, rng.f(3, 6)});
    emit(EV_SOUND, -1, SND_EXPLODE_L, at);
}

// ------------------------------------------------------------ units left on their own
// A unit that is not carrying out a player's order (idle, holding its post, guarding an area, or busy with something it picked itself)
// looks around twice a second:
//  * danger it cannot fight: being shot by something it cannot hit back (aircraft for a rifleman, a sniper it cannot see), or losing a
//    fight away from its base -> it backs off a few tiles toward its friends and holds there for a while; a badly hurt aircraft flies
//    home to its pad to be patched up; a badly hurt soldier or vehicle goes to a medic
//  * fire from beyond its guard zone or its reach, by something it can hit in a fight it can win -> it goes after the shooter
//  * a friend (its own or an ally's unit or structure) hit by an enemy nearby -> it goes to help
// Units of human players also get out of an incoming nuke's circle and out of radiation, whatever they are doing unless the player is
// moving them (the computer commander moves its own units, see AiPlayer::dodgeNukes), and pick their work up again afterwards.
// Whatever a unit takes on by itself stays leashed to its post (guard slot, zone, the spot it stood on) and it returns there afterwards.
// Only enemies count: a human's force fire on a friend never makes anybody shoot back or come to help against it.
static const float AUTO_LEASH = 12.0f;    // tiles a ground unit may stray from its post on its own initiative
static const float AIR_LEASH = 20.0f;     // the same for aircraft
static const float HELP_R = 9.0f;         // tiles around an idle ground unit's post in which it answers a friend's call
static const float AIR_HELP_R = 16.0f;    // the same for aircraft (around their pad, rally point or patrol circle)

static float dpsVs(const Entity& s, const Entity& t) {
    int wi = s.weapon(); if (wi < 0) return 0;
    const Weapon& w = WEAPONS[wi];
    if (w.cooldown <= 0) return 0;
    return w.dmg * w.mult[t.armor()] * std::max(1, w.burst) / w.cooldown;
}

Vec2 Sim::postOf(const Entity& e) const {
    if (e.zoneR > 0) return e.zone;
    if (e.isAir()) {
        if (e.loiterUntil > time) return e.loiter;
        const Entity* h = get(e.home);
        if (h) return h->hasRally ? h->rally : h->pos;
        return e.loiterUntil >= 0 ? e.loiter : e.pos;
    }
    return e.guardPos;
}

// Lanchester-style estimate of the local fight around a shooter: how long the foe's side needs to kill this unit against how long this
// unit and its friends need to kill the foe (> 1: we win). Splash, armour piercing and movement are ignored; it only has to tell a fight
// worth taking from a hopeless one.
float Sim::fightOdds(Entity& e, Entity& foe) {
    float ours = 0, theirs = 0;
    forEachNear(e.pos, 5.0f * TILE, [&](Entity& f) {
        if (f.kind == EK_RESOURCE || f.owner < 0 || enemies(e.owner, f.owner) || f.fall > 0) return;
        if (f.isBuilding() && (!f.constructed || (players[f.owner].lowPower() && f.bt().power < 0))) return;
        if (f.isUnit() && f.ut().role != UR_COMBAT) return;
        int fw = f.weapon();
        if (fw < 0 || !canTarget(f, foe)) return;
        float inRange = distToEntity(f.pos, foe) <= (WEAPONS[fw].range + 0.5f) * TILE ? 1.0f : (f.isUnit() ? 0.6f : 0.0f);
        ours += dpsVs(f, foe) * inRange;
    });
    forEachNear(foe.pos, 5.0f * TILE, [&](Entity& g) {
        if (g.kind == EK_RESOURCE || !enemies(e.owner, g.owner) || g.fall > 0) return;
        if (g.isBuilding() && (!g.constructed || (players[g.owner].lowPower() && g.bt().power < 0))) return;
        if (!canTarget(g, e)) return;
        theirs += dpsVs(g, e);
    });
    if (theirs <= 0) return 10.0f;
    if (ours <= 0) return 0.0f;
    return (e.hp / theirs) / (foe.hp / ours);
}

void Sim::autoEngage(Entity& e, Entity& t) {
    e.order = O_ATTACK; e.targetEnt = refOf(t); e.engaged = NOREF; e.autoTask = true; e.leashed = false; e.repathTimer = 0;
    e.postOrder = e.zoneR > 0 ? O_GUARDAREA : (e.isAir() ? O_IDLE : O_GUARDPOS);
    if (!e.isAir()) e.path.clear();
}

void Sim::moveAside(Entity& e, Vec2 dest, float holdFor) {
    keepSite(e);   // (a dozer that ducks out of danger goes back to its site afterwards)
    dest = Vec2(clampf(dest.x, TILE, WORLD_W - TILE), clampf(dest.y, TILE, WORLD_H - TILE));
    if (!e.isAir()) dest = e.ut().naval ? g_map.nearestWater(dest, 10, true, g_map.compAt(e.pos)) : g_map.nearestFree(dest, 6);
    e.order = O_MOVE; e.target = dest; e.engaged = NOREF; e.autoTask = true; e.leashed = false; e.postOrder = O_IDLE; e.actionTimer = 0;
    e.evadeUntil = time + holdFor;
    if (!e.isAir()) requestPath(e, dest);
}

bool Sim::escapeDestination(const Entity& e, Vec2 threat, float tiles, Vec2& dest) const {
    Vec2 away = e.pos - threat; float l = away.len();
    away = l > 1 ? away * (1.0f / l) : Vec2(-std::cos(e.angle), -std::sin(e.angle));
    Vec2 home = (players[e.owner].basePos - e.pos).norm();
    Vec2 dir = (away + home * 0.45f).norm();
    if (dir.len2() < 0.01f) dir = away;
    float angle = std::atan2(dir.y, dir.x), best = -1e18f;
    bool found = false;
    std::vector<Vec2> path;
    const float offsets[] = {0, -0.65f, 0.65f, -1.2f, 1.2f};
    for (float off : offsets) {
        Vec2 p = e.pos + Vec2(std::cos(angle + off), std::sin(angle + off)) * (tiles * TILE);
        p = Vec2(clampf(p.x, TILE, WORLD_W - TILE), clampf(p.y, TILE, WORLD_H - TILE));
        if (!e.isAir()) p = e.ut().naval ? g_map.nearestWater(p, 3, true, g_map.compAt(e.pos)) : g_map.nearestFree(p, 3);
        float gain = dist(p, threat) - l;
        if (gain < TILE * 0.4f || inFallout(p)) continue;
        bool direct = e.isAir() || (e.ut().naval ? g_map.navLineClear(e.pos, p) : g_map.lineClear(e.pos, p));
        if (!direct) {
            bool reachable = e.ut().naval ? g_map.findNavalPath(e.pos, p, path, 800) : g_map.findPath(e.pos, p, path, 800);
            if (!reachable || path.empty() || dist(path.back(), p) > TILE) continue;
            bool safe = true;
            for (Vec2 q : path) if (dist(q, threat) < l - TILE * 0.25f || inFallout(q)) safe = false;
            if (!safe) continue;
        }
        float score = gain + (dist(e.pos, players[e.owner].basePos) - dist(p, players[e.owner].basePos)) * 0.2f - (direct ? 0 : TILE);
        if (score > best) { best = score; dest = p; found = true; }
        if (off == 0 && direct) break;
    }
    return found;
}

void Sim::retreatFrom(Entity& e, Vec2 threat, float tiles) {
    Vec2 dest;
    if (!escapeDestination(e, threat, tiles, dest)) { e.evadeUntil = time + 1.0f; return; }
    if (e.zoneR <= 0) e.guardPos = dest;   // an idle unit makes its new spot its post; a zone guard goes back to its slot later
    moveAside(e, dest, 6.0f);
    e.targetEnt = NOREF;
}

bool Sim::dodgeDanger(Entity& e) {
    if (e.order == O_MOVE && !e.autoTask) return false;   // the player is moving it: their call
    // an incoming warhead: whatever it would hurt on the ground gets out of the circle while there is time (aircraft would always
    // make it, so nuking an airfield only ever scrambled its planes: they stay and burn)
    for (auto& n : nukes) {
        if (e.isAir()) break;
        bool hurts = nukeHurts(n, e.owner);
        float left = flightOf(n) - n.t;
        if (!hurts || n.super || n.t < 1.0f || left < 0.3f) continue;   // (nothing outruns a Super Nuke's blast: nobody tries)
        float R = nukeRadius(n) + 1.5f * TILE;
        if (dist(e.pos, n.pos) > R) continue;
        if (e.order == O_MOVE && dist(e.target, n.pos) > R) return false;   // already on its way out
        Vec2 away = e.pos - n.pos; float l = away.len();
        if (l < 4) { away = players[e.owner].basePos - n.pos; l = away.len(); }
        away = l > 1 ? away * (1.0f / l) : Vec2(1, 0);
        Vec2 dest = n.pos + away * (R + 2.5f * TILE);
        if (dest.x < TILE || dest.y < TILE || dest.x > WORLD_W - TILE || dest.y > WORLD_H - TILE) {   // the map edge is in the way: run along it
            Vec2 side(-away.y, away.x);
            Vec2 c(WORLD_W * 0.5f, WORLD_H * 0.5f);
            if ((side.x * (c.x - n.pos.x) + side.y * (c.y - n.pos.y)) < 0) side = side * -1.0f;
            dest = n.pos + side * (R + 2.5f * TILE);
        }
        if (e.order != O_IDLE && e.order != O_GUARDPOS && e.order != O_MOVE && e.order != O_REARM) {   // pick the work up again afterwards
            e.resumeOrder = e.order; e.resumePost = e.postOrder; e.resumeTarget = e.target;
        }
        Ref keep = e.targetEnt;
        if (e.zoneR <= 0 && e.isUnit() && !e.isAir() && e.ut().role != UR_HARVESTER) e.guardPos = dest;
        moveAside(e, dest, left + 1.0f);
        if (e.resumeOrder != O_IDLE) e.targetEnt = keep;
        return true;
    }
    // radiation: walk out of it (aircraft fly over it unharmed; an Anti-Radiation Truck is there on purpose, so it does not, and neither
    // does anything standing in a decontamination field: that air is being scrubbed)
    if (!e.isAir() && e.ut().role != UR_DECON && (e.order == O_IDLE || e.order == O_GUARDPOS || e.order == O_GUARDAREA) && inFallout(e.pos, TILE * 0.5f)) {
        for (auto& t : ents) if (t.alive && t.isUnit() && t.ut().role == UR_DECON && !t.carrier.valid() && t.disabledUntil <= time && dist(e.pos, t.pos) <= RADTRUCK_RADIUS * TILE) return false;
        for (auto& f : fallouts) {
            if (dist(e.pos, f.pos) > f.r + TILE * 0.5f) continue;
            Vec2 away = e.pos - f.pos; float l = away.len();
            away = l > 1 ? away * (1.0f / l) : (players[e.owner].basePos - f.pos).norm();
            Vec2 dest = f.pos + away * (f.r + 2.0f * TILE);
            if (e.zoneR <= 0) e.guardPos = dest;
            moveAside(e, dest, 4.0f);
            e.targetEnt = NOREF;
            return true;
        }
    }
    return false;
}

bool Sim::autonomy(Entity& e) {
    const UnitType& ut = e.ut();
    if (e.fall > 0 || e.disabledUntil > time || e.owner < 0) return false;
    if (!players[e.owner].isAI && dodgeDanger(e)) return true;
    // a hauler at work keeps out of the way of a raider: an enemy gun closing on its pile, with no friendly fighters near enough to see it off,
    // sends it away from the threat (the way it leaves is toward the depot) until the danger has passed; it goes back to the pile afterwards
    if (ut.role == UR_HARVESTER && e.order == O_HARVEST && players[e.owner].haulFlee && time > e.evadeUntil) {
        Entity* raider = nullptr; float rd = 6.5f * TILE; int guards = 0;
        forEachNear(e.pos, 8.0f * TILE, [&](Entity& f) {
            if (f.kind == EK_RESOURCE || f.owner < 0 || !f.isUnit() || f.ut().role != UR_COMBAT || f.weapon() < 0 || f.fall > 0) return;
            if (!enemies(e.owner, f.owner)) { if (!f.isAir()) guards++; return; }
            if (!canTarget(f, e)) return;
            float d = dist(f.pos, e.pos); if (d < rd) { rd = d; raider = &f; }
        });
        if (raider && guards < 2 && !inFallout(e.pos)) {
            Entity* hub = findSupplyBuilding(e);
            // eight ways out: the one that ends farthest from the raider, inside the map and not far from the depot (no running into a corner)
            Vec2 dest = e.pos; float best = -1e18f;
            for (int k = 0; k < 8; k++) {
                float a = k * 0.7854f;
                Vec2 c = e.pos + Vec2(std::cos(a), std::sin(a)) * (9.0f * TILE);
                if (c.x < 3 * TILE || c.y < 3 * TILE || c.x > WORLD_W - 3 * TILE || c.y > WORLD_H - 3 * TILE) continue;
                c = g_map.nearestFree(c, 3);
                float sc = dist(c, raider->pos) / TILE - (hub ? dist(c, hub->pos) / TILE * 0.35f : 0.0f);
                if (sc > best) { best = sc; dest = c; }
            }
            if (best < -1e17f) dest = e.pos + (e.pos - raider->pos).norm() * (6.0f * TILE);
            e.resumeOrder = O_HARVEST; e.resumePost = e.postOrder; e.resumeTarget = e.target;
            Ref keep = e.targetEnt;
            moveAside(e, dest, 4.0f);
            e.targetEnt = keep;
            return true;
        }
    }
    bool onOwn = e.order == O_IDLE || e.order == O_GUARDPOS || e.order == O_GUARDAREA || ((e.order == O_ATTACK || e.order == O_MOVE) && (e.autoTask || e.leashed));
    if (!onOwn) return false;
    bool combat = ut.role == UR_COMBAT && ut.weapon >= 0;
    Entity* att = get(e.attacker);
    bool foeHit = e.lastDamaged > 0 && time - e.lastDamaged < 1.2f && att && att->owner >= 0 && enemies(e.owner, att->owner) && att->fall <= 0;
    if (e.isAir()) {
        if (!combat) return false;
        if (foeHit && e.hp < e.maxHp * 0.3f && get(e.home) && e.order != O_REARM) {   // badly hurt and still under fire: home to be patched up
            e.order = O_REARM; e.targetEnt = NOREF; e.autoTask = true; e.leashed = false; return true;
        }
        if (ut.ammo > 0 && e.ammo <= 0) return false;
        if (foeHit && att->isAir() && canTarget(e, *att) && e.targetEnt != refOf(*att) && dist(e.pos, att->pos) < 9.0f * TILE) { autoEngage(e, *att); return true; }   // turn on the aircraft shooting at us
        if (e.order == O_ATTACK || e.order == O_MOVE) return false;
        if (foeHit && canTarget(e, *att) && distToEntity(postOf(e), *att) < AIR_LEASH * TILE) { autoEngage(e, *att); return true; }
        // a friend in trouble near our post
        Vec2 post = postOf(e); float R = (e.zoneR > 0 ? e.zoneR / TILE + 6.0f : AIR_HELP_R) * TILE;
        Entity* best = nullptr; float bd = 1e18f;
        forEachNear(post, R, [&](Entity& f) {
            if (&f == &e || f.kind == EK_RESOURCE || f.owner < 0 || enemies(e.owner, f.owner) || f.lastDamaged <= 0 || time - f.lastDamaged > 1.5f) return;
            Entity* a = get(f.attacker);
            if (!a || a->owner < 0 || !enemies(e.owner, a->owner) || a->fall > 0 || !canTarget(e, *a)) return;
            if (!a->isAir() && ut.jet && aaCover(*a, e.owner) >= 4.0f) return;   // not into a wall of flak
            float d = dist2(e.pos, a->pos); if (d < bd) { bd = d; best = a; }
        });
        if (best) { autoEngage(e, *best); return true; }
        return false;
    }
    bool nearHome = false;   // defending the base: no backing off from a fight it can answer
    forEachNear(e.pos, 6.0f * TILE, [&](Entity& b) { if (!nearHome && b.isBuilding() && b.owner >= 0 && !enemies(e.owner, b.owner)) nearHome = true; });
    if (!combat) {   // dozers, haulers and medics standing about: step away from whatever shoots at them
        if (foeHit && e.order == O_IDLE && time > e.evadeUntil) { retreatFrom(e, att->pos, 4.0f); return true; }
        return false;
    }
    const Weapon& w = WEAPONS[ut.weapon];
    Vec2 post = postOf(e);
    float leash = (e.zoneR > 0 ? e.zoneR : 0.0f) + AUTO_LEASH * TILE;
    if (foeHit) {
        bool can = canTarget(e, *att);
        float d = distToEntity(e.pos, *att) / TILE;
        float odds = can ? fightOdds(e, *att) : 0.0f;
        if (!can || (odds < 0.5f && e.hp < e.maxHp * 0.75f && !nearHome)) {
            if (time > e.evadeUntil && !(e.order == O_ATTACK && can && d <= w.range)) { retreatFrom(e, att->pos, 4.5f); return true; }
        } else if (odds >= 0.8f && d > w.range + 0.15f && e.targetEnt != refOf(*att) && distToEntity(post, *att) < leash) {
            autoEngage(e, *att); return true;   // shot at from beyond its reach or its zone: go and get the shooter
        }
    }
    if (e.order == O_ATTACK || e.order == O_MOVE) return false;
    // badly hurt and out of the fight for a moment: over to a medic
    if (e.hp < e.maxHp * 0.35f && time - e.lastDamaged > 2.0f) {
        Entity* med = nullptr; float bd = 14.0f * TILE;
        forEachNear(e.pos, bd, [&](Entity& m) {
            if (!m.isUnit() || m.ut().role != UR_HEALER || m.owner < 0 || enemies(e.owner, m.owner)) return;
            float d = dist(m.pos, e.pos); if (d < bd) { bd = d; med = &m; }
        });
        if (med && bd > HEAL_RADIUS * TILE * 0.6f) { moveAside(e, med->pos + (e.pos - med->pos).norm() * (TILE * 1.5f), 8.0f); return true; }
    }
    // a friend in trouble nearby
    float R = e.zoneR > 0 ? e.zoneR + 6.0f * TILE : HELP_R * TILE;
    Entity* best = nullptr; float bd = 1e18f;
    forEachNear(post, R, [&](Entity& f) {
        if (&f == &e || f.kind == EK_RESOURCE || f.owner < 0 || enemies(e.owner, f.owner) || f.lastDamaged <= 0 || time - f.lastDamaged > 1.5f) return;
        Entity* a = get(f.attacker);
        if (!a || a->owner < 0 || !enemies(e.owner, a->owner) || a->fall > 0 || !canTarget(e, *a)) return;
        if (distToEntity(post, *a) > R + (w.range + 1.0f) * TILE || inFallout(a->pos)) return;
        float d = dist2(e.pos, a->pos); if (d < bd) { bd = d; best = a; }
    });
    if (best && fightOdds(e, *best) >= 0.6f) { autoEngage(e, *best); return true; }
    return false;
}

// ------------------------------------------------------------ unit update
void Sim::updateUnit(Entity& e) {
    const UnitType& ut = e.ut();
    if (ut.naval && e.fade < 1.0f) e.fade = std::min(1.0f, e.fade + SIM_DT / FLEET_FADE);   // a Fleet Call ship coming out of the sea haze at the border
    if (e.fall > 0) {   // parachute descent: glide from the plane to the landing spot, easing out as the canopy flares
        e.fall -= SIM_DT;
        if (e.fall > 0) {
            float u = 1.0f - e.fall / e.fallTime, k = 1.0f - (1.0f - u) * (1.0f - u);
            e.pos = e.dropFrom + (e.dropTo - e.dropFrom) * k;
            return;
        }
        e.fall = 0; e.pos = e.dropTo; e.guardPos = e.pos; e.path.clear(); e.pathIdx = 0; e.stuckTimer = 0; e.lastPos = e.pos;
        float r = std::max(10.0f, e.radius() * 1.6f);
        fx.push_back({FX_RING, e.pos, e.pos, 0, 0.5f, rgb(214, 196, 160), r * 1.6f});
        fx.push_back({FX_SMOKE, e.pos, e.pos, 0, 1.4f, rgb(150, 138, 118), r, Vec2(rng.f(-6, 6), -9)});
        if (e.dropGo) { e.dropGo = false; e.order = ut.weapon >= 0 ? O_ATTACKMOVE : O_MOVE; e.postOrder = O_IDLE; e.target = g_map.nearestFree(e.dropGoal, 6); requestPath(e, e.target); }
        return;
    }
    if (e.cooldown > 0) e.cooldown -= SIM_DT;
    if (ut.temp && e.lifeUntil >= 0 && time >= e.lifeUntil) { destroy(e, false); return; }   // a loan whose time is up is withdrawn wherever it is
    if (e.disabledUntil > time) return;
    if (e.repathTimer > 0) e.repathTimer -= SIM_DT;
    if (ut.cargoCap > 0 && ut.kind == UK_VEH) updateJeep(e);   // the crew of a jeep mends, climbs out and fires from the seats

    // medics: a healing field around them (units at the full rate, structures slowly), pulsed four times a second
    if (ut.role == UR_HEALER && (tick + e.gen) % 5 == 0) {
        float R = HEAL_RADIUS * TILE, dt = 5 * SIM_DT; int healed = 0;
        forEachNear(e.pos, R, [&](Entity& t) {
            if (&t == &e || t.owner != e.owner || t.kind == EK_RESOURCE || t.hp >= t.maxHp) return;
            if (t.isBuilding() && !t.constructed) return;
            if (dist(t.pos, e.pos) > R + t.radius()) return;
            t.hp = std::min(t.maxHp, t.hp + t.maxHp * HEAL_RATE * (t.isBuilding() ? 0.33f : 1.0f) * dt);
            healed++;
            if (((tick / 5) + (u32)(&t - &ents[0])) % 3 == 0) fx.push_back({FX_SPARK, t.pos + Vec2(rng.f(-6, 6), rng.f(-8, 2)), Vec2(), 0, 0.5f, rgb(110, 255, 150), 4});
        });
        if (tick % 20 == (e.gen % 20) || (healed && tick % 10 == 0)) fx.push_back({FX_RING, e.pos, e.pos, 0, 1.2f, rgb(110, 255, 160), R * 0.8f});
    }

    // aircraft settle onto the pad when idle there and climb away otherwise (drawn lower and without a detached shadow)
    if (e.isAir()) {
        bool parked = false;
        if (e.order == O_IDLE || e.order == O_REARM) { Entity* h = get(e.home); if (h && dist(e.pos, padSlot(*h, e)) < 10) parked = true; }
        bool down = parked || (ut.cargoCap > 0 && (e.unloading || e.order == O_UNLOAD) && e.pos.x == e.prevPos.x && e.pos.y == e.prevPos.y);   // a lifter sets down to let people out
        e.alt = clampf(e.alt + (down ? -1.4f : 1.4f) * SIM_DT, 0.0f, 1.0f);
        if (ut.cargoCap > 0 && e.unloading) updateCarrierUnload(e);
    }

    // bombers: the stick being released leaves the aircraft one bomb at a time (an Air Strike fighter lays its own stick: see strikeJetRun)
    if (e.bombsLeft > 0 && e.order != O_STRIKERUN) {
        e.bombTimer -= SIM_DT;
        if (e.bombTimer <= 0) {
            if (e.ammo > 0) { dropBomb(e, e.pos + Vec2(std::cos(e.angle), std::sin(e.angle)) * (airTopSpeed(e) * 0.42f)); e.ammo--; }
            else e.bombsLeft = 1;
            e.bombsLeft--; e.bombTimer = 0.11f;
        }
    }
    // burst continuation (a jet's volley comes in a tight stream: it covers a lot of ground between rounds)
    if (e.burstLeft > 0) {
        e.burstTimer -= SIM_DT;
        if (e.burstTimer <= 0) {
            Entity* t = get(e.targetEnt.valid() ? e.targetEnt : e.engaged);
            if (t && canTarget(e, *t) && distToEntity(e.pos, *t) <= (WEAPONS[ut.weapon].range + (e.isAir() ? 3.0f : 0.6f)) * TILE) fireWeapon(e, *t, WEAPONS[ut.weapon]);
            e.burstLeft--; e.burstTimer = ut.jet ? 0.07f : 0.14f;
        }
    }

    // Aircraft: out of ammo -> go rearm (remember the target). A Supersonic Bomber that has just dropped its bomb keeps the dash until
    // it is clear of its own fireball (e.slowAt), then goes home.
    if (e.isAir() && ut.ammo > 0 && e.ammo <= 0 && e.order != O_REARM && !ut.temp && !(ut.nukeBomb && time < e.slowAt)) { e.order = O_REARM; }

    // Ground units standing on a blocked tile (new structure) get nudged off
    if (!e.isAir() && (tick + (u32)(&e - &ents[0])) % 10 == 0 && (ut.naval ? !g_map.navTile(tileOf(e.pos.x), tileOf(e.pos.y)) : !walkable(e, tileOf(e.pos.x), tileOf(e.pos.y))))
        e.pos = ut.naval ? g_map.nearestWater(e.pos, 14) : g_map.nearestFree(e.pos, 14);

    // jammed: it has somewhere to go but hardly moved in the last second (two units head-on in a one-tile lane between structures,
    // a soldier standing in a gap): for a couple of seconds it slips past other units instead of shoving against them
    if (!e.isAir() && (tick + e.gen) % 20 == 0) {
        bool going = e.pathIdx < e.path.size();
        if (going && dist(e.pos, e.progPos) < ut.speed * 0.3f) e.squeezeUntil = time + 2.0f;
        e.progPos = e.pos;
    }
    // a unit that ducked out of a nuke's circle picks its work up again once the warhead has landed
    if (e.order == O_IDLE && e.resumeOrder != O_IDLE && time >= e.evadeUntil) {
        e.order = e.resumeOrder; e.postOrder = e.resumePost; e.target = e.resumeTarget; e.resumeOrder = O_IDLE; e.autoTask = false;
        e.path.clear(); e.pathIdx = 0; e.repathTimer = 0; e.actionTimer = 0;
        if (e.order == O_ATTACKMOVE && !e.isAir()) requestPath(e, e.target);
    }
    // left on its own, it looks after itself and its friends twice a second
    static const bool noAuto = getenv("ONEHOUR_NOAUTO") != nullptr;   // (diagnostics: plain units, as before they looked after themselves)
    if (!noAuto && !ut.temp && (tick + e.gen) % 10 == 0) autonomy(e);   // (a new order is carried out this very tick: an aircraft never skips a beat)
    const Vec2 before = e.pos;
    runOrder(e, ut);
    // a fixed-wing aircraft whose order changed this tick (a target lost, a spot reached, a new order) still flies on: it never hangs in the air
    if (e.isAir() && fixedWing(ut) && e.alive && e.pos.x == before.x && e.pos.y == before.y && e.airspeed > 0 && e.disabledUntil <= time)
        airSteer(e, e.angle, std::max(e.airspeed, airTopSpeed(e) * AIR_STALL));
    // a ship that has no order to steer by still has its way on: it glides to a halt
    if (ut.naval && e.alive && e.airspeed > 0.5f && e.pos.x == before.x && e.pos.y == before.y) moveShip(e, ut.speed);
    // a land vehicle that did not move this tick has lost its speed (it stood still, or it was blocked)
    else if (ut.kind == UK_VEH && !ut.naval && e.alive && e.airspeed > 0 && e.pos.x == before.x && e.pos.y == before.y) e.airspeed = std::max(0.0f, e.airspeed - ut.speed * 4.0f * SIM_DT);
}

// ------------------------------------------------------------ repairs
int Sim::repairCrewCap(const Entity& b) {
    int area = b.bt().w * b.bt().h;
    return area >= 12 ? 3 : (area >= 6 ? 2 : 1);   // a big structure has room for a few dozers, a turret for one
}

int Sim::repairCrew(const Entity& b, const Entity* except) const {
    int n = 0; Ref br = refOf(b);
    for (auto& u : ents)
        if (u.alive && &u != except && u.isUnit() && (u.owner == b.owner || b.district >= 0) && u.order == O_BUILD && u.targetEnt == br && u.ut().role == UR_DOZER) n++;   // (any army's dozers may help a town)
    return n;
}

// The structure a dozer should mend next. Every damaged structure is a job; the dozer weighs how far away it is against how badly it is hurt
// and how much it matters, and a job that already has its full crew is skipped (a second dozer only joins a big or badly hurt one), so a
// group of dozers spreads over the base instead of piling onto one building. Structures under fire right now and radiation are left alone.
Entity* Sim::repairJob(Entity& dz, float maxDist) {
    Entity* best = nullptr; float bs = 1e18f;
    for (auto& b : ents) {
        if (!b.alive || !b.isBuilding() || !b.constructed || !dozerMayWork(dz, b) || (b.hp >= b.maxHp * 0.995f && b.hatch >= b.hatchMax) || time - b.lastDamaged < 4.0f || inFallout(b.pos)) continue;
        float d = dist(b.pos, dz.pos);
        if (d > maxDist || (b.district >= 0 && d > DISTRICT_HELP_RANGE * TILE)) continue;   // (a town's blocks only by a dozer that is close by)
        float hurt = 1.0f - b.hp / b.maxHp;
        int crew = repairCrew(b, &dz), cap = repairCrewCap(b);
        if (hurt < 0.5f) cap = std::min(cap, 1 + (hurt > 0.25f ? 1 : 0));   // a scratch needs one dozer
        if (crew >= cap) continue;
        BuildRole r = b.bt().role;
        float weight = (r == BR_TURRET || r == BR_AATURRET) ? 1.4f : (r == BR_HQ || r == BR_POWER || r == BR_TECH) ? 1.25f : 1.0f;
        float score = d / TILE - hurt * 14.0f * weight + crew * 10.0f;   // each dozer already there counts as ten tiles further away
        if (score < bs) { bs = score; best = &b; }
    }
    return best;
}

// An unfinished site an idle dozer should help with. Construction always comes before mending: the dozer takes the nearest site of its army that
// is not under fire and has fewer than DOZER_SITE_CREW builders (one nobody is working on counts as much nearer), so a crew of idle dozers fans out over
// the open sites instead of all piling onto one.
Entity* Sim::siteJob(Entity& dz, float maxDist) {
    Entity* best = nullptr; float bs = 1e18f;
    for (auto& b : ents) {
        if (!b.alive || !b.isBuilding() || b.constructed || !dozerMayWork(dz, b) || time - b.lastDamaged < 4.0f || inFallout(b.pos)) continue;
        float d = dist(b.pos, dz.pos);
        if (d > maxDist || (b.district >= 0 && d > DISTRICT_HELP_RANGE * TILE)) continue;   // (a town's ruins only by a dozer that is close by)
        int crew = repairCrew(b, &dz);
        if (crew >= DOZER_SITE_CREW) continue;
        float score = d / TILE + crew * 12.0f - b.progress * 6.0f;   // (a nearly finished one is worth a short detour)
        if (score < bs) { bs = score; best = &b; }
    }
    return best;
}

bool Sim::siteOpen(const Entity& dz, Ref site) const {
    const Entity* b = get(site);
    return b && b->isBuilding() && !b->constructed && dozerMayWork(dz, *b);
}

void Sim::keepSite(Entity& dz) {
    if (!dz.isUnit() || dz.ut().role != UR_DOZER || dz.order != O_BUILD || !siteOpen(dz, dz.targetEnt)) return;
    auto& q = dz.siteQueue;
    if (std::find(q.begin(), q.end(), dz.targetEnt) != q.end()) return;
    q.insert(q.begin(), dz.targetEnt);   // the site it was just on is the first one it goes back to
    if ((int)q.size() > DOZER_SITE_QUEUE) q.pop_back();
}

// What an idle dozer does next: first the sites it was pulled away from (its own build orders, kept however it was moved), then on its own any open
// site of its army, then repairs; the last two wait while a manual order's hold lasts.
bool Sim::dozerFindWork(Entity& e) {
    auto& q = e.siteQueue;
    for (size_t i = 0; i < q.size();) {
        if (!siteOpen(e, q[i])) { q.erase(q.begin() + i); continue; }
        const Entity* b = get(q[i]);
        if (time - b->lastDamaged < 4.0f || inFallout(b->pos)) { i++; continue; }   // under fire: wait and keep it
        Ref site = q[i]; q.erase(q.begin() + i);
        e.order = O_BUILD; e.targetEnt = site; e.autoTask = false; e.repathTimer = 0; e.actionTimer = 0; e.path.clear();
        return true;
    }
    if (time < e.autoHoldUntil) return false;
    Entity* best = siteJob(e);
    if (!best) best = repairJob(e);
    if (!best) return false;
    e.order = O_BUILD; e.targetEnt = refOf(*best); e.autoTask = true; e.repathTimer = 0; e.actionTimer = 0; e.path.clear();
    return true;
}

void Sim::runOrder(Entity& e, const UnitType& ut) {
    switch (e.order) {
    case O_IDLE: {
        if (ut.role == UR_HEALER) {   // drift toward the nearest wounded friendly unit
            if ((tick + e.gen) % 10 == 0) {
                Entity* best = nullptr; float bd = 14.0f * TILE;
                forEachNear(e.pos, bd, [&](Entity& t) {
                    if (&t == &e || t.owner != e.owner || !t.isUnit() || t.ut().role == UR_HEALER || t.ut().naval || t.hp >= t.maxHp * 0.98f) return;   // (a medic cannot follow a ship out onto the lake)
                    float d = dist(t.pos, e.pos); if (d < bd) { bd = d; best = &t; }
                });
                if (best && bd > HEAL_RADIUS * TILE * 0.5f) { e.order = O_MOVE; e.target = g_map.nearestFree(best->pos, 6); e.targetEnt = NOREF; requestPath(e, e.target); }
            }
            break;
        }
        if (ut.role == UR_DOZER) {    // bulldozers go back to their sites, then help build and mend on their own when they have nothing to do
            if ((tick + e.gen) % 20 == 0) dozerFindWork(e);
            break;
        }
        if (ut.role == UR_SCOUT) {    // ghost/recon drones wait briefly when spawned or called, then autonomously hunt hidden enemies as a coordinated swarm
            updateScoutSwarm(e);
            break;
        }
        if (ut.weapon < 0) break;
        if (e.isAir()) {
            // between sorties a loaded aircraft waits where it was last sent (for a while) or at its airfield's rally point, circling
            // there (fixed-wing) or hovering (helicopters); otherwise it lands on its pad, where it is rearmed and patched up
            Entity* h = get(e.home);
            if (!h) { h = findAirfield(e); if (h) e.home = refOf(*h); }
            bool loaded = ut.ammo <= 0 || e.ammo >= ut.ammo;
            int slot = ((int)(&e - &ents[0])) % AIRFIELD_CAP;
            if (!h && e.hp < e.maxHp && (tick + e.gen) % 10 == 0) {   // nowhere to land and hurt: circle over a friendly medic
                Entity* med = nullptr; float bd = 16.0f * TILE;
                forEachNear(e.pos, bd, [&](Entity& m) { if (m.isUnit() && m.ut().role == UR_HEALER && m.owner >= 0 && !enemies(e.owner, m.owner)) { float dd = dist(m.pos, e.pos); if (dd < bd) { bd = dd; med = &m; } } });
                if (med) { e.loiter = med->pos; e.loiterUntil = time + 2.0f; }
            }
            if (!h && e.loiterUntil <= time) { if (e.loiterUntil < 0) e.loiter = e.pos; e.loiterUntil = time + 2.0f; }   // nowhere to land: circle where it is
            if (e.loiterUntil > time && (e.ammo > 0 || ut.ammo <= 0 || !h)) airLoiter(e, e.loiter, (!h && e.hp < e.maxHp) ? 0.0f : (2.6f + slot * 0.35f) * TILE, 0.55f);   // (hurt and homeless: the tightest circle, over a medic if there is one)
            else if (h && h->hasRally && loaded) airLoiter(e, h->rally + (fixedWing(ut) ? Vec2() : Vec2((slot - 1.5f) * 34, 0)), (2.6f + slot * 0.35f) * TILE, 0.55f);
            else if (h) {
                Vec2 pad = padSlot(*h, e);
                if (!fixedWing(ut)) pad = heliSpot(e, pad);   // (never onto a helipad another helicopter already sits on)
                float dp = dist(pad, e.pos);
                if (fixedWing(ut)) flyTo(e, pad, ut.speed * 0.6f); else heliHover(e, pad, ut.speed * 0.6f);
                if (fixedWing(ut) && dp < 14) { float tr = baseTurn(ut) * AIR_TURN_MAX * SIM_DT; e.angle = wrapAngle(e.angle + clampf(angDiff(e.angle, -1.5708f) * 0.2f, -tr, tr)); }   // parked planes face up the runway
                if (dp < 8) {
                    if (dp < 4) e.airspeed = 0;   // down on the pad
                    if (e.ammo < ut.ammo) { e.actionTimer += SIM_DT; if (e.actionTimer >= REARM_TIME) { e.actionTimer = 0; e.ammo++; } }
                    if (e.hp < e.maxHp) e.hp = std::min(e.maxHp, e.hp + e.maxHp * AIR_PAD_REPAIR * SIM_DT);
                }
            }
            if (e.ammo > 0 || ut.ammo <= 0) {
                Entity* t = nullptr;
                if ((tick + e.gen) % 6 == 0) t = acquireTarget(e, ut.sight);
                if (t) { e.order = O_ATTACK; e.postOrder = O_IDLE; e.targetEnt = refOf(*t); e.autoTask = true; }
            }
            break;
        }
        Entity* t = get(e.engaged);
        if (!t || !canTarget(e, *t) || distToEntity(e.pos, *t) > (WEAPONS[ut.weapon].range + 0.3f) * TILE) {
            t = nullptr; e.engaged = NOREF;
            if ((tick + e.gen) % 5 == 0) {
                t = acquireTarget(e, WEAPONS[ut.weapon].range + 0.3f);
                if (t) e.engaged = refOf(*t);
                else if (ut.role == UR_COMBAT) {
                    // guard: chase nearby threats a short way, then come back
                    t = acquireTarget(e, ut.sight);
                    if (t) { e.order = O_ATTACK; e.postOrder = O_GUARDPOS; e.targetEnt = refOf(*t); e.repathTimer = 0; e.autoTask = true; t = nullptr; }
                }
            }
        }
        if (t) tryFire(e, *t);
        break;
    }
    case O_GUARDPOS: {
        if (inFallout(e.guardPos) && !inFallout(e.pos)) { e.order = O_IDLE; e.guardPos = e.pos; e.path.clear(); break; }   // its post is poisoned: hold here
        if (!e.isAir() && e.pathIdx >= e.path.size()) requestPath(e, e.guardPos);
        moveAlong(e, ut.speed);
        if (e.pathIdx >= e.path.size() || dist(e.pos, e.guardPos) < 10) { e.order = O_IDLE; e.path.clear(); }
        if ((tick + e.gen) % 5 == 0) {
            Entity* t = acquireTarget(e, WEAPONS[ut.weapon].range + 0.3f);
            if (t) { e.order = O_ATTACK; e.postOrder = O_GUARDPOS; e.targetEnt = refOf(*t); e.repathTimer = 0; }
        }
        break;
    }
    case O_GUARDAREA: {
        if (e.zoneR <= 0) { e.order = O_IDLE; e.guardPos = e.pos; break; }
        const Weapon& w = WEAPONS[ut.weapon];
        if (e.isAir()) {
            // planes patrol the zone in a wide circle; a helicopter hovers still on its own slot of the zone, nose turned out toward
            // where trouble would come from; both strike whatever enters it
            if (fixedWing(ut)) airLoiter(e, e.zone, ut.jet ? std::max(e.zoneR * 0.55f, 4.0f * TILE) : std::max(e.zoneR * 0.45f, 3.0f * TILE), ut.jet ? 0.62f : 0.6f);
            else {
                Vec2 spot = heliSpot(e, e.guardPos);
                heliHover(e, spot, ut.speed);
                Vec2 out = spot - e.zone;
                if (dist(e.pos, spot) < 10.0f && out.len2() > 16.0f * 16.0f) { float tr = 1.6f * SIM_DT; e.angle = wrapAngle(e.angle + clampf(angDiff(e.angle, std::atan2(out.y, out.x)), -tr, tr)); }
            }
            if ((tick + e.gen) % 4 == 0) {
                Entity* t = acquireZoneTarget(e);
                if (t) { e.order = O_ATTACK; e.postOrder = O_GUARDAREA; e.targetEnt = refOf(*t); e.leashed = true; e.repathTimer = 0; }
            }
            break;
        }
        Entity* t = get(e.engaged);
        if (t && (!canTarget(e, *t) || (distToEntity(e.zone, *t) > e.zoneR + TILE * 1.5f && distToEntity(e.pos, *t) > (w.range + 0.3f) * TILE))) { t = nullptr; e.engaged = NOREF; }
        if (!t && (tick + e.gen) % 4 == 0) { t = acquireZoneTarget(e); if (t) e.engaged = refOf(*t); }
        if (t) {
            float d = distToEntity(e.pos, *t) / TILE;
            if (d <= w.range + 0.15f && d >= w.minRange) { e.path.clear(); tryFire(e, *t); break; }   // hold the slot and shoot
            if (distToEntity(e.zone, *t) <= e.zoneR + TILE * 1.5f) {                                   // inside the zone: go after it
                e.order = O_ATTACK; e.postOrder = O_GUARDAREA; e.targetEnt = refOf(*t); e.leashed = true; e.engaged = NOREF; e.repathTimer = 0;
                break;
            }
            e.engaged = NOREF;
        }
        // nothing to shoot: return to the slot (unless it just backed off from danger, or the slot lies in radiation)
        if (time < e.evadeUntil || (inFallout(e.guardPos) && !inFallout(e.pos))) { e.path.clear(); break; }
        if (dist(e.pos, e.guardPos) > 14) {
            if (e.pathIdx >= e.path.size()) requestPath(e, e.guardPos);
            moveAlong(e, ut.speed);
            e.stuckTimer += SIM_DT;
            if (e.stuckTimer > 1.5f) {
                if (dist(e.pos, e.lastPos) < ut.speed * 0.25f) {
                    if (dist(e.pos, e.guardPos) < TILE * 2.5f) { e.guardPos = e.pos; e.path.clear(); }   // slot is crowded: take this spot
                    else requestPath(e, e.guardPos);
                }
                e.stuckTimer = 0; e.lastPos = e.pos;
            }
        } else e.path.clear();
        break;
    }
    case O_MOVE: case O_ATTACKMOVE: {
        if (ut.role == UR_DOZER && !e.autoTask) e.autoHoldUntil = std::max(e.autoHoldUntil, time + DOZER_MANUAL_HOLD);   // the hold runs from its arrival
        if (ut.role == UR_SCOUT && !e.autoTask) e.autoHoldUntil = std::max(e.autoHoldUntil, time + SCOUT_MANUAL_HOLD);
        if (ut.role == UR_SCOUT && e.autoTask && (tick + e.gen) % 6 == 0) {
            bool found = false;
            forEachNear(e.pos, (ut.detect - 1.5f) * TILE, [&](Entity& t) { if (stealthOf(t) && enemies(e.owner, t.owner) && !t.carrier.valid()) found = true; });
            if (found) { e.order = O_IDLE; e.autoTask = false; updateScoutSwarm(e); break; }
        }
        if (e.isAir()) {
            // a fixed-wing craft has arrived once it passes within its own turn circle of the spot; it then waits there (circling)
            bool fw = fixedWing(ut);
            Vec2 d = (!fw && dist(e.pos, e.target) < 6.0f * TILE ? heliSpot(e, e.target) : e.target) - e.pos;   // (a group of helicopters stops side by side)
            float arrive = fw ? std::max(48.0f, 1.3f * e.airspeed / airTurnRate(e)) : 6.0f;
            if (d.len() < arrive) {
                e.order = O_IDLE; e.postOrder = O_IDLE;
                if (!e.autoTask) { e.loiter = e.target; e.loiterUntil = time + AIR_LOITER_TIME; e.guardPos = e.target; }
                e.autoTask = false;
                break;
            }
            if (fw) { const float top = airTopSpeed(e); airSteer(e, std::atan2(d.y, d.x), d.len() < 5 * TILE ? top * 0.7f : top); }
            else { e.pos += d.norm() * std::min(d.len(), ut.speed * SIM_DT); e.angle = std::atan2(d.y, d.x); }
        } else {
            if (e.path.empty()) requestPath(e, e.target);
            moveAlong(e, ut.speed);
            if (e.pathIdx >= e.path.size()) {
                if (e.autoTask && e.zoneR > 0 && ut.role == UR_COMBAT && e.resumeOrder == O_IDLE) e.order = O_GUARDAREA;   // backed off: guard the zone from here for now
                else { e.order = O_IDLE; if (!e.autoTask) e.guardPos = e.pos; }
                e.autoTask = false; e.path.clear(); break;
            }
            // stuck detection
            e.stuckTimer += SIM_DT;
            if (e.stuckTimer > 1.5f) {
                if (dist(e.pos, e.lastPos) < ut.speed * 0.25f) {
                    if (dist(e.pos, e.target) < TILE * 1.5f) { e.order = (e.autoTask && e.zoneR > 0 && ut.role == UR_COMBAT && e.resumeOrder == O_IDLE) ? O_GUARDAREA : O_IDLE; if (!e.autoTask) e.guardPos = e.pos; e.autoTask = false; e.path.clear(); break; }
                    requestPath(e, e.target);
                }
                e.stuckTimer = 0; e.lastPos = e.pos;
            }
        }
        if (e.order == O_ATTACKMOVE && ut.weapon >= 0 && (tick + e.gen) % 4 == 0) {
            Entity* t = acquireTarget(e, ut.sight);
            if (t) { e.postOrder = O_ATTACKMOVE; e.order = O_ATTACK; e.targetEnt = refOf(*t); e.repathTimer = 0; }
        } else if (e.order == O_MOVE && ut.weapon >= 0 && (tick + e.gen) % 4 == 0) {
            // fire while moving if something is in range (no chase)
            Entity* t = acquireTarget(e, WEAPONS[ut.weapon].range);
            if (t) tryFire(e, *t);
        }
        break;
    }
    case O_ATTACK: {
        Entity* t = get(e.targetEnt);
        if (!t || !canTarget(e, *t)) {
            e.targetEnt = NOREF;
            if (ut.nukeBomb && e.zoneR <= 0 && e.postOrder != O_ATTACKMOVE) { e.order = O_REARM; e.postOrder = O_IDLE; e.autoTask = false; e.leashed = false; break; }   // a nuclear bomber whose target is gone flies straight home, as a bomber does: it does not hang about over the battlefield
            if (e.isAir() && e.zoneR <= 0 && (e.ammo > 0 || ut.ammo <= 0) && e.postOrder != O_ATTACKMOVE) { e.loiter = e.pos; e.loiterUntil = std::max(e.loiterUntil, time + 8.0f); }   // look around for more before heading home
            if (e.postOrder == O_ATTACKMOVE) { e.order = O_ATTACKMOVE; requestPath(e, e.target); }
            else if (e.postOrder == O_GUARDPOS) { e.order = O_GUARDPOS; e.path.clear(); }
            else if (e.postOrder == O_GUARDAREA && e.zoneR > 0) { e.order = O_GUARDAREA; e.path.clear(); e.engaged = NOREF; }
            else { e.order = O_IDLE; if (!e.autoTask || e.isAir()) e.guardPos = e.pos; }
            e.postOrder = O_IDLE; e.leashed = false; e.autoTask = false;
            break;
        }
        const Weapon& w = WEAPONS[ut.weapon];
        float d = distToEntity(e.pos, *t) / TILE;
        // a zone guard does not chase targets out of its zone (it may still shoot what is already in range)
        if (e.leashed && e.zoneR > 0 && d > w.range + 0.15f && distToEntity(e.zone, *t) > e.zoneR + TILE * 1.5f) {
            e.order = O_GUARDAREA; e.targetEnt = NOREF; e.path.clear(); e.postOrder = O_IDLE; e.leashed = false; break;
        }
        // a fight it picked itself does not drag it far from its post (aircraft: from where they patrol or wait)
        if (e.autoTask && d > w.range + 0.15f) {
            Vec2 post = postOf(e);
            float far = e.isAir() ? (e.zoneR + AIR_LEASH * TILE) : (e.zoneR + AUTO_LEASH * TILE);
            if (distToEntity(post, *t) > far + (e.isAir() ? 0.0f : w.range * TILE) || (!e.isAir() && dist(e.pos, post) > far)) {
                e.targetEnt = NOREF; e.autoTask = false; e.leashed = false; e.path.clear();
                if (e.zoneR > 0) e.order = O_GUARDAREA;
                else if (e.isAir() || e.postOrder != O_GUARDPOS) e.order = O_IDLE;
                else e.order = O_GUARDPOS;
                e.postOrder = O_IDLE;
                break;
            }
        }
        if (e.isAir()) {
            if (ut.nukeBomb && t->isAir()) {   // a nuclear bomber does not dogfight: it drops the chase and goes back to what it was doing
                e.targetEnt = NOREF; e.engaged = NOREF; e.leashed = false; e.autoTask = false;
                e.order = e.zoneR > 0 ? O_GUARDAREA : O_IDLE; e.postOrder = O_IDLE;
                break;
            }
            if (e.ammo <= 0 && ut.ammo > 0 && !(ut.nukeBomb && time < e.slowAt)) { e.order = O_REARM; break; }
            if (e.autoTask && e.loiterUntil > time) e.loiterUntil = std::max(e.loiterUntil, time + 4.0f);   // a fight picked up while waiting somewhere keeps it there
            if (ut.bomber && !t->isAir()) { bomberAttack(e, *t); break; }   // (a bomber's ground attack is its bombing run, jet or not)
            if (ut.jet) { jetAttack(e, *t); break; }
            if (fixedWing(ut)) { airDogfight(e, *t); break; }
            Vec2 dv = t->pos - e.pos;
            if (d > w.range - 0.4f) { e.pos += dv.norm() * std::min(dv.len(), ut.speed * SIM_DT); e.angle = std::atan2(dv.y, dv.x); }
            else { e.angle = std::atan2(dv.y, dv.x); tryFire(e, *t); }
            break;
        }
        // MICRO_RETARGET: a target it picked itself is looked over twice a second - something it can hit from where it stands beats one it would
        // have to chase, and a much better one in reach (the gun firing on it) beats the wall it was plinking at
        if ((players[e.owner].micro & MICRO_RETARGET) && !e.forceTarget.valid() && (tick + e.gen) % 10 == 3
            && (e.autoTask || e.leashed || e.postOrder == O_ATTACKMOVE || e.postOrder == O_GUARDPOS || e.postOrder == O_GUARDAREA)) {
            float altScore = 0;
            Entity* alt = acquireTarget(e, w.range + 0.3f, -1, &altScore);
            if (alt && alt != t) {
                bool inReach = d <= w.range + 0.15f && d >= w.minRange;
                if (!inReach || altScore + 2.5f < targetScore(e, *t, w, -1, d)) {
                    e.targetEnt = refOf(*alt); e.path.clear(); e.pathIdx = 0; e.repathTimer = 0;
                    t = alt; d = distToEntity(e.pos, *t) / TILE;
                }
            }
        }
        if (d < w.minRange) {
            // Seek a reachable firing distance, including a lateral exit when a
            // wall or shoreline blocks straight reverse. Never follow the old
            // approach path further toward a target inside minimum range.
            if (e.repathTimer <= 0 || e.pathIdx >= e.path.size() || dist(e.path.back(), t->pos) < dist(e.pos, t->pos)) {
                Vec2 dest;
                if (escapeDestination(e, t->pos, ut.naval ? 2.5f : 2.0f, dest)) requestPath(e, dest);
                else { e.path.clear(); e.pathIdx = 0; e.repathTimer = PATH_INTERVAL; }
            }
            moveAlong(e, ut.speed);
        } else if (d <= w.range) {
            e.path.clear();
            tryFire(e, *t);
            if ((players[e.owner].micro & MICRO_KITE) && e.order == O_ATTACK) kiteStep(e, *t, w, d);
        } else {
            // approach; repath if the target moved away from the path end
            bool need = e.pathIdx >= e.path.size();
            if (!need && e.repathTimer <= 0 && dist(e.path.back(), t->pos) > TILE * 1.2f) need = true;
            if (need) requestPath(e, t->pos);
            moveAlong(e, ut.speed);
            // guard posts don't chase forever
            if (e.postOrder == O_GUARDPOS && dist(e.pos, e.guardPos) > (ut.sight + 4) * TILE) { e.order = O_GUARDPOS; e.targetEnt = NOREF; e.path.clear(); }
            if (e.leashed && e.zoneR > 0 && dist(e.pos, e.zone) > e.zoneR + (ut.sight + 2) * TILE) { e.order = O_GUARDAREA; e.targetEnt = NOREF; e.path.clear(); e.postOrder = O_IDLE; e.leashed = false; }
        }
        break;
    }
    case O_HARVEST: {
        // haulers are unarmed: when shot at, run home and come back later
        if (time - e.lastDamaged < 1.0f && e.lastDamaged > 0) { Entity* s = findSupplyBuilding(e); if (s && distToEntity(e.pos, *s) > 4 * TILE) { e.order = O_RETURN; e.path.clear(); e.actionTimer = 0; break; } }
        Entity* p = get(e.targetEnt);
        if (!p || p->kind != EK_RESOURCE || p->amount <= 0 || (e.zoneR > 0 && dist(p->pos, e.zone) > e.zoneR + TILE)) {
            Entity* np = e.zoneR > 0 ? findZonePile(e) : findPile(e, 60 * TILE);
            if (!np) {
                if (e.zoneR <= 0) { e.order = O_IDLE; e.targetEnt = NOREF; break; }
                // search the assigned area: drive to its middle (revealing piles as we arrive), give up after a short look around
                if (dist(e.pos, e.zone) > TILE * 1.5f) {
                    if (e.pathIdx >= e.path.size()) requestPath(e, g_map.nearestFree(e.zone, 6));
                    moveAlong(e, ut.speed);
                    e.stuckTimer += SIM_DT;
                    if (e.stuckTimer > 2.0f) { if (dist(e.pos, e.lastPos) < 8) { e.actionTimer = 99; } e.stuckTimer = 0; e.lastPos = e.pos; }
                } else e.actionTimer += SIM_DT;
                if (e.actionTimer > 2.5f) {
                    Player& hp = players[e.owner];
                    if (time - hp.lastMine > 6.0f) { hp.lastMine = time; emit(EV_MSG, e.owner, SND_NONE, e.pos, "No supplies left in the assigned area"); }
                    e.order = O_IDLE; e.zoneR = 0; e.targetEnt = NOREF; e.actionTimer = 0; e.path.clear();
                }
                break;
            }
            e.targetEnt = refOf(*np); e.lastPile = e.targetEnt; p = np; e.path.clear(); e.actionTimer = 0;
        }
        float d = distToEntity(e.pos, *p);
        if (d <= HARVEST_REACH) {
            e.path.clear();
            e.actionTimer += SIM_DT;
            e.angle = std::atan2(p->pos.y - e.pos.y, p->pos.x - e.pos.x);
            if (e.actionTimer >= HARVEST_TIME) {
                e.actionTimer = 0;
                int take = std::min(SUPPLY_PER_TRIP, p->amount);
                p->amount -= take; e.cargo = take;
                if (p->amount <= 0) { destroy(*p, false); emit(EV_SUPPLY_EMPTY, e.owner, SND_NONE, p->pos, "Supply pile depleted"); }
                e.order = O_RETURN; e.path.clear();
            }
        } else {
            if (e.pathIdx >= e.path.size()) requestPath(e, g_map.nearestFree(p->pos, 3));
            moveAlong(e, ut.speed);
            e.stuckTimer += SIM_DT;
            if (e.stuckTimer > 2.0f) { if (dist(e.pos, e.lastPos) < 8) requestPath(e, g_map.nearestFree(p->pos + Vec2(rng.f(-40, 40), rng.f(-40, 40)), 3)); e.stuckTimer = 0; e.lastPos = e.pos; }
        }
        break;
    }
    case O_RETURN: {
        Entity* s = findSupplyBuilding(e);
        if (!s) { e.path.clear(); break; }   // no depot standing: it waits with its load and delivers once a new one is built
        float d = distToEntity(e.pos, *s);
        if (d <= HARVEST_REACH) {
            e.path.clear();
            e.actionTimer += SIM_DT;
            if (e.actionTimer >= UNLOAD_TIME) {
                e.actionTimer = 0;
                { int got = (int)(e.cargo * players[e.owner].econMul()); players[e.owner].money += got; players[e.owner].harvested += got; }
                e.cargo = 0;
                emit(EV_SOUND, e.owner, SND_SUPPLY, e.pos);
                Entity* p = get(e.lastPile);
                if (!p || p->amount <= 0 || (e.zoneR > 0 && dist(p->pos, e.zone) > e.zoneR + TILE)) p = e.zoneR > 0 ? findZonePile(e) : findPile(e, 60 * TILE);
                if (p) { e.targetEnt = refOf(*p); e.lastPile = e.targetEnt; e.order = O_HARVEST; }
                else if (e.zoneR > 0) { e.targetEnt = NOREF; e.order = O_HARVEST; e.actionTimer = 0; }   // keep searching the area
                else e.order = O_IDLE;
            }
        } else {
            if (e.pathIdx >= e.path.size() || (e.repathTimer <= 0 && dist(e.path.back(), s->pos) > s->radius() + 60)) requestPath(e, g_map.nearestFree(s->pos + Vec2(0, s->radius() + 14), 4));
            moveAlong(e, ut.speed);
            e.stuckTimer += SIM_DT;
            if (e.stuckTimer > 2.0f) { if (dist(e.pos, e.lastPos) < 8) requestPath(e, g_map.nearestFree(s->pos + Vec2(rng.f(-60, 60), s->radius() + 20), 4)); e.stuckTimer = 0; e.lastPos = e.pos; }
        }
        break;
    }
    case O_BUILD: {
        if (time - e.lastDamaged < 1.0f && e.lastDamaged > 0 && e.hp < e.maxHp * 0.5f) {   // badly hurt: back to base (an unfinished site is kept for later)
            keepSite(e);
            e.order = O_MOVE; e.target = g_map.nearestFree(players[e.owner].basePos + Vec2(0, 60), 6); e.targetEnt = NOREF; e.autoTask = true; requestPath(e, e.target); break;
        }
        Entity* b = get(e.targetEnt);
        if (!b || !b->isBuilding() || !dozerMayWork(e, *b) || (b->constructed && b->hp >= b->maxHp && b->hatch >= b->hatchMax)) {
            e.order = O_IDLE; e.targetEnt = NOREF; e.guardPos = e.pos; e.autoTask = false;
            if (b && b->constructed && dozerMayWork(e, *b)) {   // a repair is done: move straight on to the next job (a site first, then mending close by)
                if (!e.siteQueue.empty() || time < e.autoHoldUntil) { dozerFindWork(e); break; }
                Entity* next = siteJob(e);
                if (!next) next = repairJob(e, 18.0f * TILE);
                if (next) { e.order = O_BUILD; e.targetEnt = refOf(*next); e.autoTask = true; e.repathTimer = 0; e.actionTimer = 0; e.path.clear(); }
            }
            break;
        }
        // construction comes before mending: a dozer that went mending on its own drops it for a site nobody is building
        if (b->constructed && e.autoTask && (tick + e.gen) % 30 == 0) {
            Entity* site = siteJob(e);
            if (site && repairCrew(*site, &e) == 0) { e.targetEnt = refOf(*site); e.repathTimer = 0; e.actionTimer = 0; e.path.clear(); break; }
        }
        if (!dozerMayWork(e, *b)) { e.order = O_IDLE; e.targetEnt = NOREF; e.guardPos = e.pos; e.autoTask = false; break; }   // the wrong kind of dozer for this job
        float d = distToEntity(e.pos, *b);
        // a dozer boxed in by neighbouring structures cannot always get right up to the wall: after a short wait it works from where it is
        bool inReach = d <= BUILD_REACH || (e.actionTimer > 1.5f && d <= BUILD_REACH + 3 * TILE);
        if (inReach) {
            e.path.clear();
            e.angle = std::atan2(b->pos.y - e.pos.y, b->pos.x - e.pos.x);
            const BuildType& bt = b->bt();
            if (!b->constructed) {
                b->progress += SIM_DT * players[e.owner].buildMul() / bt.buildTime;
                b->hp = std::max(b->hp, b->maxHp * (0.1f + 0.9f * clampf(b->progress, 0, 1)));
                if (tick % 6 == 0) fx.push_back({FX_SPARK, b->pos + Vec2(rng.f(-bt.w * 14.0f, bt.w * 14.0f), rng.f(-bt.h * 14.0f, bt.h * 14.0f)), Vec2(), 0, 0.2f, rgb(255, 230, 150), 3});
                if (b->progress >= 1.0f) { finishBuilding(*b); e.order = O_IDLE; e.guardPos = e.pos; }
            } else {
                b->hp = std::min(b->maxHp, b->hp + b->maxHp * SIM_DT / (bt.buildTime * 1.5f));
                if (b->hatch < b->hatchMax) b->hatch = std::min(b->hatchMax, b->hatch + b->hatchMax * SIM_DT / (bt.buildTime * 0.6f));   // a dozer patches the surface hatch too
                if (tick % 6 == 0) fx.push_back({FX_SPARK, b->pos + Vec2(rng.f(-bt.w * 14.0f, bt.w * 14.0f), rng.f(-bt.h * 14.0f, bt.h * 14.0f)), Vec2(), 0, 0.2f, rgb(255, 230, 150), 3});
            }
        } else {
            if (e.pathIdx >= e.path.size()) requestSidePath(e, *b);   // the nearest face it can reach, not a fixed doorstep round the back
            Vec2 before = e.pos;
            moveAlong(e, ut.speed);
            if (dist(e.pos, before) < 0.05f) e.actionTimer += SIM_DT; else e.actionTimer = 0;   // time spent unable to advance
            if (e.actionTimer > 14.0f) { e.order = O_IDLE; e.targetEnt = NOREF; e.guardPos = e.pos; e.path.clear(); e.actionTimer = 0; e.autoTask = false; break; }   // the structure cannot be reached: give up
            e.stuckTimer += SIM_DT;
            if (e.stuckTimer > 2.0f) { if (dist(e.pos, e.lastPos) < 8) requestPath(e, g_map.nearestFree(b->pos + Vec2(rng.f(-1, 1) * b->bt().w * TILE * 0.8f, rng.f(-1, 1) * b->bt().h * TILE * 0.8f), 6)); e.stuckTimer = 0; e.lastPos = e.pos; }
        }
        break;
    }
    case O_ENTER: {   // walk to a cargo chopper / bunker and climb aboard
        Entity* c = get(e.targetEnt);
        if (!c || !canBoard(e, *c)) { e.order = O_IDLE; e.targetEnt = NOREF; e.guardPos = e.pos; e.path.clear(); break; }
        const bool ship = c->isUnit() && c->ut().naval;   // a cargo ship lies off the beach: they board from the shore beside it
        float reach = c->isBuilding() ? 40.0f : c->radius() + (ship ? CARGOSHIP_REACH * TILE : 44.0f);   // (a crowd cannot all stand at the door: whoever is close enough climbs in)
        bool open = c->isBuilding() ? (c->constructed && !c->unloading) : ((c->ut().kind == UK_VEH ? dist(c->pos, c->prevPos) < 1.2f : c->order == O_IDLE) && !c->unloading && c->disabledUntil <= time);   // a lifter takes people while it hovers in place, a jeep while it is stopped
        if (distToEntity(e.pos, *c) <= reach && open) { board(e, *c); break; }
        Vec2 toMe = (e.pos - c->pos).norm();   // each soldier heads for the side of the door it is on, so a crowd spreads around the bunker instead of jamming on one face
        Vec2 goal = c->isBuilding() ? g_map.nearestFree(c->pos + toMe * (std::max(c->bt().w, c->bt().h) * TILE * 0.5f + 22.0f), 6) : g_map.nearestFree(c->pos + toMe * (c->radius() * 0.5f), ship ? 9 : 6);
        if (e.pathIdx >= e.path.size() || (e.repathTimer <= 0 && dist(e.path.back(), goal) > TILE * 0.8f)) requestPath(e, goal);
        Vec2 before = e.pos;
        moveAlong(e, ut.speed);
        if (dist(e.pos, before) < 0.05f) e.actionTimer += SIM_DT; else e.actionTimer = 0;
        if (e.actionTimer > 1.5f) {   // blocked (a crowd at the door, something in the way): try another spot around the carrier
            float a = rng.f(0, 6.2832f), rr = (c->isBuilding() ? std::max(c->bt().w, c->bt().h) * TILE * 0.5f : c->radius() * 0.5f) + 22.0f;
            requestPath(e, g_map.nearestFree(c->pos + Vec2(std::cos(a), std::sin(a)) * rr, 5));
            e.actionTimer = 0.4f;
        }
        e.stuckTimer += (e.actionTimer > 0 ? SIM_DT : -e.stuckTimer);
        if (e.stuckTimer > 20.0f) { e.order = O_IDLE; e.targetEnt = NOREF; e.guardPos = e.pos; e.path.clear(); e.actionTimer = 0; e.stuckTimer = 0; }   // cannot get there
        break;
    }
    case O_CAPTURE: {   // spy: walk up to an enemy structure and take it
        Entity* b = get(e.targetEnt);
        if (!b || !b->isBuilding() || (!b->constructed && b->district < 0) || !spyMayTake(e.owner, *b) || !visibleTo(*b, e.owner)) {
            e.order = O_IDLE; e.targetEnt = NOREF; e.guardPos = e.pos; e.path.clear(); e.actionTimer = 0; break;
        }
        float d = distToEntity(e.pos, *b);
        bool inReach = d <= SPY_REACH || (e.stuckTimer > 3.0f && d <= SPY_REACH + 2 * TILE);   // boxed in by neighbouring structures: works from where it is
        if (inReach) {
            e.path.clear();
            e.angle = e.turret = std::atan2(b->pos.y - e.pos.y, b->pos.x - e.pos.x);
            e.actionTimer += SIM_DT;
            if (tick % 5 == 0) fx.push_back({FX_SPARK, b->pos + Vec2(rng.f(-b->bt().w * 14.0f, b->bt().w * 14.0f), rng.f(-b->bt().h * 14.0f, b->bt().h * 14.0f)), Vec2(), 0, 0.25f, players[e.owner].faction == F_CYBER ? rgb(120, 236, 255) : rgb(255, 214, 120), 3});
            if (e.actionTimer >= captureTime(*b)) { Ref bref = refOf(*b); captureBuilding(*b, e); e.order = O_IDLE; e.targetEnt = NOREF; e.actionTimer = 0; e.guardPos = e.pos; (void)bref; }
        } else {
            e.actionTimer = 0;
            if (e.pathIdx >= e.path.size() || (e.repathTimer <= 0 && dist(e.path.back(), b->pos) > b->radius() + 60)) requestSidePath(e, *b);
            Vec2 before = e.pos;
            moveAlong(e, ut.speed);
            if (dist(e.pos, before) < 0.05f) e.stuckTimer += SIM_DT; else e.stuckTimer = 0;
            if (e.stuckTimer > 3.0f && e.pathIdx >= e.path.size()) requestPath(e, g_map.nearestFree(b->pos + Vec2(rng.f(-1, 1) * b->bt().w * TILE * 0.8f, rng.f(-1, 1) * b->bt().h * TILE * 0.8f), 6));
        }
        break;
    }
    case O_UNLOAD: {   // transport: fly over to the drop spot, set down and let everybody out
        if (ut.naval) {   // a cargo ship sails to the water off the chosen beach, then lands its load
            if (e.pathIdx >= e.path.size() && dist(e.pos, e.target) > 1.5f * TILE) requestPath(e, e.target);
            moveAlong(e, ut.speed);
            if (dist(e.pos, e.target) <= 1.5f * TILE || (e.pathIdx >= e.path.size() && e.airspeed < 2.0f)) {
                e.order = O_IDLE; e.path.clear();
                if (shipLanding(e)) { e.unloading = true; e.unloadTimer = 0.3f; }
                else emit(EV_MSG, e.owner, SND_CANT, e.pos, "No beach within reach of the ship: nobody landed");
            }
            break;
        }
        Vec2 d = e.target - e.pos;
        if (d.len() > 10.0f) { e.pos += d.norm() * std::min(d.len(), ut.speed * SIM_DT); e.angle = std::atan2(d.y, d.x); }
        else { e.unloading = true; e.unloadTimer = 0.4f; e.order = O_IDLE; }
        break;
    }
    case O_STRIKERUN: strikeJetRun(e); break;
    case O_NUKERUN: nukeBomberRun(e); break;
    case O_CARRIERJET: carrierJetRun(e); break;
    case O_REARM: {
        Entity* h = get(e.home);
        if (!h) { h = findAirfield(e); if (h) e.home = refOf(*h); }
        if (!h) { e.order = O_IDLE; break; }  // nowhere to land: loiter
        Vec2 pad = padSlot(*h, e);
        if (!fixedWing(ut)) pad = heliSpot(e, pad);
        Vec2 d = pad - e.pos;
        if (d.len() > 4) { if (fixedWing(ut)) flyTo(e, pad, ut.speed); else heliHover(e, pad, ut.speed); }
        else {
            e.airspeed = 0;   // down on the pad
            if (fixedWing(ut)) { float tr = baseTurn(ut) * AIR_TURN_MAX * SIM_DT; e.angle = wrapAngle(e.angle + clampf(angDiff(e.angle, -1.5708f) * 0.25f, -tr, tr)); }
            e.actionTimer += SIM_DT;
            if (e.actionTimer >= REARM_TIME) { e.actionTimer = 0; e.ammo = std::min(ut.ammo, e.ammo + 1); }
            if (e.hp < e.maxHp) e.hp = std::min(e.maxHp, e.hp + e.maxHp * AIR_PAD_REPAIR * SIM_DT);
            if (e.ammo >= ut.ammo && (e.hp >= e.maxHp * 0.6f || !e.autoTask)) {   // (back for repairs on its own: it waits to be patched up)
                e.autoTask = false;
                Entity* t = get(e.targetEnt);
                if (t && canTarget(e, *t)) e.order = O_ATTACK;
                else if (e.zoneR > 0) { e.order = O_GUARDAREA; e.targetEnt = NOREF; e.postOrder = O_IDLE; e.leashed = false; }
                else { e.order = O_IDLE; e.targetEnt = NOREF; }
            }
        }
        break;
    }
    }
}

// ------------------------------------------------------------ buildings
void Sim::finishBuilding(Entity& b) {
    b.constructed = true; b.progress = 1; b.hp = std::max(b.hp, b.maxHp);
    if (b.district >= 0) return;   // a town block raised again from its ruins: no fanfare (see updateDistrictBlock)
    if (b.bt().role == BR_BUNKER) { b.hatchMax = b.hatch = BUNKER_HATCH_HP; b.siloTimer = time + MINI_NUKE_ARM; }
    if (b.bt().role == BR_UNUKE) b.hatchMax = b.hatch = UNUKE_DOOR_HP;
    emit(EV_BUILD_DONE, b.owner, SND_BUILD_DONE, b.pos, b.bt().name);
    b.actionTimer = 0;
    if (b.bt().role == BR_NUKE) b.actionTimer = time + 60.0f;   // arming time for the first warhead
    if (b.bt().role == BR_UNUKE) b.actionTimer = time + UNUKE_ARM;
    if (b.bt().role == BR_CIVIC) b.dozerAt = time + CIVICS[civicKind(b.type)].dozerEvery;
    if (b.bt().role == BR_HQ && b.owner >= 0) {
        // a rebuilt Command Core becomes the home base again when no other one stands (camera Home key, AI base layout)
        bool other = false;
        for (auto& e : ents) if (e.alive && &e != &b && e.isBuilding() && e.owner == b.owner && e.constructed && e.bt().role == BR_HQ) other = true;
        if (!other) players[b.owner].basePos = b.pos;
    }
    updatePower();
}

void Sim::spawnFromQueue(Entity& b) {
    int type = b.queue.front();
    b.queue.erase(b.queue.begin());
    b.queueProgress = 0;
    const UnitType& ut = UNITS[type];
    Vec2 at = ut.kind == UK_AIR ? b.pos : (ut.naval ? navalExit(b) : unitExit(b));
    Ref bref = refOf(b);
    Vec2 rally = b.rally; bool hasRally = b.hasRally; int owner = b.owner;
    Ref r = spawnUnit(type, owner, at);          // may reallocate ents: b must not be touched afterwards
    Entity& u = ents[r.idx];
    if (ut.kind == UK_AIR) {
        u.home = bref; u.order = O_IDLE;
        if (ut.role == UR_SCOUT) u.autoHoldUntil = time + 2.5f;   // wait briefly for player to call it somewhere, then join autonomous swarm
        if (hasRally) cmdMove({r}, rally, false);
    }
    else if (ut.role == UR_HARVESTER) {
        Entity* p = findPile(u, 60 * TILE);
        if (p) cmdHarvest({r}, refOf(*p));
    } else if (hasRally) cmdMove({r}, rally, false);
    else if (ut.naval) {   // out from the slipway into open water on whichever sea the yard stands by
        Vec2 yardPos = get(bref) ? get(bref)->pos : u.pos, out = (u.pos - yardPos).norm();
        if (out.len2() < 0.5f) out = Vec2(1, 0);
        cmdMove({r}, u.pos + out * (4.0f * TILE) + Vec2(rng.f(-30, 30), rng.f(-30, 30)), false);
    }
    else cmdMove({r}, rally + Vec2(rng.f(-20, 20), rng.f(-10, 10)), false);
    emit(EV_UNIT_READY, owner, SND_UNIT_READY, u.pos, ut.name);
}

void Sim::updateBuilding(Entity& b) {
    if (b.cooldown > 0) b.cooldown -= SIM_DT;
    if (b.cooldown2 > 0) b.cooldown2 -= SIM_DT;
    if (b.dmgLoad > 0) b.dmgLoad *= 0.975f;   // (a time constant of two seconds)
    if (b.heat > 0) b.heat = std::max(0.0f, b.heat - HEAT_COOL * SIM_DT);   // an indestructible structure's fire dies down
    if (b.bt().role == BR_WAYMAKER) { updateWaymaker(b); return; }
    if (b.district >= 0) { updateDistrictBlock(b); return; }   // a block of a town round the lake: it mends and rebuilds itself, the town pays (see updateDistricts)
    if (!b.constructed) return;
    if (invulnerable(b)) { b.disabledUntil = -1; if (b.bt().role == BR_ISLANDNUKE) updateIslandNuke(b); return; }   // (burning or not, nothing switches it off; no upgrades, no guns)
    if (b.disabledUntil > time) { if (b.bt().role == BR_BUNKER && b.owner >= 0) bunkerMend(b); return; }   // (knocked offline, a bunker still mends itself)
    if (b.owner < 0) return;   // a neutral map landmark has nobody to pay, repair or command it until it is garrisoned
    const BuildType& bt = b.bt();
    if (bt.role == BR_STRONGHOLD) { updateStronghold(b); return; }
    Player& pl = players[b.owner];
    bool powered = !(pl.lowPower() && bt.power < 0);
    if (pl.upg[UPG_REPAIR] && b.hp < b.maxHp && !inFallout(b.pos) && bt.role != BR_BUNKER) {   // Self-Repair: full rate once out of combat, a trickle under fire (a bunker mends itself anyway: Repair Crews only speed that up, see bunkerMend)
        float rate = SELF_REPAIR_RATE * (time - b.lastDamaged >= SELF_REPAIR_DELAY ? 1.0f : SELF_REPAIR_HOT);
        b.hp = std::min(b.maxHp, b.hp + b.maxHp * rate * SIM_DT);
        if ((tick + b.gen) % 40 == 0) fx.push_back({FX_SPARK, b.pos + Vec2(rng.f(-bt.w * 12.0f, bt.w * 12.0f), rng.f(-bt.h * 12.0f, bt.h * 12.0f)), Vec2(), 0, 0.3f, pl.faction == F_CYBER ? rgb(120, 236, 255) : rgb(255, 214, 120), 3});
    }
    if (bt.role == BR_CIVIC) { updateCivic(b); return; }
    if (bt.role == BR_HEALANT) { updateHealAntenna(b); return; }   // (no guns, no queue: it mends what stands round it)   // the town has no roof guns, no queue: it pays and sends out dozers (may spawn a unit: b is done with)
    if (pl.upg[UPG_GUNS] && bt.role != BR_BUNKER) updateRoofGun(b, powered);
    if (bt.role == BR_BUNKER) { updateBunkerSelf(b); updateBunkerUpgrades(b); updateBunkerWard(b); updateBunker(b); }
    if (bt.role == BR_UNUKE) updateSilo(b);
    if (bt.role == BR_INCOME) {
        b.actionTimer += SIM_DT * (powered ? 1.0f : 0.5f);
        if (b.actionTimer >= INCOME_INTERVAL) {
            b.actionTimer -= INCOME_INTERVAL;
            int amt = (int)((pl.faction == F_CYBER ? INCOME_CYBER : INCOME_CLANKER) * pl.econMul());
            pl.money += amt; pl.mined += amt;
            fx.push_back({FX_SPARK, b.pos, b.pos, 0, 0.6f, rgb(255, 224, 90), 9});
        }
    }
    if (bt.weapon >= 0 && powered) {
        Entity* t = get(b.engaged);
        const Weapon& w = WEAPONS[bt.weapon];
        if (!t || !canTarget(b, *t) || distToEntity(b.pos, *t) > (w.range + 0.2f) * TILE) {
            t = nullptr; b.engaged = NOREF;
            if ((tick + b.gen) % 3 == 0) {
                Entity* f = get(b.forceTarget);   // a friendly target the human turned this turret on: taken up again whenever it is in reach
                if (f && canTarget(b, *f) && distToEntity(b.pos, *f) <= w.range * TILE && distToEntity(b.pos, *f) >= w.minRange * TILE) t = f;
                else t = acquireTarget(b, w.range);
                if (t) b.engaged = refOf(*t);
            }
        }
        if (t) tryFire(b, *t);
        // burst continuation for structures
        if (b.burstLeft > 0) {
            b.burstTimer -= SIM_DT;
            if (b.burstTimer <= 0) { Entity* tt = get(b.engaged); if (tt && canTarget(b, *tt)) fireWeapon(b, *tt, w); b.burstLeft--; b.burstTimer = 0.14f; }
        }
    }
    if (!b.queue.empty()) {
        const UnitType& ut = UNITS[b.queue.front()];
        bool blocked = false;
        if (bt.role == BR_AIRFIELD && !ut.heli && padsUsed(b) >= AIRFIELD_CAP) blocked = true;   // a plane waits for a free pad; helicopters never wait
        if (!blocked) {
            // Barracks infantry production is always three times the base speed, even on low power.
            float rate = (bt.role == BR_BARRACKS) ? BARRACKS_SPEED / ut.buildTime : (pl.lowPower() ? 0.5f : 1.0f) / ut.buildTime;
            rate *= pl.buildMul();
            b.queueProgress += SIM_DT * rate;
            if (b.queueProgress >= 1.0f) spawnFromQueue(b);
        }
    }
}

// ------------------------------------------------------------ stealth, transports, bunkers and spies
int Sim::stealthOf(const Entity& e) {
    if (e.isUnit()) return UNITS[e.type].stealth;
    return ST_NONE;   // structures are never stealthy: underground bunkers are plainly visible on the surface
}

// Stealthy things (snipers and spy drones) are invisible to every army that has no detector close enough: they cannot be
// seen, selected, targeted or shot, and the computer commanders do not know they are there. The owner's team always sees them.
bool Sim::visibleTo(const Entity& e, int player) const {
    if (player < 0 || player >= numPlayers) return true;
    if (!stealthOf(e)) return true;
    if (e.owner < 0 || e.owner == player || players[e.owner].team == players[player].team) return true;
    return e.detUntil[player] > time;
}

// Light defence turrets (the Laser Turret and the Gun Nest, not the Patriot / Rocket batteries) watch the ground round them for infiltrators
static const float TURRET_INFIL_DETECT = 7.0f;

void Sim::updateDetection() {
    auto sweep = [&](const Entity& d, Vec2 at, float range, int mask, bool sensor) {
        forEachNear(at, range * TILE, [&](Entity& t) {
            int cls = stealthOf(t);
            if (!cls || !(mask & (1 << (cls - 1))) || !enemies(d.owner, t.owner) || (sensor && (!t.isUnit() || dist(t.pos, at) > range * TILE))) return;   // (a sensor finds soldiers, not another bunker)
            float linger = cls == ST_DRONE ? 1.5f : (cls == ST_INFIL ? 2.0f : 3.0f);   // a sniper, drone or infiltrator is lost again once no detector is close
            for (int q = 0; q < numPlayers; q++)
                if (q == d.owner || players[q].team == players[d.owner].team) t.detUntil[q] = std::max(t.detUntil[q], time + linger);
        });
    };
    for (size_t i = 0; i < ents.size(); i++) {
        Entity& d = ents[i];
        if (!d.alive || d.owner < 0 || d.disabledUntil > time) continue;
        if (d.isBuilding()) {
            if (!d.constructed) continue;
            BuildRole role = d.bt().role;
            if (role == BR_BUNKER) sweep(d, d.pos, BUNKER_SENSOR, DET_SPOTTER, true);   // a bunker's seismic sensors feel hidden soldiers (snipers) close by, never a drone in the air
            if (role == BR_TURRET) sweep(d, d.pos, TURRET_INFIL_DETECT, DET_INFIL, false);
            // a garrison keeps watch from inside: a sniper (or an infiltrator) in a bunker or a stronghold spots infiltrators creeping up on it
            if (!d.passengers.empty() && (role == BR_BUNKER || role == BR_STRONGHOLD)) {
                float best = 0; int mask = 0;
                for (Ref r : d.passengers) { const Entity* u = get(r); if (u && u->ut().detect > best) { best = u->ut().detect; mask |= u->ut().detectMask; } }
                if (best > 0) sweep(d, d.pos, best + d.radius() / TILE, mask, false);
            }
            continue;
        }
        if (d.carrier.valid() || d.fall > 0) continue;
        float range = d.ut().detect;
        if (range <= 0) continue;
        sweep(d, d.pos, range, d.ut().detectMask, false);
    }
}

int Sim::cargoUsed(const Entity& c) const {
    int n = 0;
    for (Ref r : c.passengers) { const Entity* u = get(r); if (u) n += cargoSize(u->ut()); }
    return n;
}
int Sim::garrisonInfantry(const Entity& c) const {
    int n = 0;
    for (Ref r : c.passengers) { const Entity* u = get(r); if (u && u->ut().kind == UK_INF) n++; }
    return n;
}
int Sim::garrisonDozers(const Entity& c) const {
    int n = 0;
    for (Ref r : c.passengers) { const Entity* u = get(r); if (u && u->ut().role == UR_DOZER) n++; }
    return n;
}
int Sim::garrisonCap(const Entity& b) const {
    if (!b.isBuilding()) return 0;
    if (b.bt().role == BR_BUNKER) return BUNKER_INF_CAP;
    if (b.bt().role == BR_STRONGHOLD) return b.bt().garrison;
    return 0;
}

bool Sim::canBoard(const Entity& u, const Entity& c) const {
    if (!u.alive || !c.alive || !u.isUnit() || u.isAir() || u.carrier.valid() || c.carrier.valid() || u.fall > 0 || c.fall > 0) return false;
    if (u.owner < 0) return false;
    if (c.isBuilding()) {
        if (c.bt().role == BR_STRONGHOLD) {   // a neutral landmark is open to any army; a claimed one to its owner and allies
            if (!c.constructed || u.ut().kind != UK_INF) return false;
            if (c.owner >= 0 && c.owner != u.owner && players[c.owner].team != players[u.owner].team) return false;
            return garrisonInfantry(c) < garrisonCap(c);
        }
        if (u.owner != c.owner) return false;
        if (c.bt().role != BR_BUNKER || !c.constructed) return false;
        if (u.ut().kind == UK_INF) return garrisonInfantry(c) < BUNKER_INF_CAP;
        if (u.ut().role == UR_DOZER) return !civicDozerType(u.type) && garrisonDozers(c) < BUNKER_DOZER_CAP;   // (a civilian dozer cannot patch a hatch)
        return false;
    }
    if (u.owner != c.owner) return false;
    if (!c.isUnit() || c.ut().cargoCap <= 0 || u.ut().temp || u.ut().naval) return false;
    if (c.ut().kind == UK_VEH && !c.ut().naval && u.ut().kind != UK_INF) return false;   // a jeep seats soldiers, nothing else (a cargo ship takes vehicles too)
    if (!u.passengers.empty()) return false;                            // a loaded jeep is not itself lifted (no loads inside loads)
    int sz = cargoSize(u.ut());
    return sz > 0 && cargoUsed(c) + sz <= c.ut().cargoCap;
}

void Sim::cmdEnter(const std::vector<Ref>& sel, Ref carrier) {
    Entity* c = get(carrier);
    if (!c) return;
    for (auto r : sel) {
        Entity* e = get(r);
        if (!e || e == c || !canBoard(*e, *c)) continue;
        e->zoneR = 0; e->leashed = false; keepSite(*e); takeOrder(*e); e->forceTarget = NOREF;
        e->order = O_ENTER; e->postOrder = O_IDLE; e->targetEnt = carrier; e->engaged = NOREF; e->path.clear(); e->pathIdx = 0; e->repathTimer = 0; e->actionTimer = 0;
    }
}

void Sim::board(Entity& u, Entity& c, bool quiet) {
    Ref uref = refOf(u);
    bool claim = c.isBuilding() && c.bt().role == BR_STRONGHOLD && c.owner < 0;   // the first infantry inside takes the empty landmark for their army
    if (claim) {
        c.owner = u.owner;
        c.attacker = NOREF; c.lastDamaged = -100; c.detUntil[MAX_PLAYERS - 1] = 0;
        char msg[128];
        snprintf(msg, sizeof msg, "Garrisoned the neutral %s", c.bt().name);
        if (c.owner >= 0) emit(EV_MSG, c.owner, SND_NONE, c.pos, msg);
        for (int q = 0; q < numPlayers; q++) if (q != c.owner && c.owner >= 0 && players[q].team == players[c.owner].team) emit(EV_MSG, q, SND_NONE, c.pos, msg);
        updateVision();
    }
    u.carrier = refOf(c);
    c.passengers.push_back(uref);
    u.order = O_IDLE; u.postOrder = O_IDLE; u.targetEnt = NOREF; u.engaged = NOREF; u.path.clear(); u.pathIdx = 0; u.actionTimer = 0; u.cooldown = 0;
    u.pos = c.pos; u.prevPos = u.pos;
    if (c.isBuilding()) fx.push_back({FX_SMOKE, c.pos, c.pos, 0, 0.8f, rgb(120, 110, 96), 7, Vec2(0, -8)});
    if (c.owner >= 0 && !quiet) emit(EV_SOUND, c.owner, SND_CLICK, c.pos);
}

// Where soldier number 'slot' stands when it climbs out: a ring around the carrier, on free ground
void Sim::ejectOne(Entity& c, Entity& u, int slot) {
    float a = slot * 2.39996f + 0.7f, rr = c.radius() + 16.0f + 7.0f * (slot / 8);
    Vec2 at = c.pos + Vec2(std::cos(a), std::sin(a)) * rr;
    u.carrier = NOREF;
    u.pos = g_map.nearestFree(at, 10); u.prevPos = u.pos; u.guardPos = u.pos;
    u.order = O_IDLE; u.postOrder = O_IDLE; u.targetEnt = NOREF; u.engaged = NOREF; u.path.clear(); u.pathIdx = 0; u.angle = a; u.turret = a;
    u.repathTimer = 0; u.autoTask = false; u.cooldown = 0.2f;
}

// Everyone out onto the ground, exposed. 'lost' = the carrier was destroyed: aboard an aircraft that is still in the air that is the end of them.
void Sim::ejectAll(Entity& c, bool lost) {
    std::vector<Ref> list = c.passengers;
    c.passengers.clear();
    bool crash = lost && ((c.isAir() && c.alt > 0.4f) || (c.isUnit() && c.ut().naval));   // (a ship that goes down takes everyone aboard with it)
    int slot = 0;
    for (Ref r : list) {
        Entity* u = get(r);
        if (!u || u->carrier != refOf(c)) continue;
        if (crash) { u->pos = c.pos; u->carrier = NOREF; destroy(*u, true); continue; }
        if (lost && c.isUnit() && !c.isAir() && rng.f() < JEEP_CASUALTIES) { u->pos = c.pos; u->carrier = NOREF; destroy(*u, true); continue; }   // killed in the wreck of their jeep
        ejectOne(c, *u, slot++);
        if (lost && c.isUnit() && !c.isAir()) u->hp = std::max(1.0f, u->hp * 0.65f);   // the others climb out hurt
    }
}

void Sim::cmdUnload(Ref carrier, bool atSpot, Vec2 spot) {
    Entity* c = get(carrier);
    if (!c || c->passengers.empty()) return;
    if (c->isBuilding()) { if ((c->bt().role == BR_BUNKER || c->bt().role == BR_STRONGHOLD) && c->constructed) { c->unloading = true; c->unloadTimer = 0; } return; }
    if (!c->isUnit() || c->ut().cargoCap <= 0) return;
    if (c->ut().naval) {   // a cargo ship: here (onto the nearest shore) or at a spot (it sails to the water nearest it first)
        c->postOrder = O_IDLE; c->targetEnt = NOREF; c->engaged = NOREF;
        if (atSpot) {
            Vec2 at = g_map.nearestWater(Vec2(clampf(spot.x, TILE, WORLD_W - TILE), clampf(spot.y, TILE, WORLD_H - TILE)), 14, true, g_map.compAt(c->pos));
            c->unloading = false; c->order = O_UNLOAD; c->target = at; c->unloadSpot = spot; requestPath(*c, at);
        } else if (shipLanding(*c)) { c->order = O_IDLE; c->path.clear(); c->unloading = true; c->unloadTimer = 0.3f; }
        else emit(EV_MSG, c->owner, SND_CANT, c->pos, "Too far from the shore to land anyone: take the ship to a beach");
        return;
    }
    if (c->ut().kind == UK_VEH) {   // a jeep stops and the crew climbs out onto the ground around it
        c->unloading = true; c->unloadTimer = 0.1f;
        c->order = O_IDLE; c->postOrder = O_IDLE; c->path.clear(); c->pathIdx = 0; c->targetEnt = NOREF;
        return;
    }
    if (atSpot) {
        c->unloading = false;
        c->order = O_UNLOAD; c->target = Vec2(clampf(spot.x, TILE, WORLD_W - TILE), clampf(spot.y, TILE, WORLD_H - TILE)); c->targetEnt = NOREF; c->loiterUntil = -1; c->zoneR = 0; c->engaged = NOREF;
    } else { c->unloading = true; c->unloadTimer = 0.5f; if (c->order == O_UNLOAD) c->order = O_IDLE; }
}

void Sim::updateCarrierUnload(Entity& c) {
    c.unloadTimer -= SIM_DT;
    if (c.unloadTimer > 0) return;
    if (c.isAir() && c.alt > 0.45f) return;   // a lifter lets people out only once it has set down
    // purge the dead, then let the next one out (vehicles first would block the doorway: soldiers go first)
    c.passengers.erase(std::remove_if(c.passengers.begin(), c.passengers.end(), [&](Ref r) { Entity* u = get(r); return !u || u->carrier != refOf(c); }), c.passengers.end());
    if (c.passengers.empty()) { c.unloading = false; return; }
    size_t pick = 0;
    for (size_t i = 0; i < c.passengers.size(); i++) { Entity* u = get(c.passengers[i]); if (u && u->ut().kind == UK_INF) { pick = i; break; } }
    Entity* u = get(c.passengers[pick]);
    c.passengers.erase(c.passengers.begin() + pick);
    if (c.isUnit() && c.ut().naval) {   // down the ramp onto the beach: the land nearest the ship, toward where the player pointed
        Vec2 land;
        if (!shipLanding(c, &land)) { c.passengers.insert(c.passengers.begin() + pick, refOf(*u)); c.unloading = false; return; }
        u->carrier = NOREF;
        float a = rng.f(0, 6.2832f);
        u->pos = g_map.nearestFree(land + Vec2(std::cos(a), std::sin(a)) * rng.f(0, 1.6f * TILE), 6); u->prevPos = u->pos; u->guardPos = u->pos;
        u->order = O_IDLE; u->postOrder = O_IDLE; u->targetEnt = NOREF; u->engaged = NOREF; u->path.clear(); u->pathIdx = 0; u->angle = a; u->turret = a; u->repathTimer = 0; u->autoTask = false; u->cooldown = 0.2f;
        c.unloadTimer = u->ut().kind == UK_INF ? 0.12f : 0.45f;
        if (c.passengers.empty()) c.unloading = false;
        return;
    }
    if (c.isAir()) {   // out of the rear ramp
        float a = c.angle + 3.14159f + rng.f(-0.5f, 0.5f);
        u->carrier = NOREF;
        u->pos = g_map.nearestFree(c.pos + Vec2(std::cos(a), std::sin(a)) * (c.radius() + 14.0f), 12); u->prevPos = u->pos; u->guardPos = u->pos;
        u->order = O_IDLE; u->targetEnt = NOREF; u->engaged = NOREF; u->path.clear(); u->pathIdx = 0; u->angle = a;
        c.unloadTimer = u->ut().kind == UK_INF ? 0.15f : 0.7f;
    } else {
        ejectOne(c, *u, (int)c.passengers.size());
        c.unloadTimer = BUNKER_UNLOAD_GAP;
    }
    if (c.hasRally && !c.isAir()) cmdMove({refOf(*u)}, c.rally, false);
    if (c.passengers.empty()) c.unloading = false;
}

void Sim::hatchBlown(Entity& b) {
    fx.push_back({FX_EXPLODE, b.pos, b.pos, 0, 0.8f, rgb(255, 190, 90), 34});
    fx.push_back({FX_SMOKE, b.pos, b.pos, 0, 3.0f, rgb(52, 48, 44), 20, Vec2(rng.f(-8, 8), -26)});
    for (int k = 0; k < 8; k++) fx.push_back({FX_DEBRIS, b.pos, b.pos, 0, rng.f(0.6f, 1.2f), rgb(120, 122, 126), rng.f(2, 5), Vec2(rng.f(-150, 150), rng.f(-190, -50))});
    emit(EV_SOUND, -1, SND_EXPLODE_L, b.pos);
    if (b.owner >= 0) emit(EV_MSG, b.owner, SND_NONE, b.pos, b.bt().role == BR_UNUKE ? "Underground Nuke blast doors destroyed: only troops can hurt the silo now, and it refits them when left alone" : "Bunker hatch destroyed: the garrison cannot fire (a dozer inside or beside it can patch it)");
}

// The spots on the apron (BUNKER_* in data.h are px from the centre of a 3x3 bunker)
static float bunkerK(const Entity& b) { return b.bt().w * TILE / 96.0f; }
Vec2 Sim::bunkerGunPos(const Entity& b, int i) const {
    return b.pos + Vec2((i == 0 ? -BUNKER_GUN_X : BUNKER_GUN_X) * bunkerK(b), 0.0f);   // on the plates either side of the hatch
}

Vec2 Sim::bunkerOverdrivePos(const Entity& b, int i) const {
    return b.pos + Vec2(i == 0 ? -BUNKER_OD_X : BUNKER_OD_X, i == 0 ? -BUNKER_OD_Y : BUNKER_OD_Y) * bunkerK(b);
}

Vec2 Sim::bunkerLaserPos(const Entity& b, int i) const {
    return b.pos + Vec2(i == 0 ? BUNKER_LZ_X : -BUNKER_LZ_X, i == 0 ? -BUNKER_LZ_Y : BUNKER_LZ_Y) * bunkerK(b);
}

bool Sim::bunkerSurrounded(const Entity& b, int* countOut) {
    int cnt = 0;
    forEachNear(b.pos, BUNKER_SURROUND_RADIUS * TILE, [&](Entity& t) {
        if (!t.isUnit() || !enemies(b.owner, t.owner) || t.carrier.valid() || t.fall > 0 || t.weapon() < 0) return;
        if (distToEntity(b.pos, t) <= BUNKER_SURROUND_RADIUS * TILE) cnt++;
    });
    if (countOut) *countOut = cnt;
    return cnt >= BUNKER_SURROUND_MIN;
}

void Sim::updateBunkerAntiNuke(Entity& b, bool surrounded) {
    if (time < b.antiNukeCd || b.disabledUntil > time) return;
    bool cy = b.bt().faction == F_CYBER;
    const bool lz = bunkerHas(b, BU_LASERS);   // a laser battery: wider cover, near-certain against an ordinary warhead, and it tries the big ones too (their resist applies)
    const int self = (int)(&b - &ents[0]);
    for (auto& n : nukes) {
        if (n.broken || n.killT >= 0 || (!lz && (n.deep || n.super)) || !enemies(n.owner, b.owner)) continue;   // (without lasers an Underground Nuke's or a Super Nuke's warhead is too much for it)
        float cover = (lz ? BUNKER_LASER_COVER : BUNKER_ANTINUKE_COVER) * TILE + nukeRadius(n);
        if (dist(n.pos, b.pos) > cover) continue;
        float left = flightOf(n) - n.t;
        const float flight = cy || lz ? 0.28f : 0.45f, gap = 0.22f;
        if (left > BUNKER_ANTINUKE_WINDOW || left < flight + 0.25f) continue;
        if (std::find(n.engagedBy.begin(), n.engagedBy.end(), self) != n.engagedBy.end()) continue;
        n.engagedBy.push_back(self);
        int shots = surrounded ? 4 : 3;
        while (shots > 1 && (shots - 1) * gap + flight > left - 0.2f) shots--;
        float chance = (lz ? BUNKER_LASER_NUKE : (surrounded ? 1.0f : BUNKER_ANTINUKE_CHANCE)) * nukeResist(n);
        int winner = rng.f() < chance ? rng.range(0, shots - 1) : -1;
        b.antiNukeCd = time + (lz ? 1.0f : (surrounded ? 1.2f : 3.5f));
        for (int k = 0; k < shots; k++) {
            Vec2 muzzle = lz ? bunkerLaserPos(b, k & 1) : bunkerOverdrivePos(b, k & 1);
            Interceptor it;
            it.from = muzzle; it.owner = b.owner; it.nukeId = n.id; it.laser = lz || cy || (k & 1) == 0; it.hit = (k == winner);
            it.launchT = time + k * gap; it.dur = flight;
            float tMeet = n.t + k * gap + flight;
            it.aim = nukeAirPos(n, tMeet);
            if (!it.hit) { Vec2 off(rng.f(-1, 1), rng.f(-1, 1)); it.aim += off.norm() * rng.f(36.0f, 84.0f); }
            interceptors.push_back(it);
            if (it.hit) n.killT = tMeet;
            (lz ? b.lzAng : b.overdriveAng)[k & 1] = std::atan2(it.aim.y - muzzle.y, it.aim.x - muzzle.x);
        }
        if (lz) b.lzFired = time + (shots - 1) * gap;
        emit(EV_SOUND, -1, cy ? SND_LASER_HEAVY : SND_MG, b.pos);
        break;
    }
}

// Autonomous coordinated swarm intelligence for Ghost Drones and Recon Drones (human and AI alike).
// When spawned or called to a spot by the player, the drone waits briefly at that post (autoHoldUntil);
// once free, all friendly spy drones act as an intelligent swarm that divides sectors across the map,
// hunts and locks onto enemy hidden elements (snipers, enemy drones) and covers frontlines.
void Sim::updateScoutSwarm(Entity& e) {
    const UnitType& ut = e.ut();
    if (time < e.autoHoldUntil) {
        heliHover(e, heliSpot(e, e.guardPos), ut.speed * 0.6f);
        return;
    }
    // If already revealing a hidden enemy element right here and no other friendly scout is closer, hold/orbit to keep it pinned!
    Entity* pinned = nullptr; float pinDist = (ut.detect - 1.0f) * TILE;
    forEachNear(e.pos, pinDist, [&](Entity& t) {
        if (!stealthOf(t) || !enemies(e.owner, t.owner) || t.carrier.valid()) return;
        float d = dist(e.pos, t.pos);
        if (d < pinDist) { pinDist = d; pinned = &t; }
    });
    if (pinned) {
        bool closerFriend = false;
        for (auto& o : ents) {
            if (!o.alive || &o == &e || !o.isUnit() || o.owner != e.owner || o.ut().role != UR_SCOUT) continue;
            if (dist(o.pos, pinned->pos) + 12.0f < dist(e.pos, pinned->pos)) { closerFriend = true; break; }
        }
        if (!closerFriend) {
            float orbitA = time * 0.9f + (e.gen % 7) * 0.9f;
            Vec2 hold = pinned->pos + Vec2(std::cos(orbitA), std::sin(orbitA)) * (2.5f * TILE);
            heliHover(e, heliSpot(e, hold), ut.speed * 0.7f);
            return;
        }
    }
    if ((tick + e.gen) % 10 != 0) {
        heliHover(e, heliSpot(e, e.pos), ut.speed * 0.5f);
        return;
    }
    // Gather the friendly scout swarm to coordinate sectors and avoid overlapping targets
    std::vector<const Entity*> swarm;
    int myIdx = 0;
    for (auto& o : ents) {
        if (!o.alive || !o.isUnit() || o.owner != e.owner || o.ut().role != UR_SCOUT || o.fall > 0) continue;
        if (&o == &e) myIdx = (int)swarm.size();
        swarm.push_back(&o);
    }
    int swarmN = std::max(1, (int)swarm.size());
    Vec2 base = e.owner >= 0 ? players[e.owner].basePos : Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f);
    Vec2 bestSpot = e.pos; float bestScore = -1e18f;
    auto evalCandidate = [&](Vec2 cand, float basePriority) {
        cand = Vec2(clampf(cand.x, 3 * TILE, WORLD_W - 3 * TILE), clampf(cand.y, 3 * TILE, WORLD_H - 3 * TILE));
        float sc = basePriority - dist(e.pos, cand) / TILE * 0.85f;
        // Swarm repulsion: penalize spots near other friendly drones or their current destinations
        for (const Entity* o : swarm) {
            if (o == &e) continue;
            Vec2 oGoal = (o->order == O_MOVE || o->order == O_ATTACKMOVE) ? o->target : o->pos;
            float dGoal = dist(cand, oGoal) / TILE, dPos = dist(cand, o->pos) / TILE;
            if (dGoal < SCOUT_SWARM_SEP) sc -= (SCOUT_SWARM_SEP - dGoal) * 18.0f;
            if (dPos < SCOUT_SWARM_SEP * 0.8f) sc -= (SCOUT_SWARM_SEP * 0.8f - dPos) * 12.0f;
        }
        if (inFallout(cand)) sc -= 120.0f;
        if (sc > bestScore) { bestScore = sc; bestSpot = cand; }
    };
    // 1. High-intelligence priority: allied units under fire from unseen attackers or suspected hidden enemy positions
    for (auto& u : ents) {
        if (!u.alive || u.kind == EK_RESOURCE) continue;
        if (u.owner == e.owner && time - u.lastDamaged < 6.0f) {
            const Entity* att = get(u.attacker);
            if (att && stealthOf(*att)) evalCandidate(att->pos, 260.0f);
            else evalCandidate(u.pos, 140.0f);
        } else if (enemies(e.owner, u.owner) && stealthOf(u)) {
            // Swarm intelligence: search around hidden enemy elements (snipers, stealth drones)
            // If already spotted recently, lock on; otherwise sweep suspect sectors around enemy presence
            bool known = visibleTo(u, e.owner);
            Vec2 hint = known ? u.pos : u.pos + Vec2(std::cos(myIdx * 2.1f + u.gen), std::sin(myIdx * 2.1f + u.gen)) * (3.5f * TILE);
            evalCandidate(hint, known ? 240.0f : 195.0f);
        }
    }
    // 2. Coordinated sector sweep across the map (golden-angle + enemy base quadrants + unexplored fog)
    for (int s = 0; s < 16; s++) {
        float ang = (myIdx * 2.39996f) + s * (6.2831853f / 16.0f) + time * 0.035f;
        float rad = (14.0f + ((s + myIdx * 3) % 5) * 11.0f) * TILE;
        Vec2 p = Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f) + Vec2(std::cos(ang), std::sin(ang)) * rad;
        p = Vec2(clampf(p.x, 4 * TILE, WORLD_W - 4 * TILE), clampf(p.y, 4 * TILE, WORLD_H - 4 * TILE));
        int tx = clampi(tileOf(p.x), 0, MAP_W - 1), ty = clampi(tileOf(p.y), 0, MAP_H - 1);
        float prio = (e.owner >= 0 && !explored(e.owner, tx, ty)) ? 135.0f : 75.0f;
        if (dist(p, base) > 22.0f * TILE) prio += 20.0f;
        evalCandidate(p, prio);
    }
    for (int p = 0; p < numPlayers; p++) if (players[p].alive && enemies(e.owner, p)) {
        float a = myIdx * (6.2831853f / swarmN) + time * 0.05f;
        evalCandidate(players[p].basePos + Vec2(std::cos(a), std::sin(a)) * (11.0f * TILE), 150.0f);
    }
    if (dist(bestSpot, e.pos) > 3.0f * TILE) {
        e.order = O_MOVE; e.postOrder = O_IDLE; e.target = bestSpot; e.targetEnt = NOREF; e.autoTask = true;
    }
}

// What a bunker does without anybody inside it and without any help: it earns a little money, mends itself, and its two miniguns
// shoot at whatever comes in reach (ground and air). When surrounded by enemies, its overdrive lasers/heavy machineguns and
// automatic radiation defense engage. Nothing here asks for power, supplies or another structure.
void Sim::updateBunkerSelf(Entity& b) {
    Player& pl = players[b.owner];
    b.actionTimer += SIM_DT;
    if (b.actionTimer >= BUNKER_INCOME_INTERVAL) {
        b.actionTimer -= BUNKER_INCOME_INTERVAL;
        int amt = (int)(BUNKER_INCOME * pl.econMul());
        pl.money += amt; pl.mined += amt;
    }
    bunkerMend(b);
    if (b.nukeSalvo > 0 && time >= b.nukeSalvoAt) {   // the next missile of the salvo leaves the hatch: the same warhead, at the same spot
        b.nukeSalvo--; b.nukeSalvoAt = time + MINI_NUKE_SALVO_GAP;
        nukes.push_back({b.pos, b.nukeSalvoPos, b.owner, 0, b.nukeSalvoForce, MINI_NUKE_SCALE});
        nukes.back().swift = bunkerHas(b, BU_SWIFT);
        emit(EV_SOUND, -1, SND_ROCKET, b.pos);
        for (int p = 0; p < numPlayers; p++) if (enemies(b.owner, p)) b.detUntil[p] = std::max(b.detUntil[p], time + MINI_NUKE_REVEAL);
    }
    // Check if enemy forces surround the bunker
    int enemyAround = 0;
    b.surrounded = bunkerSurrounded(b, &enemyAround);
    if (b.radCd > 0) b.radCd -= SIM_DT;
    bool besieged = b.lastDamaged > 0 && time - b.lastDamaged < BUNKER_RETALIATE;
    bool warding = time - b.guardHitAt < BUNKER_RETALIATE;   // a structure it watches over was hit (see updateBunkerWard)
    // Automatic radiation spreading when surrounded by enemies and getting hit big time (while the surface hatch/vents stand)
    if (b.surrounded && b.hatch > 0 && besieged && b.radCd <= 0 && !inFallout(b.pos) && (b.dmgLoad >= BUNKER_RAD_DMG || (b.hp < b.maxHp * 0.85f && b.dmgLoad >= 90.0f) || (b.hatch < b.hatchMax * 0.65f && b.dmgLoad >= 90.0f))) {
        b.radCd = BUNKER_RAD_COOLDOWN;
        float radR = BUNKER_RAD_RADIUS * TILE;
        fallouts.push_back({b.pos, radR, 0.0f, 0.0f, 1.35f, b.owner, true});
        fx.push_back({FX_FALLOUT, b.pos, b.pos, 0, FALLOUT_LIFE * 0.5f, rgb(120, 255, 90), radR});
        fx.back().tag = ++fxTagSeq;
        fallouts.back().visTag = fxTagSeq;
        fx.push_back({FX_RING, b.pos, b.pos, 0, 1.1f, rgb(120, 255, 90), radR});
        fx.push_back({FX_EMP, b.pos, b.pos, 0, 0.9f, rgb(140, 255, 110), radR * 0.85f});
        emit(EV_SOUND, -1, SND_EXPLODE_L, b.pos);
    }
    // Intercept incoming enemy nuclear warheads with bunker lasers / heavy machineguns
    updateBunkerAntiNuke(b, b.surrounded);

    // under attack: whoever is wearing it down (the last one to hit it and anything with it as its target, in reach of the pod) is answered
    Ref bref = refOf(b);
    const Weapon& pod = WEAPONS[W_BUNKER_ROCKETS];
    auto attackerOf = [&](float reachTiles, int wpn) -> Entity* {   // (the pod knows where the shots come from: it needs no sight of the shooter)
        Entity* best = nullptr; float bs = 1e9f;
        forEachNear(b.pos, reachTiles * TILE + 2 * TILE, [&](Entity& t) {
            if (!t.isUnit() && !t.isBuilding()) return;
            bool onUs = refOf(t) == b.attacker || t.targetEnt == bref || t.engaged == bref;
            if (!onUs && warding) {   // (or on a friendly structure the bunker watches over)
                const Entity* o = get(t.targetEnt); if (!o || !o->isBuilding()) o = get(t.engaged);
                onUs = o && o->isBuilding() && o->owner >= 0 && players[o->owner].team == players[b.owner].team && dist(o->pos, b.pos) <= BUNKER_GUARD_RADIUS * TILE;
            }
            if (!onUs || !enemies(b.owner, t.owner) || t.weapon() < 0) return;   // (an unarmed spy walking up to take it is not wearing it down)
            if (wpn == W_BUNKER_ROCKETS ? (t.carrier.valid() || t.fall > 0) : !canTarget(b, t, wpn)) return;
            float d = distToEntity(b.pos, t) / TILE;
            if (d > reachTiles + 0.15f) return;
            float score = d - 3.0f * WEAPONS[wpn].mult[t.armor()] - (refOf(t) == b.attacker ? 1.5f : 0.0f);
            if (score < bs) { bs = score; best = &t; }
        });
        return best;
    };
    if (b.podCd > 0) b.podCd -= SIM_DT;
    if ((besieged || warding) && b.podCd <= 0 && (tick + b.gen) % 4 == 0) {   // the rocket pod pops up (hatch or no hatch) and fires a salvo at the worst attacker
        if (Entity* t = attackerOf(pod.range, W_BUNKER_ROCKETS)) {
            b.podAng = std::atan2(t->pos.y - b.pos.y, t->pos.x - b.pos.x);
            Vec2 side(-std::sin(b.podAng), std::cos(b.podAng));
            for (int k = 0; k < pod.burst; k++) { Vec2 m = b.pos + Vec2(0, BUNKER_POD_Y) + side * ((k - 1) * 4.5f); fireWeapon(b, *t, pod, &m); }
            b.podCd = pod.cooldown;
        }
    }
    // Surrounded overdrive turrets: powerful lasers and heavy quad-machineguns that shoot aircraft, tanks and infantry while the surface hatch stands
    if (b.surrounded && b.hatch > 0) {
        int owId = b.bt().faction == F_CYBER ? W_BUNKER_LASER : W_BUNKER_HEAVY_MG;
        const Weapon& ow = WEAPONS[owId];
        float owReach = (ow.range + 0.25f) * TILE;
        for (int i = 0; i < 2; i++) {
            if (b.overdriveCd[i] > 0) b.overdriveCd[i] -= SIM_DT;
            Entity* t = get(b.overdriveTgt[i]);
            if (!t || !enemies(b.owner, t->owner) || !canTarget(b, *t, owId) || distToEntity(b.pos, *t) > owReach) {
                t = nullptr; b.overdriveTgt[i] = NOREF;
                Entity* best = nullptr; float bs = 1e9f;
                forEachNear(b.pos, owReach, [&](Entity& c) {
                    if (!c.isUnit() || !enemies(b.owner, c.owner) || !canTarget(b, c, owId)) return;
                    float d = distToEntity(b.pos, c) / TILE;
                    if (d > ow.range + 0.2f) return;
                    // Prioritize aircraft, tanks and infantry equally fiercely, with a split between the two mounts
                    float sc = d - 3.2f * ow.mult[c.armor()] - (c.isAir() && i == 0 ? 2.5f : 0.0f) - (c.armor() == AR_HEAVY && i == 1 ? 2.0f : 0.0f);
                    if (sc < bs) { bs = sc; best = &c; }
                });
                if (best) { t = best; b.overdriveTgt[i] = refOf(*best); }
            }
            if (!t) continue;
            Vec2 muzzle = bunkerOverdrivePos(b, i);
            b.overdriveAng[i] = std::atan2(t->pos.y - muzzle.y, t->pos.x - muzzle.x);
            if (b.overdriveCd[i] > 0) continue;
            // Fire both the faction overdrive weapon and a secondary laser/tracer beam so both lasers and heavy MGs tear into targets
            fireWeapon(b, *t, ow, &muzzle);
            if (ow.proj != PJ_LASER) fx.push_back({FX_BEAM, muzzle, t->pos, 0, 0.12f, rgb(110, 245, 255), 2.0f});
            b.overdriveCd[i] = ow.cooldown;
        }
    }
    const Weapon& w = WEAPONS[W_BUNKER_MINIGUN];
    float reach = (w.range + 0.2f) * TILE;
    Entity* hitter = (besieged || warding) && b.hatch > 0 ? attackerOf(w.range, W_BUNKER_MINIGUN) : nullptr;   // the hatch guns go for an attacker in reach first
    for (int i = 0; i < BUNKER_GUNS; i++) {
        if (b.gunCd[i] > 0) b.gunCd[i] -= SIM_DT;
        if (b.hatch <= 0) { b.gunTgt[i] = NOREF; continue; }   // the guns sit on the hatch: blown off, they are silent
        bool hadTgt = b.gunTgt[i].valid();
        Entity* t = get(b.gunTgt[i]);
        if (hitter && t != hitter && (i == 0 || !t)) { t = hitter; b.gunTgt[i] = refOf(*hitter); }
        if (!t || !enemies(b.owner, t->owner) || !canTarget(b, *t, W_BUNKER_MINIGUN) || distToEntity(b.pos, *t) > reach) {
            t = nullptr; b.gunTgt[i] = NOREF;
            if (hadTgt || (tick + b.gen + i) % 3 == 0) { t = acquireTarget(b, w.range, W_BUNKER_MINIGUN); if (t) b.gunTgt[i] = refOf(*t); }
        }
        if (!t) continue;
        Vec2 muzzle = bunkerGunPos(b, i);
        b.gunAng[i] = std::atan2(t->pos.y - muzzle.y, t->pos.x - muzzle.x);
        if (b.gunCd[i] > 0) continue;
        fireWeapon(b, *t, w, &muzzle);
        b.gunCd[i] = b.surrounded ? w.cooldown * 0.65f : w.cooldown;
    }
}

void Sim::updateBunker(Entity& b) {
    Ref bref = refOf(b);
    // who is really inside (and stand the soldiers around the hatch so their muzzle flashes come from different spots)
    int idx = 0, n = (int)b.passengers.size();
    for (size_t i = 0; i < b.passengers.size();) {
        Entity* u = get(b.passengers[i]);
        if (!u || u->carrier != bref) { b.passengers.erase(b.passengers.begin() + i); n--; continue; }
        float a = i * 2.39996f;
        u->pos = b.pos + Vec2(std::cos(a), std::sin(a)) * (3.0f + 9.0f * std::sqrt((float)i / (float)std::max(1, n)));
        u->prevPos = u->pos;
        idx++; i++;
    }
    if (b.unloading) updateCarrierUnload(b);
    if (b.passengers.empty()) { b.unloading = false; return; }
    // the garrison mends itself; the dozers inside patch the hatch and the bunker (each one adds)
    if ((tick + b.gen) % 5 == 0) {
        int dozers = 0;
        for (Ref r : b.passengers) {
            Entity* u = get(r); if (!u) continue;
            if (u->hp < u->maxHp) u->hp = std::min(u->maxHp, u->hp + u->maxHp * 0.015f * 0.25f);
            if (u->ut().role == UR_DOZER) dozers++;
        }
        if (dozers > 0 && time - b.lastDamaged > 3.0f) {
            b.hatch = std::min(b.hatchMax, b.hatch + b.hatchMax * 0.06f * 0.25f * dozers);
            b.hp = std::min(b.maxHp, b.hp + b.maxHp * 0.004f * 0.25f * dozers);
        }
    }
    if (b.hatch <= 0 || b.unloading) { for (Ref r : b.passengers) { Entity* u = get(r); if (u && u->cooldown > 0) u->cooldown -= SIM_DT; } return; }
    // fire: every soldier shoots with its own weapon from the hatch, at the best target it can hit (rocket troopers take aircraft and tanks)
    if ((tick + b.gen) % 2 != 0) return;
    static std::vector<Ref> cand; cand.clear();
    float reach = 12.5f * TILE;
    forEachNear(b.pos, reach, [&](Entity& t) {
        if (t.kind == EK_RESOURCE || !enemies(b.owner, t.owner) || t.carrier.valid() || t.fall > 0 || !visibleTo(t, b.owner)) return;
        cand.push_back(refOf(t));
    });
    for (Ref r : b.passengers) {
        Entity* u = get(r);
        if (!u) continue;
        if (u->cooldown > 0) u->cooldown -= 2 * SIM_DT;
        int wi = u->weapon();
        if (wi < 0 || u->cooldown > 0 || cand.empty()) continue;
        const Weapon& w = WEAPONS[wi];
        Entity* best = nullptr; float bs = 1e9f;
        for (Ref cr : cand) {
            Entity* t = get(cr);
            if (!t || !canTarget(*u, *t)) continue;
            float d = distToEntity(u->pos, *t) / TILE;
            if (d > w.range + 0.15f || d < w.minRange) continue;
            float score = d - 2.5f * w.mult[t->armor()];
            if (t->isUnit() && t->weapon() >= 0) score -= 1.5f;
            if (t->isBuilding() && !t->constructed) score += 1.0f;
            if (score < bs) { bs = score; best = t; }
        }
        if (best) { tryFire(*u, *best); b.cooldown2 = 0.6f; }   // (the hatch leaves stay open while the garrison is shooting: see drawEntity)
    }
}

// A claimed civilian stronghold: the garrison holds the windows and loopholes and fires with its own weapons. There are no hatch or floor rules:
// the men are inside the masonry and cannot be hit until the structure itself falls. A neutral stronghold (owner < 0) has no enemies and does nothing.
void Sim::updateStronghold(Entity& b) {
    Ref bref = refOf(b);
    int n = (int)b.passengers.size();
    for (size_t i = 0; i < b.passengers.size();) {
        Entity* u = get(b.passengers[i]);
        if (!u || u->carrier != bref) { b.passengers.erase(b.passengers.begin() + i); n--; continue; }
        // stand them around the inside of the footprint; their muzzle flashes come from different windows
        float a = i * 2.39996f;
        u->pos = b.pos + Vec2(std::cos(a), std::sin(a)) * (b.radius() * 0.42f * std::sqrt(n > 0 ? (float)i / n : 1.0f));
        u->prevPos = u->pos;
        i++;
    }
    if (b.unloading) updateCarrierUnload(b);
    if (b.passengers.empty()) { b.unloading = false; return; }
    if (b.owner < 0 || (tick + b.gen) % 2 != 0) return;   // a neutral place has no side to shoot for
    static std::vector<Ref> cand; cand.clear();
    forEachNear(b.pos, 13.0f * TILE, [&](Entity& t) {
        if (t.kind == EK_RESOURCE || !enemies(b.owner, t.owner) || t.carrier.valid() || t.fall > 0 || !visibleTo(t, b.owner)) return;
        cand.push_back(refOf(t));
    });
    if (cand.empty()) return;
    const float reach = b.type == B_C_FORTRESS ? FORTRESS_REACH : 0.0f;   // the fortress's high galleries see and shoot further
    for (Ref r : b.passengers) {
        Entity* u = get(r);
        if (!u) continue;
        if (u->cooldown > 0) u->cooldown -= 2 * SIM_DT;
        int wi = u->weapon();
        if (wi < 0 || u->cooldown > 0) continue;
        const Weapon& w = WEAPONS[wi];
        Entity* best = nullptr; float bs = 1e9f;
        for (Ref cr : cand) {
            Entity* t = get(cr);
            if (!t || !canTarget(*u, *t)) continue;
            float d = distToEntity(b.pos, *t) / TILE;   // fire from the building, not from an accidental window offset
            if (d > w.range + reach + 0.15f || d < w.minRange) continue;
            float score = d - 2.5f * w.mult[t->armor()];
            if (t->isUnit() && t->weapon() >= 0) score -= 1.5f;
            if (t->isBuilding() && !t->constructed) score += 1.0f;
            if (score < bs) { bs = score; best = t; }
        }
        if (best) { tryFire(*u, *best); b.cooldown2 = 0.6f; }
    }
}

// The bunker watches over the structures around it (the civilian town above all): a hit on one of them makes the bunker answer the attacker
// (see updateBunkerSelf: guardHitAt), and once the shooting has stopped its crew mends them a little at a time.
void Sim::updateBunkerWard(Entity& b) {
    if ((tick + b.gen) % 10 != 0 || b.owner < 0) return;
    const float R = BUNKER_GUARD_RADIUS * TILE, dt = SIM_DT * 10;
    forEachNear(b.pos, R, [&](Entity& o) {
        if (&o == &b || !o.isBuilding() || !o.constructed || o.owner < 0 || players[o.owner].team != players[b.owner].team || dist(o.pos, b.pos) > R) return;
        if (o.lastDamaged > 0 && time - o.lastDamaged < BUNKER_RETALIATE) { const Entity* a = get(o.attacker); if (a && enemies(b.owner, a->owner)) b.guardHitAt = std::max(b.guardHitAt, o.lastDamaged); }
        if (o.owner != b.owner || o.hp >= o.maxHp || time - o.lastDamaged < BUNKER_MEND_DELAY || inFallout(o.pos)) return;
        o.hp = std::min(o.maxHp, o.hp + o.maxHp * BUNKER_MEND_RATE * dt);
        if ((tick / 10 + o.gen) % 4 == 0) fx.push_back({FX_SPARK, o.pos + Vec2(rng.f(-o.radius() * 0.6f, o.radius() * 0.6f), rng.f(-o.radius() * 0.4f, o.radius() * 0.4f)), Vec2(), 0, 0.3f, rgb(150, 255, 170), 3});
    });
}

// A civilian structure runs on its own: no power, no supplies. It pays its army every CIVIC_INTERVAL seconds and, while the army has fewer than
// CIVIC_DOZER_CAP dozers, sends out a free one every CIVICS[].dozerEvery seconds (a military dozer and a civilian one in turn).
void Sim::updateCivic(Entity& b) {
    int k = civicKind(b.type);
    if (k < 0 || b.owner < 0) return;
    Player& pl = players[b.owner];
    b.actionTimer += SIM_DT;
    if (b.actionTimer >= CIVIC_INTERVAL) {
        b.actionTimer -= CIVIC_INTERVAL;
        int amt = (int)(CIVICS[k].income * pl.econMul());
        pl.money += amt; pl.mined += amt;
        fx.push_back({FX_SPARK, b.pos, b.pos, 0, 0.6f, rgb(255, 224, 90), 7});
    }
    if (time < b.dozerAt) return;
    int dozers = 0;
    for (auto& u : ents) if (u.alive && u.isUnit() && u.owner == b.owner && u.ut().role == UR_DOZER) dozers++;
    if (dozers >= CIVIC_DOZER_CAP) { b.dozerAt = time + 5.0f; return; }   // the town waits until the army has room for another
    b.dozerAt = time + CIVICS[k].dozerEvery;
    int type = b.dozerKind == 0 ? firstUnitOf(pl.faction) : (pl.faction == F_CYBER ? (int)U_C_CIVDOZER : (int)U_K_CIVDOZER);
    b.dozerKind ^= 1;
    int owner = b.owner;
    Vec2 at = unitExit(b), out = at + Vec2(rng.f(-24, 24), rng.f(10, 30));
    char msg[96]; snprintf(msg, sizeof msg, "The %s sent out a %s", b.bt().name, UNITS[type].name);
    Ref r = spawnUnit(type, owner, at);   // (may grow the entity array: b is not touched after this)
    cmdMove({r}, g_map.nearestFree(out, 4), false);
    if (Entity* u = get(r)) { u->autoHoldUntil = -1; u->autoTask = true; }
    emit(EV_UNIT_READY, owner, SND_UNIT_READY, at, msg);
}

// An Underground Nuke silo mends itself like a bunker: the body slowly, the blast doors faster, and blown doors are refitted after a while.
void Sim::updateSilo(Entity& b) {
    if (time - b.lastDamaged < SILO_REGEN_DELAY) return;
    b.hp = std::min(b.maxHp, b.hp + b.maxHp * SILO_REGEN * SIM_DT);
    if (b.hatchMax > 0 && (b.hatch > 0 || time - b.lastDamaged >= SILO_DOOR_REFIT)) b.hatch = std::min(b.hatchMax, b.hatch + b.hatchMax * (b.hatch > 0 ? SILO_HATCH_REGEN * SIM_DT : 0.05f));
}

bool Sim::hardened(const Entity& b) {
    return b.isBuilding() && b.constructed && (BUILDS[b.type].role == BR_BUNKER || BUILDS[b.type].role == BR_UNUKE);
}
bool Sim::canRaise(const Entity& d, int buildType) {   // (no dozer works on a Waymaker: it clears its ground by itself)
    return d.isUnit() && buildType >= 0 && buildType < B_COUNT && !neutralRole(BUILDS[buildType].role) && BUILDS[buildType].role != BR_WAYMAKER && civicDozerType(d.type) == (BUILDS[buildType].role == BR_CIVIC);
}
bool Sim::canMend(const Entity& d, int buildType) {   // (the town's free civilian dozers mending the whole base made bases nearly impossible to grind down)
    return d.isUnit() && buildType >= 0 && buildType < B_COUNT && (!civicDozerType(d.type) || BUILDS[buildType].role == BR_CIVIC);
}
int Sim::civicPeople(const Entity& b) const {
    int k = b.isBuilding() && b.constructed ? civicKind(b.type) : -1;
    if (k >= 0 && b.district >= 0) return (int)(CIVICS[k].people * (0.35f + 0.65f * b.hp / b.maxHp) + 0.5f);   // a town block's people come back as it is rebuilt
    return k < 0 ? 0 : CIVICS[k].people;
}

float Sim::captureTime(const Entity& b) const {
    if (b.district >= 0) return DISTRICT_CAPTURE;   // a town is taken as a whole, through any of its blocks
    return SPY_CAPTURE_BASE + SPY_CAPTURE_PER_TILE * (b.bt().w * b.bt().h);
}

void Sim::cmdCapture(const std::vector<Ref>& sel, Ref building) {
    Entity* b = get(building);
    if (!b || !b->isBuilding() || (!b->constructed && b->district < 0)) return;   // (a town is taken through any block, a ruin too)
    for (auto r : sel) {
        Entity* e = get(r);
        if (!e || !e->isUnit() || e->ut().role != UR_SPY || e->carrier.valid() || !spyMayTake(e->owner, *b) || !visibleTo(*b, e->owner)) continue;
        e->zoneR = 0; e->leashed = false; takeOrder(*e); e->forceTarget = NOREF;
        e->order = O_CAPTURE; e->postOrder = O_IDLE; e->targetEnt = building; e->engaged = NOREF; e->path.clear(); e->pathIdx = 0; e->repathTimer = 0; e->actionTimer = 0; e->stuckTimer = 0;
    }
}

// A spy finishes the job: the structure changes hands, with what it holds. Its production queue is refunded to the old owner, a bunker's
// garrison is turned out onto the ground, and the health is carried over in proportion.
void Sim::captureBuilding(Entity& b, Entity& spy) {
    int from = b.owner, to = spy.owner;
    if (to < 0 || from == to) return;
    if (b.district >= 0) { captureDistrict(b.district, to); return; }   // the whole town changes hands
    if (!b.passengers.empty()) ejectAll(b, false);
    if (from >= 0) for (int t : b.queue) players[from].money += UNITS[t].cost;
    b.queue.clear(); b.queueProgress = 0; b.hasRally = false; b.engaged = NOREF; b.engaged2 = NOREF; b.forceTarget = NOREF; b.unloading = false;
    float oldMax = b.maxHp;
    b.owner = to;
    if (b.bt().role == BR_ISLANDNUKE) { b.siloTimer = std::max(b.siloTimer, time + ISLAND_NUKE_ARM); players[to].launchersTaken++; }   // the new crew needs a while to take over the launch codes
    b.maxHp = b.bt().hp * (!civilianRole(b.bt().role) && hasUpgrade(to, UPG_RUGGED) ? RUGGED_HP : 1.0f) * (bunkerHas(b, BU_SHELL) ? BUNKER_SHELL_HP : 1.0f);   // (a bunker's own upgrades stay with it)
    b.hp = std::max(1.0f, b.hp * (b.maxHp / std::max(1.0f, oldMax)));
    b.disabledUntil = std::max(b.disabledUntil, time + 3.0f);
    b.attacker = NOREF; b.lastDamaged = -100;
    for (int q = 0; q < MAX_PLAYERS; q++) b.detUntil[q] = 0;
    { Ref bref = refOf(b); for (auto& u : ents) if (u.alive && u.isUnit() && u.home == bref) u.home = NOREF; }   // aircraft based here must find another airfield: they never land on an enemy pad
    const bool ally = from >= 0 && !enemies(to, from);   // a human's infiltrator took an ally's structure (see allyTakeable)
    if (from >= 0 && !ally) { players[to].structuresKilled++; players[from].structuresLost++; }
    for (int k = 0; k < 10; k++) fx.push_back({FX_SPARK, b.pos + Vec2(rng.f(-b.radius(), b.radius()), rng.f(-b.radius(), b.radius())), Vec2(), -rng.f(0, 0.4f), 0.5f, players[to].faction == F_CYBER ? rgb(120, 236, 255) : rgb(255, 214, 120), 5});
    fx.push_back({FX_RING, b.pos, b.pos, 0, 0.9f, players[to].faction == F_CYBER ? rgb(120, 236, 255) : rgb(255, 214, 120), b.radius() * 1.3f});
    char msg[96];
    snprintf(msg, sizeof msg, ally ? "Took over an ally's %s" : from >= 0 ? "Captured an enemy %s" : "Captured the %s", b.bt().name);
    emit(EV_MSG, to, SND_NONE, b.pos, msg);
    emit(EV_SOUND, to, SND_BUILD_DONE, b.pos);
    if (ally) {   // an ally loses it to its own side: no alarm, just word of it
        snprintf(msg, sizeof msg, "Our ally's infiltrator took over our %s", b.bt().name);
        emit(EV_MSG, from, SND_NONE, b.pos, msg);
    } else {
        snprintf(msg, sizeof msg, "A spy captured our %s", b.bt().name);
        if (from >= 0) emit(EV_UNDER_ATTACK, from, SND_ATTACKED, b.pos, msg);
        snprintf(msg, sizeof msg, "A spy captured your ally's %s", b.bt().name);
        if (from >= 0) for (int q = 0; q < numPlayers; q++) if (q != from && players[q].team == players[from].team) emit(EV_UNDER_ATTACK, q, SND_ATTACKED, b.pos, msg);
    }
    updatePower();
}

// Infantry earn promotions: veterans (4 kills) and elites (10) hit harder and are tougher
void Sim::awardKill(Ref killer, const Entity& victim) {
    Entity* k = get(killer);
    if (!k || !k->isUnit() || k->ut().kind != UK_INF || k->owner < 0 || !enemies(k->owner, victim.owner)) return;
    k->kills++;
    int rank = k->kills >= ELITE_KILLS ? 2 : (k->kills >= VET_KILLS ? 1 : 0);
    if (rank > k->rank) {
        k->rank = (u8)rank;
        float newMax = k->ut().hp * (rank == 2 ? 1.5f : 1.25f);
        k->hp += newMax - k->maxHp; k->maxHp = newMax;
        fx.push_back({FX_RING, k->pos, k->pos, 0, 0.8f, rank == 2 ? rgb(255, 226, 120) : rgb(190, 235, 255), 22});
        if (k->owner >= 0) emit(EV_MSG, k->owner, SND_NONE, k->pos, rank == 2 ? "A soldier became Elite" : "A soldier became a Veteran");
    }
}

// ------------------------------------------------------------ Defense Guns: every structure's roof laser / machine gun
int Sim::roofGun(const Entity& b) const {
    if (!b.isBuilding() || !b.constructed || !hasUpgrade(b.owner, UPG_GUNS) || civilianRole(b.bt().role)) return -1;   // (no roof gun on the town, the map's neutral structures or a Bastion Wall)
    return BUILDS[b.type].faction == F_CYBER ? W_DEFENSE_LASER : W_DEFENSE_MG;
}

Vec2 Sim::roofGunPos(const Entity& b) const {
    const BuildType& bt = b.bt();
    if (bt.w * bt.h <= 1) return b.pos + Vec2(-10, -10);            // beside the main gun of a small turret
    return b.pos + Vec2(-bt.w * TILE * 0.30f, -bt.h * TILE * 0.28f);   // the roof corner away from the team flag
}

void Sim::updateRoofGun(Entity& b, bool powered) {
    int wi = roofGun(b);
    if (wi < 0) return;
    const Weapon& w = WEAPONS[wi];
    float reach = (w.range + 0.2f) * TILE;
    Entity* t = get(b.engaged2);
    if (!t || !enemies(b.owner, t->owner) || !canTarget(b, *t, wi) || distToEntity(b.pos, *t) > reach) {
        t = nullptr; b.engaged2 = NOREF;
        if ((tick + b.gen) % 3 == 1) { t = acquireTarget(b, w.range, wi); if (t) b.engaged2 = refOf(*t); }
    }
    if (!t) return;
    Vec2 muzzle = roofGunPos(b);
    b.turret2 = std::atan2(t->pos.y - muzzle.y, t->pos.x - muzzle.x);
    if (b.cooldown2 > 0) return;
    fireWeapon(b, *t, w, &muzzle);
    b.cooldown2 = w.cooldown * (powered ? 1.0f : 2.0f);   // runs on reserve power at half rate when the base is short of power
}

void Sim::updatePower() {
    for (int p = 0; p < numPlayers; p++) { players[p].powerMade = 0; players[p].powerUsed = 0; }
    for (auto& e : ents) {
        if (!e.alive || !e.isBuilding() || !e.constructed || e.owner < 0) continue;
        int pw = e.bt().power;
        if (pw > 0) players[e.owner].powerMade += pw; else players[e.owner].powerUsed -= pw;
    }
}

void Sim::updateVision() {
    for (auto& e : ents) {
        if (!e.alive || e.owner < 0 || e.carrier.valid()) continue;
        int cx = tileOf(e.pos.x), cy = tileOf(e.pos.y);
        int key = (e.owner * MAP_H + clampi(cy, 0, MAP_H - 1)) * MAP_W + clampi(cx, 0, MAP_W - 1);
        if (key == e.visKey) continue;   // nothing new in sight since it last looked (explored ground stays explored)
        e.visKey = key;
        // allies share what they see: the sight of every unit and structure is written into the map of each member of its team
        Player* seers[MAX_PLAYERS]; int ns = 0;
        for (int p = 0; p < numPlayers; p++) if (p == e.owner || players[p].team == players[e.owner].team) seers[ns++] = &players[p];
        float s = e.sight();
        int r = (int)std::ceil(s);
        float s2 = s * s;
        for (int dy = -r; dy <= r; dy++) {
            int y = cy + dy; if (y < 0 || y >= MAP_H) continue;
            for (int dx = -r; dx <= r; dx++) {
                int x = cx + dx; if (x < 0 || x >= MAP_W) continue;
                if (dx * dx + dy * dy <= s2) for (int k = 0; k < ns; k++) seers[k]->explored[y * MAP_W + x] = 1;
            }
        }
    }
}

void Sim::updateSight() {
    if (humanPlayer < 0 || humanPlayer >= numPlayers) return;
    inSight.assign((size_t)MAP_W * MAP_H, 0);
    const int team = players[humanPlayer].team;
    for (auto& e : ents) {
        if (!e.alive || e.owner < 0 || e.owner >= numPlayers || e.carrier.valid() || players[e.owner].team != team) continue;
        if (e.isBuilding() && !e.constructed && e.progress <= 0) continue;
        int cx = tileOf(e.pos.x), cy = tileOf(e.pos.y);
        float sgt = e.sight();
        int r = (int)std::ceil(sgt);
        float s2 = sgt * sgt;
        for (int dy = -r; dy <= r; dy++) {
            int y = cy + dy; if (y < 0 || y >= MAP_H) continue;
            u8* row = &inSight[(size_t)y * MAP_W];
            for (int dx = -r; dx <= r; dx++) {
                int x = cx + dx; if (x < 0 || x >= MAP_W) continue;
                if (dx * dx + dy * dy <= s2) row[x] = 1;
            }
        }
    }
    sightGen++;
}

bool Sim::shownToHuman(const Entity& e) const {
    const int hp = humanPlayer;
    if (e.owner == hp || revealed(hp)) return true;
    if (e.owner >= 0 && e.owner < numPlayers && players[e.owner].team == players[hp].team) return true;   // an ally's: shared vision
    int tx = clampi(tileOf(e.pos.x), 0, MAP_W - 1), ty = clampi(tileOf(e.pos.y), 0, MAP_H - 1);
    if (e.kind == EK_RESOURCE || e.isBuilding()) return explored(hp, tx, ty);   // the lie of the land and what stands on it is remembered
    if (e.isBuilding() == false && e.isUnit() && !e.isAir()) {   // a ship or a ground unit: only while one of yours sees it
        float r = e.radius() * 0.5f;   // (a big hull half in sight shows)
        for (int k = 0; k < 5; k++) {
            int qx = clampi(tileOf(e.pos.x + (k == 1 ? r : k == 2 ? -r : 0)), 0, MAP_W - 1), qy = clampi(tileOf(e.pos.y + (k == 3 ? r : k == 4 ? -r : 0)), 0, MAP_H - 1);
            if (seenNow(qx, qy)) return true;
        }
        return false;
    }
    return seenNow(tx, ty);
}

void Sim::separateUnits() {
    // what the pair test needs, packed per entity once a tick (walking the full Entity and its type table for every neighbour pair was a tenth
    // of a crowded tick); positions are kept current as units are pushed
    struct Sep { float x, y, r; u8 f; };   // f: 1 a unit taking part, 2 airborne, 4 a ship, 8 a helicopter, 16 squeezing, 32 idle or guarding
    static std::vector<Sep> sp;
    sp.resize(ents.size());
    for (size_t i = 0; i < ents.size(); i++) {
        const Entity& e = ents[i];
        Sep& q = sp[i]; q.f = 0;
        if (!e.alive || !e.isUnit() || e.fall > 0) continue;
        const UnitType& ut = e.ut();
        q.x = e.pos.x; q.y = e.pos.y; q.r = ut.radius;
        q.f = (u8)(1 | (ut.kind == UK_AIR ? 2 : 0) | (ut.naval ? 4 : 0) | (ut.heli ? 8 : 0) | (e.squeezeUntil > time ? 16 : 0) | ((e.order == O_IDLE || e.order == O_GUARDAREA) ? 32 : 0));
    }
    for (int i = 0; i < (int)ents.size(); i++) {
        Entity& a = ents[i];
        if (!a.alive || !a.isUnit() || a.fall > 0 || a.carrier.valid()) continue;
        const u8 fa = sp[i].f;
        bool air = (fa & 2) != 0;
        float ra = sp[i].r;
        int cx = clampi((int)(a.pos.x / GRID_CELL), 0, GRID_W - 1), cy = clampi((int)(a.pos.y / GRID_CELL), 0, GRID_H - 1);
        Vec2 push;
        for (int gy = std::max(0, cy - 1); gy <= std::min(GRID_H - 1, cy + 1); gy++)
        for (int gx = std::max(0, cx - 1); gx <= std::min(GRID_W - 1, cx + 1); gx++) {
            for (int j : grid[gy * GRID_W + gx]) {
                if (j == i) continue;
                const Sep& b = sp[j];
                if (!(b.f & 1) || ((b.f & 2) != 0) != air) continue;
                if (!air && ((b.f ^ fa) & 4)) continue;   // a ship and a land unit never share ground (a shore unit is not pushed by a hull)
                if (!air && ((fa | b.f) & 16)) continue;  // a jammed unit slips past
                float rr = ra + b.r;
                if (air && (fa & 8) && (b.f & 8)) rr = HELI_SPACE;   // helicopters keep a small gap between their hulls
                Vec2 d(a.pos.x - b.x, a.pos.y - b.y);
                float l2 = d.len2();
                if (l2 >= rr * rr || l2 < 1e-4f) { if (l2 < 1e-4f) push += Vec2(rng.f(-1, 1), rng.f(-1, 1)); continue; }
                float l = std::sqrt(l2);
                float overlap = (rr - l) / rr;
                float wgt = ((b.f & 32) && !(fa & 32)) ? 0.4f : 0.8f;  // movers push idlers aside
                push += d * (overlap * wgt * 9.0f / l);
            }
        }
        if (air && !a.ut().heli) {   // planes in the air only drift apart sideways: the push never slows or stops a fixed-wing craft
            Vec2 n(-std::sin(a.angle), std::cos(a.angle));
            push = n * (push.x * n.x + push.y * n.y);
        }
        if (push.len2() > 0.01f) {
            Vec2 np = a.pos + push * SIM_DT * 14.0f;
            if (air || ((fa & 4) ? g_map.navTile(tileOf(np.x), tileOf(np.y)) : g_map.passable(tileOf(np.x), tileOf(np.y)))) a.pos = np;
        }
        a.pos.x = clampf(a.pos.x, 8, WORLD_W - 8); a.pos.y = clampf(a.pos.y, 8, WORLD_H - 8);
        sp[i].x = a.pos.x; sp[i].y = a.pos.y;
    }
}

void Sim::checkVictory() {
    bool anyDied = false;
    for (int p = 0; p < numPlayers; p++) {
        Player& pl = players[p];
        if (!pl.alive) continue;
        // an army is out only when nothing of its own still stands: a hidden sniper, spy drone or bunker counts (the enemy has to find it),
        // and so does what sits inside a bunker or a transport. A bunker that falls turns its garrison out onto the ground, where it counts too.
        // Civilian dozers do not: they are civilians, they cannot raise a base, and nobody hunts them (the town's structures do count)
        bool any = false;
        for (auto& e : ents) if (e.alive && e.owner == p && e.kind != EK_RESOURCE && !(e.isUnit() && (e.ut().temp || civicDozerType(e.type))) && !invulnerable(e) && e.district < 0) { any = true; break; }   // (a town it holds is not its army either)   // (a Hotline Strike fighter on loan is no army, nor is an indestructible launcher or wall it holds)
        if (!any) {
            pl.alive = false; anyDied = true; emit(EV_PLAYER_DEAD, p, SND_NONE, pl.basePos, FACTION_NAME[pl.faction]);
            for (auto& e : ents) if (e.alive && e.owner == p && e.isUnit() && civicDozerType(e.type)) destroy(e, false);   // its civilian dozers leave the war behind
            for (auto& e : ents) if (e.alive && e.owner == p && invulnerable(e)) { e.owner = -1; e.attacker = NOREF; for (int q = 0; q < MAX_PLAYERS; q++) e.detUntil[q] = 0; }   // its island launchers and Bastion Walls fall to nobody (any spy may take them)
            for (int d = 0; d < (int)districts.size(); d++) if (districts[d].owner == p) captureDistrict(d, -1);   // and its towns are free again
        }
    }
    if (!anyDied) return;
    int aliveTeam = -1; bool multi = false;
    for (int p = 0; p < numPlayers; p++) if (players[p].alive) { if (aliveTeam < 0) aliveTeam = players[p].team; else if (players[p].team != aliveTeam) multi = true; }
    if (!multi) { gameOver = true; winnerTeam = aliveTeam; return; }
    // once every human player is out the match is lost, even if a computer-controlled ally still stands
    int humans = 0, humansAlive = 0;
    for (int p = 0; p < numPlayers; p++) if (!players[p].isAI) { humans++; if (players[p].alive) humansAlive++; }
    if (humans > 0 && humansAlive == 0) {
        gameOver = true; winnerTeam = aliveTeam;
        // (the first army standing may be the human's own computer ally: the winner is whoever of the other teams still stands)
        int humanTeam = players[humanPlayer].team;
        for (int p = 0; p < numPlayers; p++) if (players[p].alive && players[p].team != humanTeam) { winnerTeam = players[p].team; break; }
    }
}

// ------------------------------------------------------------ main step
void Sim::step() {
    if (gameOver) return;
    tick++; time += SIM_DT;
    for (auto& e : ents) if (e.alive) e.prevPos = e.pos;
    for (auto& p : projs) p.prevPos = p.pos;
    rebuildGrid();
    if (tick % 2 == 0) updateDetection();
    // index loop: producing a unit can grow (reallocate) the entity array mid-iteration
    for (size_t i = 0; i < ents.size(); i++) {
        Entity& e = ents[i];
        if (!e.alive) continue;
        if (e.carrier.valid()) {   // stowed: rides along with its host (a bunker places its soldiers itself); the host fires for the garrison
            if (Entity* c = get(e.carrier)) { if (c->isUnit()) e.pos = c->pos; } else e.carrier = NOREF;
            e.prevPos = e.pos;
            continue;
        }
        if (e.isUnit()) {
            updateUnit(e);
            if (ents[i].alive && (ents[i].type == U_C_CARRIER || ents[i].type == U_K_CARRIER) && ents[i].isUnit()) updateCarrier(ents[i]);   // (after the unit's own update: a launch may grow the array)
        }
        else if (e.isBuilding()) updateBuilding(e);
    }
    updateProjectiles();
    updateStorms();
    updateNukes();
    updateAirlifts();
    updateStrikeMarks();
    updateInterceptors();
    updateAidDrops();
    updateFleetArrivals();
    updateFallout();
    separateUnits();
    updateFx();
    if (tick % 4 == 0) { updateVision(); updateSight(); }
    if (tick % 10 == 0) updatePower();
    if (tick % 5 == 0) updateResearch();
    if (tick % 20 == 0) checkVictory();
    if (tick % 10 == 3) updateDistricts();
    if (tick % 20 == 7) updateRiots();
    // low-power notices
    for (int p = 0; p < numPlayers; p++) {
        bool low = players[p].lowPower();
        if (low && !players[p].wasLowPower) emit(EV_LOWPOWER, p, SND_LOWPOWER, players[p].basePos, "Low power");
        players[p].wasLowPower = low;
    }
}
