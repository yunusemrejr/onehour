// One Hour - deterministic game simulation (entities, orders, combat, economy)
#pragma once
#include "common.h"
#include "data.h"
#include "map.h"
#include <functional>

enum EntKind : u8 { EK_UNIT = 0, EK_BUILDING, EK_RESOURCE };
enum Order : u8 { O_IDLE = 0, O_MOVE, O_ATTACKMOVE, O_ATTACK, O_HARVEST, O_RETURN, O_BUILD, O_REARM, O_GUARDPOS, O_GUARDAREA,
                  O_ENTER,      // walk to a transport / bunker (targetEnt) and climb aboard
                  O_CAPTURE,    // spy: take the enemy structure targetEnt
                  O_UNLOAD,     // transport: fly to 'target' and let the passengers out there
                  O_STRIKERUN,  // Hotline Strike fighter: rocket passes over 'target', then it leaves the map (see Sim::strikeJetRun)
                  O_NUKERUN };  // Nuclear Bomber call-in: straight over 'target', one bomb, on off the map (see Sim::nukeBomberRun)

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
    // Lifetime combat evidence: only completed trials train composition, never a
    // purchase window while this unit may still be travelling to the front.
    float combatValue = 0;
    float combatMix[3] = {};
    bool combatSeen = false, combatRecorded = false;
    u32 waveSerial = 0;
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
    float slowAt = -1;      // Supersonic Bomber: it keeps its dash until this sim time after a drop (clearing its own fireball), then slows down
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
    float strikeTimer = 0;  // tech structure: sim time it can take another Phone Strike call
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
    // stealth, carriers, veterancy
    Ref carrier;                      // stowed aboard this transport / bunker: off the map, not drawn, cannot be hit, acts only as part of its host
    std::vector<Ref> passengers;      // transport / bunker: who is aboard
    float hatch = 0, hatchMax = 0;    // bunker: health of the surface opening the garrison fires from (0 = blown off, the garrison is silent)
    float siloTimer = 0;              // bunker: sim time its next missile is ready
    float gunCd[2] = {0, 0};          // bunker: cooldown, target and facing of each of its two miniguns
    Ref gunTgt[2];
    float gunAng[2] = {0, 0};
    bool unloading = false;           // transport / bunker: letting the passengers out one at a time
    float unloadTimer = 0;
    float detUntil[MAX_PLAYERS] = {}; // stealthy entity: sim time until which each player can see it (a detector close by keeps it up to date)
    u8 rank = 0;                      // infantry veterancy: 0 recruit, 1 veteran, 2 elite
    u16 kills = 0;
    float droneAt[3] = {0, 0, 0};     // protector drones (jeeps, ships): sim time each one can fire again (see Sim::droneCount) ...
    u8 droneLoad[3] = {0, 0, 0};      // ... and the interceptors it has left (0 once the reload has run out means a full magazine)
    u8 strikePass = 0;                // Hotline Strike fighter: passes made over the target so far
    u8 salvoLeft = 0;                 // ... rockets still to fire from the salvo of the pass in progress
    float lifeUntil = -1;             // a loan (UnitType.temp): sim time at which it is withdrawn whatever it is doing (-1 = never)
    float autoHoldUntil = -1;         // dozer: a direct move / target order keeps it from wandering off to mend things on its own until then
    std::vector<Ref> siteQueue;       // dozer: construction sites it was pulled away from before they were finished (resumed once it is idle)
    u8 nukeSalvo = 0;                 // bunker: missiles of the launch in progress still to go ...
    float nukeSalvoAt = 0;            // ... when the next one leaves the hatch, where it goes, and whether it may hit friendly ground
    Vec2 nukeSalvoPos; bool nukeSalvoForce = false;
    float podCd = 0, podAng = 0;      // bunker: the pop-up rocket pod that answers whoever is wearing it down (cooldown, facing)
    float ciwsAt = 0;                 // bunker: sim time its hatch guns can swat the next rocket or shell aimed at it
    float radCd = 0;                  // bunker: cooldown before it can vent another emergency radiation ring when surrounded and hit hard
    float overdriveCd[2] = {0, 0}, overdriveAng[2] = {0, 0};   // bunker: surrounded-mode heavy lasers / machineguns
    Ref overdriveTgt[2];
    float antiNukeCd = 0;             // bunker: cooldown for engaging incoming nuclear warheads
    float pdAt = 0;                   // anti-nuke / Anti-Rocket wall: cooldown for aircraft and projectile point-defence
    bool surrounded = false;          // bunker: enemy forces currently surround it (activates overdrive guns and radiation defense)
    float guardHitAt = -100;          // bunker: the last time a friendly structure it watches over was hit (its pod and guns answer that too)
    float dozerAt = 0;                // civilian structure: sim time its next free dozer comes out ...
    u8 dozerKind = 0;                 // ... and which kind: 0 a military dozer, 1 a civilian one (they take turns)
    float heat = 0;                   // indestructible structure (island launcher, Bastion Wall): how hard it is burning (0 cold .. HEAT_MAX ablaze); cosmetic only
    float bomberTimer = 0;            // tech structure: sim time its next Nuclear Bomber call is ready
    bool bombDropped = false;         // Nuclear Bomber: its bomb has left the bay
    int visKey = -1;                  // the tile and owner its sight was last written from (explored ground is never forgotten: unmoved, it is skipped)

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
    float nukeScale = 0;   // a Nuclear Bomber's bomb (W_NUKE_BOMB): an ordinary warhead of this share of a full blast area (0: the Supersonic Bomber's silver one)
};

