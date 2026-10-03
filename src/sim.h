// One Hour - deterministic game simulation (entities, orders, combat, economy)
#pragma once
#include "common.h"
#include "data.h"
#include "map.h"
#include <functional>

enum EntKind : u8 { EK_UNIT = 0, EK_BUILDING, EK_RESOURCE };
enum Order : u8 { O_IDLE = 0, O_MOVE, O_ATTACKMOVE, O_ATTACK, O_HARVEST, O_RETURN, O_BUILD, O_REARM, O_GUARDPOS, O_GUARDAREA };

struct Ref {
    i32 idx = -1; u32 gen = 0;
    bool valid() const { return idx >= 0; }
    bool operator==(const Ref& o) const { return idx == o.idx && gen == o.gen; }
    bool operator!=(const Ref& o) const { return !(*this == o); }
};
static const Ref NOREF;

struct Entity {
    u32 gen = 0;
    bool alive = false;
    EntKind kind = EK_UNIT;
    int type = 0;           // UnitTypeId / BuildTypeId
    int owner = -1;         // player index, -1 neutral
    Vec2 pos;
    Vec2 prevPos;           // position at the previous tick (render interpolation)
    float angle = 0;        // facing (radians), also turret angle for buildings
    float turret = 0;       // turret facing for vehicles
    float hp = 0, maxHp = 0;
    float cooldown = 0;
    float lastDamaged = -100;
    u32 fxTick = 0;         // render-side: last tick this unit emitted a dust puff
    float disabledUntil = -1;
    Ref attacker;           // last entity that damaged us
    // unit
    Order order = O_IDLE;
    Order postOrder = O_IDLE;   // what to resume after engaging (attack-move / guard)
    Vec2 target;            // move destination
    Vec2 guardPos;          // hold position (area guard: this unit's slot inside the zone)
    Vec2 zone;              // assigned area: guard circle (combat/air) or supply search circle (haulers)
    float zoneR = 0;        // px, 0 = no area assigned
    bool leashed = false;   // current target was auto-acquired for the zone: do not chase it out of the zone
    float orbit = 0;        // aircraft loiter phase
    float alt = 1;          // aircraft altitude 0 (parked on the pad) .. 1 (airborne); visual only
    float jetBreak = 0;     // jets: seconds left of the breakaway after a strafing pass
    Ref targetEnt;          // attack / harvest / build target
    Ref engaged;            // current auto-acquired enemy
    std::vector<Vec2> path;
    size_t pathIdx = 0;
    float repathTimer = 0;
    float stuckTimer = 0;
    Vec2 lastPos;
    int ammo = 0;
    int cargo = 0;
    Ref lastPile;
    float actionTimer = 0;
    Ref home;               // aircraft: airfield
    int burstLeft = 0;
    float burstTimer = 0;
    // building
    int tx = 0, ty = 0;     // top-left tile
    bool constructed = true;
    float progress = 0;     // 0..1 construction
    std::vector<int> queue; // unit types
    float queueProgress = 0;
    Vec2 rally;
    bool hasRally = false;
    // resource
    int amount = 0;

    bool isUnit() const { return kind == EK_UNIT; }
    bool isBuilding() const { return kind == EK_BUILDING; }
    const UnitType& ut() const { return UNITS[type]; }
    const BuildType& bt() const { return BUILDS[type]; }
    float radius() const { return kind == EK_UNIT ? UNITS[type].radius : (std::max(BUILDS[type].w, BUILDS[type].h) * TILE * 0.5f); }
    int weapon() const { return kind == EK_UNIT ? UNITS[type].weapon : (kind == EK_BUILDING ? BUILDS[type].weapon : -1); }
    Armor armor() const { return kind == EK_UNIT ? UNITS[type].armor : AR_STRUCT; }
    bool isAir() const { return kind == EK_UNIT && UNITS[type].kind == UK_AIR; }
    float sight() const { return kind == EK_UNIT ? UNITS[type].sight : BUILDS[type].sight; }
    Vec2 center() const { return pos; }
};

struct Projectile {
    Vec2 pos, vel;
    Vec2 prevPos;
    Ref shooter;
    Vec2 dest;          // for shells
    Ref target;
    int weapon;
    int owner;
    float life;
    float arcT = 0, arcLen = 1;
    bool alive = true;
};

enum FxType { FX_BEAM = 0, FX_ARC, FX_RAIL, FX_FLASH, FX_EXPLODE, FX_SMOKE, FX_SPARK, FX_RING, FX_DEBRIS, FX_EMP, FX_WRECK, FX_RUBBLE, FX_MUSHROOM, FX_FALLOUT };
struct Fx {
    FxType type; Vec2 a, b; float t = 0, life = 1; Color color; float size = 8; Vec2 vel;
    bool seen = false;      // render side: cosmetic particles for this effect have been spawned
};

