// One Hour - skirmish AI (utility-driven base building, economy, army composition and attack waves)
#pragma once
#include "sim.h"
#include "brain.h"

// Features of the pro commander, each switchable on its own so --evalai can measure what a single one is worth (ONEHOUR_BASEMASK)
// (the estimate-based retreat was tried and measured no better than the cost-based one)
enum AiFeat { FEAT_MACRO = 1, FEAT_LAUNCH = 2, FEAT_COMP = 4, FEAT_ECON = 8, FEAT_ALLY = 16, FEAT_SPECIAL = 32,   // FEAT_SPECIAL: spy drones with the army, spies that capture structures
              FEAT_DOCPICK = 0x800,  // the opening is chosen knowing that an all-in rush throws its army at defences that hold (units that pick their targets
                                     // well make a base very hard to crack early): no rush unless this commander's own record says rushing pays here
              FEAT_NAVY = 64,    // a Naval Yard on the lake, a fleet of its own, the Guard Drones upgrade, jeeps loaded with infantry
              FEAT_AEGIS = 128,  // anti-nuke defences round the base and nuke targeting that reckons with the enemy's (and fires a salvo at a covered target)
              FEAT_RECOVER = 256,   // an army that has lost most of its base rebuilds the core first, with every free dozer, and keeps its last units home
              FEAT_CIVIC = 512,     // a civilian dozer raises a town behind the base once the economy is running (income and free dozers)
              FEAT_DEEPNUKE = 1024, // a rich commander with a Nuke Ramp sinks an Underground Nuke silo and fires its silver warhead
              FEAT_FRONTIER = 0x8000, // the island launchers (a lifter flies an infiltrator out to take one), their small nukes and the Nuclear Bomber call-in
              FEAT_TOWNS = 0x10000,   // the towns round the lake (ai_civil.cpp): infiltrators take them with an escort, the army defends and rebuilds them, retakes lost ones
              FEAT_RIOT = 0x20000,    // riots: a home guard stays behind while order is low, rioters do not skew the army's composition, soft spots get a turret
              FEAT_TEAM = 0x40000,    // allied commanders play as a team: one focus enemy for the whole team, joint attacks, plans called out to a human ally
              FEAT_PERSONA = 0x80000, // every commander is a person with a style of their own (see Persona): raids, patience, appetite for towns and spies
              FEAT_FLEET2 = 0x100000, // the fleet: ships already on their way or fighting keep at it, a carrier stands off and lets its jets fight
              FEAT_HOLD = 0x200000,   // defence and wave orders leave units that are already fighting at the spot alone (no mid-fight re-routing)
              FEAT_FLEETCALL = 0x400000, // the tech structure's Fleet Call: a battle fleet from the open sea, sent where the commander's fleet is needed
              FEAT_EASYWIT = 0x800000,   // an Easy commander plays sensibly (dodges nukes late, leaves radiation, helps allies, breaks off a lost attack,
                                         // uses its tech structure's powers); its economy, army size and reaction times stay Easy's
              FEAT_COMBO = 0x1000000,    // Hard and Brutal: combined arms - an Air Strike goes in on the objective's defences as a wave sets off
              FEAT_BUNKERUP = 0x2000000, // Normal and up: the bunkers' own upgrades bought when the money is there (shell and lasers first), Para Backup called
              // ---- the October micro & commander update
              FEAT_PRESS = 0x4000000,    // a wave that has taken its objective picks the next one near where it stands, not near home (no march back)
              FEAT_MEASURED = 0x8000000, // a raid on the base is met with enough to beat it (by the combat estimate), nearest first: the rest stay put
              FEAT_MEDEVAC = 0x10000000, // a valuable unit nearly dead in a wave pulls out to be patched up at home instead of dying for nothing
              FEAT_REASSESS = 0x20000000, // a wave that comes in sight of its objective weighs what it now sees there and turns back before a lost assault
              FEAT_ADAPT = 0x40000000,   // a plan that is failing changes: after a wave thrown away against the enemy's defences, an all-in opening
                                         // (rush, air first) turns into a defended, economic game with bigger waves instead of feeding it more armies
              FEAT_ALL = 0x7FFFFFFF,
    FEAT_DEFAULT = FEAT_MACRO | FEAT_LAUNCH | FEAT_COMP | FEAT_ECON | FEAT_ALLY | FEAT_SPECIAL | FEAT_NAVY | FEAT_AEGIS | FEAT_RECOVER | FEAT_CIVIC | FEAT_DEEPNUKE | FEAT_FRONTIER
                 | FEAT_TOWNS | FEAT_RIOT | FEAT_TEAM | FEAT_PERSONA | FEAT_FLEET2 | FEAT_HOLD | FEAT_FLEETCALL | FEAT_EASYWIT | FEAT_COMBO | FEAT_BUNKERUP
                 | FEAT_PRESS | FEAT_MEASURED | FEAT_MEDEVAC | FEAT_REASSESS | FEAT_ADAPT | FEAT_DOCPICK };   // what ships