enum FxType { FX_BEAM = 0, FX_ARC, FX_RAIL, FX_FLASH, FX_EXPLODE, FX_SMOKE, FX_SPARK, FX_RING, FX_DEBRIS, FX_EMP, FX_WRECK, FX_RUBBLE, FX_MUSHROOM, FX_FALLOUT };
struct Fx {
    FxType type; Vec2 a, b; float t = 0, life = 1; Color color; float size = 8; Vec2 vel;
    bool seen = false;      // render side: cosmetic particles for this effect have been spawned
    int tag = 0;            // a handle back to the gameplay thing this effect visualises (a fallout zone's haze: see Fallout.visTag)
};

struct UnitOutcome { int type; float value; float mix[3]; };

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
    int islandLaunches = 0, bomberCalls = 0, launchersTaken = 0;   // the October update's strategic assets used (self-play reports)
    float valueDealt[U_COUNT] = {};   // enemy value destroyed by each of our unit types (AI adapts composition to this)
    float spentOn[U_COUNT] = {};
    std::vector<UnitOutcome> unitOutcomes;   // bounded completed trials, drained by the commander
    u32 waveSerial = 0;
    float waveCredit = 0;       // only damage from members of the current wave
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
    // power: what is left of the warhead's effect (1 = whole; a warhead an anti-nuke defence shot down comes down in pieces with ANTINUKE_FALLOFF);
    // killT: flight time at which a defence's shot gets it (-1 = none), after which only its pieces fall on 'pos'; id ties the defences' shots to it
    struct Nuke { Vec2 from, pos; int owner; float t; bool force = false; float scale = 1.0f; float power = 1.0f; float killT = -1; bool broken = false; int id = 0; std::vector<int> engagedBy;
                  bool deep = false; Ref source; };   // force: the human chose to hit friendly ground too; scale: share of a full warhead's blast area (a bunker's missile is MINI_NUKE_SCALE);
                                          // deep: the silver warhead of an Underground Nuke (UNUKE_SCALE, UNUKE_DAMAGE, UNUKE_FALLOUT; only an Anti-Nuke Laser can stop it)
                                          // source: the Supersonic Bomber that dropped it; the one aircraft its own compact bomb does not bring down (see nukeBlast)
    static float nukeRadius(const Nuke& n) { return NUKE_RADIUS * TILE * std::sqrt(n.scale); }   // px
    std::vector<Nuke> nukes;
    static constexpr float NUKE_FLIGHT = 7.0f;
    int nukeSeq = 0;
    int fxTagSeq = 0;   // handles into Sim::fx (a fallout zone shrinks its own haze as an Anti-Radiation Truck scrubs it)
    // The air position of a warhead t seconds into its flight: a long arc over the ground line from the ramp to the target (the renderer draws the missile here too).
    static Vec2 nukeAirPos(const Nuke& n, float t) { float u = clampf(t / NUKE_FLIGHT, 0.0f, 1.0f); Vec2 p = n.from + (n.pos - n.from) * u; p.y -= std::sin(u * 3.14159f) * 260.0f; return p; }
    // A shot of an anti-nuke defence in the air: a laser beam or an interceptor rocket on its way to meet the warhead at 'aim' (hit: it will get it).
    struct Interceptor { Vec2 from, aim; int owner = -1; int nukeId = 0; float launchT = 0, dur = 0.5f; bool laser = true, hit = false, done = false; };
    std::vector<Interceptor> interceptors;
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
    // Where a Hotline Strike was called in: the spot stays marked for a while (smoke and a reticle) so the swarm and the two fighters have something to
    // work on and everybody can see it coming
    struct StrikeMark { Vec2 pos; int owner = -1; float t0 = 0; };
    std::vector<StrikeMark> strikeMarks;
    static constexpr float STRIKE_MARK_LIFE = 45.0f;
    static constexpr float AID_SPEED = 230.0f;
    static constexpr float AID_LINGER = 14.0f;   // seconds the opened crates stay on the ground after the delivery
    struct Fallout { Vec2 pos; float r; float t; float tick; float power = 1.0f; int owner = -1; bool enemyOnly = false; float life = FALLOUT_LIFE; int visTag = 0; };   // life: seconds it lasts (an Underground Nuke's three times as long); visTag: the FX_FALLOUT haze it is drawn as
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
    void finishUnitExperience(Entity& e);   // close a combat trial on death or a completed match
    bool escapeDestination(const Entity& e, Vec2 threat, float tiles, Vec2& dest) const;

    // commands (validated; safe to call with anything)
    void cmdMove(const std::vector<Ref>& sel, Vec2 dest, bool attackMove);
    void cmdAttack(const std::vector<Ref>& sel, Ref target, bool force = false);   // force: attack a friendly target on purpose (human armies only)
    void cmdStop(const std::vector<Ref>& sel);
    void cmdHarvest(const std::vector<Ref>& sel, Ref pile);
    // area assignments (Zero Hour style): combat units and aircraft protect the circle, haulers search it for supplies
    void cmdGuardArea(const std::vector<Ref>& sel, Vec2 center, float radius);
    void cmdGatherArea(const std::vector<Ref>& sel, Vec2 center, float radius);
    void cmdArea(const std::vector<Ref>& sel, Vec2 center, float radius);   // guard for fighters, gather for haulers
    bool cmdBuild(Ref dozer, int buildType, int tx, int ty);  // places a foundation and sends the dozer (a Waymaker needs no dozer: it clears its ground itself)
    bool waymakerFits(int player, int tx, int ty, int w, int h) const;   // explored land a Waymaker may clear, with something on it to clear
    void cmdAssist(const std::vector<Ref>& sel, Ref building); // dozer: continue construction or repair
    bool cmdTrain(Ref building, int unitType);
    // ---- stealth, transports, bunkers, spies
    static int stealthOf(const Entity& e);                       // StealthClass of an entity (structures are never stealthy)
    bool visibleTo(const Entity& e, int player) const;           // can this player see e? (always, unless e is a stealthy unit and no detector of the player's team is close)
    bool canTarget(const Entity& e, const Entity& t, int wpn = -1) const;   // may e shoot t right now (visible, in the right class, not a bunker it cannot hurt); wpn: a weapon other than the entity's own (a roof gun)
    bool canBoard(const Entity& u, const Entity& carrier) const; // may u climb aboard right now (room left, right kind of unit)?
    int cargoUsed(const Entity& carrier) const;                  // transport slots taken
    int garrisonInfantry(const Entity& bunker) const;
    int garrisonDozers(const Entity& bunker) const;
    int garrisonCap(const Entity& b) const;                     // infantry slots of a bunker / neutral stronghold (0 for anything else)
    static bool stronghold(const Entity& e) { return e.isBuilding() && e.bt().role == BR_STRONGHOLD; }
    void spawnMapLandmarks();                                  // put the map's neutral castles / bunkers / watchtowers on the field
    void updateStronghold(Entity& b);                          // a claimed civilian stronghold: the garrison fires from its windows
    void cmdEnter(const std::vector<Ref>& sel, Ref carrier);     // walk to a cargo chopper / bunker and board it
    void cmdUnload(Ref carrier, bool atSpot, Vec2 spot);         // let everybody out: here, or after flying to 'spot' (cargo choppers)
    void cmdCapture(const std::vector<Ref>& sel, Ref building);  // spies take an enemy structure
    float captureTime(const Entity& b) const;                    // seconds a spy needs to take structure b
    void cmdCancelTrain(Ref building, int queueIndex);
    void cmdSetRally(Ref building, Vec2 p);
    void cmdSell(Ref building);
    bool cmdPower(int player, Vec2 pos, bool force = false);

    // ---- the drop manifest: what a tech structure sends sits in Data.h's DROP_*_POOL, and how far into them a drop reaches is the
    // army's drop tier, which climbs with the Advanced Program and with each finished structure upgrade. A drop is therefore never the
    // same force twice: it is composed from what the army can field at the moment the plane is called in.
    int dropTier(int player) const;                     // 0-based into DROP_TIERS / DTI_COUNT entries
    std::vector<int> dropManifest(int player) const;    // the load a cargo plane would carry for this army right now (air last)
    bool cmdNuke(int player, Vec2 pos, bool force = false);       // fires one ready Nuke Ramp at pos
    bool cmdDeepNuke(int player, Vec2 pos, bool force = false, Ref from = NOREF);   // fires one ready Underground Nuke silo at pos (from that silo if it is ready)
    int deepNukesReady(int player) const;     // Underground Nuke silos that can launch right now
    float deepNukeWait(int player) const;     // seconds until the soonest silo is ready (0 = ready, -1 = no silo)
    static bool hardened(const Entity& b);    // a finished bunker or Underground Nuke silo: doors/hatch first, plating, and only troops can finish it with conventional fire - warheads ignore the floor and can destroy it
    static bool wall(const Entity& b) { return b.isBuilding() && b.bt().role == BR_WALL; }
    static bool invulnerable(const Entity& b) { return b.isBuilding() && invulnerableRole(b.bt().role); }   // island launchers and Bastion Walls: never damaged, they only burn
    static bool swimmer(const Entity& e) { return e.isUnit() && !e.isAir() && e.ut().role == UR_SPY; }   // infiltrators paddle across water they cannot walk round (see SWIM_COST)
    static bool walkable(const Entity& e, int tx, int ty) { return swimmer(e) ? g_map.swimPassable(tx, ty) : g_map.passable(tx, ty); }
    static bool neutralTakeable(const Entity& b) { return b.isBuilding() && b.owner < 0 && (invulnerableRole(b.bt().role) || b.bt().role == BR_STRONGHOLD); }   // nobody's: any army's spy may take it
    void heatUp(Entity& b, float amount);   // an indestructible structure takes a hit: it burns hotter (flames, smoke), nothing else changes
    // ---- the Island Nuke Launchers: neutral at the start, an infiltrator takes one, its army fires small nukes from it on a bunker's cooldown
    bool cmdIslandNuke(int player, Vec2 pos, bool force = false, Ref from = NOREF);
    int islandNukesReady(int player) const;
    float islandNukeWait(int player) const;   // seconds until the soonest launcher it holds is ready (0 = ready, -1 = it holds none)
    // ---- the Nuclear Bomber call-in of the tech structure: a strategic bomber from the map corner, one mid-sized nuke, gone
    bool cmdNukeBomber(int player, Vec2 pos);
    int bombersReady(int player) const;
    float bomberWait(int player) const;       // seconds until the soonest tech structure can call one (0 = ready, -1 = no tech structure)
    Vec2 bomberEntry(int player, Vec2 target) const;   // the map corner the bomber comes in from (the one behind the caller's base)
    static bool canRaise(const Entity& dozer, int buildType);   // civilian dozers raise only the civilian town, the others only military structures
    static bool canMend(const Entity& dozer, int buildType);    // civilian dozers mend only the town; the others mend anything
    int civicPeople(const Entity& b) const;   // townsfolk walking about a civilian structure (decoration; 0 for anything else)
    bool cmdMiniNuke(int player, Vec2 pos, bool force = false, Ref from = NOREF);   // fires one ready bunker's small missile at pos (from that bunker if it is ready)
    bool cmdParadrop(int player, Vec2 pos, bool attackOn = false);   // tech structure: a cargo plane drops the army's airborne force at pos
    bool cmdPhoneStrike(int player, Vec2 pos);   // tech structure: call in a strike package on a spot of your choice (see PhoneStrike)
    int strikesReady(int player) const;          // tech structures that can take a call right now
    float strikeWait(int player) const;          // seconds until the soonest one is ready (0 = ready, -1 = no tech structure)
    std::vector<int> strikeManifest(int player) const;   // the aircraft that would answer the phone now
    int dropsReady(int player) const;         // tech structures that can send a paradrop right now (after the opening delay)
    float dropWait(int player) const;         // seconds until the soonest paradrop is ready (0 = ready, -1 = no tech structure)
    bool cmdAidDrop(int player, Vec2 pos);    // tech structure, human players only: a relief flight drops $20000 and a dozer to the army nearest pos
    int aidsReady(int player) const;          // tech structures that can send an aid flight right now (0 for computer armies)
    float aidWait(int player) const;          // seconds until the soonest aid flight is ready (0 = ready, -1 = no tech structure)
    // the armies an aid drop at 'at' would go to: every army but the sender at the smallest distance to its nearest unit or structure
    // (edge distance, 0 when the spot is on it); more than one only on an exact tie, which is drawn at random. Returns the count.
    int aidCandidates(int sender, Vec2 at, int* out) const;
    // ---- jeeps, protector drones, anti-nuke defences, ships
    float airTopSpeed(const Entity& e) const;   // fixed-wing aircraft: the top speed right now (a sprinting nuclear bomber is slower once the bomb is gone)
    int dropJeeps(int player) const;                         // loaded jeeps an airdrop carries: DROP_JEEPS_MIN .. DROP_JEEPS_MAX by what the army has unlocked
    std::vector<int> jeepCrew(int player) const;             // the seven infantry types a jeep of this army is dropped with (one of each useful kind, the rest rocket troopers)
    int droneCount(const Entity& e) const;                   // protector drones it flies with right now (0 while disabled or under a parachute)
    Vec2 dronePos(const Entity& e, int i) const { return dronePosAt(e, i, time); }   // where its i-th drone is (they circle their host)
    Vec2 dronePosAt(const Entity& e, int i, float t) const;
    // chance the defences covering 'at' stop a warhead of 'scale' (of anybody who would hurt 'victim'): 1 - prod(1 - each one's antiNukeShot)
    float antiNukeChance(Vec2 at, int victim, bool knownOnly = false, float scale = 1.0f, bool deep = false) const;   // deep: an Underground Nuke (only lasers count)
    float antiNukeWarheadShot(const Entity& b) const;        // its chance against a nuclear warhead (the lasers' sighting nerf lives here; rockets, shells and aircraft use antiNukeShot)
    int antiNukeCovering(Vec2 at, int victim, bool knownOnly = false, float scale = 1.0f, bool deep = false) const;   // how many working defences cover 'at'
    bool antiNukeWorking(const Entity& b) const;             // a finished anti-nuke defence (or an Anti-Rocket wall) that is switched on and powered
    bool isAntiRocketWall(const Entity& b) const;            // a finished wall of an army that has researched Anti-Rocket
    void updatePointDefenseAir(Entity& b);                   // Q defences and Anti-Rocket walls shoot at enemy aircraft in reach
    float antiNukeCover(const Entity& b, float scale) const; // px: how far from it a warhead's target may be for it to engage (lasers: their blast reaching the circle is enough)
    int antiNukeLinked(const Entity& b) const;               // working Anti-Nuke Lasers of the same army standing close by (they share tracking: see ANTINUKE_LINK)
    float antiNukeShot(const Entity& b) const;               // its own chance of getting a warhead it engages
    void applyDamagePublic(Entity& tgt, float dmg, int attackerOwner, Ref attacker) { applyDamage(tgt, dmg, attackerOwner, attacker, nullptr); }   // (test scenes; no nuke flag: floors apply)
    void nukeBlastPublic(const Nuke& n) { nukeBlast(n); }   // (test scenes: detonate a warhead directly)
    void boardQuiet(Entity& u, Entity& c) { board(u, c, true); }   // put a soldier aboard a carrier at once (test scenes; the game walks them in with cmdEnter)
    Vec2 navalExit(const Entity& yard) const;                // where a ship leaves the yard into the water
    bool shoreSite(int player, int buildType, Vec2 near, int& tx, int& ty) const;   // a legal spot for a structure that must touch the lake, nearest to 'near' (false: none known)
    int nukesReady(int player) const;         // ramps that can launch right now
    float nukeWait(int player) const;         // seconds until the soonest ramp is ready (0 = ready, -1 = no ramp)
    int miniNukesReady(int player) const;     // bunkers whose missile is ready
    float miniNukeWait(int player) const;     // seconds until the soonest bunker missile is ready (0 = ready, -1 = no bunker)
    Vec2 bunkerGunPos(const Entity& b, int i) const;   // where each of a bunker's two miniguns sits (world px)
    Vec2 bunkerOverdrivePos(const Entity& b, int i) const;   // where each of a bunker's two overdrive laser/heavy-MG mounts sits (world px)
    bool bunkerSurrounded(const Entity& b, int* countOut = nullptr);
    void updateBunkerAntiNuke(Entity& b, bool surrounded);
    void updateScoutSwarm(Entity& e);
    bool atIncomeLimit(int player, int buildType) const;
    bool atBuildLimit(int player, int buildType) const;   // income structures and Command Cores are capped per player (walls are deliberately unlimited)
    bool cmdScan(int player);                 // tech structure: reveal the whole map for SCANS[].duration
    bool cmdResearch(int player);             // tech structure: research the Advanced Program (unlocks special units)
    bool programAvailable(int player) const;  // tech structure standing, program not yet researched or running
    bool cmdUpgrade(int player, int upg);     // tech structure: research a structure upgrade (UPGRADES[faction][upg])
    bool upgradeAvailable(int player, int upg) const;   // tech structure standing, upgrade not yet finished or running
    bool hasUpgrade(int player, int upg) const { return player >= 0 && player < numPlayers && players[player].upg[upg]; }
    int roofGun(const Entity& b) const;       // WEAPONS index of a structure's Defense Guns roof gun, -1 without the upgrade
    Vec2 roofGunPos(const Entity& b) const;   // where that gun sits (world px)
    Entity* repairJob(Entity& dozer, float maxDist = 1e9f);   // the damaged structure a dozer should mend next (spreads dozers out, -> nullptr: nothing to do)
    Entity* siteJob(Entity& dozer, float maxDist = 1e9f);     // the unfinished construction site an idle dozer should help with (always before any repair)
    bool dozerFindWork(Entity& dozer);                        // an idle dozer: its own unfinished sites first, then any site, then repairs (not during a manual hold)
    void keepSite(Entity& dozer);                             // a dozer pulled off an unfinished site remembers it (see Entity.siteQueue)
    bool siteOpen(const Entity& dozer, Ref site) const;       // a structure of the dozer's army that still has construction left
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
    // Every live entity whose edge is within r px of p. A template so the per-entity callback inlines (a std::function call per neighbour was a
    // seventh of a crowded tick); the distance test is done on squares first, without a square root.
    template <class F> void forEachNear(Vec2 p, float r, F&& fn) {
        float rr = r + 96;  // buildings are inserted at their center; widen the cell search
        int x0 = clampi((int)((p.x - rr) / GRID_CELL), 0, GRID_W - 1), x1 = clampi((int)((p.x + rr) / GRID_CELL), 0, GRID_W - 1);
        int y0 = clampi((int)((p.y - rr) / GRID_CELL), 0, GRID_H - 1), y1 = clampi((int)((p.y + rr) / GRID_CELL), 0, GRID_H - 1);
        const float r2 = r * r;
        for (int cy = y0; cy <= y1; cy++) for (int cx = x0; cx <= x1; cx++) {
            const std::vector<int>& cell = grid[cy * GRID_W + cx];
            for (size_t k = 0; k < cell.size(); k++) {
                Entity& e = ents[cell[k]];
                if (!e.alive) continue;
                if (e.kind == EK_BUILDING) {
                    const BuildType& b = BUILDS[e.type];
                    float bx0 = e.tx * (float)TILE, by0 = e.ty * (float)TILE, bx1 = bx0 + b.w * TILE, by1 = by0 + b.h * TILE;
                    float dx = std::max(std::max(bx0 - p.x, 0.0f), p.x - bx1), dy = std::max(std::max(by0 - p.y, 0.0f), p.y - by1);
                    if (dx * dx + dy * dy > r2) continue;
                } else {
                    float reach = r + e.radius();
                    if (dist2(p, e.pos) > reach * reach) continue;
                }
                fn(e);
            }
        }
    }
    Entity* findPile(Entity& h, float maxDist);   // nearest supply pile with supplies left
    Vec2 unitExit(const Entity& b) const;
    int padsUsed(const Entity& airfield, const Entity* except = nullptr) const;   // fixed-wing aircraft based at an airfield (helicopters do not take a pad)
    // what the player can see: everything explored so far, or the whole map while a scan is running
    bool explored(int player, int tx, int ty) const { return time < players[player].revealUntil || players[player].explored[ty * MAP_W + tx] != 0; }
    bool exploredRaw(int player, int tx, int ty) const { return players[player].explored[ty * MAP_W + tx] != 0; }
    bool revealed(int player) const { return time < players[player].revealUntil; }
    Vec2 buildingCenter(int type, int tx, int ty) const { return Vec2(tx * TILE + BUILDS[type].w * TILE * 0.5f, ty * TILE + BUILDS[type].h * TILE * 0.5f); }
    float distToEntity(Vec2 p, const Entity& e) const {   // edge distance in px
        if (e.kind == EK_BUILDING) {
            const BuildType& b = BUILDS[e.type];
            float x0 = e.tx * (float)TILE, y0 = e.ty * (float)TILE, x1 = x0 + b.w * TILE, y1 = y0 + b.h * TILE;
            float dx = std::max(std::max(x0 - p.x, 0.0f), p.x - x1), dy = std::max(std::max(y0 - p.y, 0.0f), p.y - y1);
            return std::sqrt(dx * dx + dy * dy);
        }
        return std::max(0.0f, dist(p, e.pos) - e.radius());
    }
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
    void updateStrikeMarks();
    void updateAidDrops();
    void updateAntiNuke(Entity& b);                      // a defence watching the sky over its area: engages a warhead closing in on it
    void breakNuke(Nuke& n);                             // a warhead shot down: the airburst, and only its pieces fall on the target now
    void updateInterceptors();
    bool interceptProjectile(Projectile& p);             // a protector drone shoots a rocket or shell aimed at its host down (true: it did)
    void updateJeep(Entity& jeep);                       // the crew mends, climbs out when told to, and fires from the seats
    bool passengersFire(Entity& host, float reachTiles); // every passenger shoots its own weapon at the best target it can hit from the host; true if anybody fired
    void moveShip(Entity& e, float speed);              // a ship's drive: it turns at a limited rate and builds up speed (the lake has no corners to slip round)
    void strikeJetRun(Entity& e);                        // Hotline Strike fighter: line up, salvo of rockets on the called spot, break away, come round again, then leave
    void nukeBomberRun(Entity& e);                       // Nuclear Bomber: hold the line to the target, release, fly on and leave the map
    void updateIslandNuke(Entity& b);                    // an island launcher: keeps its clock, cools down, flares its laser chamber
    void updateWaymaker(Entity& b);                      // a Waymaker clears its ground by itself, then is gone (see WAYMAKER_TIME)
    void fireStrikeRocket(Entity& e);
    void deliverAid(AidDrop& a);
    void releaseUnit(Airlift& a);
    Vec2 landingSpot(Airlift& a, float spacing);
    void updateFallout();
    void updateResearch();
    void finishUpgrade(int player, int upg);
    void updateRoofGun(Entity& b, bool powered);
    void updateDetection();                              // detectors (spy drones, up-close drones) reveal stealthy enemies to their whole team
    void updateBunkerSelf(Entity& b);                    // the bunker's own business: income, mending, miniguns
    void updateBunkerWard(Entity& b);                    // the bunker watches over the structures around it: seismic sensors, mending crews
    void updateCivic(Entity& b);                         // a civilian structure: pays its army and sends out a free dozer now and then
    void updateSilo(Entity& b);                          // an Underground Nuke silo mends its body and blast doors when left alone
    void updateBunker(Entity& b);                        // garrison: heals, repairs the hatch, fires from the hatch, lets soldiers out
    void updateCarrierUnload(Entity& c);                 // one passenger out every few ticks while 'unloading'
    void board(Entity& u, Entity& c, bool quiet = false);   // quiet: no click sound (a dropped jeep's crew is put aboard in bulk)
    void ejectAll(Entity& c, bool lost);                 // everybody out onto the ground (lost: aboard a downed aircraft they die instead)
    void ejectOne(Entity& c, Entity& u, int slot);
    void captureBuilding(Entity& b, Entity& spy);
    void hatchBlown(Entity& b);
    void awardKill(Ref killer, const Entity& victim);    // infantry earn promotions
    void separateUnits();
    void checkVictory();
    void moveAlong(Entity& e, float speed);
    bool requestPath(Entity& e, Vec2 dest);
    bool requestSidePath(Entity& e, const Entity& b);   // ground unit to the nearest reachable side of a structure (dozers, spies)
    bool tryFire(Entity& e, Entity& tgt);
    void fireWeapon(Entity& e, Entity& tgt, const Weapon& w, const Vec2* muzzle = nullptr);
    void applyDamage(Entity& tgt, float dmg, int attackerOwner, Ref attacker, const Weapon* w, float cap = -1.0f, bool nuke = false);   // nuke: warhead damage (it ignores the hardened floor: a bunker / silo can be destroyed by them)
    void noteCombat(Entity& e, const Entity& opponent);
    void splashDamage(Vec2 at, float radiusTiles, float dmg, int owner, Ref attacker, const Weapon& w, Ref direct, bool forced = false);
    Entity* acquireTarget(Entity& e, float range, int wpn = -1);
    Entity* acquireZoneTarget(Entity& e);
    Entity* findZonePile(Entity& h);
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
