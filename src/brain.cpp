#include "brain.h"
#include <filesystem>
#include <unistd.h>

Brain g_brain;
std::string g_brainPath;

void Brain::reset() {
    // prior = the hand-written heuristic: launch when the army is ~1.3x the defence it meets
    for (int i = 0; i < WAVE_F; i++) ww[i] = 0;
    ww[0] = -0.9f; ww[1] = 3.2f; ww[2] = 0.5f; ww[3] = 0.35f; ww[4] = 0.25f;
    for (int t = 0; t < U_COUNT; t++) { unitSamples[t] = 0; for (int i = 0; i < UNIT_F; i++) wu[t][i] = 0; wu[t][0] = 1.0f; }
    waveSamples = 0; games = 0;
    for (int d = 0; d < DOCTRINES; d++) { docQ[d] = 0.5f; docN[d] = 0; }
    for (int c = 0; c < DOC_CTX; c++) for (int d = 0; d < DOCTRINES; d++) { ctxQ[c][d] = 0.5f; ctxN[c][d] = 0; }
    for (int c = 0; c < DOC_CTX; c++) prof[c] = OppProfile();
}

void Brain::waveFeatures(float A, float D, float E, float minutes, int armyCount, float* x) {
    x[0] = 1.0f;
    x[1] = clampf(std::log((A + 500.0f) / (D + 500.0f)), -2.5f, 2.5f);          // army vs defence at the target
    x[2] = clampf(std::log((A + 500.0f) / (E + 500.0f)), -2.5f, 2.5f) * 0.5f;   // army vs the enemy field army
    x[3] = std::min(minutes, 30.0f) / 15.0f - 1.0f;
    x[4] = std::min((float)armyCount, 40.0f) / 20.0f - 1.0f;
}
void Brain::waveFeaturesR(float Rt, float Ra, float minutes, int armyCount, float* x) {
    x[0] = 1.0f;
    x[1] = clampf(std::log(std::max(Rt, 0.05f)), -2.5f, 2.5f);
    x[2] = clampf(std::log(std::max(Ra, 0.05f)), -2.5f, 2.5f) * 0.5f;
    x[3] = std::min(minutes, 30.0f) / 15.0f - 1.0f;
    x[4] = std::min((float)armyCount, 40.0f) / 20.0f - 1.0f;
}
static float sigmoid(float z) { return 1.0f / (1.0f + std::exp(-clampf(z, -12, 12))); }
static bool finiteFeatures(const float* x, int n) {
    for (int i = 0; i < n; i++) if (!std::isfinite(x[i]) || std::abs(x[i]) > 8) return false;
    return true;
}
float Brain::waveProb(const float* x) const { if (!finiteFeatures(x, WAVE_F)) return 0.5f; float z = 0; for (int i = 0; i < WAVE_F; i++) z += ww[i] * x[i]; return sigmoid(z); }
// Online logistic regression pulled toward the prior: waves only launch where the estimate already says they should win, so the outcomes cover a narrow
// band of ratios and every single result is noisy. A small step and a pull back toward the calibrated weights let the model follow what the opponents
// really do (it takes many waves to move far) without being thrown off by a lucky or unlucky few.
void Brain::learnWave(const float* x, bool success) {
    if (!learning || !finiteFeatures(x, WAVE_F)) return;
    static const Brain prior;
    float p = waveProb(x), err = (success ? 1.0f : 0.0f) - p;
    float lr = 0.06f / (1.0f + waveSamples * 0.02f) + 0.008f;
    for (int i = 0; i < WAVE_F; i++) ww[i] = clampf(ww[i] + lr * (err * x[i] - 0.25f * (ww[i] - prior.ww[i])), -6, 6);
    waveSamples = std::min(waveSamples + 1, 100000000);
}

