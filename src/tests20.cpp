// One Hour - headless tests 20: the learning update (the opponent model, the counterattack, the trained numbers, the reachable rally)
#include "ai.h"
#include "testutil.h"
#include <unistd.h>
#include <functional>

#define VERIFY20(c, m) do { if (!(c)) { fprintf(stderr, "learnupdatetest: %s\n", m); return false; } } while (0)

// A headless match between computer armies (everyone's own feature mask and opponent model), set up; run20 plays it for 'secs' seconds, asking
// 'each' after every tick (before the tick's events are cleared) whether to go on
static void aiScene20(u64 seed, int players, const int* diff, const int* masks, Brain* brains) {
    Faction fac[4] = { F_CYBER, F_CLANKER, F_CLANKER, F_CYBER }; bool ai[4] = { true, true, true, true }; int team[4] = { 0, 1, 2, 3 };
    g_sim.init(players, fac, ai, diff, team, seed);
    for (int p = 0; p < 4; p++) { g_ai.brainEnabled[p] = true; g_ai.smartEnabled[p] = true; g_ai.proEnabled[p] = true; g_ai.featMask[p] = masks[p]; g_ai.brainOverride[p] = brains ? &brains[p] : nullptr; }
    g_ai.init(seed);
}
static void run20(float secs, const std::function<bool()>& each = nullptr) {
    for (int t = 0; t < (int)(secs * SIM_HZ) && !g_sim.gameOver; t++) { g_sim.step(); g_ai.update(); bool go = !each || each(); g_sim.events.clear(); if (!go) return; }
}

