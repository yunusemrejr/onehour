#include "sim.h"

Sim g_sim;

static const float BUILD_REACH = 26.0f;     // px from structure edge for dozers
static const float HARVEST_REACH = 34.0f;   // px from the pile's edge: a hauler parked on a diagonal neighbour tile is about 29 px away
static const float PATH_INTERVAL = 0.6f;
static const int AIRFIELD_CAP = 4;
static const float EMP_DURATION = 8.0f;

// ------------------------------------------------------------ init
void Sim::init(int nPlayers, const Faction* factions, const bool* isAI, const int* difficulties, const int* teams, u64 seed) {
    ents.clear(); freeList.clear(); projs.clear(); fx.clear(); events.clear(); storms.clear(); nukes.clear(); airlifts.clear(); fallouts.clear();
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
        if (pl.isAI && pl.difficulty == 3) pl.money += 2000;
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
    e.maxHp = b.hp;
    e.constructed = instant;
    e.progress = instant ? 1.0f : 0.0f;
    e.hp = instant ? b.hp : b.hp * 0.1f;
    e.rally = e.pos + Vec2(0, b.h * TILE * 0.5f + 40);
    e.angle = -1.5708f;
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
        e.zoneR = 0; e.leashed = false;
        Vec2 d = g_map.nearestFree(Vec2(clampf(dest.x + offs[i].x, TILE, WORLD_W - TILE), clampf(dest.y + offs[i].y, TILE, WORLD_H - TILE)), 6);
        e.order = attackMove && e.weapon() >= 0 ? O_ATTACKMOVE : O_MOVE;
        e.postOrder = O_IDLE;
        e.target = d; e.targetEnt = NOREF; e.engaged = NOREF; e.actionTimer = 0;
        requestPath(e, d);
    }
    for (auto* a : air) {
        a->zoneR = 0; a->leashed = false;
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
        if (e->isBuilding()) {   // a turret or battery can be turned on a friendly target too
            if (ff && e->constructed && e->bt().weapon >= 0) { e->forceTarget = target; e->engaged = target; }
            continue;
        }
        if (!e->isUnit()) continue;
        e->forceTarget = ff ? target : NOREF;
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
        e->zoneR = 0; e->leashed = false;
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
        e->zoneR = 0;
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
        e.zone = center; e.zoneR = radius; e.leashed = false;
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
    d->order = O_BUILD; d->targetEnt = site; d->engaged = NOREF; d->repathTimer = 0; d->actionTimer = 0;
    emit(EV_SOUND, p, SND_PLACE, d->pos);
    return true;
}

void Sim::cmdAssist(const std::vector<Ref>& sel, Ref building) {
    Entity* b = get(building);
    if (!b || !b->isBuilding()) return;
    for (auto r : sel) {
        Entity* e = get(r);
        if (!e || !e->isUnit() || e->ut().role != UR_DOZER || e->owner != b->owner) continue;
        e->order = O_BUILD; e->targetEnt = building; e->repathTimer = 0; e->actionTimer = 0;
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
    if (!hasRole(player, BR_TECH)) return false;
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
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.bt().role == BR_TECH && time >= e.dropTimer) n++;
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
// A cargo plane crosses the map along the line from the owner's base through the target and releases its load while it flies over:
// infantry and vehicles hang under parachutes for a few seconds (they cannot be hit or act until they land), the aircraft simply
// leave the cargo bay and take up guard over the zone. Anti-air that covers the plane's track can shoot it down, and the load with it.
bool Sim::cmdParadrop(int player, Vec2 pos, bool attackOn) {
    if (player < 0 || player >= numPlayers) return false;
    Player& pl = players[player];
    if (!pl.alive || time < pl.dropReady) return false;
    Entity* tech = nullptr;   // like the nuke ramps: every Data Center / Arms Lab has its own cooldown, so more of them means more drops
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.bt().role == BR_TECH && time >= e.dropTimer) { tech = &e; break; }
    if (!tech) return false;
    const DropType& dt = DROPS[pl.faction];
    pos = Vec2(clampf(pos.x, 2.0f * TILE, WORLD_W - 2.0f * TILE), clampf(pos.y, 2.0f * TILE, WORLD_H - 2.0f * TILE));
    tech->dropTimer = time + dt.cooldown;
    Airlift a;
    a.owner = player; a.target = pos; a.attackOn = attackOn;
    a.hp = a.maxHp = AIRLIFT_HP;
    Vec2 d = pos - pl.basePos;
    Vec2 dir = d.len() > 6.0f * TILE ? d.norm() : (Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f) - pos).norm();
    if (dir.len2() < 0.5f) dir = Vec2(1, 0);
    a.dir = dir;
    // the plane comes in from beyond the map edge, at least a few hundred pixels before the target
    float s = 1e9f;
    if (dir.x > 1e-4f) s = std::min(s, (pos.x + 160.0f) / dir.x); else if (dir.x < -1e-4f) s = std::min(s, (pos.x - WORLD_W - 160.0f) / dir.x);
    if (dir.y > 1e-4f) s = std::min(s, (pos.y + 160.0f) / dir.y); else if (dir.y < -1e-4f) s = std::min(s, (pos.y - WORLD_H - 160.0f) / dir.y);
    s = clampf(s, 560.0f, 3200.0f);
    a.pos = a.prevPos = pos - dir * s;
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
        e.alt = 1;
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