float Brain::unitEff(int type, const float* x) const {
    if (type < 0 || type >= U_COUNT || !finiteFeatures(x, UNIT_F)) return 1.0f;
    float z = 0; for (int i = 0; i < UNIT_F; i++) z += wu[type][i] * x[i];
    return clampf(z, 0.05f, 6.0f);
}
void Brain::learnUnit(int type, const float* x, float obs) {
    if (!learning || type < 0 || type >= U_COUNT || !std::isfinite(obs) || !finiteFeatures(x, UNIT_F)) return;
    obs = clampf(obs, 0, 4.0f);
    float pred = 0; for (int i = 0; i < UNIT_F; i++) pred += wu[type][i] * x[i];
    float norm = 0; for (int i = 0; i < UNIT_F; i++) norm += x[i] * x[i];
    float err = obs - pred, lr = (0.15f / (1.0f + unitSamples[type] * 0.05f) + 0.01f) / std::max(1.0f, norm);
    for (int i = 0; i < UNIT_F; i++) {
        float prior = i == 0 ? 1.0f : 0.0f;
        wu[type][i] = clampf(wu[type][i] + lr * (err * x[i] - 0.01f * (wu[type][i] - prior)), -4, 6);
    }
    unitSamples[type] = std::min(unitSamples[type] + 1, 100000000);
}

float Brain::doctrineValue(int ctx, int d) const {
    const float K = 4.0f;   // the overall table counts as this many games in every context
    ctx = clampi(ctx, 0, DOC_CTX - 1);
    return (ctxN[ctx][d] * ctxQ[ctx][d] + K * docQ[d]) / (ctxN[ctx][d] + K);
}
int Brain::pickDoctrine(Rng& rng, int ctx, int exclude) const {
    ctx = clampi(ctx, 0, DOC_CTX - 1);
    int total = 0; for (int d = 0; d < DOCTRINES; d++) total += ctxN[ctx][d];
    int best = DOC_BALANCED; float bs = -1e9f;
    for (int d = 0; d < DOCTRINES; d++) {
        if (exclude >> d & 1) continue;
        float bonus = learning ? 0.45f * std::sqrt(std::log((float)total + 2.0f) / (ctxN[ctx][d] + 1.0f)) : 0.0f;
        float sc = doctrineValue(ctx, d) + bonus + rng.f(0.0f, 0.04f);
        if (sc > bs) { bs = sc; best = d; }
    }
    return best;
}
void Brain::learnDoctrine(int ctx, int d, float reward) {
    if (!learning || d < 0 || d >= DOCTRINES || !std::isfinite(reward)) return;
    reward = clampf(reward, 0, 1);
    ctx = clampi(ctx, 0, DOC_CTX - 1);
    docN[d] = std::min(docN[d] + 1, 100000000);
    docQ[d] += (reward - docQ[d]) / std::min(docN[d] + 1.0f, 24.0f);   // a running average that forgets slowly, so the table follows the player's habits
    ctxN[ctx][d] = std::min(ctxN[ctx][d] + 1, 100000000);
    ctxQ[ctx][d] += (reward - ctxQ[ctx][d]) / std::min(ctxN[ctx][d] + 1.0f, 16.0f);
}

void Brain::learnProfile(int ctx, float airPeak, const float* mix, float firstHit, float nukeAt, float airAt, float span) {
    if (!(learning || profileLearning) || !std::isfinite(span) || span < 150.0f) return;
    ctx = clampi(ctx, 0, DOC_CTX - 1);
    OppProfile& P = prof[ctx];
    const float a = 1.0f / std::min(P.n + 1.0f, 6.0f);   // a running average over about the last six matches
    auto lerpTo = [&](float& v, float x) { if (std::isfinite(x)) v += (x - v) * a; };
    lerpTo(P.airPeak, clampf(airPeak, 0, 1));
    float ms = mix[0] + mix[1] + mix[2];
    if (std::isfinite(ms) && ms > 0.01f) for (int k = 0; k < 3; k++) lerpTo(P.mix[k], mix[k] / ms);
    // a time: what happened, or "not before the end of what was watched" - which only says something once the watch passed the current belief
    auto time = [&](float& v, float t, float cap) { if (t >= 0) lerpTo(v, std::min(t, cap)); else if (span > v) lerpTo(v, std::min(span, cap)); };
    time(P.firstHit, firstHit, 1200.0f);
    time(P.nukeAt, nukeAt, 1800.0f);
    time(P.airAt, airAt, 1500.0f);
    P.n = std::min(P.n + 1, 100000000);
}

std::string Brain::defaultPath() {
    if (const char* e = getenv("ONEHOUR_BRAIN")) return e;
    const char* home = getenv("HOME");
    if (!home) return "";
    std::string dir = std::string(home) + "/.local/share/onehour";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) return "";
    return dir + "/brain.txt";
}

