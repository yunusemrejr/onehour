// One Hour - skirmish AI (utility-driven base building, economy, army composition and attack waves)
#pragma once
#include "sim.h"
#include "brain.h"

// Features of the pro commander, each switchable on its own so --evalai can measure what a single one is worth (ONEHOUR_BASEMASK)
// (the estimate-based retreat was tried and measured no better than the cost-based one)
enum AiFeat { FEAT_MACRO = 1, FEAT_LAUNCH = 2, FEAT_COMP = 4, FEAT_ECON = 8, FEAT_ALLY = 16, FEAT_SPECIAL = 32, FEAT_ALL = 0xFFFF,   // FEAT_SPECIAL: spy drones with the army, spies that capture structures
    FEAT_DEFAULT = FEAT_MACRO | FEAT_LAUNCH | FEAT_COMP | FEAT_ECON | FEAT_ALLY | FEAT_SPECIAL };   // what ships

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
    struct Style { float first = 1, thr = 1, def = 1, tech = 1, air = 1, nuke = 1, army = 1; int airCap = 6, incomes = 2; float incomeFrom = 3.0f; } style;
    float lastSpyOrder = -100;     // when the spies were last sent after a structure
    float lastBombRun = -100, lastAllyHelp = -100, lastNukeDodge = -100, retreatVotes = 0;
    int dodges = 0, medicsBuilt = 0;   // statistics for the self-play reports
    float waveX[WAVE_F] = {};   // features at launch, for learning from the outcome
    float waveDealt0 = 0;       // enemy value destroyed by this player when the wave launched
    bool waveHasSample = false;
    float nextLearn = 30;
    float prevSpent[U_COUNT] = {}, prevDealt[U_COUNT] = {};
    float mixFi = 0.33f, mixFv = 0.33f, mixFa = 0.33f;
    Vec2 humanFront; bool hasFront = false;   // where a human teammate's army is fighting (computer allies converge on it)
    float lastJoin = -100, lastAllyNote = -100;
    float incomeRate = 0, incomeAt = 0, incomeT = 0;   // credits per second coming in, measured over the last ten seconds or so

    bool on(int f) const { return pro && (feat & f) != 0; }
    void init(int p, u64 seed);
    void endWave(float remainingValue);
    void think();
    void dodgeNukes();                  // cheap per-tick check: pull units out of the circle of an incoming enemy nuke
private:
    bool findSpot(int buildType, Vec2 preferNear, int maxRing, int& tx, int& ty);
    bool tryBuild(int buildType, Vec2 preferNear, int maxRing = 22);
    int chooseUnit(BuildRole role, int enemyInf, int enemyVeh, int enemyAir);
    Entity* pickAttackTarget();
    float assaultRatio(const std::vector<Ref>& attackers, Vec2 at, float radiusTiles, bool wholeEnemy);
    float enemyStrengthNear(Vec2 p, float radiusTiles);
    Entity* idleDozer();
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
