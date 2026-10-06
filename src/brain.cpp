#include "brain.h"

Brain g_brain;
std::string g_brainPath;

void Brain::reset() {
    // prior = the hand-written heuristic: launch when the army is ~1.3x the defence it meets
    for (int i = 0; i < WAVE_F; i++) ww[i] = 0;
    ww[0] = -0.35f; ww[1] = 2.2f;
    for (int t = 0; t < U_COUNT; t++) { unitSamples[t] = 0; for (int i = 0; i < UNIT_F; i++) wu[t][i] = 0; wu[t][0] = 1.0f; }
    waveSamples = 0; games = 0;
    for (int d = 0; d < DOCTRINES; d++) { docQ[d] = 0.5f; docN[d] = 0; }
}

void Brain::waveFeatures(float A, float D, float E, float minutes, int armyCount, float* x) {
    x[0] = 1.0f;
    x[1] = clampf(std::log((A + 500.0f) / (D + 500.0f)), -2.5f, 2.5f);          // army vs defence at the target
    x[2] = clampf(std::log((A + 500.0f) / (E + 500.0f)), -2.5f, 2.5f) * 0.5f;   // army vs the enemy field army
    x[3] = std::min(minutes, 30.0f) / 15.0f - 1.0f;
    x[4] = std::min((float)armyCount, 40.0f) / 20.0f - 1.0f;
}
static float sigmoid(float z) { return 1.0f / (1.0f + std::exp(-clampf(z, -12, 12))); }
float Brain::waveProb(const float* x) const { float z = 0; for (int i = 0; i < WAVE_F; i++) z += ww[i] * x[i]; return sigmoid(z); }
void Brain::learnWave(const float* x, bool success) {
    if (!learning) return;
    float p = waveProb(x), err = (success ? 1.0f : 0.0f) - p;
    float lr = 0.25f / (1.0f + waveSamples * 0.02f) + 0.03f;     // decaying step, small floor to keep adapting
    for (int i = 0; i < WAVE_F; i++) ww[i] = clampf(ww[i] + lr * err * x[i], -6, 6);
    waveSamples++;
}

float Brain::unitEff(int type, const float* x) const { float z = 0; for (int i = 0; i < UNIT_F; i++) z += wu[type][i] * x[i]; return clampf(z, 0.05f, 6.0f); }
void Brain::learnUnit(int type, const float* x, float obs) {
    if (!learning) return;
    obs = clampf(obs, 0, 4.0f);
    float pred = 0; for (int i = 0; i < UNIT_F; i++) pred += wu[type][i] * x[i];
    float err = obs - pred, lr = 0.15f / (1.0f + unitSamples[type] * 0.05f) + 0.01f;
    for (int i = 0; i < UNIT_F; i++) wu[type][i] = clampf(wu[type][i] + lr * err * x[i], -4, 6);
    unitSamples[type]++;
}

int Brain::pickDoctrine(Rng& rng) const {
    int total = 0; for (int d = 0; d < DOCTRINES; d++) total += docN[d];
    int best = DOC_BALANCED; float bs = -1e9f;
    for (int d = 0; d < DOCTRINES; d++) {
        float bonus = learning ? 0.45f * std::sqrt(std::log((float)total + 2.0f) / (docN[d] + 1.0f)) : 0.0f;
        float sc = docQ[d] + bonus + rng.f(0.0f, 0.04f);
        if (sc > bs) { bs = sc; best = d; }
    }
    return best;
}
void Brain::learnDoctrine(int d, float reward) {
    if (!learning || d < 0 || d >= DOCTRINES) return;
    docN[d]++;
    docQ[d] += (reward - docQ[d]) / std::min(docN[d] + 1.0f, 24.0f);   // a running average that forgets slowly, so the table follows the player's habits
}

std::string Brain::defaultPath() {
    if (const char* e = getenv("ONEHOUR_BRAIN")) return e;
    const char* home = getenv("HOME");
    if (!home) return "";
    std::string dir = std::string(home) + "/.local/share/onehour";
    std::string cmd = "mkdir -p '" + dir + "' 2>/dev/null";
    if (system(cmd.c_str()) != 0) return "";
    return dir + "/brain.txt";
}

bool Brain::save(const char* path) const {
    FILE* f = fopen(path, "w");
    if (!f) return false;
    fprintf(f, "onehour-brain 7 %d %d\n", games, waveSamples);   // v7: the Hornet Gunship joined the unit table; v5: doctrine statistics follow the unit rows (v4: the medics joined the unit table, v3: the jets); per-type rows are indexed by UnitTypeId
    for (int i = 0; i < WAVE_F; i++) fprintf(f, "%.5f ", ww[i]);
    fprintf(f, "\n");
    for (int t = 0; t < U_COUNT; t++) { fprintf(f, "%d", unitSamples[t]); for (int i = 0; i < UNIT_F; i++) fprintf(f, " %.5f", wu[t][i]); fprintf(f, "\n"); }
    fprintf(f, "doctrines");
    for (int d = 0; d < DOCTRINES; d++) fprintf(f, " %d %.5f", docN[d], docQ[d]);
    fprintf(f, "\n");
    fclose(f);
    return true;
}

bool Brain::load(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return false;
    Brain b;
    int ver = 0;
    bool ok = fscanf(f, "onehour-brain %d %d %d", &ver, &b.games, &b.waveSamples) == 3 && (ver >= 2 && ver <= 7);   // older layouts index a different unit table: start fresh
    for (int i = 0; ok && i < WAVE_F; i++) ok = fscanf(f, "%f", &b.ww[i]) == 1;
    // v2 had 22 rows (11 per army, no jets): map them onto the new table, the jets keep the prior
    for (int row = 0; ok && row < (ver == 2 ? 22 : (ver == 3 ? 24 : (ver <= 5 ? 26 : (ver == 6 ? 28 : (int)U_COUNT)))); row++) {   // v4 and v5 share a 26 row table (the snipers joined in v6, the Hornet in v7: newcomers keep the prior)
        int t = row;
        if (ver == 2 && row >= 11) t = row + 1;            // the Clanker block moved up by one to make room for the Cyber jet
        ok = fscanf(f, "%d", &b.unitSamples[t]) == 1;
        for (int i = 0; ok && i < UNIT_F; i++) ok = fscanf(f, "%f", &b.wu[t][i]) == 1;
    }
    if (ok && ver >= 5) {   // a missing or damaged doctrine line only resets the doctrines
        char tag[16] = {};
        bool dok = fscanf(f, "%15s", tag) == 1 && std::strcmp(tag, "doctrines") == 0;
        for (int d = 0; dok && d < DOCTRINES; d++) dok = fscanf(f, "%d %f", &b.docN[d], &b.docQ[d]) == 2;
        if (!dok) for (int d = 0; d < DOCTRINES; d++) { b.docN[d] = 0; b.docQ[d] = 0.5f; }
    }
    fclose(f);
    if (!ok) return false;
    bool lrn = learning;
    *this = b; learning = lrn;
    return true;
}
