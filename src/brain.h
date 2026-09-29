// One Hour - the AI's learned components (online, persisted between games)
//  * wave model:  logistic regression predicting whether an attack wave will trade favourably
//  * unit model:  per unit type linear regression predicting value destroyed per credit spent,
//                 conditioned on the observed enemy mix (infantry / vehicles / aircraft)
#pragma once
#include "common.h"
#include "data.h"

static const int WAVE_F = 5;
static const int UNIT_F = 4;

struct Brain {
    float ww[WAVE_F];
    float wu[U_COUNT][UNIT_F];
    int waveSamples = 0;
    int unitSamples[U_COUNT];
    int games = 0;
    bool learning = true;

    Brain() { reset(); }
    void reset();
    // wave: probability that launching now succeeds
    static void waveFeatures(float armyValue, float defenseValue, float enemyArmy, float minutes, int armyCount, float* x);
    float waveProb(const float* x) const;
    void learnWave(const float* x, bool success);
    // units: predicted efficiency (dealt/spent) given the enemy mix
    static void unitFeatures(float fi, float fv, float fa, float* x) { x[0] = 1; x[1] = fi; x[2] = fv; x[3] = fa; }
    float unitEff(int type, const float* x) const;
    void learnUnit(int type, const float* x, float observedEff);
    bool load(const char* path);
    bool save(const char* path) const;
    static std::string defaultPath();
};

extern Brain g_brain;
extern std::string g_brainPath;   // set only by the interactive game: where the brain is loaded from / saved to
