#include "sim.h"

Sim g_sim;

static const float BUILD_REACH = 26.0f;     // px from structure edge for dozers
static const float HARVEST_REACH = 34.0f;   // px from the pile's edge: a hauler parked on a diagonal neighbour tile is about 29 px away
static const float PATH_INTERVAL = 0.6f;
static const float EMP_DURATION = 8.0f;

// ------------------------------------------------------------ init
void Sim::init(int nPlayers, const Faction* factions, const bool* isAI, const int* difficulties, const int* teams, u64 seed) {
    ents.clear(); freeList.clear(); projs.clear(); fx.clear(); events.clear(); storms.clear(); nukes.clear(); airlifts.clear(); aidDrops.clear(); fallouts.clear();
    ents.reserve(1024);
    time = 0; tick = 0; gameOver = false; winnerTeam = -1;
    rng = Rng(seed);
    g_map.generate();
    numPlayers = clampi(nPlayers, 1, MAX_PLAYERS);
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
    for (auto& s : g_map.supplies) spawnResource(s.tx, s.ty, s.amount);
    for (int p = 0; p < numPlayers; p++) { players[p].powerReady = 120.0f; players[p].scanReady = 90.0f; players[p].dropReady = 150.0f; } // powers unlock a couple of minutes in (and need the tech structure)
    rebuildGrid();
    updateVision();
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
    e.pos = t.kind == UK_AIR ? pos : g_map.nearestFree(pos);
    e.lastPos = e.pos; e.prevPos = e.pos;
    e.hp = e.maxHp = t.hp;
    e.ammo = t.ammo;
    e.guardPos = e.pos;
    e.angle = rng.f(0, 6.28f);
    e.turret = e.angle;
    if (owner >= 0) players[owner].unitsBuilt++;
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
    e.maxHp = b.hp * (hasUpgrade(owner, UPG_RUGGED) ? RUGGED_HP : 1.0f);
    e.constructed = instant;
    e.progress = instant ? 1.0f : 0.0f;
    e.hp = instant ? e.maxHp : e.maxHp * 0.1f;
    e.rally = e.pos + Vec2(0, b.h * TILE * 0.5f + 40);
    e.angle = -1.5708f;
    e.turret2 = 2.4f;   // (the roof gun idles looking out over a corner)
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
        if (e.ut().kind == UK_VEH) fx.push_back({FX_WRECK, e.pos, Vec2((float)e.type, (float)std::max(0, e.owner)), 0, 16.0f, rgb(255, 255, 255), e.turret, Vec2(e.angle, 0)});
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
    if (violent) deathFx(e);
    if (e.isBuilding()) {
        g_map.setStructure(e.tx, e.ty, e.bt().w, e.bt().h, false);
        // refund queued units
        if (e.owner >= 0) for (int t : e.queue) players[e.owner].money += UNITS[t].cost;
        if (e.owner >= 0 && violent) players[e.owner].structuresLost++;
    } else if (e.kind == EK_RESOURCE) {
        g_map.setResource(e.tx, e.ty, false);
    } else if (e.owner >= 0 && violent) {
        players[e.owner].unitsLost++;
    }
    e.alive = false;
    freeList.push_back((int)(&e - &ents[0]));
}

// ------------------------------------------------------------ helpers
void Sim::emit(EventType t, int player, Sound s, Vec2 pos, const char* msg) {
    Event ev; ev.type = t; ev.player = player; ev.sound = s; ev.pos = pos; ev.msg = msg;
    events.push_back(ev);
}

float Sim::distToEntity(Vec2 p, const Entity& e) const {
    if (e.isBuilding()) {
        const BuildType& b = e.bt();
        float x0 = e.tx * TILE, y0 = e.ty * TILE, x1 = x0 + b.w * TILE, y1 = y0 + b.h * TILE;
        float dx = std::max({x0 - p.x, 0.0f, p.x - x1});
        float dy = std::max({y0 - p.y, 0.0f, p.y - y1});
        return std::sqrt(dx * dx + dy * dy);
    }
    return std::max(0.0f, dist(p, e.pos) - e.radius());
}

void Sim::rebuildGrid() {
    for (auto& c : grid) c.clear();
    for (int i = 0; i < (int)ents.size(); i++) {
        Entity& e = ents[i];
        if (!e.alive) continue;
        int cx = clampi((int)(e.pos.x / GRID_CELL), 0, GRID_W - 1), cy = clampi((int)(e.pos.y / GRID_CELL), 0, GRID_H - 1);
        grid[cy * GRID_W + cx].push_back(i);
    }
    heliList.clear();
    for (int i = 0; i < (int)ents.size(); i++) if (ents[i].alive && ents[i].isUnit() && ents[i].ut().heli) heliList.push_back(i);
}

void Sim::forEachNear(Vec2 p, float r, const std::function<void(Entity&)>& fn) {
    float rr = r + 96;  // buildings are inserted at their center; widen the cell search
    int x0 = clampi((int)((p.x - rr) / GRID_CELL), 0, GRID_W - 1), x1 = clampi((int)((p.x + rr) / GRID_CELL), 0, GRID_W - 1);
    int y0 = clampi((int)((p.y - rr) / GRID_CELL), 0, GRID_H - 1), y1 = clampi((int)((p.y + rr) / GRID_CELL), 0, GRID_H - 1);
    for (int cy = y0; cy <= y1; cy++) for (int cx = x0; cx <= x1; cx++) {
        const std::vector<int>& cell = grid[cy * GRID_W + cx];
        for (size_t k = 0; k < cell.size(); k++) {
            Entity& e = ents[cell[k]];
            if (!e.alive) continue;
            if (distToEntity(p, e) <= r) fn(e);
        }
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

bool Sim::hasBuilding(int player, int buildType) const {
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.type == buildType && e.constructed) return true;
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
    return BUILDS[buildType].role == BR_HQ && countRole(player, BR_HQ, false) >= HQ_MAX;
}
int Sim::countUnits(int player, int type) const {
    int n = 0;
    for (auto& e : ents) if (e.alive && e.isUnit() && e.owner == player && (type < 0 || e.type == type)) n++;
    return n;
}
int Sim::countBuildings(int player, int type, bool onlyConstructed) const {
    int n = 0;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && (type < 0 || e.type == type) && (!onlyConstructed || e.constructed)) n++;
    return n;
}
int Sim::countRole(int player, BuildRole role, bool onlyConstructed) const {
    int n = 0;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.bt().role == role && (!onlyConstructed || e.constructed)) n++;
    return n;
}

