// One Hour - the AI's learned components (online, persisted between games)
//  * wave model:  logistic regression predicting whether an attack wave will trade favourably
//  * unit model:  per unit type linear regression predicting value destroyed per credit spent,
//                 conditioned on the observed enemy mix (infantry / vehicles / aircraft)
#pragma once
#include "common.h"
#include "data.h"

static const int WAVE_F = 5;
static const int UNIT_F = 4;
// Strategic doctrines an AI commander can open a game with. Which one it picks is a multi-armed bandit learned from match results.
static const int DOCTRINES = 5;
enum Doctrine { DOC_BALANCED = 0, DOC_RUSH, DOC_TURTLE, DOC_AIR, DOC_BOOM };
static const char* const DOCTRINE_NAME[DOCTRINES] = { "balanced", "rush", "turtle", "air", "boom" };
// What the doctrine statistics are kept for: who the commander faces. A doctrine that beats the computer commanders need not beat a person, and
// each army has its own habits, so the table is split by (opponent is a human) x (opponent's faction) and shrunk toward the overall table
// while a context has few games.
static const int DOC_CTX = 4;
static inline int doctrineContext(bool opponentHuman, int opponentFaction) { return (opponentHuman ? 2 : 0) + (opponentFaction == 1 ? 1 : 0); }

struct Brain {
    float ww[WAVE_F];
    float wu[U_COUNT][UNIT_F];
    int waveSamples = 0;
    int unitSamples[U_COUNT];
    int games = 0;
    bool learning = true;
    float docQ[DOCTRINES];     // average reward (win = 1) of each doctrine, over every opponent
    int docN[DOCTRINES];
    float ctxQ[DOC_CTX][DOCTRINES];   // the same per context (see doctrineContext)
    int ctxN[DOC_CTX][DOCTRINES];

    Brain() { reset(); }
    void reset();
    // wave: probability that launching now succeeds
    static void waveFeatures(float armyValue, float defenseValue, float enemyArmy, float minutes, int armyCount, float* x);
    // estimate-based features: ln of the assault ratio at the target, ln of the ratio against the whole enemy army, minutes, army size
    static void waveFeaturesR(float ratioTarget, float ratioAll, float minutes, int armyCount, float* x);
    float waveProb(const float* x) const;
    void learnWave(const float* x, bool success);
    // units: predicted efficiency (dealt/spent) given the enemy mix
    static void unitFeatures(float fi, float fv, float fa, float* x) { x[0] = 1; x[1] = fi; x[2] = fv; x[3] = fa; }
    float unitEff(int type, const float* x) const;
    void learnUnit(int type, const float* x, float observedEff);
    // doctrines: UCB1 over the average reward, so promising openings are repeated and the others still get tried
    int pickDoctrine(Rng& rng, int ctx = 0) const;
    float doctrineValue(int ctx, int d) const;   // the context's average reward, shrunk toward the overall one while it has few games
    void learnDoctrine(int ctx, int d, float reward);
    bool load(const char* path);
    bool save(const char* path) const;
    static std::string defaultPath();
};

extern Brain g_brain;
extern std::string g_brainPath;   // set only by the interactive game: where the brain is loaded from / saved to
