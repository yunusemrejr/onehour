// One Hour - skirmish AI (utility-driven base building, economy, army composition and attack waves)
#pragma once
#include "sim.h"
#include "brain.h"

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

    bool useBrain = true;       // false = the original hand-tuned heuristics only (baseline for evaluation)
    float waveX[WAVE_F] = {};   // features at launch, for learning from the outcome
    float waveDealt0 = 0;       // enemy value destroyed by this player when the wave launched
    bool waveHasSample = false;
    float nextLearn = 30;
    float prevSpent[U_COUNT] = {}, prevDealt[U_COUNT] = {};
    float mixFi = 0.33f, mixFv = 0.33f, mixFa = 0.33f;

    void init(int p, u64 seed);
    void endWave(float remainingValue);
    void think();
private:
    bool findSpot(int buildType, Vec2 preferNear, int maxRing, int& tx, int& ty);
    bool tryBuild(int buildType, Vec2 preferNear, int maxRing = 22);
    int chooseUnit(BuildRole role, int enemyInf, int enemyVeh, int enemyAir);
    Entity* pickAttackTarget();
    float enemyStrengthNear(Vec2 p, float radiusTiles);
    Entity* idleDozer();
    void managePower();
};

struct AiManager {
    AiPlayer ais[MAX_PLAYERS];
    bool brainEnabled[MAX_PLAYERS] = { true, true, true, true };
    void init(u64 seed);
    void update();
};
extern AiManager g_ai;