bool Sim::canPlace(int player, int buildType, int tx, int ty) const {
    const BuildType& b = BUILDS[buildType];
    for (int j = ty; j < ty + b.h; j++) for (int i = tx; i < tx + b.w; i++) {
        if (!g_map.buildable(i, j)) return false;
        if (!exploredRaw(player, i, j)) return false;
    }
    // keep a one tile gap around resource piles and other structures' edges so paths stay open
    for (int j = ty - 1; j <= ty + b.h; j++) for (int i = tx - 1; i <= tx + b.w; i++) {
        if (!inMap(i, j)) continue;
        if (g_map.blocked[j * MAP_W + i] & 4) return false;
    }
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
static void takeOrder(Entity& e) { e.autoTask = false; e.resumeOrder = O_IDLE; e.evadeUntil = -1; e.jetBreak = 0; }

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
    for (auto r : sel) { Entity* e = get(r); if (e && e->isUnit()) (e->isAir() ? air : ground).push_back(e); }
    std::vector<Vec2> offs;
    formationOffsets(ground, dest, offs);
    for (size_t i = 0; i < ground.size(); i++) {
        Entity& e = *ground[i];
        e.zoneR = 0; e.leashed = false; takeOrder(e); e.forceTarget = NOREF;
        Vec2 d = g_map.nearestFree(Vec2(clampf(dest.x + offs[i].x, TILE, WORLD_W - TILE), clampf(dest.y + offs[i].y, TILE, WORLD_H - TILE)), 6);
        e.order = attackMove && e.weapon() >= 0 ? O_ATTACKMOVE : O_MOVE;
        e.postOrder = O_IDLE;
        e.target = d; e.targetEnt = NOREF; e.engaged = NOREF; e.actionTimer = 0;
        requestPath(e, d);
    }
    for (auto* a : air) {
        a->zoneR = 0; a->leashed = false; takeOrder(*a); a->forceTarget = NOREF; a->loiterUntil = -1;
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
        if (!e || e == t) continue;
        bool ff = force && !players[e->owner].isAI && t->owner >= 0 && t->kind != EK_RESOURCE && !enemies(e->owner, t->owner);
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
            if (t->isBuilding() && t->owner == e->owner) { cmdAssist({r}, target); continue; }
            cmdMove({r}, t->pos, false); continue;
        }
        if (ut.role == UR_HARVESTER) {
            if (t->kind == EK_RESOURCE) { cmdHarvest({r}, target); continue; }
            cmdMove({r}, t->pos, false); continue;
        }
        if (ut.weapon < 0 || !canTarget(*e, *t)) { cmdMove({r}, t->pos, false); continue; }
        if (e->isAir() && e->ammo <= 0) { e->targetEnt = target; e->order = O_REARM; e->leashed = false; continue; }
        e->order = O_ATTACK; e->postOrder = e->zoneR > 0 ? O_GUARDAREA : O_IDLE; e->targetEnt = target; e->engaged = NOREF; e->leashed = false;
        e->repathTimer = 0;
    }
}

