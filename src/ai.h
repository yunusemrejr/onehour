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
              // ---- the learning update
              FEAT_TUNED = 0x1000,       // Normal and up: the commander's numbers (TuneId) as trained by self-play against the commander before it
                                         // (tools/tune.py), not the hand-set ones
              FEAT_PUNISH = 0x2000,      // a beaten-off attack is answered at once: the attacker's army is spent, so a counterattack goes in before it rebuilds
              FEAT_PROFILE = 0x4000,     // the opponent model (OppProfile, brain.h): what this kind of opponent did in earlier matches - an air force, an early
                                         // attack, early nukes - readies the answer before it shows (a person's habits above all)
              FEAT_ALL = 0x7FFFFFFF,
    FEAT_DEFAULT = FEAT_MACRO | FEAT_LAUNCH | FEAT_COMP | FEAT_ECON | FEAT_ALLY | FEAT_SPECIAL | FEAT_NAVY | FEAT_AEGIS | FEAT_RECOVER | FEAT_CIVIC | FEAT_DEEPNUKE | FEAT_FRONTIER
                 | FEAT_TOWNS | FEAT_RIOT | FEAT_TEAM | FEAT_PERSONA | FEAT_FLEET2 | FEAT_HOLD | FEAT_FLEETCALL | FEAT_EASYWIT | FEAT_COMBO | FEAT_BUNKERUP
                 | FEAT_PRESS | FEAT_MEASURED | FEAT_MEDEVAC | FEAT_REASSESS | FEAT_ADAPT | FEAT_DOCPICK
                 | FEAT_TUNED | FEAT_PUNISH | FEAT_PROFILE };   // what ships

// The second set of commander features (the first set is full), each switchable for --evalai (ONEHOUR_BASEMASK2 / ONEHOUR_NEWMASK2)
enum AiFeat2 { F2_CHOSEN = 1,     // The Chosen (ai_chosen.cpp): trained once a tech structure stands, sent on a war of its own - the enemy's production taken
                                  // (and sold off, far from home), its armies turned, the Shadow Guard called in where it hurts - and pulled out of a helicopter's eye
               F2_HELIHUNT = 2,   // helicopters against an enemy Chosen: a watch over the base once one could exist, a hunt where the unseen struck
               F2_GUILE = 4,      // the waves are harder to read: a fast detachment feints at the enemy's economy as the main wave sets off (a random pace
                                  // for the first waves was tried too: it lost 41-50 to the trained timing, so the timing stays as trained)
               F2_CAND = 8,       // (training only, never shipped: plays TUNE_CAND, see readTuneEnv in ai.cpp)
               F2_ALL = 7,
               F2_DEFAULT = F2_CHOSEN | F2_HELIHUNT };   // (feints are not on: Hard 42-51 with them, 51-40 without, against a 50-42 baseline; no gain on Brutal)

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