bool Brain::save(const char* path) const {
    if (!path || !*path) return false;
    // Write beside the destination and replace only after a complete, flushed write.
    // A crash or a full disk must not truncate the last good learned state.
    std::string pattern = std::string(path) + ".tmp.XXXXXX";
    std::vector<char> tmp(pattern.begin(), pattern.end()); tmp.push_back('\0');
    int fd = mkstemp(tmp.data());
    if (fd < 0) return false;
    FILE* f = fdopen(fd, "w");
    if (!f) { close(fd); remove(tmp.data()); return false; }
    fprintf(f, "onehour-brain 19 %d %d\n", games, waveSamples);   // v19: the opponent model follows the contexts, the laser commandos are in the unit table (v18 files have 63 unit rows, or 65); v18: the aircraft carriers, cargo ships and carrier jets joined the unit table (v17 files have 57 rows); v17: the rioters joined the unit table (v16 files have 55 rows); v16: the patrol boats joined the unit table (v15 files have 53 rows); v15: the plain fighters and the call-in nuclear bombers joined the unit table (v14 files have 49 rows); v14: the Anti-Radiation Trucks joined the unit table (v13 files have 47 rows); v13: the Supersonic Bombers joined it (v12 files have 45 rows); v12: the civilian dozers joined it (v11 files have 43 rows); v11: the jeeps, warships and strike fighters joined it (v10 files have 35 rows); v10: the spies, spy drones and cargo choppers joined it (v9 files have 29 rows);   // v7: the Hornet Gunship joined the unit table; v5: doctrine statistics follow the unit rows (v4: the medics joined the unit table, v3: the jets); per-type rows are indexed by UnitTypeId
    for (int i = 0; i < WAVE_F; i++) fprintf(f, "%.5f ", ww[i]);
    fprintf(f, "\n");
    for (int t = 0; t < U_COUNT; t++) { fprintf(f, "%d", unitSamples[t]); for (int i = 0; i < UNIT_F; i++) fprintf(f, " %.5f", wu[t][i]); fprintf(f, "\n"); }
    fprintf(f, "doctrines");
    for (int d = 0; d < DOCTRINES; d++) fprintf(f, " %d %.5f", docN[d], docQ[d]);
    fprintf(f, "\n");
    fprintf(f, "contexts\n");   // v9: the doctrine table per opponent context
    for (int c = 0; c < DOC_CTX; c++) { for (int d = 0; d < DOCTRINES; d++) fprintf(f, " %d %.5f", ctxN[c][d], ctxQ[c][d]); fprintf(f, "\n"); }
    fprintf(f, "profiles\n");   // v19: the opponent model per context
    for (int c = 0; c < DOC_CTX; c++) { const OppProfile& P = prof[c]; fprintf(f, " %d %.4f %.4f %.4f %.4f %.1f %.1f %.1f\n", P.n, P.airPeak, P.mix[0], P.mix[1], P.mix[2], P.firstHit, P.nukeAt, P.airAt); }
    bool ok = !ferror(f) && fflush(f) == 0 && fsync(fd) == 0;
    if (fclose(f) != 0) ok = false;
    if (ok && rename(tmp.data(), path) == 0) return true;
    remove(tmp.data()); return false;
}