void Sim::cmdStop(const std::vector<Ref>& sel) {
    for (auto r : sel) {
        Entity* e = get(r);
        if (!e || !e->isUnit()) continue;
        e->zoneR = 0; e->leashed = false; takeOrder(*e); e->forceTarget = NOREF; e->loiterUntil = -1;
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
    for (auto r : sel) { Entity* e = get(r); if (e && e->isUnit() && e->ut().role == UR_COMBAT && e->weapon() >= 0) us.push_back(e); }
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
            slot = g_map.nearestFree(slot, 6);
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
    if (!d || !d->isUnit() || d->ut().role != UR_DOZER) return false;
    int p = d->owner;
    if (!buildAvailable(p, buildType) || !canPlace(p, buildType, tx, ty)) return false;
    const BuildType& b = BUILDS[buildType];
    if (!canAfford(p, b.cost)) { emit(EV_NOFUNDS, p, SND_NOFUNDS, d->pos, "Insufficient funds"); return false; }
    players[p].money -= b.cost;
    Ref site = placeBuilding(buildType, p, tx, ty, false);
    d = get(dozer);   // (placing may grow the entity array)
    if (!d) return true;
    takeOrder(*d);
    d->order = O_BUILD; d->targetEnt = site; d->engaged = NOREF; d->repathTimer = 0; d->actionTimer = 0;
    emit(EV_SOUND, p, SND_PLACE, d->pos);
    return true;
}

// Construction takes every dozer sent. A repair takes the closest few the structure has room for; the rest of a big group fan out to the
// other damaged structures around it (and only pile on when there is nothing else to mend nearby).
void Sim::cmdAssist(const std::vector<Ref>& sel, Ref building) {
    Entity* b = get(building);
    if (!b || !b->isBuilding()) return;
    std::vector<Entity*> dz;
    for (auto r : sel) {
        Entity* e = get(r);
        if (e && e->isUnit() && e->ut().role == UR_DOZER && e->owner == b->owner) dz.push_back(e);
    }
    Vec2 at = b->pos;
    std::stable_sort(dz.begin(), dz.end(), [&](const Entity* x, const Entity* y) { return dist2(x->pos, at) < dist2(y->pos, at); });
    int room = std::max(2, repairCrewCap(*b));
    for (auto* e : dz) {
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
    if (!b || !b->isBuilding() || b->owner < 0) return;
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
    for (auto& f : fallouts) if (f.t < FALLOUT_LIFE - 8.0f && dist(p, f.pos) < f.r + margin) return true;
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
    pos = Vec2(clampf(pos.x, 2.0f * TILE, WORLD_W - 2.0f * TILE), clampf(pos.y, 2.0f * TILE, WORLD_H - 2.0f * TILE));
    tech->dropTimer = time + dt.cooldown;
    Airlift a;
    a.owner = player; a.target = pos; a.attackOn = attackOn;
    a.hp = a.maxHp = AIRLIFT_HP;
    a.dir = flightTrack(pl.basePos, pos, a.pos);   // the plane comes in from beyond the map edge, at least a few hundred pixels before the target
    a.prevPos = a.pos;
    // release order: vehicles spread through the infantry, the aircraft leave last
    int vi = 0, ii = 0;
    for (int k = 0; k < DROP_INF + DROP_VEH; k++) {
        if ((k % 3 == 0 && vi < DROP_VEH) || ii >= DROP_INF) a.load.push_back(DROP_VEH_TYPES[pl.faction][vi++]);
        else a.load.push_back(DROP_INF_TYPES[pl.faction][ii++]);
    }
    for (int k = 0; k < DROP_AIR; k++) a.load.push_back(DROP_AIR_TYPES[pl.faction][k]);
    float span = 0;
    for (int t : a.load) span += (UNITS[t].kind == UK_AIR ? 0.12f : (UNITS[t].kind == UK_VEH ? 0.09f : 0.045f)) * AIRLIFT_SPEED;
    a.releaseAt = -span * 0.5f;   // the stick is centred on the target
    airlifts.push_back(a);
    emit(EV_SOUND, player, SND_AIR, a.pos);
    emit(EV_MSG, player, SND_NONE, pos, (std::string(dt.name) + " inbound").c_str());
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
        if (!e.alive || e.kind == EK_RESOURCE || e.owner < 0 || e.owner >= numPlayers || e.owner == sender || !players[e.owner].alive) continue;
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
    for (auto& n : nukes) n.t += SIM_DT;
    for (size_t i = 0; i < nukes.size();) {
        if (nukes[i].t < NUKE_FLIGHT) { i++; continue; }
        Nuke n = nukes[i];
        nukes.erase(nukes.begin() + i);
        nukeBlast(n);
    }
}

void Sim::nukeBlast(const Nuke& n) {
    float R = NUKE_RADIUS * TILE;
    std::vector<Ref> hit;
    forEachNear(n.pos, R + 80, [&](Entity& e) { if (e.kind != EK_RESOURCE) hit.push_back(refOf(e)); });
    for (auto r : hit) {
        Entity* e = get(r); if (!e) continue;
        float d = std::max(0.0f, dist(e->pos, n.pos) - e->radius() * 0.5f);
        if (d > R) continue;
        float f = d < R * 0.35f ? 1.0f : 1.0f - 0.75f * ((d - R * 0.35f) / (R * 0.65f));   // flat lethal core, then a falloff to a quarter at the rim
        bool foe = nukeHurts(n, e->owner);   // a human's nuke flattens allies and the human's own army as well
        if (e->isAir()) {   // nothing flying survives the fireball and the shock front, whoever owns it: torn apart, it falls out of the sky
            fx.push_back({FX_EXPLODE, e->pos, e->pos, 0, 0.9f, rgb(255, 214, 140), 50.0f});
            fx.push_back({FX_SMOKE, e->pos, e->pos, 0, 3.2f, rgb(40, 38, 36), 26.0f, Vec2(rng.f(-14, 14), -26)});
            for (int k = 0; k < 6; k++) fx.push_back({FX_DEBRIS, e->pos, e->pos, 0, rng.f(0.7f, 1.4f), rgb(84, 84, 86), rng.f(2, 4), Vec2(rng.f(-170, 170), rng.f(-190, -40))});
            applyDamage(*e, e->maxHp * 2.0f, n.owner, NOREF, nullptr);
            continue;
        }
        Vec2 away = (e->pos - n.pos); float l = away.len(); away = l > 1 ? away * (1.0f / l) : Vec2(1, 0);
        if (foe) {
            if (e->isBuilding()) {
                bool small = e->bt().w * e->bt().h <= NUKE_SMALL_AREA;
                bool rugged = hasUpgrade(e->owner, UPG_RUGGED);
                float dmg;
                if (rugged) dmg = e->maxHp * RUGGED_NUKE * f / RUGGED_ARMOR;                   // (after the armour) a bit over half at ground zero: it takes two warheads
                else if (small) dmg = e->maxHp * 1.6f * f;                                        // f above ~0.63 (the inner blast) brings it down
                else {
                    dmg = e->maxHp * 0.80f * f * rng.f(0.9f, 1.1f);                          // a large structure is left holding on: 80% lost at ground zero, a fifth at the rim
                    dmg = std::min(dmg, std::max(0.0f, e->hp - e->maxHp * 0.06f));           // never lethal
                }
                if (dmg > 0) applyDamage(*e, dmg, n.owner, NOREF, nullptr);
                e = get(r); if (!e) continue;
                e->disabledUntil = std::max(e->disabledUntil, time + 12.0f * f * (rugged ? 0.5f : 1.0f));   // the pulse knocks survivors offline for a while
                if (e->hp < e->maxHp * 0.5f) {                                                 // wrecked but standing: fire and smoke pour out of it
                    for (int k = 0; k < 4; k++) {
                        Vec2 p = e->pos + Vec2(rng.f(-e->radius() * 0.7f, e->radius() * 0.7f), rng.f(-e->radius() * 0.5f, e->radius() * 0.5f));
                        fx.push_back({FX_EXPLODE, p, p, -rng.f(0.0f, 1.2f), 0.7f, rgb(255, 160, 60), rng.f(14, 26)});
                        fx.push_back({FX_SMOKE, p, p, -rng.f(0.0f, 1.0f), 4.0f, rgb(52, 48, 44), rng.f(14, 24), Vec2(rng.f(-8, 8), rng.f(-32, -14))});
                    }
                }
            } else {
                applyDamage(*e, 6000.0f * f, n.owner, NOREF, nullptr);
                e = get(r); if (!e) continue;
                e->disabledUntil = std::max(e->disabledUntil, time + 10.0f * f);           // survivors are stunned by the pulse
            }
        }
        if (e->isUnit()) {   // the shockwave throws everything that survives, friend or foe
            Vec2 np = e->pos + away * (90.0f * f);
            if (g_map.passable(tileOf(np.x), tileOf(np.y))) e->pos = np;
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
    // the fireball, the shock rings, a rolling field of secondary blasts and the mushroom cloud
    fx.push_back({FX_EXPLODE, n.pos, n.pos, 0, 2.0f, rgb(255, 240, 200), R * 1.3f});
    fx.push_back({FX_RING, n.pos, n.pos, 0, 1.8f, rgb(255, 210, 140), R * 1.2f});
    fx.push_back({FX_RING, n.pos, n.pos, -0.5f, 2.4f, rgb(255, 180, 100), R * 1.7f});
    fx.push_back({FX_EMP, n.pos, n.pos, 0, 2.0f, rgb(255, 220, 160), R * 1.3f});
    fx.push_back({FX_MUSHROOM, n.pos, n.pos, 0, 18.0f, rgb(255, 255, 255), R});
    for (int k = 0; k < 44; k++) {
        float a = rng.f(0, 6.283f), r = R * std::sqrt(rng.f(0.0f, 1.0f)) * 1.05f; Vec2 p = n.pos + Vec2(std::cos(a) * r, std::sin(a) * r);
        fx.push_back({FX_EXPLODE, p, p, -rng.f(0.0f, 3.0f) - r / R * 0.8f, 1.0f, rgb(255, 150, 50), rng.f(26, 70)});
        fx.push_back({FX_SMOKE, p, p, -rng.f(0, 3.5f), 6.0f, rgb(60, 54, 50), rng.f(28, 56), Vec2(rng.f(-12, 12), rng.f(-44, -18))});
    }
    for (int k = 0; k < 40; k++) fx.push_back({FX_DEBRIS, n.pos, n.pos, -rng.f(0, 0.6f), rng.f(0.9f, 2.0f), rgb(70, 66, 62), rng.f(2, 6), Vec2(rng.f(-420, 420), rng.f(-520, -120))});
    fx.push_back({FX_FALLOUT, n.pos, n.pos, 0, FALLOUT_LIFE, rgb(120, 255, 90), R * 0.95f});
    fallouts.push_back({n.pos, R * 0.95f, 0.0f, 0.0f});
    emit(EV_SOUND, -1, SND_EXPLODE_L, n.pos);
    emit(EV_SOUND, -1, SND_EXPLODE_L, n.pos);
    emit(EV_SOUND, -1, SND_EXPLODE_L, n.pos);
}

// Radioactive fallout: for FALLOUT_LIFE seconds everything on the ground inside the zone is poisoned, friend and foe alike;
// during the first seconds fires and secondary blasts keep tearing through it.
void Sim::updateFallout() {
    for (size_t i = 0; i < fallouts.size();) {
        Fallout& f = fallouts[i];
        f.t += SIM_DT; f.tick += SIM_DT;
        if (f.t >= FALLOUT_LIFE) { fallouts.erase(fallouts.begin() + i); continue; }
        if (f.t < 12.0f && rng.f() < 0.06f) {   // chaos: random blasts and flame inside the crater
            float a = rng.f(0, 6.283f), r = f.r * std::sqrt(rng.f(0.0f, 1.0f)) * 0.9f; Vec2 p = f.pos + Vec2(std::cos(a) * r, std::sin(a) * r);
            fx.push_back({FX_EXPLODE, p, p, 0, 0.8f, rgb(255, 150, 60), rng.f(20, 44)});
            fx.push_back({FX_SMOKE, p, p, 0, 4.0f, rgb(52, 48, 44), rng.f(18, 36), Vec2(rng.f(-8, 8), rng.f(-34, -16))});
        }
        if (f.tick >= 0.5f) {
            f.tick -= 0.5f;
            float strength = f.t < FALLOUT_LIFE - 15.0f ? 1.0f : (FALLOUT_LIFE - f.t) / 15.0f;   // dies down at the end
            std::vector<Ref> in;
            forEachNear(f.pos, f.r, [&](Entity& e) { if (e.kind != EK_RESOURCE && !e.isAir() && dist(e.pos, f.pos) <= f.r) in.push_back(refOf(e)); });
            for (auto r : in) {
                Entity* e = get(r); if (!e) continue;
                if (e->isBuilding()) {   // radiation only weakens structures: a slow, non-lethal drain that stops at a tenth of their health
                    if (e->hp > e->maxHp * 0.12f) applyDamage(*e, e->maxHp * 0.0012f * strength, -1, NOREF, nullptr);
                    continue;
                }
                float dmg = std::max(7.0f, e->maxHp * 0.03f);
                applyDamage(*e, dmg * strength, -1, NOREF, nullptr);
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
        if (upg == UPG_RUGGED) { e.maxHp *= RUGGED_HP; e.hp *= RUGGED_HP; }
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
bool Sim::requestPath(Entity& e, Vec2 dest) {
    e.path.clear(); e.pathIdx = 0;
    e.repathTimer = PATH_INTERVAL;
    e.stuckTimer = 0; e.lastPos = e.pos;
    if (e.isAir()) { e.path.push_back(dest); return true; }
    bool ok = g_map.findPath(e.pos, dest, e.path);
    if (e.path.empty()) e.path.push_back(g_map.passable(tileOf(dest.x), tileOf(dest.y)) ? dest : g_map.nearestFree(dest, 4));   // (already next to a blocked spot: stop on the free tile beside it, never bump into it forever)
    return ok;
}

void Sim::moveAlong(Entity& e, float speed) {
    if (e.pathIdx >= e.path.size()) return;
    Vec2 wp = e.path[e.pathIdx];
    Vec2 d = wp - e.pos;
    float len = d.len();
    float step = speed * SIM_DT;
    if (len <= step + 0.5f) {
        if (!e.isAir() && !g_map.passable(tileOf(wp.x), tileOf(wp.y))) { requestPath(e, e.path.back()); return; }
        e.pos = wp; e.pathIdx++;
        return;
    }
    Vec2 dir = d * (1.0f / len);
    Vec2 np = e.pos + dir * step;
    if (!e.isAir()) {
        int nx = tileOf(np.x), ny = tileOf(np.y);
        if (!g_map.passable(nx, ny)) {
            // slide along axes if possible
            Vec2 alt1(e.pos.x + dir.x * step, e.pos.y), alt2(e.pos.x, e.pos.y + dir.y * step);
            if (std::abs(dir.x) > 0.01f && g_map.passable(tileOf(alt1.x), tileOf(alt1.y))) np = alt1;
            else if (std::abs(dir.y) > 0.01f && g_map.passable(tileOf(alt2.x), tileOf(alt2.y))) np = alt2;
            else { requestPath(e, e.path.back()); return; }
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
    if (t.isUnit() && t.ut().sniper && !(e.isUnit() && e.ut().kind != UK_INF)) return false;   // only vehicles and aircraft can spot a sniper
    if (e.isUnit() && e.ut().sniper && !(t.isUnit() && t.ut().kind == UK_INF && !t.ut().sniper)) return false;   // a sniper shoots infantry, never another sniper
    if (!enemies(e.owner, t.owner)) {   // a friendly is only a legal target for the one the human ordered it to hit
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

Entity* Sim::acquireTarget(Entity& e, float rangeTiles, int wpn) {
    int w = wpn >= 0 ? wpn : e.weapon();
    if (w < 0) return nullptr;
    const Weapon& wp = WEAPONS[w];
    Entity* best = nullptr; float bs = 1e9f;
    forEachNear(e.pos, rangeTiles * TILE, [&](Entity& t) {
        if (&t == &e || !enemies(e.owner, t.owner) || !canTarget(e, t, w)) return;   // (auto-acquisition never picks a friend)
        float d = distToEntity(e.pos, t) / TILE;
        if (d < wp.minRange) return;
        float score = d - 2.5f * wp.mult[t.armor()];
        if (t.isUnit() && t.weapon() >= 0) score -= 1.5f;          // shoot back at things that shoot
        if (t.isUnit() && t.ut().role == UR_HARVESTER) score -= 0.5f;
        if (t.isBuilding() && !t.constructed) score += 1.0f;
        if (t.isBuilding() && t.bt().role == BR_HQ) score += 2.0f;  // HQ is a slog; prefer softer targets
        if (wpn >= 0 && t.isAir()) score -= 1.0f;                  // a roof gun is there to keep aircraft off the base first
        if (e.isAir() && !t.isAir()) {
            if (e.ut().jet) score += std::min(8.0f, aaCover(t, e.owner) * 1.6f);
            if (e.ut().bomber) { score += std::min(7.0f, aaCover(t, e.owner) * 1.2f); if (t.isBuilding()) score -= 3.0f; }
            else if (t.isBuilding() && t.bt().role != BR_AATURRET && t.bt().weapon < 0) score += 1.5f;
        }
        if (score < bs) { bs = score; best = &t; }
    });
    return best;
}

// Best enemy for a unit guarding a zone: anything inside the circle, or anything it can already shoot from where it stands.
Entity* Sim::acquireZoneTarget(Entity& e) {
    int w = e.weapon();
    if (w < 0 || e.zoneR <= 0) return nullptr;
    const Weapon& wp = WEAPONS[w];
    Entity* best = nullptr; float bs = 1e9f;
    forEachNear(e.zone, e.zoneR + (wp.range + 1.0f) * TILE, [&](Entity& t) {
        if (&t == &e || !enemies(e.owner, t.owner) || !canTarget(e, t)) return;
        float d = distToEntity(e.pos, t) / TILE;
        if (d < wp.minRange) return;
        bool inZone = distToEntity(e.zone, t) <= e.zoneR + (e.isAir() ? 4.0f : 2.0f) * TILE;   // a little beyond the circle counts too
        if (!inZone && d > wp.range + 0.3f) return;
        float score = d - 2.5f * wp.mult[t.armor()];
        if (t.isUnit() && t.weapon() >= 0) score -= 1.5f;
        if (t.isUnit() && t.ut().role == UR_HARVESTER) score -= 0.5f;
        if (t.isBuilding() && !t.constructed) score += 1.0f;
        if (t.isBuilding() && t.bt().role == BR_HQ) score += 2.0f;
        if (e.isAir() && !t.isAir() && e.ut().jet) score += std::min(8.0f, aaCover(t, e.owner) * 1.6f);
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

void Sim::applyDamage(Entity& tgt, float dmg, int attackerOwner, Ref attacker, const Weapon* w) {
    if (!tgt.alive || tgt.kind == EK_RESOURCE || tgt.fall > 0) return;
    float m = w ? w->mult[tgt.armor()] : 1.0f;
    float real = dmg * m;
    if (tgt.isBuilding() && !tgt.constructed) real *= 1.5f;
    if (tgt.isBuilding() && hasUpgrade(tgt.owner, UPG_RUGGED)) {   // Rugged: half damage, until so much lands at once that the plating is swamped
        float load = tgt.dmgLoad / tgt.maxHp;
        real *= RUGGED_ARMOR + (1.0f - RUGGED_ARMOR) * clampf((load - RUGGED_SWAMP_LO) / (RUGGED_SWAMP_HI - RUGGED_SWAMP_LO), 0, 1);
    }
    if (tgt.isBuilding()) tgt.dmgLoad += real;
    if (attackerOwner >= 0 && enemies(attackerOwner, tgt.owner)) real *= players[attackerOwner].damageMul();
    tgt.hp -= real;
    tgt.lastDamaged = time;
    if (attacker.valid()) tgt.attacker = attacker;
    // credit the attacking unit type with the value it chewed through (for adaptive AI composition; friendly fire earns nothing)
    bool foe = attackerOwner >= 0 && tgt.owner >= 0 && enemies(attackerOwner, tgt.owner);
    if (foe) {
        const Entity* a = get(attacker);
        if (a && a->isUnit()) {
            float tv = tgt.isUnit() ? UNITS[tgt.type].cost : BUILDS[tgt.type].cost * 0.5f;
            players[attackerOwner].valueDealt[a->type] += tv * std::min(real, std::max(tgt.hp + real, 0.0f)) / tgt.maxHp;
        }
    }
    if (foe) {   // (a human's force fire on an ally raises no alarm)
        Player& pl = players[tgt.owner];
        // "base under attack" style notice, throttled per player (state lives in Player so a new game starts clean)
        if (time - pl.lastNotice > 10.0f) {
            pl.lastNotice = time;
            emit(EV_UNDER_ATTACK, tgt.owner, SND_ATTACKED, tgt.pos, tgt.isBuilding() ? "Our base is under attack" : "Our forces are under attack");
            for (int q = 0; q < numPlayers; q++)   // human teammates hear about it too (an AI ally being hit)
                if (q != tgt.owner && !players[q].isAI && players[q].team == pl.team) emit(EV_UNDER_ATTACK, q, SND_ATTACKED, tgt.pos, tgt.isBuilding() ? "Your ally's base is under attack" : "Your ally's forces are under attack");
        }
    }
    if (tgt.hp <= 0) {
        if (foe) {
            if (tgt.isBuilding()) players[attackerOwner].structuresKilled++; else players[attackerOwner].unitsKilled++;
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
        if (t.isUnit() && t.ut().sniper) { const Entity* at = get(attacker); if (at && !(at->isUnit() && at->ut().kind != UK_INF)) return; }   // an infantry or structure blast cannot find a sniper
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
static bool predPile(const Entity& e, void* ctx) { return e.kind == EK_RESOURCE && e.amount > 0 && !(ctx && ((const Sim*)ctx)->inFallout(e.pos)); }   // (haulers do not drive into radiation)
static bool predAirfield(const Entity& e, void* ctx) { int owner = *(int*)ctx; return e.alive && e.isBuilding() && e.owner == owner && e.constructed && e.bt().role == BR_AIRFIELD; }   // (alive: never the ruins of a destroyed pad)

Entity* Sim::findSupplyBuilding(Entity& h) { int o = h.owner; return nearestEntity(h.pos, 1e9f, predSupply, &o); }
Entity* Sim::findPile(Entity& h, float maxDist) {
    Entity* p = nearestEntity(h.pos, maxDist, predPile, this);
    return p ? p : nearestEntity(h.pos, maxDist, predPile, nullptr);   // only radioactive piles left: better than standing idle
}
Entity* Sim::findZonePile(Entity& h) {
    Entity* best = nullptr; float bd = 1e18f;
    for (auto& e : ents) {
        if (!e.alive || e.kind != EK_RESOURCE || e.amount <= 0) continue;
        if (dist(e.pos, h.zone) > h.zoneR + TILE * 0.5f) continue;
        if (!exploredRaw(h.owner, clampi(e.tx, 0, MAP_W - 1), clampi(e.ty, 0, MAP_H - 1))) continue;   // haulers only work what their side has seen
        float d = dist2(h.pos, e.pos);
        if (inFallout(e.pos)) d += 1e12f;   // radioactive piles come last
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
    return h.pos + Vec2((slot - 1.5f) * 30, 0);
}

// Fixed-wing aircraft (the jets and the Wraith flying wing) fly like aeroplanes: they hold an airspeed between their stall speed and their
// top speed, speed up and brake at finite rates, and turn under a g-limit (the slower they fly, the tighter they turn). They never hover:
// an aircraft waiting in the air circles, and only the final approach to the pad is flown slower than the stall speed. The Vulture
// Gunship is a helicopter and hovers.
static inline bool fixedWing(const UnitType& ut) { return ut.kind == UK_AIR && !ut.heli; }
static inline float wrapAngle(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
static inline float baseTurn(const UnitType& ut) { return ut.jet ? JET_TURN : BOMBER_TURN; }   // turn rate at top speed

float Sim::airTurnRate(const Entity& e) const {
    const UnitType& ut = e.ut();
    float base = baseTurn(ut);
    return std::min(base * AIR_TURN_MAX, base * ut.speed / std::max(1.0f, e.airspeed));
}

void Sim::airSteer(Entity& e, float want, float wantSpeed, bool keepOnMap) {
    const UnitType& ut = e.ut();
    float vmin = ut.heli ? 0.0f : ut.speed * AIR_STALL;
    if (keepOnMap) {   // about to leave the map within about one turn: pull round toward the middle, tightly
        float look = e.airspeed / airTurnRate(e) * 1.3f + 40.0f;
        Vec2 ahead = e.pos + Vec2(std::cos(e.angle), std::sin(e.angle)) * look;
        if (ahead.x < 8 || ahead.y < 8 || ahead.x > WORLD_W - 8 || ahead.y > WORLD_H - 8) {
            Vec2 c = Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f) - e.pos; want = std::atan2(c.y, c.x);
            wantSpeed = std::min(wantSpeed, ut.speed * 0.5f);
        }
    }
    wantSpeed = clampf(wantSpeed, vmin, ut.speed);
    if (ut.heli) e.airspeed = wantSpeed;
    else if (e.airspeed < wantSpeed) e.airspeed = std::min(wantSpeed, e.airspeed + ut.speed * 1.1f * SIM_DT);   // throttle up
    else e.airspeed = std::max(wantSpeed, e.airspeed - ut.speed * 1.6f * SIM_DT);                            // airbrakes
    float turn = airTurnRate(e) * SIM_DT;
    e.angle = wrapAngle(e.angle + clampf(angDiff(e.angle, want), -turn, turn));
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
    if (e.airspeed > vt) e.airspeed = std::max(vt, e.airspeed - ut.speed * 1.6f * SIM_DT);
    else e.airspeed = std::min(vt, e.airspeed + ut.speed * 1.1f * SIM_DT);
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
    float v = ut.speed * speedFrac;
    float rTurn = v / std::min(baseTurn(ut) * AIR_TURN_MAX, baseTurn(ut) * ut.speed / v);
    R = std::min(std::max(R, rTurn * 1.5f + 12.0f), WORLD_H * 0.3f);
    float m = R + rTurn * 1.3f + 48.0f;   // the whole circle stays far enough inside the map that the edge look-ahead never fires on it
    c = Vec2(clampf(c.x, m, WORLD_W - m), clampf(c.y, m, WORLD_H - m));
    Vec2 r = e.pos - c; float l = r.len();
    if (l > R * 2.5f) { airSteer(e, std::atan2(-r.y, -r.x), ut.speed * std::max(speedFrac, 0.85f)); return; }   // still on the way there
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
    float v = ut.speed;
    float rTurn = std::max(e.airspeed, 1.0f) / airTurnRate(e);
    if (e.jetBreak > 0) {
        e.jetBreak -= SIM_DT;
        want = e.angle;                                                       // extending: straight out at full power
    } else {
        if (off > 0.45f) v = ut.speed * 0.5f;                                  // corner speed: the tightest turn the airframe can pull
        else if (D < 2.5f * TILE) v = std::max(ut.speed * 0.5f, std::min(ut.speed, tv.len() * 1.1f));   // tucked in behind: do not overshoot
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
    float v = ut.speed;
    if (e.jetBreak > 0) {
        e.jetBreak -= SIM_DT;
        want = e.jetMode == 1 ? e.angle : std::atan2(-to.y, -to.x);          // extend straight out, or peel away from the target's defences
    } else if (d <= w.range && off < 0.3f) {
        if (e.cooldown <= 0 && tryFire(e, t)) { e.stuckTimer = 0; e.jetBreak = 1.25f; e.jetMode = 0; emit(EV_SOUND, e.owner, SND_JET, e.pos); }
    } else if (d < 2.0f && e.cooldown > 0) {
        e.jetBreak = 0.9f; e.jetMode = 0;                                     // arrived with the guns still cooling: peel off and come round again
    } else if (d < w.range + 5.0f) {
        if (off > 0.5f) v = ut.speed * 0.6f;                                  // pull round hard onto the target
        e.stuckTimer += SIM_DT;
        if (e.stuckTimer > 1.1f) { e.jetBreak = 0.8f; e.jetMode = 1; e.stuckTimer = 0; }
    } else e.stuckTimer = 0;
    airSteer(e, want, v);
}

// A bombing run. The aircraft steers at the target (leading a moving one), and when the middle of a stick of bombs would land on it
// it releases them one after another along the flight line, then flies straight on for a second before swinging round for another pass.
void Sim::bomberAttack(Entity& e, Entity& t) {
    const UnitType& ut = e.ut();
    Vec2 tp = t.pos;
    if (t.isUnit()) tp += (t.pos - t.prevPos) * (1.0f / SIM_DT) * 0.9f;
    Vec2 to = tp - e.pos;
    float D = to.len();
    float want = std::atan2(to.y, to.x);
    float carry = ut.speed * 0.42f;                                   // a bomb keeps the aircraft's forward speed while it falls
    float release = carry + ut.speed * 0.11f * (BOMB_STICK - 1) * 0.5f;   // distance at which the middle of the stick lands on the target
    if (e.jetBreak > 0) {
        e.jetBreak -= SIM_DT;
        want = e.angle;                                               // straight on over and past the target
    } else if (e.bombsLeft <= 0 && e.ammo > 0 && D <= release + 8 && D >= release - 40 && std::abs(angDiff(e.angle, want)) < 0.2f) {
        e.bombsLeft = std::min(BOMB_STICK, e.ammo); e.bombTimer = 0; e.jetBreak = 1.15f; e.stuckTimer = 0;
        emit(EV_SOUND, e.owner, SND_AIR, e.pos);
    } else if (D < release - 40 && e.bombsLeft <= 0) {
        e.jetBreak = 0.8f;                                            // too close or badly lined up: fly through and come round again
    } else if (e.bombsLeft <= 0 && D < release + 160) {
        // near the target but never lining up (it sits inside the bomber's turn circle): straighten out and make a fresh run
        e.stuckTimer += SIM_DT;
        if (e.stuckTimer > 1.6f) { e.jetBreak = 1.0f; e.stuckTimer = 0; }
    } else e.stuckTimer = 0;
    airSteer(e, want, ut.speed);
}

void Sim::dropBomb(Entity& e, Vec2 at) {
    Projectile p;
    p.pos = e.pos; p.prevPos = e.pos; p.vel = e.pos;      // vel keeps the release point (the ground point under the aircraft), like a shell's launch point
    p.dest = at; p.target = NOREF; p.owner = e.owner; p.shooter = refOf(e);
    { const Entity* ft = get(e.targetEnt); p.forced = ft && e.order == O_ATTACK && !enemies(e.owner, ft->owner); }   // a human-ordered run on a friendly: the bombs hurt everything near it
    p.weapon = e.ut().faction == F_CYBER ? W_BOMB_CYBER : W_BOMB_CLANKER;
    p.arcLen = 0.62f; p.arcT = 0; p.life = p.arcLen + 0.3f;
    projs.push_back(p);
}

// A bomb going off: a fireball with secondary bursts along the ground, a shockwave ring, a rising smoke column, flung debris and sparks.
void Sim::bombImpact(Projectile& p) {
    const Weapon& w = WEAPONS[p.weapon];
    Vec2 at = p.dest;
    float R = w.splash * TILE;
    splashDamage(at, w.splash, w.dmg, p.owner, p.shooter, w, NOREF, p.forced);
    bool cy = p.weapon == W_BOMB_CYBER;
    Color fire = cy ? rgb(190, 235, 255) : rgb(255, 210, 140), hot = cy ? rgb(120, 220, 255) : rgb(255, 150, 60);
    fx.push_back({FX_EXPLODE, at, at, 0, 0.75f, fire, R * 1.25f});
    fx.push_back({FX_RING, at, at, 0, 0.8f, hot, R * 2.3f});
    fx.push_back({FX_RING, at, at, -0.1f, 0.6f, fire, R * 1.4f});
    for (int k = 0; k < 4; k++) {
        float a = rng.f(0, 6.283f), r = R * rng.f(0.35f, 0.95f); Vec2 q = at + Vec2(std::cos(a) * r, std::sin(a) * r * 0.8f);
        fx.push_back({FX_EXPLODE, q, q, -rng.f(0.05f, 0.4f), 0.65f, hot, R * rng.f(0.5f, 0.85f)});
    }
    for (int k = 0; k < 3; k++) fx.push_back({FX_SMOKE, at, at, -rng.f(0.0f, 0.5f), 3.6f, rgb(54, 50, 48), R * rng.f(0.55f, 0.85f), Vec2(rng.f(-12, 12), -rng.f(26, 48))});
    for (int k = 0; k < 12; k++) fx.push_back({FX_DEBRIS, at, at, 0, rng.f(0.6f, 1.3f), rgb(80, 76, 72), rng.f(2, 5), Vec2(rng.f(-210, 210), rng.f(-300, -60))});
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
    dest = Vec2(clampf(dest.x, TILE, WORLD_W - TILE), clampf(dest.y, TILE, WORLD_H - TILE));
    if (!e.isAir()) dest = g_map.nearestFree(dest, 6);
    e.order = O_MOVE; e.target = dest; e.engaged = NOREF; e.autoTask = true; e.leashed = false; e.postOrder = O_IDLE; e.actionTimer = 0;
    e.evadeUntil = time + holdFor;
    if (!e.isAir()) requestPath(e, dest);
}

void Sim::retreatFrom(Entity& e, Vec2 threat, float tiles) {
    Vec2 away = e.pos - threat; float l = away.len();
    away = l > 1 ? away * (1.0f / l) : Vec2(-std::cos(e.angle), -std::sin(e.angle));
    Vec2 home = (players[e.owner].basePos - e.pos).norm();
    Vec2 dir = (away + home * 0.45f).norm();
    if (dir.len2() < 0.01f) dir = away;
    Vec2 dest = e.pos + dir * (tiles * TILE);
    if (inFallout(dest)) dest = e.pos + away * (tiles * TILE);
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
        float left = NUKE_FLIGHT - n.t;
        if (!hurts || n.t < 1.0f || left < 0.3f) continue;
        float R = (NUKE_RADIUS + 1.5f) * TILE;
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
    // radiation: walk out of it (aircraft fly over it unharmed)
    if (!e.isAir() && (e.order == O_IDLE || e.order == O_GUARDPOS || e.order == O_GUARDAREA) && inFallout(e.pos, TILE * 0.5f)) {
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
    if (e.disabledUntil > time) return;
    if (e.repathTimer > 0) e.repathTimer -= SIM_DT;

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
        e.alt = clampf(e.alt + (parked ? -1.4f : 1.4f) * SIM_DT, 0.0f, 1.0f);
    }

    // bombers: the stick being released leaves the aircraft one bomb at a time
    if (e.bombsLeft > 0) {
        e.bombTimer -= SIM_DT;
        if (e.bombTimer <= 0) {
            if (e.ammo > 0) { dropBomb(e, e.pos + Vec2(std::cos(e.angle), std::sin(e.angle)) * (ut.speed * 0.42f)); e.ammo--; }
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

    // Aircraft: out of ammo -> go rearm (remember the target)
    if (e.isAir() && ut.ammo > 0 && e.ammo <= 0 && e.order != O_REARM) { e.order = O_REARM; }

    // Ground units standing on a blocked tile (new structure) get nudged off
    if (!e.isAir() && (tick + (u32)(&e - &ents[0])) % 10 == 0 && !g_map.passable(tileOf(e.pos.x), tileOf(e.pos.y)))
        e.pos = g_map.nearestFree(e.pos, 14);

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
    if (!noAuto && (tick + e.gen) % 10 == 0) autonomy(e);   // (a new order is carried out this very tick: an aircraft never skips a beat)
    const Vec2 before = e.pos;
    runOrder(e, ut);
    // a fixed-wing aircraft whose order changed this tick (a target lost, a spot reached, a new order) still flies on: it never hangs in the air
    if (e.isAir() && fixedWing(ut) && e.alive && e.pos.x == before.x && e.pos.y == before.y && e.airspeed > 0 && e.disabledUntil <= time)
        airSteer(e, e.angle, std::max(e.airspeed, ut.speed * AIR_STALL));
}

// ------------------------------------------------------------ repairs
int Sim::repairCrewCap(const Entity& b) {
    int area = b.bt().w * b.bt().h;
    return area >= 12 ? 3 : (area >= 6 ? 2 : 1);   // a big structure has room for a few dozers, a turret for one
}

int Sim::repairCrew(const Entity& b, const Entity* except) const {
    int n = 0; Ref br = refOf(b);
    for (auto& u : ents)
        if (u.alive && &u != except && u.isUnit() && u.owner == b.owner && u.order == O_BUILD && u.targetEnt == br && u.ut().role == UR_DOZER) n++;
    return n;
}

// The structure a dozer should mend next. Every damaged structure is a job; the dozer weighs how far away it is against how badly it is hurt
// and how much it matters, and a job that already has its full crew is skipped (a second dozer only joins a big or badly hurt one), so a
// group of dozers spreads over the base instead of piling onto one building. Structures under fire right now and radiation are left alone.
Entity* Sim::repairJob(Entity& dz, float maxDist) {
    Entity* best = nullptr; float bs = 1e18f;
    for (auto& b : ents) {
        if (!b.alive || !b.isBuilding() || b.owner != dz.owner || !b.constructed || b.hp >= b.maxHp * 0.995f || time - b.lastDamaged < 4.0f || inFallout(b.pos)) continue;
        float d = dist(b.pos, dz.pos);
        if (d > maxDist) continue;
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

void Sim::runOrder(Entity& e, const UnitType& ut) {
    switch (e.order) {
    case O_IDLE: {
        if (ut.role == UR_HEALER) {   // drift toward the nearest wounded friendly unit
            if ((tick + e.gen) % 10 == 0) {
                Entity* best = nullptr; float bd = 14.0f * TILE;
                forEachNear(e.pos, bd, [&](Entity& t) {
                    if (&t == &e || t.owner != e.owner || !t.isUnit() || t.ut().role == UR_HEALER || t.hp >= t.maxHp * 0.98f) return;
                    float d = dist(t.pos, e.pos); if (d < bd) { bd = d; best = &t; }
                });
                if (best && bd > HEAL_RADIUS * TILE * 0.5f) { e.order = O_MOVE; e.target = g_map.nearestFree(best->pos, 6); e.targetEnt = NOREF; requestPath(e, e.target); }
            }
            break;
        }
        if (ut.role == UR_DOZER) {    // bulldozers mend damaged structures on their own when they have nothing to do, each taking its own job
            if ((tick + e.gen) % 20 == 0) {
                Entity* best = repairJob(e);
                if (best) { e.order = O_BUILD; e.targetEnt = refOf(*best); e.repathTimer = 0; e.actionTimer = 0; e.path.clear(); }
            }
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
        if (e.isAir()) {
            // a fixed-wing craft has arrived once it passes within its own turn circle of the spot; it then waits there (circling)
            bool fw = fixedWing(ut);
            Vec2 d = (!fw && dist(e.pos, e.target) < 6.0f * TILE ? heliSpot(e, e.target) : e.target) - e.pos;   // (a group of helicopters stops side by side)
            float arrive = fw ? std::max(48.0f, 1.3f * e.airspeed / airTurnRate(e)) : 6.0f;
            if (d.len() < arrive) {
                e.order = O_IDLE; e.postOrder = O_IDLE;
                if (!e.autoTask) { e.loiter = e.target; e.loiterUntil = time + AIR_LOITER_TIME; }
                e.autoTask = false;
                break;
            }
            if (fw) airSteer(e, std::atan2(d.y, d.x), d.len() < 5 * TILE ? ut.speed * 0.7f : ut.speed);
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
            if (e.ammo <= 0 && ut.ammo > 0) { e.order = O_REARM; break; }
            if (e.autoTask && e.loiterUntil > time) e.loiterUntil = std::max(e.loiterUntil, time + 4.0f);   // a fight picked up while waiting somewhere keeps it there
            if (ut.jet) { jetAttack(e, *t); break; }
            if (ut.bomber && !t->isAir()) { bomberAttack(e, *t); break; }
            if (fixedWing(ut)) { airDogfight(e, *t); break; }
            Vec2 dv = t->pos - e.pos;
            if (d > w.range - 0.4f) { e.pos += dv.norm() * std::min(dv.len(), ut.speed * SIM_DT); e.angle = std::atan2(dv.y, dv.x); }
            else { e.angle = std::atan2(dv.y, dv.x); tryFire(e, *t); }
            break;
        }
        if (d < w.minRange) {
            // back away from the target
            Vec2 away = (e.pos - t->pos).norm();
            Vec2 dest = g_map.nearestFree(e.pos + away * TILE * 2.0f, 4);
            if (e.repathTimer <= 0 || e.pathIdx >= e.path.size()) requestPath(e, dest);
            moveAlong(e, ut.speed);
        } else if (d <= w.range) {
            e.path.clear();
            tryFire(e, *t);
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
        if (time - e.lastDamaged < 1.0f && e.lastDamaged > 0 && e.hp < e.maxHp * 0.5f) { e.order = O_MOVE; e.target = g_map.nearestFree(players[e.owner].basePos + Vec2(0, 60), 6); e.targetEnt = NOREF; requestPath(e, e.target); break; }
        Entity* b = get(e.targetEnt);
        if (!b || !b->isBuilding() || b->owner != e.owner || (b->constructed && b->hp >= b->maxHp)) {
            e.order = O_IDLE; e.targetEnt = NOREF; e.guardPos = e.pos;
            if (b && b->constructed && b->owner == e.owner) {   // a repair is done: move straight on to the next job close by
                Entity* next = repairJob(e, 18.0f * TILE);
                if (next) { e.order = O_BUILD; e.targetEnt = refOf(*next); e.repathTimer = 0; e.actionTimer = 0; e.path.clear(); }
            }
            break;
        }
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
                if (tick % 6 == 0) fx.push_back({FX_SPARK, b->pos + Vec2(rng.f(-bt.w * 14.0f, bt.w * 14.0f), rng.f(-bt.h * 14.0f, bt.h * 14.0f)), Vec2(), 0, 0.2f, rgb(255, 230, 150), 3});
            }
        } else {
            if (e.pathIdx >= e.path.size()) requestPath(e, g_map.nearestFree(b->pos + Vec2(0, b->bt().h * TILE * 0.5f + 12), 6));
            Vec2 before = e.pos;
            moveAlong(e, ut.speed);
            if (dist(e.pos, before) < 0.05f) e.actionTimer += SIM_DT; else e.actionTimer = 0;   // time spent unable to advance
            if (e.actionTimer > 14.0f) { e.order = O_IDLE; e.targetEnt = NOREF; e.guardPos = e.pos; e.path.clear(); e.actionTimer = 0; break; }   // the structure cannot be reached: give up
            e.stuckTimer += SIM_DT;
            if (e.stuckTimer > 2.0f) { if (dist(e.pos, e.lastPos) < 8) requestPath(e, g_map.nearestFree(b->pos + Vec2(rng.f(-1, 1) * b->bt().w * TILE * 0.8f, rng.f(-1, 1) * b->bt().h * TILE * 0.8f), 6)); e.stuckTimer = 0; e.lastPos = e.pos; }
        }
        break;
    }
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
    emit(EV_BUILD_DONE, b.owner, SND_BUILD_DONE, b.pos, b.bt().name);
    b.actionTimer = 0;
    if (b.bt().role == BR_NUKE) b.actionTimer = time + 60.0f;   // arming time for the first warhead
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
    Vec2 at = ut.kind == UK_AIR ? b.pos : unitExit(b);
    Ref bref = refOf(b);
    Vec2 rally = b.rally; bool hasRally = b.hasRally; int owner = b.owner;
    Ref r = spawnUnit(type, owner, at);          // may reallocate ents: b must not be touched afterwards
    Entity& u = ents[r.idx];
    if (ut.kind == UK_AIR) { u.home = bref; u.order = O_IDLE; if (hasRally) cmdMove({r}, rally, false); }
    else if (ut.role == UR_HARVESTER) {
        Entity* p = findPile(u, 60 * TILE);
        if (p) cmdHarvest({r}, refOf(*p));
    } else if (hasRally) cmdMove({r}, rally, false);
    else cmdMove({r}, rally + Vec2(rng.f(-20, 20), rng.f(-10, 10)), false);
    emit(EV_UNIT_READY, owner, SND_UNIT_READY, u.pos, ut.name);
}

void Sim::updateBuilding(Entity& b) {
    if (b.cooldown > 0) b.cooldown -= SIM_DT;
    if (b.cooldown2 > 0) b.cooldown2 -= SIM_DT;
    if (b.dmgLoad > 0) b.dmgLoad *= 0.975f;   // (a time constant of two seconds)
    if (!b.constructed) return;
    if (b.disabledUntil > time) return;
    const BuildType& bt = b.bt();
    Player& pl = players[b.owner];
    bool powered = !(pl.lowPower() && bt.power < 0);
    if (pl.upg[UPG_REPAIR] && b.hp < b.maxHp && !inFallout(b.pos)) {   // Self-Repair: full rate once out of combat, a trickle under fire
        float rate = SELF_REPAIR_RATE * (time - b.lastDamaged >= SELF_REPAIR_DELAY ? 1.0f : SELF_REPAIR_HOT);
        b.hp = std::min(b.maxHp, b.hp + b.maxHp * rate * SIM_DT);
        if ((tick + b.gen) % 40 == 0) fx.push_back({FX_SPARK, b.pos + Vec2(rng.f(-bt.w * 12.0f, bt.w * 12.0f), rng.f(-bt.h * 12.0f, bt.h * 12.0f)), Vec2(), 0, 0.3f, pl.faction == F_CYBER ? rgb(120, 236, 255) : rgb(255, 214, 120), 3});
    }
    if (pl.upg[UPG_GUNS]) updateRoofGun(b, powered);
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
            float rate = (pl.lowPower() ? 0.5f : 1.0f) / ut.buildTime;
            rate *= pl.buildMul();
            b.queueProgress += SIM_DT * rate;
            if (b.queueProgress >= 1.0f) spawnFromQueue(b);
        }
    }
}

// ------------------------------------------------------------ Defense Guns: every structure's roof laser / machine gun
int Sim::roofGun(const Entity& b) const {
    if (!b.isBuilding() || !b.constructed || !hasUpgrade(b.owner, UPG_GUNS)) return -1;
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
        if (!e.alive || e.owner < 0) continue;
        // allies share what they see: the sight of every unit and structure is written into the map of each member of its team
        Player* seers[MAX_PLAYERS]; int ns = 0;
        for (int p = 0; p < numPlayers; p++) if (p == e.owner || players[p].team == players[e.owner].team) seers[ns++] = &players[p];
        float s = e.sight();
        int cx = tileOf(e.pos.x), cy = tileOf(e.pos.y);
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

void Sim::separateUnits() {
    for (int i = 0; i < (int)ents.size(); i++) {
        Entity& a = ents[i];
        if (!a.alive || !a.isUnit() || a.fall > 0) continue;
        bool air = a.isAir();
        float ra = a.radius();
        int cx = clampi((int)(a.pos.x / GRID_CELL), 0, GRID_W - 1), cy = clampi((int)(a.pos.y / GRID_CELL), 0, GRID_H - 1);
        Vec2 push;
        for (int gy = std::max(0, cy - 1); gy <= std::min(GRID_H - 1, cy + 1); gy++)
        for (int gx = std::max(0, cx - 1); gx <= std::min(GRID_W - 1, cx + 1); gx++) {
            for (int j : grid[gy * GRID_W + gx]) {
                if (j == i) continue;
                Entity& b = ents[j];
                if (!b.alive || !b.isUnit() || b.isAir() != air || b.fall > 0) continue;
                if (!air && (a.squeezeUntil > time || b.squeezeUntil > time)) continue;   // a jammed unit slips past
                float rr = ra + b.radius();
                if (air && a.ut().heli && b.ut().heli) rr = HELI_SPACE;   // helicopters keep a small gap between their hulls
                Vec2 d = a.pos - b.pos;
                float l2 = d.len2();
                if (l2 >= rr * rr || l2 < 1e-4f) { if (l2 < 1e-4f) push += Vec2(rng.f(-1, 1), rng.f(-1, 1)); continue; }
                float l = std::sqrt(l2);
                float overlap = (rr - l) / rr;
                float wgt = ((b.order == O_IDLE || b.order == O_GUARDAREA) && a.order != O_IDLE && a.order != O_GUARDAREA) ? 0.4f : 0.8f;  // movers push idlers aside
                push += d * (overlap * wgt * 9.0f / l);
            }
        }
        if (air && !a.ut().heli) {   // planes in the air only drift apart sideways: the push never slows or stops a fixed-wing craft
            Vec2 n(-std::sin(a.angle), std::cos(a.angle));
            push = n * (push.x * n.x + push.y * n.y);
        }
        if (push.len2() > 0.01f) {
            Vec2 np = a.pos + push * SIM_DT * 14.0f;
            if (air || g_map.passable(tileOf(np.x), tileOf(np.y))) a.pos = np;
        }
        a.pos.x = clampf(a.pos.x, 8, WORLD_W - 8); a.pos.y = clampf(a.pos.y, 8, WORLD_H - 8);
    }
}

void Sim::checkVictory() {
    bool anyDied = false;
    for (int p = 0; p < numPlayers; p++) {
        Player& pl = players[p];
        if (!pl.alive) continue;
        bool any = false;
        for (auto& e : ents) if (e.alive && e.owner == p && e.kind != EK_RESOURCE) { any = true; break; }
        if (!any) { pl.alive = false; anyDied = true; emit(EV_PLAYER_DEAD, p, SND_NONE, pl.basePos, FACTION_NAME[pl.faction]); }
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
    // index loop: producing a unit can grow (reallocate) the entity array mid-iteration
    for (size_t i = 0; i < ents.size(); i++) {
        Entity& e = ents[i];
        if (!e.alive) continue;
        if (e.isUnit()) updateUnit(e);
        else if (e.isBuilding()) updateBuilding(e);
    }
    updateProjectiles();
    updateStorms();
    updateNukes();
    updateAirlifts();
    updateAidDrops();
    updateFallout();
    separateUnits();
    updateFx();
    if (tick % 4 == 0) updateVision();
    if (tick % 10 == 0) updatePower();
    if (tick % 5 == 0) updateResearch();
    if (tick % 20 == 0) checkVictory();
    // low-power notices
    for (int p = 0; p < numPlayers; p++) {
        bool low = players[p].lowPower();
        if (low && !players[p].wasLowPower) emit(EV_LOWPOWER, p, SND_LOWPOWER, players[p].basePos, "Low power");
        players[p].wasLowPower = low;
    }
}
