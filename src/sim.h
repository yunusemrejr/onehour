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
                  O_STRIKERUN,  // Air Strike fighter: one bombing pass over 'target', a hard break, then off the map (see Sim::strikeJetRun)
                  O_NUKERUN,    // Nuclear Bomber call-in: straight over 'target', one bomb, on off the map (see Sim::nukeBomberRun)
                  O_CARRIERJET };   // a carrier's jet: fights near its carrier ('home') and lands on it again (see Sim::carrierJetRun)

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
    float fleetTimer = 0;   // tech structure: sim time it can call another fleet (Fleet Call)
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
    u8 strikePass = 0;                // Air Strike fighter: 0 on the run-in, 1 breaking away, 2 leaving the map
    u8 salvoLeft = 0;                 // ... rockets still to fire on the run-in
    Vec2 runDir;                      // ... the heading of its wave across the called spot
    float roll = 0;                   // aircraft: bank / roll angle of a manoeuvre (radians; drawn as the sprite foreshortening and flipping)
    float fade = 1;                   // drawn opacity: a departing Air Strike fighter fades out over the ocean before it is gone
    Vec2 unloadSpot;                  // cargo ship: where the player pointed when it was sent to land its load (it uses the beach nearest it)
    std::vector<float> deck;          // aircraft carrier: one entry per jet on deck, the sim time it is rearmed and ready to launch
    float deckReplace = -1;           // ... when the next replacement for a lost jet arrives (-1: none under way)
    float deckLaunch = 0;             // ... earliest time of the next launch
    u8 catSide = 0;                   // ... the catapult the next launch uses (port / starboard in turn)
    float catAt = -10;                // ... when the last jet went off a catapult (the blast deflector and the steam are drawn for a moment)
    u8 deckPhase = 0;                 // carrier jet: 0 flying, 1 taxiing to the catapult, 2 on the catapult, 3 climbing out, 4 going round for the approach,
                                      // 5 on the glide path, 6 rolling out after the trap, 7 taxiing to its parking spot
    float deckT = 0;                  // ... seconds into that phase
    Vec2 deckFrom, deckTo;            // ... where the phase starts and ends, in the carrier's own frame (px ahead, px to starboard)
    float deckAng = 0;                // ... its heading relative to the hull when the glide path began (it eases onto the angled deck's)
    u8 goRounds = 0;                  // ... approaches it has had to break off and go round again (after two it comes in from where it is)
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
    u8 bunkerUpg = 0;                 // bunker: the upgrades fitted to this bunker alone (bit 1 << BunkerUpg) ...
    int bunkerBusy = -1;              // ... the one being fitted now (-1: none) and how far along it is (0..1)
    float bunkerProgress = 0;
    float paraReady = 0;              // ... sim time its Para Backup can be called again (BU_PARA)
    float lzCd[2] = {0, 0}, lzAng[2] = {0, 0};   // ... its laser battery's two roof lasers (BU_LASERS): cooldown, facing and target
    Ref lzTgt[2];
    float lzFired = -10;              // ... when one of them last fired (the emitters glow for a moment)
    float dozerAt = 0;                // civilian structure: sim time its next free dozer comes out ...
    u8 dozerKind = 0;                 // ... and which kind: 0 a military dozer, 1 a civilian one (they take turns)
    u32 mendTick = 0;                 // the last tick a Healing Antenna mended it (antennas do not stack: one pulse per tick, see updateHealAntenna)
    float heat = 0;                   // indestructible structure (island launcher, Bastion Wall): how hard it is burning (0 cold .. HEAT_MAX ablaze); cosmetic only
    float bomberTimer = 0;            // tech structure: sim time its next Nuclear Bomber call is ready
    bool bombDropped = false;         // Nuclear Bomber: its bomb has left the bay
    int visKey = -1;                  // the tile and owner its sight was last written from (explored ground is never forgotten: unmoved, it is skipped)
    int district = -1;                // a block of one of the civilian areas round the lake (index into Sim::districts), -1 for anything else
    int riot = -1;                    // a rioter: the id of the crowd it marches with (see Sim::riots)

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

// Unit-level behaviours of the October micro update, each switchable for --micromatrix (mirrored fights, one side without them)
enum MicroBit {
    MICRO_RETARGET = 1,   // a unit that picked its own target keeps looking: it turns on something it can hit right now instead of chasing one out of reach,
                          // and on a far better one in reach (the gun shooting at it rather than the wall it was plinking)
    MICRO_THREAT = 2,     // targets are weighed by what killing them is worth: the damage they deal per second against the seconds it takes to kill them
    MICRO_FOCUS = 4,      // a target already taking fire (and not yet doomed) is preferred: a group finishes one enemy before the next
    MICRO_KITE = 8,       // a unit that outranges what it fights (and is not slower) backs off between shots while the foe closes in
    MICRO_ALL = 15
};