// A commander's personality (FEAT_PERSONA). The opening doctrine (learned from match results) decides the broad plan; the persona is the person
// carrying it out and colours everything around it: how hard and how often they raid, how patient they are before a big push, how much they
// care for the towns round the lake and for infiltrators, and how they treat a beaten enemy.
enum PersonaId { PE_STRATEGIST = 0, PE_BLITZ, PE_IRONWALL, PE_SKYMARSHAL, PE_TYCOON, PE_SHADOW, PE_COUNT };
struct Persona {
    const char* title;      // "Blitz", "Iron Wall" ...
    const char* blurb;      // what the opponent should expect
    float raid;             // harassment: how often and how big (1 = the old default)
    float patience;         // multiplies the wave threshold (below 1 attacks sooner with less)
    float towns;            // appetite for the towns round the lake (infiltrators kept for them, escorts, defence)
    float spies;            // infiltrators used on enemy structures
    float guard;            // share of the army kept home (also more turrets at the soft spots)
    float finisher;         // weight on attacking the weakest enemy (one that riots and is losing) instead of the nearest
};
extern const Persona PERSONAS[PE_COUNT];

struct AiPlayer {
    int player = -1;
    Rng rng;
    float nextThink = 0;
    Vec2 rally;
    Vec2 enemyDir;              // unit vector from base toward the main enemy
    int mainEnemy = -1;
    bool attacking = false;
    float waveValue = 0;        // army value when the wave launched
    float attackStarted = 0;
    float regroupUntil = 0;
    float lastDefend = 0;
    float lastHarass = 0;
    int nextTurretKind = 0;
    Ref attackTarget;
    float nextOrderTime = 0;
    std::vector<Ref> wave;      // units committed to the current attack
    bool reassessed = false;    // FEAT_REASSESS: the wave has had its look at the objective
    int failedWaves = 0; bool adapted = false;   // FEAT_ADAPT: waves thrown away so far, and whether the plan has changed
    Ref droppedTarget; float lastRetarget = -100;   // the objective last dropped for radiation, and when the wave was last sent to a new one
    Ref failedTarget; float failedAt = -1000;   // last target a wave broke against
    Vec2 huntPoint; float huntAt = -1000;       // where the army sweeps when only the enemy's hidden things are left, and when to pick the next spot

