// One Hour - skirmish AI (utility-driven base building, economy, army composition and attack waves)
#pragma once
#include "sim.h"
#include "brain.h"

// Features of the pro commander, each switchable on its own so --evalai can measure what a single one is worth (ONEHOUR_BASEMASK)
// (the estimate-based retreat was tried and measured no better than the cost-based one)
enum AiFeat { FEAT_MACRO = 1, FEAT_LAUNCH = 2, FEAT_COMP = 4, FEAT_ECON = 8, FEAT_ALLY = 16, FEAT_SPECIAL = 32,   // FEAT_SPECIAL: spy drones with the army, spies that capture structures
              FEAT_NAVY = 64,    // a Naval Yard on the lake, a fleet of its own, the Guard Drones upgrade, jeeps loaded with infantry
              FEAT_AEGIS = 128,  // anti-nuke defences round the base and nuke targeting that reckons with the enemy's (and fires a salvo at a covered target)
              FEAT_RECOVER = 256,   // an army that has lost most of its base rebuilds the core first, with every free dozer, and keeps its last units home
              FEAT_CIVIC = 512,     // a civilian dozer raises a town behind the base once the economy is running (income and free dozers)
              FEAT_DEEPNUKE = 1024, // a rich commander with a Nuke Ramp sinks an Underground Nuke silo and fires its silver warhead
              FEAT_ALL = 0xFFFF,
    FEAT_DEFAULT = FEAT_MACRO | FEAT_LAUNCH | FEAT_COMP | FEAT_ECON | FEAT_ALLY | FEAT_SPECIAL | FEAT_NAVY | FEAT_AEGIS | FEAT_RECOVER | FEAT_CIVIC | FEAT_DEEPNUKE };   // what ships

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
    float lastFleetOrder = -100, lastJeepLoad = -100, nextYardTry = 0, nextBunkerTry = 0, nextCivicTry = 0;   // when the fleet was last given a target, and the jeeps last loaded
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

    bool on(int f) const { return pro && (feat & f) != 0; }
    void init(int p, u64 seed);
    void endWave(float remainingValue);
    void learnOutcomes();
    void beginWaveLearning();
    void garrisonStrongholds();
    void think();
    void dodgeNukes();                  // cheap per-tick check: pull units out of the circle of an incoming enemy nuke
    bool findSpot(int buildType, Vec2 preferNear, int maxRing, int& tx, int& ty);
    bool findBunkerSpot(int buildType, int& tx, int& ty);
private:
    bool tryBuild(int buildType, Vec2 preferNear, int maxRing = 22);
    int chooseUnit(BuildRole role, int enemyInf, int enemyVeh, int enemyAir);
    Entity* pickAttackTarget();
    float assaultRatio(const std::vector<Ref>& attackers, Vec2 at, float radiusTiles, bool wholeEnemy);
    float enemyStrengthNear(Vec2 p, float radiusTiles);
    Entity* idleDozer(bool civic = false);   // civic: a civilian dozer (they raise only the town; the others only the base)
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
};
extern AiManager g_ai;
