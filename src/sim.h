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
    float jetBreak = 0;     // jets and bombers: seconds left of the breakaway after a pass
    u8 jetMode = 0;         // what the breakaway does: 0 peel away from the target, 1 extend straight out (target inside the turn circle)
    float airspeed = 0;     // fixed-wing aircraft: current speed along the heading (px/s)
    Vec2 loiter;            // aircraft: the spot it circles (fixed-wing) or hovers over while it waits in the air
    float loiterUntil = -1; // sim time it stops waiting there and flies home (-1 = never sent anywhere)
    bool autoTask = false;  // the current attack / move was the unit's own idea (helping a friend, answering fire, getting out of danger),
                            // not the player's: it gives up beyond its leash and goes back to its post afterwards
    float evadeUntil = -1;  // a unit that backed away from danger holds its new spot until this time instead of walking straight back
    float squeezeUntil = 0; // a ground unit that wants to move but is jammed (a one-lane gap, a unit standing in the way) slips past other units until then
    Vec2 progPos;           // where it was at the last progress check (once a second)
    Order resumeOrder = O_IDLE;   // what a unit that ducked out of a nuke's circle goes back to once the danger has passed
    Order resumePost = O_IDLE;
    Vec2 resumeTarget;
    Ref targetEnt;          // attack / harvest / build target
    Ref engaged;            // current auto-acquired enemy
    float dropTimer = 0;    // tech structure: sim time its next paradrop is ready (each Data Center / Arms Lab drops once per cooldown)
    float aidTimer = 0;     // tech structure: sim time its next Aid Drop flight is ready (human players only)
    float cooldown2 = 0;    // structure: the roof gun of the Defense Guns upgrade (its own target, cooldown and facing)
    Ref engaged2;
    float turret2 = 0;
    float dmgLoad = 0;      // structure: damage taken over the last couple of seconds (decaying); a big load overwhelms Rugged plating
    Ref forceTarget;        // a friendly (own or allied) target the human ordered it to attack on purpose (never set for computer armies)
    Vec2 hoverSpot;         // helicopter: the spot it last claimed to hover on (see heliSpot) ...
    u32 hoverTick = 0;      // ... and the tick it claimed it (a claim older than a tick has lapsed)
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
    int bombsLeft = 0;      // bombers: bombs still to drop from the stick being released
    float bombTimer = 0;
    float fall = 0;         // paradrop: seconds of parachute descent left (the unit glides from dropFrom to dropTo, cannot act and cannot be hit)
    float fallTime = 1;
    Vec2 dropFrom, dropTo;
    Vec2 dropGoal; bool dropGo = false;   // attack-move here once landed
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
    float radius() const { return kind == EK_UNIT ? UNITS[type].radius : kind == EK_RESOURCE ? TILE * 0.5f : (std::max(BUILDS[type].w, BUILDS[type].h) * TILE * 0.5f); }
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
    bool forced = false;   // fired on purpose at a friendly: the blast hurts everybody
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
    float dropReady = 0;        // sim time when paradrops unlock (the opening delay); after that every tech structure has its own cooldown
    float revealUntil = -1;     // the whole map is visible until this sim time
    bool advTech = false;       // Advanced Program researched: special units unlocked
    bool researching = false;
    float researchProgress = 0; // 0..1
    bool upg[UPG_COUNT] = {};           // structure upgrades finished (they last the whole match)
    bool upgBusy[UPG_COUNT] = {};       // being researched at the tech structure
    float upgProgress[UPG_COUNT] = {};  // 0..1
    float lastNotice = -100, lastMine = -100;   // throttles for alerts (reset every game)
    bool wasLowPower = false;
    bool haulFlee = true;       // haulers keep clear of raiders: an unescorted enemy gun closing on the pile sends them away until it has passed
    Vec2 basePos;
    int startIdx = 0;
    // stats
    int mined = 0;                // income from Oil Wells / Bitcoin Datacenters
    int unitsBuilt = 0, unitsLost = 0, unitsKilled = 0, structuresLost = 0, structuresKilled = 0, harvested = 0;
    float valueDealt[U_COUNT] = {};   // enemy value destroyed by each of our unit types (AI adapts composition to this)
    float spentOn[U_COUNT] = {};
    std::vector<u8> explored;   // MAP_W*MAP_H
    bool lowPower() const { return powerUsed > powerMade; }
    // Brutal computer armies play with a commander's edge: a bigger war chest, richer income, faster factories and dozers,
    // and harder-hitting weapons (no other difficulty and no human army gets any of it)
    bool brutal() const { return isAI && difficulty >= 3; }
    float econMul() const { return brutal() ? 1.5f : 1.0f; }      // supply deliveries and income structures
    float buildMul() const { return brutal() ? 1.6f : 1.0f; }     // unit production and construction speed
    float damageMul() const { return brutal() ? 1.25f : 1.0f; }   // damage dealt to enemies
};