    Brain* brain = &g_brain;    // the learned components this commander consults (a different one per army only in --evalai)
    bool useBrain = true;       // false = the original hand-tuned heuristics only (baseline for evaluation)
    bool smart = true;          // false = the oldest generation of commander: no doctrines, nuke evasion, medics, bomber tactics, ally defence
    int feat = FEAT_DEFAULT;        // which pro features are on (see AiFeat)
    bool pro = true;            // false = the previous generation (baseline for --evalai): no spending engine, combat estimates, adaptive defence or scouting
    int doctrine = DOC_BALANCED;
    int docCtx = 0;             // what the doctrine statistics were consulted for: who this commander faces (see doctrineContext)
    // what the doctrine and difficulty make of the shared build logic (1 = unchanged)
    struct Style { float first = 1, thr = 1, def = 1, tech = 1, air = 1, nuke = 1, army = 1; int airCap = 25, incomes = 2; float incomeFrom = 3.0f; } style;
    float lastSpyOrder = -100;     // when the spies were last sent after a structure
    float lastFleetOrder = -100, lastJeepLoad = -100, nextYardTry = 0, nextBunkerTry = 0, nextCivicTry = 0, nextFleetCall = 0;   // when the fleet was last given a target, and the jeeps last loaded
    float nukeHoldSince = -1;   // a covered nuke target is waited out for the second ramp since this time
    int nukesSeen = 0;             // enemy warheads launched so far (a commander that has been nuked builds anti-nuke defences)
    int seenNukeIds = 0;
    float lastBombRun = -100, lastAllyHelp = -100, lastNukeDodge = -100, retreatVotes = 0;
    int dodges = 0, medicsBuilt = 0;   // statistics for the self-play reports
    float waveX[WAVE_F] = {};   // features at launch, for learning from the outcome
    bool waveHasSample = false;
    float nextFortOrder = 0;
    Vec2 humanFront; bool hasFront = false;   // where a human teammate's army is fighting (computer allies converge on it)
    float lastJoin = -100, lastAllyNote = -100;
    float incomeRate = 0, incomeAt = 0, incomeT = 0;   // credits per second coming in, measured over the last ten seconds or so
    bool recovering = false;    // most of the base is gone: rebuild the core before anything else (see FEAT_RECOVER)
    float relocatedAt = -100;   // when the commander last moved its home to a new site (relocateBase)
    int relocations = 0;        // (statistics)
    Vec2 groundFrom; bool groundCheck = false;   // findSpot: only spots a dozer standing at groundFrom can drive to
    // ---- the newer commander (ai_civil.cpp): persona, towns, riots, team play
    int persona = PE_STRATEGIST;
    const char* commander = "";     // the commander's name (messages, the selftest report)
    float nextCivil = 0, lastTownGuard = -100, lastTownRebuild = -100, lastEscort = -100, lastAnnounce = -100, lastTeamJoin = -100, nextSoftTurret = 0;
    int townTarget = -1;            // the town its infiltrators are after (-1 none)
    std::vector<Ref> escort;        // the squad covering the infiltrator on its way to a town
    int townDefences = 0, townRebuilds = 0, teamJoins = 0;   // (statistics for the self-play report)
    bool introduced = false;        // the commander has introduced itself to the human player
    const Persona& pers() const { return PERSONAS[on(FEAT_PERSONA) ? persona : PE_STRATEGIST]; }
    void choosePersona();
    void civilAffairs(const std::vector<Ref>& army, const std::vector<Ref>& spies, bool threat, int dozers);   // FEAT_TOWNS
    int teamFocus() const;          // FEAT_TEAM: the enemy the whole team goes after (-1: no team plan)
    bool teamJoin(const std::vector<Ref>& army, float armyValue, float waveThreshold);   // FEAT_TEAM: join a teammate's attack under way
    void announce(const char* what, Vec2 at);   // a computer ally tells the human teammate what it is doing

    bool on(int f) const { return pro && (feat & f) != 0; }
    void init(int p, u64 seed);
    void endWave(float remainingValue);
    void learnOutcomes();
    void beginWaveLearning();
    void garrisonStrongholds();
    void frontier();                    // FEAT_FRONTIER (ai_frontier.cpp): take the island launchers, fire them, call the Nuclear Bomber
    float nextFrontier = 0;
    void think();
    void dodgeNukes();                  // cheap per-tick check: pull units out of the circle of an incoming enemy nuke
    bool findSpot(int buildType, Vec2 preferNear, int maxRing, int& tx, int& ty);
    bool findBunkerSpot(int buildType, int& tx, int& ty);
private:
    bool tryBuild(int buildType, Vec2 preferNear, int maxRing = 22);
    int chooseUnit(BuildRole role, int enemyInf, int enemyVeh, int enemyAir);
    Entity* pickAttackTarget(const Vec2* from = nullptr);   // from: score the distance from there (a wave in the field) instead of from home
    float assaultRatio(const std::vector<Ref>& attackers, Vec2 at, float radiusTiles, bool wholeEnemy);
    float enemyStrengthNear(Vec2 p, float radiusTiles);
    Entity* idleDozer(bool civic = false);   // civic: a civilian dozer (they raise only the town; the others only the base)
    bool relocateBase();                        // recovery without a Command structure: found the new base where a surviving dozer can build and live
    bool recoverBase(int dozers, bool hasHq);   // recovery mode: every free dozer raises the next missing core structure (-> something was placed)
    void managePower();
};

struct AiManager {
    AiPlayer ais[MAX_PLAYERS];
    bool brainEnabled[MAX_PLAYERS] = { true, true, true, true };
    bool smartEnabled[MAX_PLAYERS] = { true, true, true, true };
    bool proEnabled[MAX_PLAYERS] = { true, true, true, true };
    Brain* brainOverride[MAX_PLAYERS] = {};   // (--evalai: the baseline side plays with its own brain)
    int featMask[MAX_PLAYERS] = { FEAT_DEFAULT, FEAT_DEFAULT, FEAT_DEFAULT, FEAT_DEFAULT };
    void init(u64 seed);
    void update();
    void finish(int winnerTeam);        // the match is over: every commander's doctrine learns whether it won
    int focus[MAX_PLAYERS];             // FEAT_TEAM: per team index (0..MAX_PLAYERS-1 by team id modulo), the enemy army the team goes after
    float focusAt = 0;
    void updateTeamFocus();
    bool raiseDifficulty(int p);        // the human raised a computer army's difficulty mid-match (pause menu): one step up, never down
};
extern AiManager g_ai;