// What a detonation does (all of it to enemies of the launcher; friendly units are thrown but never hurt):
//  * ground units: a lethal core, then falling damage out to the rim (they collapse, as ever)
//  * aircraft: those inside the fireball fall out of the sky, those in the shock ring beyond it are badly mauled
//  * small structures (footprint of NUKE_SMALL_AREA tiles or less: turrets, batteries, reactors, barracks, income structures) collapse
//    in the inner blast; every large structure survives with heavy damage that tapers with the distance and is never lethal
static const int NUKE_SMALL_AREA = 6;
static const float NUKE_AIR_KILL = 0.7f;   // fraction of the blast radius that is "in direct contact" with the fireball for aircraft

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
        bool foe = enemies(n.owner, e->owner) || (n.force && !players[n.owner].isAI && e->owner >= 0);   // a nuke the human aimed at friendly ground flattens friendly ground
        if (e->isAir()) {
            if (!foe) continue;
            float core = R * NUKE_AIR_KILL;
            if (d <= core) {   // caught in the fireball: the aircraft is torn apart and falls
                fx.push_back({FX_EXPLODE, e->pos, e->pos, 0, 0.9f, rgb(255, 214, 140), 50.0f});
                fx.push_back({FX_SMOKE, e->pos, e->pos, 0, 3.2f, rgb(40, 38, 36), 26.0f, Vec2(rng.f(-14, 14), -26)});
                for (int k = 0; k < 6; k++) fx.push_back({FX_DEBRIS, e->pos, e->pos, 0, rng.f(0.7f, 1.4f), rgb(84, 84, 86), rng.f(2, 4), Vec2(rng.f(-170, 170), rng.f(-190, -40))});
                applyDamage(*e, e->maxHp * 2.0f, n.owner, NOREF, nullptr);
            } else {           // the shock ring beyond it mauls what it does not destroy
                float k = 1.0f - (d - core) / std::max(1.0f, R - core);
                applyDamage(*e, e->maxHp * 0.62f * k, n.owner, NOREF, nullptr);
            }
            continue;
        }
        Vec2 away = (e->pos - n.pos); float l = away.len(); away = l > 1 ? away * (1.0f / l) : Vec2(1, 0);
        if (foe) {
            if (e->isBuilding()) {
                bool small = e->bt().w * e->bt().h <= NUKE_SMALL_AREA;
                float dmg;
                if (small) dmg = e->maxHp * 1.6f * f;                                        // f above ~0.63 (the inner blast) brings it down
                else {
                    dmg = e->maxHp * 0.80f * f * rng.f(0.9f, 1.1f);                          // a large structure is left holding on: 80% lost at ground zero, a fifth at the rim
                    dmg = std::min(dmg, std::max(0.0f, e->hp - e->maxHp * 0.06f));           // never lethal
                }
                if (dmg > 0) applyDamage(*e, dmg, n.owner, NOREF, nullptr);
                e = get(r); if (!e) continue;
                e->disabledUntil = std::max(e->disabledUntil, time + 12.0f * f);            // the pulse knocks survivors offline for a while
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
    return !pl.advTech && !pl.researching && hasRole(player, BR_TECH);
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
    if (time < pl.scanReady || !hasRole(player, BR_TECH)) return false;
    const ScanType& sc = SCANS[pl.faction];
    pl.scanReady = time + sc.cooldown;
    pl.revealUntil = time + sc.duration;
    fx.push_back({FX_EMP, pl.basePos, pl.basePos, 0, 1.6f, pl.faction == F_CYBER ? rgb(110, 230, 255) : rgb(222, 178, 60), (float)WORLD_W * 0.6f});
    emit(EV_SOUND, player, SND_AIR, pl.basePos);
    emit(EV_MSG, player, SND_NONE, pl.basePos, (std::string(sc.name) + ": the whole map is visible").c_str());
    return true;
}

void Sim::updateResearch() {
    for (int p = 0; p < numPlayers; p++) {
        Player& pl = players[p];
        if (!pl.researching) continue;
        if (!hasRole(p, BR_TECH)) continue;            // paused while the tech structure is down
        pl.researchProgress += 5 * SIM_DT * (pl.lowPower() ? 0.5f : 1.0f) / PROGRAMS[pl.faction].time;   // called every 5th tick
        if (pl.researchProgress >= 1.0f) {
            pl.researching = false; pl.advTech = true; pl.researchProgress = 1;
            emit(EV_MSG, p, SND_BUILD_DONE, pl.basePos, (std::string(PROGRAMS[pl.faction].name) + " complete: " + PROGRAMS[pl.faction].desc).c_str());
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
    if (e.path.empty()) e.path.push_back(dest);
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
bool Sim::canTarget(const Entity& e, const Entity& t) const {
    if (!t.alive || t.kind == EK_RESOURCE) return false;
    if (t.fall > 0) return false;   // still under the parachute
    if (t.isUnit() && t.ut().sniper && !(e.isUnit() && e.ut().kind != UK_INF)) return false;   // only vehicles and aircraft can spot a sniper
    if (e.isUnit() && e.ut().sniper && !(t.isUnit() && t.ut().kind == UK_INF && !t.ut().sniper)) return false;   // a sniper shoots infantry, never another sniper
    if (!enemies(e.owner, t.owner)) {   // a friendly is only a legal target for the one the human ordered it to hit
        if (t.owner < 0 || !e.forceTarget.valid() || players[e.owner].isAI) return false;
        if (get(e.forceTarget) != &t) return false;
    }
    int w = e.weapon();
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

Entity* Sim::acquireTarget(Entity& e, float rangeTiles) {
    int w = e.weapon();
    if (w < 0) return nullptr;
    const Weapon& wp = WEAPONS[w];
    Entity* best = nullptr; float bs = 1e9f;
    forEachNear(e.pos, rangeTiles * TILE, [&](Entity& t) {
        if (&t == &e || !enemies(e.owner, t.owner) || !canTarget(e, t)) return;   // (auto-acquisition never picks a friend)
        float d = distToEntity(e.pos, t) / TILE;
        if (d < wp.minRange) return;
        float score = d - 2.5f * wp.mult[t.armor()];
        if (t.isUnit() && t.weapon() >= 0) score -= 1.5f;          // shoot back at things that shoot
        if (t.isUnit() && t.ut().role == UR_HARVESTER) score -= 0.5f;
        if (t.isBuilding() && !t.constructed) score += 1.0f;
        if (t.isBuilding() && t.bt().role == BR_HQ) score += 2.0f;  // HQ is a slog; prefer softer targets
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
        bool inZone = distToEntity(e.zone, t) <= e.zoneR;
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

void Sim::fireWeapon(Entity& e, Entity& tgt, const Weapon& w) {
    Vec2 from = e.pos;
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
            if (forced ? (&t == &e || !t.alive || t.owner < 0 || t.kind == EK_RESOURCE || t.fall > 0 || (t.isAir() ? !w.air : !w.ground)) : !canTarget(e, t)) return;
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
    tgt.hp -= real;
    tgt.lastDamaged = time;
    if (attacker.valid()) tgt.attacker = attacker;
    // credit the attacking unit type with the value it chewed through (for adaptive AI composition)
    if (attackerOwner >= 0 && tgt.owner >= 0 && attackerOwner != tgt.owner) {
        const Entity* a = get(attacker);
        if (a && a->isUnit()) {
            float tv = tgt.isUnit() ? UNITS[tgt.type].cost : BUILDS[tgt.type].cost * 0.5f;
            players[attackerOwner].valueDealt[a->type] += tv * std::min(real, std::max(tgt.hp + real, 0.0f)) / tgt.maxHp;
        }
    }
    if (tgt.owner >= 0 && attackerOwner >= 0 && attackerOwner != tgt.owner) {
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
        if (attackerOwner >= 0 && attackerOwner != tgt.owner) {
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
        if (t.kind == EK_RESOURCE || !(enemies(owner, t.owner) || (forced && t.owner >= 0))) return;
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
static bool predPile(const Entity& e, void*) { return e.kind == EK_RESOURCE && e.amount > 0; }
static bool predAirfield(const Entity& e, void* ctx) { int owner = *(int*)ctx; return e.isBuilding() && e.owner == owner && e.constructed && e.bt().role == BR_AIRFIELD; }

Entity* Sim::findSupplyBuilding(Entity& h) { int o = h.owner; return nearestEntity(h.pos, 1e9f, predSupply, &o); }
Entity* Sim::findPile(Entity& h, float maxDist) { return nearestEntity(h.pos, maxDist, predPile, nullptr); }
Entity* Sim::findZonePile(Entity& h) {
    Entity* best = nullptr; float bd = 1e18f;
    for (auto& e : ents) {
        if (!e.alive || e.kind != EK_RESOURCE || e.amount <= 0) continue;
        if (dist(e.pos, h.zone) > h.zoneR + TILE * 0.5f) continue;
        if (!exploredRaw(h.owner, clampi(e.tx, 0, MAP_W - 1), clampi(e.ty, 0, MAP_H - 1))) continue;   // haulers only work what their side has seen
        float d = dist2(h.pos, e.pos);
        if (d < bd) { bd = d; best = &e; }
    }
    return best;
}
Entity* Sim::findAirfield(Entity& a) {
    int o = a.owner;
    // prefer an airfield with free capacity
    Entity* best = nullptr; float bd = 1e18f;
    for (auto& e : ents) {
        if (!predAirfield(e, &o)) continue;
        int n = 0;
        for (auto& u : ents) if (u.alive && u.isUnit() && u.isAir() && u.home == refOf(e) && &u != &a) n++;
        if (n >= AIRFIELD_CAP) continue;
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
    int slot = ((int)(&e - &ents[0])) % AIRFIELD_CAP;
    return h.pos + Vec2((slot - 1.5f) * 30, 0);
}

void Sim::flyTo(Entity& e, Vec2 dest, float speed) {
    Vec2 d = dest - e.pos;
    float l = d.len();
    if (!e.ut().jet) {
        if (l > 3) { e.pos += d.norm() * std::min(l, speed * SIM_DT); e.angle = std::atan2(d.y, d.x); }
        return;
    }
    // a fixed-wing craft cannot turn on the spot: swing at JET_TURN and bleed speed in proportion to the distance left,
    // which keeps the turn circle (speed / JET_TURN) under half the distance, so it always spirals in instead of orbiting the spot
    if (l < 4) return;
    float want = std::atan2(d.y, d.x);
    float v = std::max(36.0f, std::min(speed, l * 1.6f));
    float turn = JET_TURN * SIM_DT * (l < 120 ? 2.0f : 1.0f);
    e.angle += clampf(angDiff(e.angle, want), -turn, turn);
    if (l < 14) { float a = angDiff(e.angle, want); e.angle += a * 0.5f; }
    Vec2 step(std::cos(e.angle) * v * SIM_DT, std::sin(e.angle) * v * SIM_DT);
    e.pos += step.len() >= l ? d : step;
}

void Sim::jetAttack(Entity& e, Entity& t) {
    const UnitType& ut = e.ut();
    const Weapon& w = WEAPONS[ut.weapon];
    Vec2 to = t.pos - e.pos;
    float d = distToEntity(e.pos, t) / TILE;
    float want = std::atan2(to.y, to.x);
    if (e.jetBreak > 0) {
        e.jetBreak -= SIM_DT;
        want = std::atan2(-to.y, -to.x);                   // peel away: turn back out of range instead of flying through the defences
    } else if (d <= w.range && std::abs(angDiff(e.angle, want)) < (t.isAir() ? 0.9f : 0.3f)) {   // (a dogfight is fought on the turn: a fighter fires inside a wide cone at another aircraft)
        if (e.cooldown <= 0 && tryFire(e, t)) { e.stuckTimer = 0; e.jetBreak = t.isAir() ? 0.5f : 1.25f; emit(EV_SOUND, e.owner, SND_JET, e.pos); }
    } else if (d < 2.0f && e.cooldown > 0) {
        e.jetBreak = 0.9f;                                 // arrived with the guns still cooling: peel off and come round again
    } else if (d < w.range + 5.0f) {
        // close to the target but not lined up: a target inside the jet's own turn circle would be circled forever, so give up
        // on the turn after a moment, fly out straight and make a fresh run from far enough away
        e.stuckTimer += SIM_DT;
        if (e.stuckTimer > 1.1f) { e.jetBreak = 1.0f; e.stuckTimer = 0; }
    } else e.stuckTimer = 0;
    // stay over the map: near an edge, the breakaway curves back toward the middle
    const float edge = 3.5f * TILE;
    if (e.pos.x < edge || e.pos.y < edge || e.pos.x > WORLD_W - edge || e.pos.y > WORLD_H - edge) {
        Vec2 c = Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f) - e.pos;
        want = std::atan2(c.y, c.x);
    }
    float turn = JET_TURN * SIM_DT;
    e.angle += clampf(angDiff(e.angle, want), -turn, turn);
    e.pos += Vec2(std::cos(e.angle), std::sin(e.angle)) * (ut.speed * SIM_DT);
    e.pos.x = clampf(e.pos.x, 8, WORLD_W - 8); e.pos.y = clampf(e.pos.y, 8, WORLD_H - 8);
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
    const float edge = 3.0f * TILE;
    if (e.pos.x < edge || e.pos.y < edge || e.pos.x > WORLD_W - edge || e.pos.y > WORLD_H - edge) {
        Vec2 c = Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f) - e.pos;
        want = std::atan2(c.y, c.x);
    }
    float turn = BOMBER_TURN * SIM_DT;
    e.angle += clampf(angDiff(e.angle, want), -turn, turn);
    e.pos += Vec2(std::cos(e.angle), std::sin(e.angle)) * (ut.speed * SIM_DT);
    e.pos.x = clampf(e.pos.x, 8, WORLD_W - 8); e.pos.y = clampf(e.pos.y, 8, WORLD_H - 8);
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
        if (e.dropGo) { e.dropGo = false; e.order = O_ATTACKMOVE; e.postOrder = O_IDLE; e.target = e.dropGoal; requestPath(e, e.dropGoal); }
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
            if (t && canTarget(e, *t)) fireWeapon(e, *t, WEAPONS[ut.weapon]);
            e.burstLeft--; e.burstTimer = ut.jet ? 0.07f : 0.14f;
        }
    }

    // Aircraft: out of ammo -> go rearm (remember the target)
    if (e.isAir() && ut.ammo > 0 && e.ammo <= 0 && e.order != O_REARM) { e.order = O_REARM; }

    // Ground units standing on a blocked tile (new structure) get nudged off
    if (!e.isAir() && (tick + (u32)(&e - &ents[0])) % 10 == 0 && !g_map.passable(tileOf(e.pos.x), tileOf(e.pos.y)))
        e.pos = g_map.nearestFree(e.pos, 14);

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
        if (ut.role == UR_DOZER) {    // bulldozers mend damaged structures on their own when they have nothing to do
            if ((tick + e.gen) % 20 == 0) {
                Entity* best = nullptr; float bd = 1e18f;
                for (auto& b : ents) {
                    if (!b.alive || !b.isBuilding() || b.owner != e.owner || !b.constructed || b.hp >= b.maxHp * 0.995f || time - b.lastDamaged < 4.0f || inFallout(b.pos)) continue;   // (idle dozers do not drive into radiation)
                    float d = dist2(b.pos, e.pos); if (d < bd) { bd = d; best = &b; }
                }
                if (best) { e.order = O_BUILD; e.targetEnt = refOf(*best); e.repathTimer = 0; e.actionTimer = 0; e.path.clear(); }
            }
            break;
        }
        if (ut.weapon < 0) break;
        if (e.isAir()) {
            // hover home
            Entity* h = get(e.home);
            if (!h) { h = findAirfield(e); if (h) e.home = refOf(*h); }
            if (h) {
                int slot = ((int)(&e - &ents[0])) % AIRFIELD_CAP;
                Vec2 pad = padSlot(*h, e);
                if (h->hasRally && e.ammo >= ut.ammo) pad = h->rally + Vec2((slot - 1.5f) * 34, 0);   // loaded aircraft wait at the rally point, empty ones return to the pad
                Vec2 d = pad - e.pos;
                flyTo(e, pad, ut.speed * 0.6f);
                if (ut.jet && d.len() < 14 && pad.y == padSlot(*h, e).y && pad.x == padSlot(*h, e).x) e.angle += angDiff(e.angle, -1.5708f) * 0.2f;   // parked jets face up the runway
                if (e.ammo < ut.ammo && d.len() < 8) { e.actionTimer += SIM_DT; if (e.actionTimer >= REARM_TIME) { e.actionTimer = 0; e.ammo++; } }
            }
            if (e.ammo > 0) {
                Entity* t = nullptr;
                if ((tick + e.gen) % 6 == 0) t = acquireTarget(e, ut.sight);
                if (t) { e.order = O_ATTACK; e.postOrder = O_IDLE; e.targetEnt = refOf(*t); }
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
                    if (t) { e.order = O_ATTACK; e.postOrder = O_GUARDPOS; e.targetEnt = refOf(*t); e.repathTimer = 0; t = nullptr; }
                }
            }
        }
        if (t) tryFire(e, *t);
        break;
    }
    case O_GUARDPOS: {
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
            // loiter in a slow circle over the zone, strike whatever enters it
            float orbR = ut.jet ? std::max(e.zoneR * 0.55f, 4.0f * TILE) : e.zoneR * 0.45f;
            e.orbit += SIM_DT * ut.speed * (ut.jet ? 0.5f : 0.4f) / std::max(48.0f, orbR);
            Vec2 wp = e.zone + Vec2(std::cos(e.orbit), std::sin(e.orbit)) * orbR;
            Vec2 d = wp - e.pos; float l = d.len();
            if (ut.jet) flyTo(e, wp, ut.speed * 0.8f);
            else if (l > 2) { e.pos += d.norm() * std::min(l, ut.speed * SIM_DT * (l > 140 ? 1.0f : 0.55f)); e.angle = std::atan2(d.y, d.x); }
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
        // nothing to shoot: return to the slot
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
            Vec2 d = e.target - e.pos;
            if (d.len() < 6) { e.order = O_IDLE; e.postOrder = O_IDLE; break; }
            if (ut.jet) flyTo(e, e.target, ut.speed);
            else { e.pos += d.norm() * std::min(d.len(), ut.speed * SIM_DT); e.angle = std::atan2(d.y, d.x); }
        } else {
            if (e.path.empty()) requestPath(e, e.target);
            moveAlong(e, ut.speed);
            if (e.pathIdx >= e.path.size()) { e.order = O_IDLE; e.guardPos = e.pos; e.path.clear(); break; }
            // stuck detection
            e.stuckTimer += SIM_DT;
            if (e.stuckTimer > 1.5f) {
                if (dist(e.pos, e.lastPos) < ut.speed * 0.25f) {
                    if (dist(e.pos, e.target) < TILE * 1.5f) { e.order = O_IDLE; e.guardPos = e.pos; e.path.clear(); break; }
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
            if (e.postOrder == O_ATTACKMOVE) { e.order = O_ATTACKMOVE; requestPath(e, e.target); }
            else if (e.postOrder == O_GUARDPOS) { e.order = O_GUARDPOS; e.path.clear(); }
            else if (e.postOrder == O_GUARDAREA && e.zoneR > 0) { e.order = O_GUARDAREA; e.path.clear(); e.engaged = NOREF; }
            else { e.order = O_IDLE; e.guardPos = e.pos; }
            e.postOrder = O_IDLE; e.leashed = false;
            break;
        }
        const Weapon& w = WEAPONS[ut.weapon];
        float d = distToEntity(e.pos, *t) / TILE;
        // a zone guard does not chase targets out of its zone (it may still shoot what is already in range)
        if (e.leashed && e.zoneR > 0 && d > w.range + 0.15f && distToEntity(e.zone, *t) > e.zoneR + TILE * 1.5f) {
            e.order = O_GUARDAREA; e.targetEnt = NOREF; e.path.clear(); e.postOrder = O_IDLE; e.leashed = false; break;
        }
        if (e.isAir()) {
            if (e.ammo <= 0 && ut.ammo > 0) { e.order = O_REARM; break; }
            if (ut.jet) { jetAttack(e, *t); break; }
            if (ut.bomber && !t->isAir()) { bomberAttack(e, *t); break; }
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
        if (!s) { e.order = O_IDLE; break; }
        float d = distToEntity(e.pos, *s);
        if (d <= HARVEST_REACH) {
            e.path.clear();
            e.actionTimer += SIM_DT;
            if (e.actionTimer >= UNLOAD_TIME) {
                e.actionTimer = 0;
                players[e.owner].money += e.cargo; players[e.owner].harvested += e.cargo;
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
        if (!b || !b->isBuilding() || b->owner != e.owner || (b->constructed && b->hp >= b->maxHp)) { e.order = O_IDLE; e.targetEnt = NOREF; e.guardPos = e.pos; break; }
        float d = distToEntity(e.pos, *b);
        // a dozer boxed in by neighbouring structures cannot always get right up to the wall: after a short wait it works from where it is
        bool inReach = d <= BUILD_REACH || (e.actionTimer > 1.5f && d <= BUILD_REACH + 3 * TILE);
        if (inReach) {
            e.path.clear();
            e.angle = std::atan2(b->pos.y - e.pos.y, b->pos.x - e.pos.x);
            const BuildType& bt = b->bt();
            if (!b->constructed) {
                b->progress += SIM_DT / bt.buildTime;
                b->hp = std::max(b->hp, bt.hp * (0.1f + 0.9f * clampf(b->progress, 0, 1)));
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
        Vec2 d = pad - e.pos;
        if (d.len() > 4) flyTo(e, pad, ut.speed);
        else {
            if (ut.jet) e.angle += angDiff(e.angle, -1.5708f) * 0.25f;
            e.actionTimer += SIM_DT;
            if (e.actionTimer >= REARM_TIME) { e.actionTimer = 0; e.ammo = std::min(ut.ammo, e.ammo + 1); }
            if (e.ammo >= ut.ammo) {
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
    if (!b.constructed) return;
    if (b.disabledUntil > time) return;
    const BuildType& bt = b.bt();
    Player& pl = players[b.owner];
    bool powered = !(pl.lowPower() && bt.power < 0);
    if (bt.role == BR_INCOME) {
        b.actionTimer += SIM_DT * (powered ? 1.0f : 0.5f);
        if (b.actionTimer >= INCOME_INTERVAL) {
            b.actionTimer -= INCOME_INTERVAL;
            int amt = pl.faction == F_CYBER ? INCOME_CYBER : INCOME_CLANKER;
            pl.money += amt; pl.mined += amt;
            fx.push_back({FX_SPARK, b.pos, b.pos, 0, 0.6f, rgb(255, 224, 90), 9});
        }
    }
    if (bt.weapon >= 0 && powered) {
        Entity* t = get(b.engaged);
        const Weapon& w = WEAPONS[bt.weapon];
        if (!t || !canTarget(b, *t) || distToEntity(b.pos, *t) > (w.range + 0.2f) * TILE) {
            t = nullptr; b.engaged = NOREF;
            if ((tick + b.gen) % 3 == 0) { t = acquireTarget(b, w.range); if (t) b.engaged = refOf(*t); }
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
        if (bt.role == BR_AIRFIELD) {
            int n = 0; Ref self = refOf(b);
            for (auto& u : ents) if (u.alive && u.isUnit() && u.isAir() && u.home == self) n++;
            if (n >= AIRFIELD_CAP) blocked = true;
        }
        if (!blocked) {
            float rate = (pl.lowPower() ? 0.5f : 1.0f) / ut.buildTime;
            if (pl.isAI && pl.difficulty == 3) rate *= 1.15f;
            b.queueProgress += SIM_DT * rate;
            if (b.queueProgress >= 1.0f) spawnFromQueue(b);
        }
    }
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
                float rr = ra + b.radius();
                Vec2 d = a.pos - b.pos;
                float l2 = d.len2();
                if (l2 >= rr * rr || l2 < 1e-4f) { if (l2 < 1e-4f) push += Vec2(rng.f(-1, 1), rng.f(-1, 1)); continue; }
                float l = std::sqrt(l2);
                float overlap = (rr - l) / rr;
                float wgt = ((b.order == O_IDLE || b.order == O_GUARDAREA) && a.order != O_IDLE && a.order != O_GUARDAREA) ? 0.4f : 0.8f;  // movers push idlers aside
                push += d * (overlap * wgt * 9.0f / l);
            }
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
    if (humans > 0 && humansAlive == 0) { gameOver = true; winnerTeam = aliveTeam; }
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