struct Player {
    bool active = false;
    bool alive = true;
    bool isAI = false;
    int difficulty = 1;         // 0 easy, 1 normal, 2 hard, 3 brutal
    int team = 0;
    Faction faction = F_CYBER;
    int money = START_CASH;
    int powerMade = 0, powerUsed = 0;
    float powerReady = 0;       // sim time when the strike power is available
    float scanReady = 0;        // sim time when the map scan is available
    float revealUntil = -1;     // the whole map is visible until this sim time
    bool advTech = false;       // Advanced Program researched: special units unlocked
    bool researching = false;
    float researchProgress = 0; // 0..1
    float lastNotice = -100, lastMine = -100;   // throttles for alerts (reset every game)
    bool wasLowPower = false;
    Vec2 basePos;
    int startIdx = 0;
    // stats
    int mined = 0;                // income from Oil Wells / Bitcoin Datacenters
    int unitsBuilt = 0, unitsLost = 0, unitsKilled = 0, structuresLost = 0, structuresKilled = 0, harvested = 0;
    float valueDealt[U_COUNT] = {};   // enemy value destroyed by each of our unit types (AI adapts composition to this)
    float spentOn[U_COUNT] = {};
    std::vector<u8> explored;   // MAP_W*MAP_H
    bool lowPower() const { return powerUsed > powerMade; }
};

enum EventType { EV_SOUND = 0, EV_MSG, EV_BUILD_DONE, EV_UNIT_READY, EV_UNDER_ATTACK, EV_NOFUNDS, EV_LOWPOWER, EV_PLAYER_DEAD, EV_SUPPLY_EMPTY };
struct Event { EventType type; int player; Sound sound; Vec2 pos; std::string msg; };

struct Sim {
    std::vector<Entity> ents;
    std::vector<int> freeList;
    std::vector<Projectile> projs;
    std::vector<Fx> fx;
    std::vector<Event> events;
    Player players[MAX_PLAYERS];
    int numPlayers = 0;
    int humanPlayer = 0;
    float time = 0;
    u32 tick = 0;
    Rng rng;
    bool gameOver = false;
    int winnerTeam = -1;
    struct Storm { Vec2 pos; float radius; int owner; float t; int shellsLeft; float nextShell; };
    std::vector<Storm> storms;
    // tactical nukes in flight: launched from a Nuke Ramp, they detonate at 'pos' after NUKE_FLIGHT seconds
    struct Nuke { Vec2 from, pos; int owner; float t; };
    std::vector<Nuke> nukes;
    static constexpr float NUKE_FLIGHT = 7.0f;
    static constexpr float FALLOUT_LIFE = 80.0f;
    struct Fallout { Vec2 pos; float r; float t; float tick; };
    std::vector<Fallout> fallouts;   // radiation zones left by detonations

    // spatial grid
    static const int GRID_CELL = 64;
    static const int GRID_W = WORLD_W / GRID_CELL, GRID_H = WORLD_H / GRID_CELL;
    std::vector<int> grid[GRID_W * GRID_H];

    void init(int nPlayers, const Faction* factions, const bool* isAI, const int* difficulties, const int* teams, u64 seed);
    void step();
    Entity* get(Ref r) { if (r.idx < 0 || r.idx >= (int)ents.size()) return nullptr; Entity& e = ents[r.idx]; return (e.alive && e.gen == r.gen) ? &e : nullptr; }
    const Entity* get(Ref r) const { if (r.idx < 0 || r.idx >= (int)ents.size()) return nullptr; const Entity& e = ents[r.idx]; return (e.alive && e.gen == r.gen) ? &e : nullptr; }
    Ref refOf(const Entity& e) const { return Ref{ (i32)(&e - &ents[0]), e.gen }; }
    Ref refOf(int idx) const { return Ref{ idx, ents[idx].gen }; }
    bool enemies(int a, int b) const { return a >= 0 && b >= 0 && a != b && players[a].team != players[b].team; }

    // creation
    Ref spawnUnit(int type, int owner, Vec2 pos);
    Ref placeBuilding(int type, int owner, int tx, int ty, bool instant);
    Ref spawnResource(int tx, int ty, int amount);
    void destroy(Entity& e, bool violent);