enum EventType { EV_SOUND = 0, EV_MSG, EV_BUILD_DONE, EV_UNIT_READY, EV_UNDER_ATTACK, EV_NOFUNDS, EV_LOWPOWER, EV_PLAYER_DEAD, EV_SUPPLY_EMPTY, EV_AID };
struct Event { EventType type; int player; Sound sound; Vec2 pos; std::string msg; int other = -1; };   // other: EV_AID, the army that received it (-1 nobody)

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
    struct Storm { Vec2 pos; float radius; int owner; float t; int shellsLeft; float nextShell; bool force = false; };
    std::vector<Storm> storms;
    // tactical nukes in flight: launched from a Nuke Ramp, they detonate at 'pos' after NUKE_FLIGHT seconds
    struct Nuke { Vec2 from, pos; int owner; float t; bool force = false; };   // force: the human chose to hit friendly ground too
    std::vector<Nuke> nukes;
    static constexpr float NUKE_FLIGHT = 7.0f;
    // paradrop: a cargo plane crosses the map over the target and releases its load on parachutes (it can be shot down by anti-air)
    struct Airlift {
        Vec2 pos, prevPos, dir, target;
        int owner = -1;
        float hp = 0, maxHp = 0;
        float t = 0;
        std::vector<int> load;      // unit types in release order
        size_t next = 0;            // next unit to release
        float releaseAt = 0;        // distance along the track from the target at which the next unit leaves the plane
        bool attackOn = false;      // landed troops attack-move to the target (computer players)
        std::vector<Vec2> spots;    // landing spots already taken
        float lastHit = -100;
    };
    std::vector<Airlift> airlifts;
    static constexpr float AIRLIFT_SPEED = 270.0f;
    static constexpr float AIRLIFT_HP = 1100.0f;
    static constexpr float FALLOUT_LIFE = 80.0f;
    // Aid Drop (human players only): a white relief plane crosses the map over the spot and drops crates on parachutes. No enemy and no
    // computer army ever harms it (it is not an entity, no gun targets it, their nukes spare it); only the sender's own nuke brings it down,
    // like every aircraft in the blast. When the crates that left the plane are down, the army nearest to the spot other than the sender
    // gets AID_MONEY and a dozer (see aidCandidates); the opened crates stay on the ground for a while.
    struct AidCrate { Vec2 from, to; float fall = 0, fallTime = 1; };
    struct AidDrop {
        Vec2 pos, prevPos, dir, target;
        int owner = -1;
        float t = 0;
        float releaseAt = 0;            // distance along the track from the target at which the next crate leaves the plane
        std::vector<AidCrate> crates;   // released so far
        bool downed = false;            // caught in the sender's own nuke: the crates still aboard are lost, the ones already falling land
        bool delivered = false;
        int recipient = -1;             // the army that got it (-1: nobody was left to take it, or nothing got out of the plane)
        float deliveredAt = -1;
    };
    std::vector<AidDrop> aidDrops;
    static constexpr float AID_SPEED = 230.0f;
    static constexpr float AID_LINGER = 14.0f;   // seconds the opened crates stay on the ground after the delivery
    struct Fallout { Vec2 pos; float r; float t; float tick; };
    std::vector<Fallout> fallouts;   // radiation zones left by detonations

    // spatial grid
    static const int GRID_CELL = 64;
    static const int GRID_W = WORLD_W / GRID_CELL, GRID_H = WORLD_H / GRID_CELL;
    std::vector<int> grid[GRID_W * GRID_H];
    std::vector<int> heliList;   // living helicopters (rebuilt with the grid every tick)

    void init(int nPlayers, const Faction* factions, const bool* isAI, const int* difficulties, const int* teams, u64 seed);
    void step();
    Entity* get(Ref r) { if (r.idx < 0 || r.idx >= (int)ents.size()) return nullptr; Entity& e = ents[r.idx]; return (e.alive && e.gen == r.gen) ? &e : nullptr; }
    const Entity* get(Ref r) const { if (r.idx < 0 || r.idx >= (int)ents.size()) return nullptr; const Entity& e = ents[r.idx]; return (e.alive && e.gen == r.gen) ? &e : nullptr; }
    Ref refOf(const Entity& e) const { return Ref{ (i32)(&e - &ents[0]), e.gen }; }
    Ref refOf(int idx) const { return Ref{ idx, ents[idx].gen }; }
    bool enemies(int a, int b) const { return a >= 0 && b >= 0 && a != b && players[a].team != players[b].team; }
    // Whom a warhead hurts on the ground: the launcher's enemies always; a human's nuke everybody caught in it, allies and the human's
    // own army included, exactly as hard as an enemy (computer armies never hurt their own side). Aircraft: see nukeBlast.
    bool nukeHurts(const Nuke& n, int owner) const { return enemies(n.owner, owner) || (owner >= 0 && n.owner >= 0 && !players[n.owner].isAI); }

    // creation
    Ref spawnUnit(int type, int owner, Vec2 pos);
    Ref placeBuilding(int type, int owner, int tx, int ty, bool instant);
    Ref spawnResource(int tx, int ty, int amount);
    void destroy(Entity& e, bool violent);

    // commands (validated; safe to call with anything)
    void cmdMove(const std::vector<Ref>& sel, Vec2 dest, bool attackMove);
    void cmdAttack(const std::vector<Ref>& sel, Ref target, bool force = false);   // force: attack a friendly target on purpose (human armies only)
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
    bool cmdPower(int player, Vec2 pos, bool force = false);
    bool cmdNuke(int player, Vec2 pos, bool force = false);       // fires one ready Nuke Ramp at pos
    bool cmdParadrop(int player, Vec2 pos, bool attackOn = false);   // tech structure: a cargo plane drops the army's airborne force at pos
    int dropsReady(int player) const;         // tech structures that can send a paradrop right now (after the opening delay)
    float dropWait(int player) const;         // seconds until the soonest paradrop is ready (0 = ready, -1 = no tech structure)
    bool cmdAidDrop(int player, Vec2 pos);    // tech structure, human players only: a relief flight drops $20000 and a dozer to the army nearest pos
    int aidsReady(int player) const;          // tech structures that can send an aid flight right now (0 for computer armies)
    float aidWait(int player) const;          // seconds until the soonest aid flight is ready (0 = ready, -1 = no tech structure)
    // the armies an aid drop at 'at' would go to: every army but the sender at the smallest distance to its nearest unit or structure
    // (edge distance, 0 when the spot is on it); more than one only on an exact tie, which is drawn at random. Returns the count.
    int aidCandidates(int sender, Vec2 at, int* out) const;
    int nukesReady(int player) const;         // ramps that can launch right now
    float nukeWait(int player) const;         // seconds until the soonest ramp is ready (0 = ready, -1 = no ramp)
    bool atIncomeLimit(int player, int buildType) const;
    bool atBuildLimit(int player, int buildType) const;   // income structures and Command Cores are capped per player
    bool cmdScan(int player);                 // tech structure: reveal the whole map for SCANS[].duration
    bool cmdResearch(int player);             // tech structure: research the Advanced Program (unlocks special units)
    bool programAvailable(int player) const;  // tech structure standing, program not yet researched or running
    bool cmdUpgrade(int player, int upg);     // tech structure: research a structure upgrade (UPGRADES[faction][upg])
    bool upgradeAvailable(int player, int upg) const;   // tech structure standing, upgrade not yet finished or running
    bool hasUpgrade(int player, int upg) const { return player >= 0 && player < numPlayers && players[player].upg[upg]; }
    int roofGun(const Entity& b) const;       // WEAPONS index of a structure's Defense Guns roof gun, -1 without the upgrade
    Vec2 roofGunPos(const Entity& b) const;   // where that gun sits (world px)
    Entity* repairJob(Entity& dozer, float maxDist = 1e9f);   // the damaged structure a dozer should mend next (spreads dozers out, -> nullptr: nothing to do)
    int repairCrew(const Entity& b, const Entity* except = nullptr) const;   // dozers already mending this structure
    static int repairCrewCap(const Entity& b);   // how many dozers a structure needs at most

    // queries
    bool canPlace(int player, int buildType, int tx, int ty) const;
    bool canAfford(int player, int cost) const { return players[player].money >= cost; }
    bool hasBuilding(int player, int buildType) const;
    bool hasRole(int player, BuildRole role) const;
    bool techOnline(int player) const;   // a tech structure stands and no EMP has switched it off
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
    int padsUsed(const Entity& airfield, const Entity* except = nullptr) const;   // fixed-wing aircraft based at an airfield (helicopters do not take a pad)
    // what the player can see: everything explored so far, or the whole map while a scan is running
    bool explored(int player, int tx, int ty) const { return time < players[player].revealUntil || players[player].explored[ty * MAP_W + tx] != 0; }
    bool exploredRaw(int player, int tx, int ty) const { return players[player].explored[ty * MAP_W + tx] != 0; }
    bool revealed(int player) const { return time < players[player].revealUntil; }
    Vec2 buildingCenter(int type, int tx, int ty) const { return Vec2(tx * TILE + BUILDS[type].w * TILE * 0.5f, ty * TILE + BUILDS[type].h * TILE * 0.5f); }
    float distToEntity(Vec2 p, const Entity& e) const; // edge distance in px
    bool inFallout(Vec2 p, float margin = 0) const;      // inside a radiation zone that is still dangerous
    float aaCover(const Entity& t, int owner);          // how many anti-air guns of players hostile to 'owner' cover a target (flak a bomber would fly into)

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
    void updateAirlifts();
    void updateAidDrops();
    void deliverAid(AidDrop& a);
    void releaseUnit(Airlift& a);
    Vec2 landingSpot(Airlift& a, float spacing);
    void updateFallout();
    void updateResearch();
    void finishUpgrade(int player, int upg);
    void updateRoofGun(Entity& b, bool powered);
    void separateUnits();
    void checkVictory();
    void moveAlong(Entity& e, float speed);
    bool requestPath(Entity& e, Vec2 dest);
    bool tryFire(Entity& e, Entity& tgt);
    void fireWeapon(Entity& e, Entity& tgt, const Weapon& w, const Vec2* muzzle = nullptr);
    void applyDamage(Entity& tgt, float dmg, int attackerOwner, Ref attacker, const Weapon* w);
    void splashDamage(Vec2 at, float radiusTiles, float dmg, int owner, Ref attacker, const Weapon& w, Ref direct, bool forced = false);
    Entity* acquireTarget(Entity& e, float range, int wpn = -1);
    Entity* acquireZoneTarget(Entity& e);
    Entity* findZonePile(Entity& h);
    bool canTarget(const Entity& e, const Entity& t, int wpn = -1) const;   // wpn: a weapon other than the entity's own (a roof gun)
    void finishBuilding(Entity& b);
    void spawnFromQueue(Entity& b);
    Entity* findSupplyBuilding(Entity& h);
    Entity* findAirfield(Entity& a);
    void deathFx(Entity& e);
    Vec2 padSlot(const Entity& airfield, const Entity& craft) const;
    void flyTo(Entity& e, Vec2 dest, float speed);       // aircraft: straight slide (helicopters), fixed-wing: final approach to a spot (the pad)
    float airTurnRate(const Entity& e) const;            // fixed-wing: g-limited turn rate at the current airspeed
    void airSteer(Entity& e, float want, float wantSpeed, bool keepOnMap = true);   // fixed-wing: bank toward a heading, throttle toward a speed, fly on
    void airLoiter(Entity& e, Vec2 center, float radius, float speedFrac);         // circle a spot (helicopters hover over it)
    Vec2 heliSpot(Entity& e, Vec2 want);                 // helicopters: claim a spot to hover on for 'want' that no other helicopter holds
    void heliHover(Entity& e, Vec2 spot, float speed);   // helicopters: fly to a hover spot (easing in) and hold there
    void airDogfight(Entity& e, Entity& t);              // fixed-wing air-to-air: lead pursuit, corner speed, extend and re-engage
    void runOrder(Entity& e, const UnitType& ut);         // carry out the unit's current order for one tick (the body of updateUnit)
    bool autonomy(Entity& e);                            // a unit left on its own: dodge danger, answer fire, help friends (true = took a new order)
    bool dodgeDanger(Entity& e);                         // out of an incoming nuke's circle and out of fallout
    void retreatFrom(Entity& e, Vec2 threat, float tiles);   // back away a little from a threat (toward friends), keeping its post
    void autoEngage(Entity& e, Entity& t);               // go after a target on its own initiative (leashed to its post)
    float fightOdds(Entity& e, Entity& foe);             // > 1: the unit and the friends around it win the local fight against foe's side
    Vec2 postOf(const Entity& e) const;                  // where a unit left on its own belongs (its guard slot / zone / where it stood)
    void moveAside(Entity& e, Vec2 dest, float holdFor);  // an internal (autonomous) move that keeps the unit's post and zone
    void jetAttack(Entity& e, Entity& t);                // fixed-wing strafing pass: dive on the target, fire, break away, come round again
    void bomberAttack(Entity& e, Entity& t);             // bombing run: line up on the target, release a stick of bombs, fly on, loop back
    void dropBomb(Entity& e, Vec2 at);                   // one bomb falls from the aircraft toward the ground point 'at'
    void bombImpact(Projectile& p);                      // fireball, shockwave, debris and splash damage of a bomb
    void nukeBlast(const Nuke& n);                       // detonation of a tactical nuke (see updateNukes)
};

extern Sim g_sim;