// The numbers behind the commander's big decisions, in one place so self-play can train them (tools/tune.py plays the commander with a candidate set
// against the commander before it, many headless matches at a time, and keeps what wins). The hand-set table is what every commander played with
// before: the multipliers 1, the offsets 0, the thresholds their old constants.
enum TuneId {
    TU_THR,         // army value a wave waits for (x the clock-based threshold)
    TU_FIRST,       // earliest first wave (x the difficulty's time)
    TU_LAUNCH,      // added to the launch model's log-odds (above 0: launches on a thinner margin)
    TU_CAP,         // army size cap (x)
    TU_DEF,         // turrets round the base (x)
    TU_WALLS,       // wall segments round the base (x)
    TU_REASSESS,    // a wave in sight of its objective turns back below this assault ratio
    TU_RETREAT,     // ... and breaks off when the enemy round it outweighs it this many times
    TU_REINFORCE,   // reserves join a wave under way once worth this share of the wave threshold
    TU_GUARD,       // share of the army kept home when a wave goes (added to the persona's)
    TU_HARV,        // haulers (x)
    TU_COUNTER,     // how hard the combat estimate steers the counter-pick of units (0 = not at all)
    TU_PUNISH_THR,  // FEAT_PUNISH: the counterattack goes with this share of the wave threshold ...
    TU_PUNISH_RA,   // ... when the estimate against the enemy's whole army is at least this
    TU_COUNT
};
extern const char* const TUNE_NAME[TU_COUNT];
extern const float TUNE_HAND[TU_COUNT];   // the hand-set numbers (every commander before the training played with these)
extern float TUNE_TRAINED[TU_COUNT];      // FEAT_TUNED, Hard and Brutal; ONEHOUR_TUNE="name=value,..." replaces some of them in a headless run (the trainer's candidates)
extern float TUNE_CAND[TU_COUNT];         // F2_CAND: the candidate of a training run against today's commander (ONEHOUR_TUNECAND)
extern float TUNE_TRAINED_N[TU_COUNT];    // FEAT_TUNED, Normal (trained on its own: a slower economy); Easy keeps TUNE_HAND

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
    const float* tu = TUNE_HAND;   // its numbers (see TuneId)
    const float* tuneFor(int difficulty) const { return on2(F2_CAND) && difficulty >= 2 ? TUNE_CAND : (!on(FEAT_TUNED) || difficulty < 1 ? TUNE_HAND : (difficulty == 1 ? TUNE_TRAINED_N : TUNE_TRAINED)); }
    // ---- the learning update: the counterattack (FEAT_PUNISH) and what the opponent model is told at the end of the match (FEAT_PROFILE)
    float raidPeak = 0, lastRaidT = -100, punishUntil = -1;   // the biggest enemy force at the base during the raid under way, when it was last there,
                                                              // and until when a counterattack may go in
    int punishes = 0;               // (statistics for the self-play report)
    float obsAirPeak = 0, obsInf = 0, obsVeh = 0, obsAir = 0; int obsN = 0;   // the opponent's air share at its peak, and its mix summed over the match
    float obsFirstHit = -1, obsNukeAt = -1, obsAirAt = -1;   // when it first hit the base in force, first fired a nuke and first flew three aircraft
    void observe(int enemyInf, int enemyVeh, int enemyAir, float threatValue);   // FEAT_PROFILE: one look at the opponent per think
    bool profiled = false;          // (what it saw has gone into the opponent model: AiManager::learnProfiles)
    const OppProfile* oppProf = nullptr;   // the opponent model this think consults (null: none yet, or FEAT_PROFILE off)
    u64 thinkId = 0;                // this think's number (0: outside a think), for what is worked out once per think
    int ownByType[U_COUNT] = {};    // its own units by type, counted in this think's survey
    int unitsOf(int type) const { return thinkId ? ownByType[type] : g_sim.countUnits(player, type); }
    bool useBrain = true;       // false = the original hand-tuned heuristics only (baseline for evaluation)
    bool smart = true;          // false = the oldest generation of commander: no doctrines, nuke evasion, medics, bomber tactics, ally defence
    int feat = FEAT_DEFAULT;        // which pro features are on (see AiFeat)
    bool pro = true;            // false = the previous generation (baseline for --evalai): no spending engine, combat estimates, adaptive defence or scouting
    int doctrine = DOC_BALANCED;
    int docCtx = 0;             // what the doctrine statistics were consulted for: who this commander faces (see doctrineContext)
    // what the doctrine and difficulty make of the shared build logic (1 = unchanged)
    struct Style { float first = 1, thr = 1, def = 1, tech = 1, air = 1, nuke = 1, army = 1; int airCap = 25, incomes = 2; float incomeFrom = 3.0f; } style;
    float lastSpyOrder = -100;     // when the spies were last sent after a structure
    float lastYardRally = -100, lastFleetOrder = -100, lastJeepLoad = -100, nextYardTry = 0, nextBunkerTry = 0, nextCivicTry = 0, nextFleetCall = 0;   // when the fleet was last given a target, and the jeeps last loaded
    float nextNukeLook = 0, nextPowerLook = 0, nextStrikeLook = 0;   // when a ready warhead, power or strike looks for a target again
    // a big group's move order is given a slice at a time (see groupMove): who is still waiting for it, with the order each had when it was given
    struct Pending { Ref ref; Order order; Vec2 target; Ref targetEnt; };
    struct PendingMove { std::vector<Pending> units; Vec2 to; bool attack = false; };
    std::vector<PendingMove> pendingMoves;
    void groupMove(const std::vector<Ref>& units, Vec2 to, bool attack);
    void flushMoves();
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
    int feat2 = F2_DEFAULT;         // the second set (AiFeat2)
    bool on2(int f) const { return pro && (feat2 & f) != 0; }
    // ---- The Chosen (ai_chosen.cpp)
    Ref chosenGoal;                 // the structure or the spot it is after
    Vec2 chosenGoalPos; float chosenGoalAt = -100, chosenFleeUntil = -100, nextChosenLook = 0, nextShadowOrder = 0, nextHeliOrder = 0, nextSellLook = 0;
    Ref chosenBadGoal; float chosenBadAt = -1000;   // a goal it could not reach (walled in, out on the water): left alone for a while
    std::vector<Ref> shadows;       // its Shadow Guard (led apart from the waves)
    std::vector<Ref> feint;         // F2_GUILE: the detachment feinting at the enemy's economy, and until when
    float feintUntil = -100;
    int chosenSold = 0, feints = 0, heliHunts = 0;   // (statistics)
    int chosenBought = 0; bool chosenAlive = false; float chosenLostAt = -1000, chosenFirstAt = -1;   // Chosen trained so far, when the last one fell, when the first took the field
    void chosenOps(Ref chosen, Entity* threat, float threatValue);
    void shadowOps(Ref chosen, Entity* threat, float threatValue, const std::vector<Ref>& shadowNow);
    void heliHunt(const std::vector<Ref>& aircraft, const std::vector<Entity*>& airfields, int reserve, bool hunting);
    void sellTaken();               // structures the Chosen took deep in enemy ground are sold off (the enemy loses them, the money is ours)
    bool launchFeint(std::vector<Ref>& waveUnits, Entity* mainTarget);
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
    int featMask2[MAX_PLAYERS] = { F2_DEFAULT, F2_DEFAULT, F2_DEFAULT, F2_DEFAULT, F2_DEFAULT };
    void init(u64 seed);
    void update();
    void finish(int winnerTeam);        // the match is over: every commander's doctrine learns whether it won
    void left();                        // the human left a match before its end: the opponent models still learn what it showed so far
    void learnProfiles();               // FEAT_PROFILE: one lesson per match and kind of opponent, pooled from every commander that faced it
    int focus[MAX_PLAYERS];             // FEAT_TEAM: per team index (0..MAX_PLAYERS-1 by team id modulo), the enemy army the team goes after
    float focusAt = 0;
    void updateTeamFocus();
    bool raiseDifficulty(int p);        // the human raised a computer army's difficulty mid-match (pause menu): one step up, never down
};
extern AiManager g_ai;