    // commands (validated; safe to call with anything)
    void cmdMove(const std::vector<Ref>& sel, Vec2 dest, bool attackMove);
    void cmdAttack(const std::vector<Ref>& sel, Ref target);
    void cmdStop(const std::vector<Ref>& sel);
    void cmdHarvest(const std::vector<Ref>& sel, Ref pile);
    // area assignments (Zero Hour style): combat units and aircraft protect the circle, haulers search it for supplies
    void cmdGuardArea(const std::vector<Ref>& sel, Vec2 center, float radius);
    void cmdGatherArea(const std::vector<Ref>& sel, Vec2 center, float radius);
    void cmdArea(const std::vector<Ref>& sel, Vec2 center, float radius);   // guard for fighters, gather for haulers
    bool cmdBuild(Ref dozer, int buildType, int tx, int ty);  // places a foundation and sends the dozer
    void cmdAssist(const std::vector<Ref>& sel, Ref building); // dozer: continue construction or repair
    bool cmdTrain(Ref building, int unitType);
    void cmdCancelTrain(Ref building, int queueIndex);
    void cmdSetRally(Ref building, Vec2 p);
    void cmdSell(Ref building);
    bool cmdPower(int player, Vec2 pos);
    bool cmdNuke(int player, Vec2 pos);       // fires one ready Nuke Ramp at pos
    int nukesReady(int player) const;         // ramps that can launch right now
    float nukeWait(int player) const;         // seconds until the soonest ramp is ready (0 = ready, -1 = no ramp)
    bool atIncomeLimit(int player, int buildType) const;
    bool atBuildLimit(int player, int buildType) const;   // income structures and Command Cores are capped per player
    bool cmdScan(int player);                 // tech structure: reveal the whole map for SCANS[].duration
    bool cmdResearch(int player);             // tech structure: research the Advanced Program (unlocks special units)
    bool programAvailable(int player) const;  // tech structure standing, program not yet researched or running

    // queries
    bool canPlace(int player, int buildType, int tx, int ty) const;
    bool canAfford(int player, int cost) const { return players[player].money >= cost; }
    bool hasBuilding(int player, int buildType) const;
    bool hasRole(int player, BuildRole role) const;
    bool prereqMet(int player, int req) const { return req < 0 || hasBuilding(player, req); }
    bool unitAvailable(int player, int unitType) const;
    bool buildAvailable(int player, int buildType) const;
    int countUnits(int player, int type = -1) const;
    int countBuildings(int player, int type = -1, bool onlyConstructed = false) const;
    int countRole(int player, BuildRole role, bool onlyConstructed) const;
    Entity* nearestEntity(Vec2 p, float maxDist, bool (*pred)(const Entity&, void*), void* ctx);
    void forEachNear(Vec2 p, float r, const std::function<void(Entity&)>& fn);
    Entity* findPile(Entity& h, float maxDist);   // nearest supply pile with supplies left
    Vec2 unitExit(const Entity& b) const;
    // what the player can see: everything explored so far, or the whole map while a scan is running
    bool explored(int player, int tx, int ty) const { return time < players[player].revealUntil || players[player].explored[ty * MAP_W + tx] != 0; }
    bool exploredRaw(int player, int tx, int ty) const { return players[player].explored[ty * MAP_W + tx] != 0; }
    bool revealed(int player) const { return time < players[player].revealUntil; }
    Vec2 buildingCenter(int type, int tx, int ty) const { return Vec2(tx * TILE + BUILDS[type].w * TILE * 0.5f, ty * TILE + BUILDS[type].h * TILE * 0.5f); }
    float distToEntity(Vec2 p, const Entity& e) const; // edge distance in px

    void emit(EventType t, int player, Sound s, Vec2 pos, const char* msg = "");
    void updatePowerPublic() { updatePower(); }
private:
    void rebuildGrid();
    void updatePower();
    void updateVision();
    void updateUnit(Entity& e);
    void updateBuilding(Entity& e);
    void updateProjectiles();
    void updateFx();
    void updateStorms();
    void updateNukes();
    void updateFallout();
    void updateResearch();
    void separateUnits();
    void checkVictory();
    void moveAlong(Entity& e, float speed);
    bool requestPath(Entity& e, Vec2 dest);
    bool tryFire(Entity& e, Entity& tgt);
    void fireWeapon(Entity& e, Entity& tgt, const Weapon& w);
    void applyDamage(Entity& tgt, float dmg, int attackerOwner, Ref attacker, const Weapon* w);
    void splashDamage(Vec2 at, float radiusTiles, float dmg, int owner, Ref attacker, const Weapon& w, Ref direct);
    float aaCover(const Entity& t, int owner);
    Entity* acquireTarget(Entity& e, float range);
    Entity* acquireZoneTarget(Entity& e);
    Entity* findZonePile(Entity& h);
    bool canTarget(const Entity& e, const Entity& t) const;
    void finishBuilding(Entity& b);
    void spawnFromQueue(Entity& b);
    Entity* findSupplyBuilding(Entity& h);
    Entity* findAirfield(Entity& a);
    void deathFx(Entity& e);
    Vec2 padSlot(const Entity& airfield, const Entity& craft) const;
    void flyTo(Entity& e, Vec2 dest, float speed);       // aircraft: straight slide, jets: turn-limited and slowing as they close in
    void jetAttack(Entity& e, Entity& t);                // fixed-wing strafing pass: dive on the target, fire, break away, come round again
};

extern Sim g_sim;