bool learnUpdateTest(u64 seed) {
    std::string dir = getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp";
    std::string path = dir + "/onehour_learnupdate_" + std::to_string((long)getpid()) + ".txt";
    // ---- the opponent model learns: a running average that follows the last half dozen matches; a thing that never came counts as "not before
    // the end of the match", and a match too short to tell teaches nothing
    {
        Brain b; b.learning = true;
        float mixAir[3] = { 10, 20, 70 };
        for (int k = 0; k < 6; k++) b.learnProfile(2, 0.7f, mixAir, 240.0f, -1.0f, 300.0f, 900.0f);
        const OppProfile& P = b.prof[2];
        VERIFY20(P.n == 6 && P.airPeak > 0.6f && P.mix[2] > 0.6f && std::abs(P.firstHit - 240.0f) < 30.0f && std::abs(P.airAt - 300.0f) < 40.0f, "six air-heavy matches with an early attack did not shape the model");
        VERIFY20(P.nukeAt == OppProfile().nukeAt, "no nuke in 900 s watched says nothing against the belief 'about 1200 s'");
        { Brain e; e.learning = true; for (int k = 0; k < 6; k++) e.learnProfile(0, 0.1f, mixAir, -1.0f, 300.0f, -1.0f, 1400.0f);
          for (int k = 0; k < 3; k++) e.learnProfile(0, 0.1f, mixAir, -1.0f, -1.0f, -1.0f, 1400.0f);
          VERIFY20(e.prof[0].nukeAt > 500.0f && e.prof[0].nukeAt < 1400.0f, "matches without a nuke (1400 s watched) did not push an early-nuke belief later"); }
        VERIFY20(b.prof[0].n == 0 && b.prof[3].n == 0 && b.prof[0].airPeak == OppProfile().airPeak, "another context was touched");
        Brain c = b; float mix[3] = { 50, 50, 0 };
        c.learnProfile(2, 0.0f, mix, -1.0f, -1.0f, -1.0f, 100.0f);
        VERIFY20(c.prof[2].n == 6 && c.prof[2].airPeak == P.airPeak, "a 100 s match taught the model");
        for (int k = 0; k < 8; k++) c.learnProfile(2, 0.0f, mix, 700.0f, 500.0f, -1.0f, 1000.0f);
        VERIFY20(c.prof[2].airPeak < 0.2f && c.prof[2].firstHit > 550.0f && c.prof[2].nukeAt < 700.0f, "the model did not follow an opponent that changed its habits");
        Brain off; off.learning = false; off.learnProfile(1, 0.5f, mixAir, 100.0f, 100.0f, 100.0f, 900.0f);
        VERIFY20(off.prof[1].n == 0, "the model learned with learning off");
        off.profileLearning = true; off.learnProfile(1, 0.5f, mixAir, 100.0f, 100.0f, 100.0f, 900.0f);
        VERIFY20(off.prof[1].n == 1, "profileLearning (headless evaluation) did not let the model learn");
        float bad[3] = { std::nanf(""), 1, 1 };
        Brain d = b; d.learnProfile(2, std::nanf(""), bad, std::nanf(""), -1.0f, -1.0f, 900.0f);
        VERIFY20(std::isfinite(d.prof[2].airPeak) && std::isfinite(d.prof[2].mix[0]) && std::isfinite(d.prof[2].firstHit), "nonfinite evidence poisoned the model");
        // ---- persistence: v19 round trip; a v18 file (63 unit rows, no model) keeps everything and starts the model empty; a damaged model line
        // resets only the model
        b.unitSamples[U_C_TANK] = 17; b.docN[DOC_TURTLE] = 5;
        VERIFY20(b.save(path.c_str()), "save failed");
        Brain r; VERIFY20(r.load(path.c_str()), "a v19 file did not load");
        VERIFY20(r.prof[2].n == 6 && std::abs(r.prof[2].airPeak - P.airPeak) < 1e-3f && std::abs(r.prof[2].firstHit - P.firstHit) < 0.11f && r.unitSamples[U_C_TANK] == 17 && r.docN[DOC_TURTLE] == 5, "the opponent model did not round-trip");
        FILE* f = fopen(path.c_str(), "r"); VERIFY20(f, "cannot read the brain file");
        std::string text; char buf[512]; while (fgets(buf, sizeof buf, f)) text += buf; fclose(f);
        VERIFY20(text.compare(0, 16, "onehour-brain 19") == 0, "the file is not v19");
        // the same brain as a v18 file: header 18, only the first 63 unit rows, no model
        std::string v18 = "onehour-brain 18" + text.substr(16);
        size_t rows = v18.find('\n', v18.find('\n') + 1) + 1;   // first unit row
        size_t cut = rows; for (int k = 0; k < 63; k++) cut = v18.find('\n', cut) + 1;
        size_t doc = v18.find("doctrines");
        v18 = v18.substr(0, cut) + v18.substr(doc, v18.find("profiles") - doc);
        f = fopen(path.c_str(), "w"); fputs(v18.c_str(), f); fclose(f);
        Brain o; VERIFY20(o.load(path.c_str()), "a v18 file (the player's brain before this update) did not load");
        VERIFY20(o.unitSamples[U_C_TANK] == 17 && o.docN[DOC_TURTLE] == 5 && o.prof[2].n == 0 && o.ctxN[0][0] == b.ctxN[0][0], "a v18 file lost its rows or doctrines, or the model did not start empty");
        // ... and one of the v18 files written with 65 rows (the laser commandos had joined the table without a new version)
        std::string v18b = "onehour-brain 18" + text.substr(16);
        size_t cutb = rows; for (int k = 0; k < 65; k++) cutb = v18b.find('\n', cutb) + 1;
        v18b = v18b.substr(0, cutb) + v18b.substr(v18b.find("doctrines"), v18b.find("profiles") - v18b.find("doctrines"));
        f = fopen(path.c_str(), "w"); fputs(v18b.c_str(), f); fclose(f);
        Brain o2; VERIFY20(o2.load(path.c_str()) && o2.unitSamples[U_C_TANK] == 17 && o2.docN[DOC_TURTLE] == 5, "a 65-row v18 file did not load");
        std::string damaged = text; size_t pr = damaged.find("profiles"); damaged.replace(damaged.find('\n', pr) + 1, 3, " x ");
        f = fopen(path.c_str(), "w"); fputs(damaged.c_str(), f); fclose(f);
        Brain q; VERIFY20(q.load(path.c_str()), "a damaged model line refused the whole brain");
        VERIFY20(q.prof[2].n == 0 && q.unitSamples[U_C_TANK] == 17 && q.docN[DOC_TURTLE] == 5, "a damaged model line was not reset alone");
        remove(path.c_str());
    }
    // ---- three commanders that faced the same opponent teach the model one lesson per match, pooled (not three)
    {
        Brain shared; shared.learning = true;
        int masks[4] = { FEAT_DEFAULT | FEAT_PROFILE, FEAT_DEFAULT | FEAT_PROFILE, FEAT_DEFAULT | FEAT_PROFILE, FEAT_DEFAULT | FEAT_PROFILE };
        int diff[4] = { 2, 2, 2, 2 };
        Brain brains[4];
        aiScene20(seed, 3, diff, masks, brains);
        for (int p = 0; p < 3; p++) { AiPlayer& a = g_ai.ais[p]; a.brain = &shared; a.docCtx = 2; a.profiled = false; a.obsFirstHit = -1; a.obsNukeAt = -1; a.obsAirAt = -1; a.obsInf = a.obsVeh = a.obsAir = 0; }
        g_ai.ais[0].obsAirPeak = 0.1f; g_ai.ais[1].obsAirPeak = 0.6f; g_ai.ais[2].obsAirPeak = 0.3f;
        g_ai.ais[1].obsFirstHit = 300.0f; g_ai.ais[2].obsFirstHit = 200.0f;
        g_ai.ais[0].obsAir = 10; g_ai.ais[1].obsInf = 10;
        g_sim.time = 600.0f;
        g_ai.learnProfiles(); g_ai.learnProfiles();   // (the second call finds nothing new)
        const OppProfile& P = shared.prof[2];
        VERIFY20(P.n == 1, "one match taught the opponent model more than once");
        VERIFY20(std::abs(P.airPeak - 0.6f) < 1e-3f && std::abs(P.firstHit - 200.0f) < 0.5f && std::abs(P.mix[0] - 0.5f) < 1e-3f && std::abs(P.mix[2] - 0.5f) < 1e-3f, "the commanders' observations were not pooled (biggest air force, earliest attack, the whole mix)");
    }
    // ---- the commander readies the answer: told that this opponent fields an air force early, it raises anti-air batteries before any enemy
    // aircraft show; without the model it does not
    int aaWith = 0, aaWithout = 0; float firstAA = -1;
    for (int run = 0; run < 2; run++) {
        Brain brains[4];
        for (auto& b : brains) b.learning = false;
        const int noModel = FEAT_DEFAULT & ~FEAT_PROFILE;   // (masked out, not left out: FEAT_DEFAULT has it)
        int masks[4] = { noModel | (run == 0 ? FEAT_PROFILE : 0), noModel, noModel, noModel };
        int diff[4] = { 2, 1, 1, 1 };
        // the context p0 will see: a computer opponent of the Clanker faction
        OppProfile& P = brains[0].prof[doctrineContext(false, F_CLANKER)];
        P.n = 5; P.airPeak = 0.6f; P.mix[0] = 0.2f; P.mix[1] = 0.3f; P.mix[2] = 0.5f; P.airAt = 260.0f; P.firstHit = 900.0f; P.nukeAt = 1500.0f;
        aiScene20(seed, 2, diff, masks, brains);
        bool enemyAir = false;
        run20(390.0f, [&]() {
            for (auto& e : g_sim.ents) if (e.alive && e.owner == 1 && e.isUnit() && e.isAir() && e.ut().role == UR_COMBAT) enemyAir = true;
            if (run == 0 && firstAA < 0 && !enemyAir) for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isBuilding() && e.bt().role == BR_AATURRET) firstAA = g_sim.time;
            return true;
        });
        int aa = g_sim.countRole(0, BR_AATURRET, false);
        (run == 0 ? aaWith : aaWithout) = aa;
        VERIFY20(g_ai.ais[0].oppProf == (run == 0 ? &brains[0].prof[doctrineContext(false, F_CLANKER)] : nullptr), "the commander did not consult the opponent model (or did without FEAT_PROFILE)");
    }
    VERIFY20(aaWith >= 3 && aaWith > aaWithout && firstAA > 0, "an air force foretold by the model got no anti-air ahead of it");
    // ---- an enemy commander that has met this person before tells them what it has readied for (and one meeting them first says nothing of it)
    for (int run = 0; run < 2; run++) {
        Brain brains[4];
        for (auto& b : brains) b.learning = false;
        OppProfile& P = brains[1].prof[doctrineContext(true, F_CYBER)];
        P.n = run == 0 ? 4 : 1; P.firstHit = 200.0f;
        Faction fac[4] = { F_CYBER, F_CLANKER, F_CLANKER, F_CYBER }; bool ai[4] = { false, true, true, true }; int diff[4] = { 1, 2, 1, 1 }, team[4] = { 0, 1, 2, 3 };
        g_sim.init(2, fac, ai, diff, team, seed);
        for (int p = 0; p < 4; p++) { g_ai.brainEnabled[p] = true; g_ai.smartEnabled[p] = true; g_ai.proEnabled[p] = true; g_ai.featMask[p] = FEAT_DEFAULT | FEAT_PROFILE; g_ai.brainOverride[p] = &brains[p]; }
        g_ai.init(seed);
        bool said = false;
        run20(8.0f, [&]() { for (auto& ev : g_sim.events) if (ev.type == EV_MSG && ev.player == 0 && ev.msg.find("strike early") != std::string::npos) said = true; return true; });
        VERIFY20(said == (run == 0), run == 0 ? "a commander that knows the player's early attacks did not say so" : "a commander that barely knows the player claimed to know its habits");
    }
    // ---- the counterattack window: a raid on the base beaten off at home opens it; the window is not opened for enemies met while our wave is out
    {
        const int plain = FEAT_DEFAULT & ~(FEAT_PUNISH | FEAT_TUNED);
        int masks[4] = { plain | FEAT_PUNISH, plain, plain, plain };
        int diff[4] = { 2, 2, 1, 1 };
        aiScene20(seed + 1, 2, diff, masks, nullptr);
        run20(60.0f);
        AiPlayer& a = g_ai.ais[0];
        VERIFY20(!a.attacking && a.punishUntil < 0, "a counterattack window before any raid");
        Vec2 base = g_sim.players[0].basePos;
        std::vector<Ref> raid;
        for (int k = 0; k < 6; k++) raid.push_back(g_sim.spawnUnit(U_K_TANK, 1, g_map.nearestFree(base + Vec2((k - 3) * TILE, 6 * TILE), 6)));
        for (int k = 0; k < 10; k++) g_sim.spawnUnit(U_C_TANK, 0, g_map.nearestFree(base + Vec2((k - 5) * TILE, 3 * TILE), 6));
        bool opened = false;
        run20(60.0f, [&]() { if (a.punishUntil > g_sim.time || a.punishes > 0) opened = true; return !opened; });
        bool raidGone = true; for (Ref r : raid) if (const Entity* e = g_sim.get(r)) if (dist(e->pos, base) < 14 * TILE) raidGone = false;   // (dead, or gone home)
        if (!(raidGone && opened)) fprintf(stderr, "learnupdatetest: raid gone %d, window %d, peak %.0f, last raid %.0f s, t %.0f s, attacking %d\n", (int)raidGone, (int)opened, a.raidPeak, a.lastRaidT, g_sim.time, (int)a.attacking);
        VERIFY20(raidGone && opened, "a raid beaten off at home did not open the counterattack window");
    }
    // ---- the trained numbers: a commander plays them only with FEAT_TUNED
    {
        const int hand = FEAT_DEFAULT & ~FEAT_TUNED;
        int masks[4] = { hand | FEAT_TUNED, hand, hand, hand };
        int diff[4] = { 2, 2, 1, 1 };
        aiScene20(seed, 2, diff, masks, nullptr);
        VERIFY20(g_ai.ais[0].tu == TUNE_TRAINED && g_ai.ais[1].tu == TUNE_HAND, "FEAT_TUNED does not pick the trained numbers");
        AiPlayer& a = g_ai.ais[0];
        VERIFY20(a.tuneFor(3) == TUNE_TRAINED && a.tuneFor(2) == TUNE_TRAINED && a.tuneFor(1) == TUNE_TRAINED_N && a.tuneFor(0) == TUNE_HAND, "the wrong table for a difficulty (Brutal/Hard trained, Normal its own, Easy hand-set)");
        g_sim.players[0].difficulty = 1; run20(3.0f);   // (the pause menu can raise an army's difficulty mid-match: the next think follows it)
        VERIFY20(a.tu == TUNE_TRAINED_N, "a commander did not switch tables with its difficulty");
        bool sane = true;
        for (int k = 0; k < TU_COUNT; k++) sane = sane && std::isfinite(TUNE_TRAINED[k]) && TUNE_NAME[k] && *TUNE_NAME[k];
        VERIFY20(sane, "a trained number is not finite");
    }
    // ---- the rally stays where new units can walk to, all match long (four Brutal armies raise wall belts round their bases)
    int checks = 0;
    {
        int masks[4] = { FEAT_DEFAULT, FEAT_DEFAULT, FEAT_DEFAULT, FEAT_DEFAULT };
        int diff[4] = { 3, 3, 3, 3 };
        aiScene20(seed + 2, 4, diff, masks, nullptr);
        bool bad = false; int badP = -1; float badT = 0;
        int cutOff[4] = {};   // consecutive checks (5 s apart) with the rally cut off: a structure placed later in the same think may close it for a moment,
                              // until the next think moves the rally; only one that stays cut off is a fault
        run20(900.0f, [&]() {
            if (g_sim.tick % (5 * SIM_HZ) != 0) return true;
            for (int p = 0; p < 4; p++) {
                const AiPlayer& a = g_ai.ais[p];
                if (!g_sim.players[p].alive) continue;
                const Entity* bar = nullptr;
                for (auto& e : g_sim.ents) if (e.alive && e.owner == p && e.isBuilding() && e.constructed && e.bt().role == BR_BARRACKS) { bar = &e; break; }
                if (!bar) continue;
                Vec2 ex = g_sim.unitExit(*bar);
                if (g_map.region[clampi(tileOf(ex.y), 0, MAP_H - 1) * MAP_W + clampi(tileOf(ex.x), 0, MAP_W - 1)] == 0) continue;
                checks++;
                if (!g_map.sameGround(a.rally, ex)) {
                    // (the first barracks may not be the one the rally was checked against: any production exit that reaches it will do)
                    bool any = false;
                    for (auto& e : g_sim.ents) if (e.alive && e.owner == p && e.isBuilding() && e.constructed && (e.bt().role == BR_BARRACKS || e.bt().role == BR_FACTORY) && g_map.sameGround(a.rally, g_sim.unitExit(e))) any = true;
                    if (!any && ++cutOff[p] >= 2) { bad = true; badP = p; badT = g_sim.time; return false; }
                    if (any) cutOff[p] = 0;
                } else cutOff[p] = 0;
            }
            return true;
        });
        if (bad) fprintf(stderr, "learnupdatetest: army %d's rally cut off from its production at %.0f s\n", badP, badT);
        VERIFY20(!bad && checks > 100, "a rally the new units cannot walk to");
    }
    printf("learnupdatetest: opponent model learns, forgets and persists (v19, v18 files load), anti-air ahead of a foretold air force (%d batteries from %.0f s, %d without the model), a known player told what the commander readied for, counterattack window after a raid, trained numbers behind FEAT_TUNED, rally reachable in %d checks\n", aaWith, firstAA, aaWithout, checks);
    return true;
}
