// One Hour - skirmish AI (utility-driven base building, economy, army composition and attack waves)
#pragma once
#include "sim.h"

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

    void init(int p, u64 seed);
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
    void init(u64 seed);
    void update();
};
extern AiManager g_ai;