struct Player {
    bool active = false;
    bool alive = true;
    bool isAI = false;
    int difficulty = 1;         // 0 easy, 1 normal, 2 hard, 3 brutal
    bool sharpFire = true;      // units picking their own targets pass over one already doomed by what is in flight to it (see Sim::sharpScore); --duelmatrix switches it off for A/B
    int micro = MICRO_ALL;      // the smarter unit behaviours (MicroBit) this army's units use; --micromatrix plays a side with none of them for A/B
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

// A civilian area round the lake (see DISTRICT_* in data.h): its blocks are civic buildings with Entity.district set. It changes hands as a whole.
struct District {
    const char* name = "";
    bool city = false;
    Vec2 center; float radius = 0;   // px: the circle its blocks stand in
    int owner = -1;                  // the army that holds it (-1: nobody)
    std::vector<Ref> blocks;
    float payAt = 0;                 // sim time of the next payout
    float integrity = 1;             // health of the whole area, ruins included (0..1)
    int ruins = 0;                   // blocks razed and being raised again
    int pay = 0;                     // credits per payout while it is intact enough
    float lastHit = -100;            // the last time any block was hit
    float takenAt = -100;            // the last time it changed hands
    int people = 0;                  // townsfolk about (grows back as the blocks are rebuilt; decoration and the panel)
};
// A crowd of rioters (see RIOT_* in data.h): civilians that turned on the army 'victim' after it lost control.
struct Riot {
    int id = 0;
    int victim = -1;                 // the army it rose against
    bool city = false;               // marching on a town the victim holds rather than on its base
    Vec2 origin;
    std::vector<Ref> members;
    Ref target;                      // the structure it is wrecking
    float until = 0;                 // the survivors melt away after this
    float nextOrder = 0;
};
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
                  bool deep = false; Ref source; bool super = false; bool swift = false; };   // swift: a bunker missile with BU_SWIFT (BUNKER_SWIFT_*: faster, harder to shoot down)   // force: the human chose to hit friendly ground too; scale: share of a full warhead's blast area (a bunker's missile is MINI_NUKE_SCALE);
                                          // deep: the silver warhead of an Underground Nuke (UNUKE_SCALE, UNUKE_DAMAGE, UNUKE_FALLOUT; only an Anti-Nuke Laser can stop it)
                                          // source: the Supersonic Bomber that dropped it; the one aircraft its own compact bomb does not bring down (see nukeBlast)
                                          // super: an island launcher's Super Nuke (ISLAND_NUKE_*: twice an Underground Nuke's radius, ISLAND_NUKE_FLIGHT, only lasers and barely)
    static float nukeRadius(const Nuke& n) { return NUKE_RADIUS * TILE * std::sqrt(n.scale); }   // px
    std::vector<Nuke> nukes;
    bool anyBunkerLasers = false;   // a bunker's laser battery was fitted this match: only then is every rocket and shell checked against them (bunkerLaserIntercept)
    static constexpr float NUKE_FLIGHT = 7.0f;
    static float flightOf(const Nuke& n) { return n.super ? ISLAND_NUKE_FLIGHT : (n.swift ? NUKE_FLIGHT * BUNKER_SWIFT_FLIGHT : NUKE_FLIGHT); }   // seconds from launch to impact (a Super Nuke is much faster)
    // Share of a defence's usual chance to shoot this warhead down: the silver casing halves it, a Super Nuke's is a tenth, a swift bunker missile's 30%
    static float nukeResist(const Nuke& n) { return (n.super ? ISLAND_NUKE_INTERCEPT_RESIST : (n.deep ? UNUKE_INTERCEPT_RESIST : 1.0f)) * (n.swift ? BUNKER_SWIFT_RESIST : 1.0f); }
    int nukeSeq = 0;
    int fxTagSeq = 0;   // handles into Sim::fx (a fallout zone shrinks its own haze as an Anti-Radiation Truck scrubs it)
    // The air position of a warhead t seconds into its flight: a long arc over the ground line from the ramp to the target (the renderer draws the missile here too).
    static Vec2 nukeAirPos(const Nuke& n, float t) { float u = clampf(t / flightOf(n), 0.0f, 1.0f); Vec2 p = n.from + (n.pos - n.from) * u; p.y -= std::sin(u * 3.14159f) * 260.0f; return p; }
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
    // the waves of an Air Strike still to come in (each a V formation launched at 'at' from the map edge on its own heading)
    struct StrikeWave { int owner = -1, type = 0, n = 0; Vec2 target, dir; float at = 0; };
    std::vector<StrikeWave> strikeWaves;
    // the ships of a Fleet Call still to come out of the haze at the map border: each enters at 'at' at 'from' heading 'dir' and sails to its place 'to'
    struct FleetShip { int owner = -1, type = 0; Vec2 from, dir, to; float at = 0; };
    std::vector<FleetShip> fleetArrivals;
    // where a called fleet comes in (for the caller's eyes: the lane is drawn while the ships are still arriving)
    struct FleetMark { Vec2 entry, goal; int owner = -1; float t0 = 0; };
    std::vector<FleetMark> fleetMarks;
    static constexpr float STRIKE_MARK_LIFE = 45.0f;
    static constexpr float AID_SPEED = 230.0f;
    static constexpr float AID_LINGER = 14.0f;   // seconds the opened crates stay on the ground after the delivery
    // ---- the civilian areas round the lake and the riots (sim2.cpp)
    std::vector<District> districts;
    std::vector<Riot> riots;
    int riotSeq = 0;
    bool riotsOn = true;                      // (tests that need a quiet base switch the riots off)
    static inline bool riotsDefault = true;   // what Sim::init sets riotsOn to: the headless scene tests turn it off (main.cpp), full matches keep it
    float unrest[MAX_PLAYERS] = {};           // per army: fills toward the next riot (1 = a crowd forms)
    float control[MAX_PLAYERS] = {};          // per army: 0 lost all control .. 1 firmly in control (refreshed every couple of seconds)
    int riotsRaised[MAX_PLAYERS] = {};        // crowds that rose against each army (stats)
    float lossHeat[MAX_PLAYERS] = {};         // per army: the value of what it lost lately (decays with a half-life of about a minute and a half)
    float basePeak[MAX_PLAYERS] = {};         // per army: what its base was worth at its best lately (slowly forgotten)
    float riotNoteAt[MAX_PLAYERS] = {};       // throttle for the riot warnings
    static bool districtBlock(const Entity& e) { return e.isBuilding() && e.district >= 0; }
    static bool districtRuin(const Entity& e) { return districtBlock(e) && !e.constructed; }
    void spawnDistricts();                    // the map's towns and cities, neutral at the start
    void updateDistricts();                   // pay, integrity, townsfolk (every half second)
    void updateDistrictBlock(Entity& b);      // a block mends itself, a ruin is raised again by its people
    void collapseBlock(Entity& b, int attackerOwner);   // a razed block falls into a ruin instead of vanishing
    void captureDistrict(int d, int to);      // the whole area changes hands
    bool dozerMayWork(const Entity& dozer, const Entity& b) const;   // may this dozer raise / mend this structure (its own, or a block of a town that is not an enemy's)
    float armyControl(int p) const;           // see control[]
    void updateRiots();                       // unrest, new crowds, and the crowds' orders (once a second)
    bool riotSpawnSpot(int victim, Vec2& out);
    Entity* riotPickTarget(Riot& r, Vec2 from);
    int riotersAlive() const;
    struct Fallout { Vec2 pos; float r; float t; float tick; float power = 1.0f; int owner = -1; bool enemyOnly = false; float life = FALLOUT_LIFE; int visTag = 0; };   // life: seconds it lasts (an Underground Nuke's three times as long); visTag: the FX_FALLOUT haze it is drawn as
    std::vector<Fallout> fallouts;   // radiation zones left by detonations

    // spatial grid
    static constexpr int GRID_CELL = 64;
    static constexpr int GRID_W = WORLD_W / GRID_CELL, GRID_H = WORLD_H / GRID_CELL;
    std::vector<int> grid[GRID_W * GRID_H];
    std::vector<int> heliList;   // living helicopters (rebuilt with the grid every tick)
    std::vector<float> incoming; // per entity index: damage already on its way in projectiles (rebuilt with the grid every tick; sharp fire skips the doomed)

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
    int cmdBuildLine(const std::vector<Ref>& dozers, int buildType, const std::vector<std::pair<int, int>>& spots);   // a whole line of foundations at once (shift-drag), shared out among the dozers
    bool waymakerClears(int player, int i, int j) const;   // a tile a Waymaker of this player would clear (explored tree, rock or ridge, nothing built on it)
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
    static bool neutralTakeable(const Entity& b) { return b.isBuilding() && b.owner < 0 && (invulnerableRole(b.bt().role) || b.bt().role == BR_STRONGHOLD || b.district >= 0); }   // nobody's: any army's spy may take it (a town: the whole of it)
    // what an infiltrator of 'player' may capture: an enemy structure, a neutral one (see neutralTakeable), and - for a human player only, never a
    // computer army - an ally's structure (the human takes it over; the ally is not asked)
    bool allyTakeable(int player, const Entity& b) const { return b.isBuilding() && player >= 0 && b.owner >= 0 && b.owner != player && !players[player].isAI && !enemies(player, b.owner); }
    bool spyMayTake(int player, const Entity& b) const { return enemies(player, b.owner) || neutralTakeable(b) || allyTakeable(player, b); }
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
    // Per-bunker upgrades (BunkerUpg, sim_bunker.cpp): each bunker is fitted with its own, one at a time, paid when the fitting starts
    static bool bunkerHas(const Entity& b, int u) { return (b.bunkerUpg >> u) & 1; }
    bool bunkerUpgAvailable(const Entity& b, int u) const;   // its owner's finished bunker, without it and with nothing else being fitted
    bool cmdBunkerUpgrade(Ref bunker, int u);
    bool cmdBunkerPara(Ref bunker, Vec2 pos, bool attackOn = false);   // BU_PARA: a cargo plane drops BUNKER_PARA_TROOPS paratroopers at pos
    std::vector<int> bunkerParaManifest(int player) const;   // the infantry such a drop carries (what the army has unlocked; never infiltrators)
    float bunkerParaWait(const Entity& b) const;             // seconds until it can call one (0 ready, -1 not fitted)
    bool cmdParadrop(int player, Vec2 pos, bool attackOn = false);   // tech structure: a cargo plane drops the army's airborne force at pos
    bool cmdPhoneStrike(int player, Vec2 pos);   // tech structure: call in a strike package on a spot of your choice (see PhoneStrike)
    int strikesReady(int player) const;          // tech structures that can take a call right now
    float strikeWait(int player) const;          // seconds until the soonest one is ready (0 = ready, -1 = no tech structure)
    std::vector<int> strikeManifest(int player) const;   // the fighters that would answer the call now
    bool cmdFleetCall(int player, Vec2 pos);     // tech structure: a battle fleet sails in from the map border nearest pos (on the ocean) and forms up there
    int fleetsReady(int player) const;           // tech structures that can call a fleet right now (after the paradrops' opening delay)
    float fleetWait(int player) const;           // seconds until the soonest one is ready (0 = ready, -1 = no tech structure)
    std::vector<int> fleetManifest(int player) const;   // the ships of a fleet: patrol boats, the warships, the carrier (never a cargo ship)
    // where a fleet called on pos would come in: the spot moved onto the open ocean ('goal'), the border point it enters at and the heading in.
    // False if pos is not on (or within FLEET_REACH of) the ocean, or no stretch of border there is open sea
    bool fleetEntry(Vec2 pos, Vec2& goal, Vec2& entry, Vec2& inward) const;
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
    float antiNukeChance(Vec2 at, int victim, bool knownOnly = false, float scale = 1.0f, bool deep = false, bool super = false) const;   // deep: an Underground Nuke (only lasers count); super: an island Super Nuke
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
    bool shipLanding(const Entity& c, Vec2* land = nullptr) const;   // a cargo ship near enough a shore to land its load: the free land tile to use
    void boardPublic(Entity& u, Entity& c) { board(u, c, true); }    // (test scenes: put a unit aboard directly)
    bool shoreSite(int player, int buildType, Vec2 near, int& tx, int& ty) const;   // a legal spot for a structure that must touch the lake or the ocean, nearest to 'near' (false: none known)
    int nukesReady(int player) const;         // ramps that can launch right now
    float nukeWait(int player) const;         // seconds until the soonest ramp is ready (0 = ready, -1 = no ramp)
    int miniNukesReady(int player) const;     // bunkers whose missile is ready
    float miniNukeWait(int player) const;     // seconds until the soonest bunker missile is ready (0 = ready, -1 = no bunker)
    Vec2 bunkerGunPos(const Entity& b, int i) const;   // where each of a bunker's two miniguns sits (world px)
    Vec2 bunkerOverdrivePos(const Entity& b, int i) const;   // where each of a bunker's two overdrive laser/heavy-MG mounts sits (world px)
    Vec2 bunkerLaserPos(const Entity& b, int i) const;   // ... and of a laser battery's two roof lasers (BU_LASERS)
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
    // What the human player's side can see right now (the view's fog of war; the simulation and the computer commanders go by 'explored'):
    // ground in sight of a unit or structure of the human's team, refreshed with the vision. Explored ground out of sight is remembered terrain and
    // structures, but enemy units are only shown in sight (or during a scan). sightGen counts the refreshes, so the view knows when to redraw its fog.
    std::vector<u8> inSight;
    u32 sightGen = 0;
    bool seenNow(int tx, int ty) const { return revealed(humanPlayer) || (!inSight.empty() && inSight[ty * MAP_W + tx] != 0); }
    bool shownToHuman(const Entity& e) const;   // whether the human's view may show e now (own and allied always; see inSight)
    void updateSight();
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
    void updateFleetArrivals();
    void updateAntiNuke(Entity& b);                      // a defence watching the sky over its area: engages a warhead closing in on it
    void breakNuke(Nuke& n);                             // a warhead shot down: the airburst, and only its pieces fall on the target now
    void updateInterceptors();
    bool interceptProjectile(Projectile& p);             // a protector drone shoots a rocket or shell aimed at its host down (true: it did)
    bool bunkerLaserIntercept(Projectile& p);            // ... or a bunker's laser battery burns it (BU_LASERS)
    void updateJeep(Entity& jeep);                       // the crew mends, climbs out when told to, and fires from the seats
    bool passengersFire(Entity& host, float reachTiles); // every passenger shoots its own weapon at the best target it can hit from the host; true if anybody fired
    void moveShip(Entity& e, float speed);              // a ship's drive: it turns at a limited rate and builds up speed (the lake has no corners to slip round)
    void strikeJetRun(Entity& e);                        // Air Strike fighter: rockets and a stick of bombs on one pass, a hard break, then off the map
    void launchStrikeWave(const StrikeWave& w);
    void updateCarrier(Entity& c);                       // aircraft carrier: replaces lost jets, launches ready ones at enemies near it
    void carrierJetRun(Entity& e);                       // a carrier's jet: fight near the carrier, land on it to rearm
    static Vec2 carrierPoint(const Entity& c, Vec2 local) { float ca = std::cos(c.angle), sa = std::sin(c.angle); return c.pos + Vec2(local.x * ca - local.y * sa, local.x * sa + local.y * ca); }   // the carrier's own frame to the world
    static Vec2 carrierLocal(const Entity& c, Vec2 world) { Vec2 d = world - c.pos; float ca = std::cos(c.angle), sa = std::sin(c.angle); return Vec2(d.x * ca + d.y * sa, -d.x * sa + d.y * ca); }
    Entity* carrierTarget(const Entity& c, const Entity* jet);   // the enemy a carrier's jet should go for (nullptr: nothing near)
    void leaveMap(Entity& e, Vec2 away);                 // a loan flies off: straight out to the nearest edge, fading over the ocean, then gone
    void nukeBomberRun(Entity& e);                       // Nuclear Bomber: hold the line to the target, release, fly on and leave the map
    void updateIslandNuke(Entity& b);                    // an island launcher: keeps its clock, cools down, flares its laser chamber
    void updateHealAntenna(Entity& b);                   // a Healing Antenna mends everything friendly round it (see HEALANT_*)
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
    void bunkerMend(Entity& b);                          // ... its own mending (it goes on while the bunker is knocked offline)
    void updateBunkerUpgrades(Entity& b);                // ... fitting an upgrade, the laser battery and the field hospital
    void bunkerNuked(Entity& b, float k, int attackerOwner);   // a warhead's blast at strength k (1: a direct ordinary hit) on a nuclear-hardened bunker
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
    Entity* acquireTarget(Entity& e, float range, int wpn = -1, float* scoreOut = nullptr);
    float targetScore(const Entity& e, const Entity& t, const Weapon& wp, int wpn, float d);   // how much a shooter wants a target (lower goes first)
    bool kiteStep(Entity& e, const Entity& t, const Weapon& w, float d);   // MICRO_KITE: backs off a step from a shorter-ranged foe (true: it moved)
    float sharpScore(const Entity& e, const Entity& t, float d, const Weapon& wp) const;   // Player::sharpFire: the extra score of a target (lower goes first)
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