bool Brain::load(const char* path) {
    if (!path || !*path) return false;
    FILE* f = fopen(path, "r");
    if (!f) return false;
    Brain b;
    int ver = 0;
    bool ok = fscanf(f, "onehour-brain %d %d %d", &ver, &b.games, &b.waveSamples) == 3 && (ver >= 2 && ver <= 19);   // older layouts index a different unit table: start fresh
    for (int i = 0; ok && i < WAVE_F; i++) { float v = 0; ok = fscanf(f, "%f", &v) == 1; if (ver >= 8) b.ww[i] = v; }   // (v8: the wave features are the combat-estimate ratios; older weights mean something else, so the prior stays)
    if (ver < 8) b.waveSamples = 0;
    // v18 was written with 63 unit rows, and for a while with 65 (the laser commandos joined the table without a new version): count them
    int rows18 = 63;
    if (ok && ver == 18) {
        long at = ftell(f); char line[512]; int n = -1;   // (the first line read is the end of the wave weights' line)
        while (fgets(line, sizeof line, f) && strncmp(line, "doctrines", 9) != 0) n++;
        if (n == 63 || n == 65) rows18 = n;
        fseek(f, at, SEEK_SET);
    }
    // v2 had 22 rows (11 per army, no jets): map them onto the new table, the jets keep the prior
    for (int row = 0; ok && row < (ver == 2 ? 22 : (ver == 3 ? 24 : (ver <= 5 ? 26 : (ver == 6 ? 28 : (ver <= 9 ? 29 : (ver == 10 ? 35 : (ver == 11 ? 43 : (ver == 12 ? 45 : (ver == 13 ? 47 : (ver == 14 ? 49 : (ver == 15 ? 53 : (ver == 16 ? 55 : (ver == 17 ? 57 : (ver == 18 ? rows18 : (int)U_COUNT)))))))))))))); row++) {   // v4 and v5 share a 26 row table (the snipers joined in v6, the Hornet in v7, the civilians in v12, the Supersonic Bombers in v13 the Anti-Radiation Trucks in v14, the fighters and call-in bombers in v15: newcomers keep the prior)
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
        if (dok && ver >= 9) {   // (older files have no contexts: they start empty and lean on the overall table)
            bool cok = fscanf(f, "%15s", tag) == 1 && std::strcmp(tag, "contexts") == 0;
            for (int c = 0; cok && c < DOC_CTX; c++) for (int d = 0; cok && d < DOCTRINES; d++) cok = fscanf(f, "%d %f", &b.ctxN[c][d], &b.ctxQ[c][d]) == 2;
            if (!cok) for (int c = 0; c < DOC_CTX; c++) for (int d = 0; d < DOCTRINES; d++) { b.ctxN[c][d] = 0; b.ctxQ[c][d] = 0.5f; }
            if (cok && ver >= 19) {   // (a missing or damaged opponent model only resets the model)
                bool pok = fscanf(f, "%15s", tag) == 1 && std::strcmp(tag, "profiles") == 0;
                for (int c = 0; pok && c < DOC_CTX; c++) { OppProfile& P = b.prof[c]; pok = fscanf(f, "%d %f %f %f %f %f %f %f", &P.n, &P.airPeak, &P.mix[0], &P.mix[1], &P.mix[2], &P.firstHit, &P.nukeAt, &P.airAt) == 8; }
                if (!pok) for (int c = 0; c < DOC_CTX; c++) b.prof[c] = OppProfile();
            }
        }
    }
    fclose(f);
    // Validate before installing anything; NaN, negative counts and huge counters
    // otherwise poison all future matches or overflow the bandit's arithmetic.
    auto countOk = [](int n) { return n >= 0 && n <= 100000000; };
    ok = ok && countOk(b.games) && countOk(b.waveSamples);
    for (float v : b.ww) ok = ok && std::isfinite(v) && v >= -6 && v <= 6;
    for (int t = 0; t < U_COUNT; t++) {
        ok = ok && countOk(b.unitSamples[t]);
        for (float v : b.wu[t]) ok = ok && std::isfinite(v) && v >= -4 && v <= 6;
    }
    for (int d = 0; d < DOCTRINES; d++) {
        if (!countOk(b.docN[d]) || !std::isfinite(b.docQ[d]) || b.docQ[d] < 0 || b.docQ[d] > 1) { b.docN[d] = 0; b.docQ[d] = 0.5f; }
        for (int c = 0; c < DOC_CTX; c++)
            if (!countOk(b.ctxN[c][d]) || !std::isfinite(b.ctxQ[c][d]) || b.ctxQ[c][d] < 0 || b.ctxQ[c][d] > 1) { b.ctxN[c][d] = 0; b.ctxQ[c][d] = 0.5f; }
    }
    for (int c = 0; c < DOC_CTX; c++) {
        const OppProfile& P = b.prof[c];
        bool pok = countOk(P.n) && P.airPeak >= 0 && P.airPeak <= 1 && P.firstHit >= 0 && P.firstHit <= 1200 && P.nukeAt >= 0 && P.nukeAt <= 1800 && P.airAt >= 0 && P.airAt <= 1500;
        for (float m : P.mix) pok = pok && m >= 0 && m <= 1;   // (NaN fails every comparison)
        if (!pok) b.prof[c] = OppProfile();
    }
    if (!ok) return false;
    bool lrn = learning, plrn = profileLearning;
    *this = b; learning = lrn; profileLearning = plrn;
    return true;
}
