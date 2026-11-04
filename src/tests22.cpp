// One Hour - --balancematrix: what a credit buys in a straight fight. Every combat unit of one army meets every combat unit of the other
// with the same money behind each side (BM_BUDGET), on open ground, both told to attack-move through each other. The score of a fight is the
// value (cost x health share) the first side has left minus what the second has left, over the budget: +1 a clean win, -1 a clean loss.
// Rows are printed per unit with the average over its opponents, so outliers (a unit that wins everything or nothing for its price) stand out.
// ONEHOUR_BMBUDGET overrides the budget; ONEHOUR_BMROW=i runs only row i (to split the matrix over processes).
#include "sim.h"
#include "testutil.h"
#include <cstdlib>

namespace {
const int ROSTER_C[] = { U_C_INF1, U_C_INF2, U_C_INF3, U_C_TANK, U_C_VOLT, U_C_RAIL, U_C_AIR, U_C_ELITE, U_C_TITAN, U_C_JET, U_C_SNIPER, U_C_HELI, U_C_FIGHTER, U_C_JEEP };
const int ROSTER_K[] = { U_K_INF1, U_K_INF2, U_K_INF3, U_K_TANK, U_K_GATLING, U_K_MLRS, U_K_AIR, U_K_ELITE, U_K_TITAN, U_K_JET, U_K_SNIPER, U_K_AIR, U_K_FIGHTER, U_K_JEEP };
const int ROSTER_N = (int)(sizeof(ROSTER_C) / sizeof(ROSTER_C[0]));

// one fight: 'a' for player 0 (Cyber), 'b' for player 1 (Clanker); returns the score for player 0
float fight(int a, int b, float budget, u64 seed) {
    Faction fac[2] = { F_CYBER, F_CLANKER }; bool ai[2] = { false, false }; int diff[2] = { 1, 1 }, team[2] = { 0, 1 };
    g_sim.init(2, fac, ai, diff, team, seed);
    g_sim.riotsOn = false;
    for (int p = 0; p < 2; p++) std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1);
    Vec2 mid = openGroundNear(Vec2(WORLD_W * 0.5f, ISLE_X0 * TILE + 40.0f * TILE), 14);
    flattenArena(mid, 24, 16);
    std::vector<Ref> side[2];
    const int types[2] = { a, b };
    float spent[2] = { 0, 0 };
    for (int s = 0; s < 2; s++) {
        int n = std::max(1, (int)std::lround(budget / UNITS[types[s]].cost));
        n = std::min(n, 40);
        spent[s] = (float)n * UNITS[types[s]].cost;
        for (int k = 0; k < n; k++) {
            Vec2 at = mid + Vec2((s ? 1.0f : -1.0f) * (12.0f * TILE + (k / 8) * 34.0f), ((k % 8) - 3.5f) * 36.0f);
            Ref r = g_sim.spawnUnit(types[s], s, at);
            if (Entity* e = g_sim.get(r)) e->angle = s ? 3.14159f : 0.0f;
            side[s].push_back(r);
        }
    }
    for (int s = 0; s < 2; s++) g_sim.cmdMove(side[s], mid + Vec2((s ? -1.0f : 1.0f) * 9.0f * TILE, 0), true);
    auto value = [&](int s) { double v = 0; for (Ref r : side[s]) if (Entity* e = g_sim.get(r)) v += UNITS[e->type].cost * e->hp / e->maxHp; return v; };
    for (int t = 0; t < 100 * SIM_HZ; t++) {
        g_sim.step(); g_sim.events.clear();
        if (t % 20 == 0 && (value(0) <= 0 || value(1) <= 0)) break;
        if (t % (15 * SIM_HZ) == 0 && t > 0) for (int s = 0; s < 2; s++) {   // whoever stopped goes looking again (aircraft circle back to the fight)
            std::vector<Ref> idle;
            for (Ref r : side[s]) if (Entity* e = g_sim.get(r)) if (e->order == O_IDLE || e->order == O_GUARDPOS) idle.push_back(r);
            if (!idle.empty()) g_sim.cmdMove(idle, mid + Vec2((s ? -1.0f : 1.0f) * 10.0f * TILE, 0), true);
        }
    }
    return (float)((value(0) / spent[0] - value(1) / spent[1]));
}
}

bool balanceMatrix(u64 seed) {
    float budget = getenv("ONEHOUR_BMBUDGET") ? (float)atof(getenv("ONEHOUR_BMBUDGET")) : 6000.0f;
    int onlyRow = getenv("ONEHOUR_BMROW") ? atoi(getenv("ONEHOUR_BMROW")) : -1;
    printf("balancematrix: budget %.0f per side, score = Cyber value left - Clanker value left (shares of what each spent)\n%-16s", budget, "Cyber \\ Clanker");
    for (int j = 0; j < ROSTER_N; j++) printf(" %6.6s", UNITS[ROSTER_K[j]].name);
    printf("   avg\n");
    std::vector<double> col(ROSTER_N, 0.0);
    for (int i = 0; i < ROSTER_N; i++) {
        if (onlyRow >= 0 && i != onlyRow) continue;
        printf("%-16.16s", UNITS[ROSTER_C[i]].name);
        double sum = 0;
        for (int j = 0; j < ROSTER_N; j++) {
            float sc = 0.5f * (fight(ROSTER_C[i], ROSTER_K[j], budget, seed + i * 977 + j * 131) + fight(ROSTER_C[i], ROSTER_K[j], budget, seed + i * 977 + j * 131 + 5));
            printf(" %+6.2f", sc); fflush(stdout);
            sum += sc; col[j] += sc;
        }
        printf("  %+5.2f\n", sum / ROSTER_N);
    }
    if (onlyRow < 0) {
        printf("%-16s", "Clanker avg");
        for (int j = 0; j < ROSTER_N; j++) printf(" %+6.2f", -col[j] / ROSTER_N);
        printf("\n");
    }
    return true;
}

// --assaultmatrix: BM_BUDGET of each combat unit attacks one finished defensive structure of the other army (both armies' turrets). Prints the
// share of the turret's health that is left and the share of the attackers' value left: what a credit of attackers is worth against a defence.
bool assaultMatrix(u64 seed) {
    float budget = getenv("ONEHOUR_BMBUDGET") ? (float)atof(getenv("ONEHOUR_BMBUDGET")) : 4000.0f;
    const int TUR[4] = { B_C_LASER, B_C_PATRIOT, B_K_MG, B_K_ROCKET };
    printf("assaultmatrix: %.0f of attackers vs one turret; turret hp left / attacker value left\n%-16s", budget, "attacker");
    for (int t = 0; t < 4; t++) printf(" %15.15s", BUILDS[TUR[t]].name);
    printf("\n");
    for (int fac = 0; fac < 2; fac++) for (int i = 0; i < ROSTER_N; i++) {
        int ut = fac == 0 ? ROSTER_C[i] : ROSTER_K[i];
        if (fac == 1 && i == 11) continue;   // (the Clanker roster lists its gunship twice)
        printf("%-16.16s", UNITS[ut].name);
        for (int t = 0; t < 4; t++) {
            Faction tf = BUILDS[TUR[t]].faction, af = (Faction)fac;
            if (tf == af) { printf(" %15s", "-"); continue; }
            Faction facs[2] = { af, tf }; bool ai[2] = { false, false }; int diff[2] = { 1, 1 }, team[2] = { 0, 1 };
            g_sim.init(2, facs, ai, diff, team, seed + i * 31 + t * 7);
            g_sim.riotsOn = false;
            for (int p = 0; p < 2; p++) { std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); g_sim.players[p].money = 0; }
            Vec2 mid = openGroundNear(Vec2(WORLD_W * 0.5f, ISLE_X0 * TILE + 40.0f * TILE), 14);
            flattenArena(mid, 24, 16);
            // power for the turret (a reactor out of the fight) so it is not blind
            g_sim.placeBuilding(tf == F_CYBER ? B_C_POWER : B_K_POWER, 1, tileOf(mid.x) + 16, tileOf(mid.y) - 12, true);
            g_sim.placeBuilding(tf == F_CYBER ? B_C_POWER : B_K_POWER, 1, tileOf(mid.x) + 16, tileOf(mid.y) + 10, true);
            Ref tr = g_sim.placeBuilding(TUR[t], 1, tileOf(mid.x) + 4, tileOf(mid.y), true);
            std::vector<Ref> side;
            int n = std::min(40, std::max(1, (int)std::lround(budget / UNITS[ut].cost)));
            for (int k = 0; k < n; k++) { Vec2 at = mid + Vec2(-12.0f * TILE - (k / 8) * 34.0f, ((k % 8) - 3.5f) * 36.0f); side.push_back(g_sim.spawnUnit(ut, 0, at)); }
            Entity* T = g_sim.get(tr);
            if (!T) { printf(" %15s", "noplace"); continue; }
            Vec2 tp = T->pos;
            g_sim.cmdAttack(side, tr);
            auto value = [&]() { double v = 0; for (Ref r : side) if (Entity* e = g_sim.get(r)) v += UNITS[e->type].cost * e->hp / e->maxHp; return v; };
            for (int s = 0; s < 120 * SIM_HZ; s++) {
                g_sim.step(); g_sim.events.clear();
                if (s % 20 == 0 && (!g_sim.get(tr) || value() <= 0)) break;
                if (s % (15 * SIM_HZ) == 0 && s > 0) g_sim.cmdAttack(side, tr);
            }
            Entity* T2 = g_sim.get(tr);
            float th = T2 ? T2->hp / T2->maxHp : 0.0f;
            (void)tp;
            printf("      %4.2f/%4.2f", th, (float)(value() / (n * UNITS[ut].cost)));
            fflush(stdout);
        }
        printf("\n");
    }
    return true;
}

// Two fleets in the open sea west of the main island, each told to attack-move through the other (shared by --navaltrace and the
// ONEHOUR_NAVAL showcase). Per side: 6 patrol boats, 3 destroyers, 2 cruisers.
Vec2 navalBattleScene(std::vector<Ref> side[2]) {
    Vec2 sea = g_map.nearestWater(Vec2((ISLE_X0 - 30) * TILE, WORLD_H * 0.5f), 12, true, g_map.oceanComp);
    for (int s = 0; s < 2; s++) {
        side[s].clear();
        int f = firstUnitOf(g_sim.players[s].faction) == U_C_DOZER;   // 1: Cyber
        const int types[3] = { f ? U_C_PATROL : U_K_PATROL, f ? U_C_DESTROYER : U_K_DESTROYER, f ? U_C_CRUISER : U_K_CRUISER };
        const int count[3] = { 6, 3, 2 };
        int k = 0;
        for (int t = 0; t < 3; t++) for (int j = 0; j < count[t]; j++, k++) {
            Vec2 at = sea + Vec2(((k % 4) - 1.5f) * 3.5f * TILE, (s ? 1.0f : -1.0f) * (22.0f + (k / 4) * 3.0f) * TILE);
            Ref r = g_sim.spawnUnit(types[t], s, g_map.nearestWater(at, 6, true, g_map.oceanComp));
            if (Entity* e = g_sim.get(r)) e->angle = s ? -1.5708f : 1.5708f;
            side[s].push_back(r);
        }
    }
    for (int s = 0; s < 2; s++) g_sim.cmdMove(side[s], sea + Vec2(0, (s ? -1.0f : 1.0f) * 14.0f * TILE), true);
    return sea;
}

// --navaltrace: how ship-like the fleets move. Per ship and second: mean |turn rate| (rad/s), heading reversals (the turn changes sign hard),
// how close hulls come (closest neighbour distance over the sum of radii: under 1 = overlapping) and how many shots were fired.
bool navalTrace(u64 seed) {
    Faction fac[2] = { F_CYBER, F_CLANKER }; bool ai[2] = { false, false }; int diff[2] = { 1, 1 }, team[2] = { 0, 1 };
    double turnSum = 0, samples = 0, reversals = 0, overlapSamples = 0, nnSum = 0, nnN = 0, speedSum = 0;
    int sunk[2] = { 0, 0 };
    for (int rep = 0; rep < 3; rep++) {
        g_sim.init(2, fac, ai, diff, team, seed + rep * 17);
        g_sim.riotsOn = false;
        for (int p = 0; p < 2; p++) std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1);
        std::vector<Ref> side[2];
        navalBattleScene(side);
        std::vector<float> lastAng(64, 0), lastTurn(64, 0);
        for (int t = 0; t < 90 * SIM_HZ; t++) {
            g_sim.step(); g_sim.events.clear();
            if (t % 10) continue;
            int k = 0;
            for (int s = 0; s < 2; s++) for (Ref r : side[s]) {
                Entity* e = g_sim.get(r); int idx = k++;
                if (!e) continue;
                float da = std::fmod(e->angle - lastAng[idx] + 9.42477796f, 6.28318531f); if (da < 0) da += 6.28318531f; float turn = (da - 3.14159265f) / (10 * SIM_DT);
                if (t > 0) {
                    turnSum += std::abs(turn); samples++; speedSum += e->airspeed;
                    if (lastTurn[idx] * turn < 0 && std::abs(turn) > 0.25f && std::abs(lastTurn[idx]) > 0.25f) reversals++;
                }
                lastAng[idx] = e->angle; lastTurn[idx] = turn;
                float best = 1e9f;
                for (int s2 = 0; s2 < 2; s2++) for (Ref r2 : side[s2]) { Entity* o = g_sim.get(r2); if (!o || o == e) continue; best = std::min(best, dist(o->pos, e->pos) / (e->radius() + o->radius())); }
                if (best < 1e8f) { nnSum += best; nnN++; if (best < 1.0f) overlapSamples++; }
            }
        }
        for (int s = 0; s < 2; s++) for (Ref r : side[s]) if (!g_sim.get(r)) sunk[s]++;
    }
    double secs = samples * 10 * SIM_DT;
    printf("navaltrace: mean |turn| %.3f rad/s, reversals %.2f per ship-minute, mean closest hull %.2f radii, overlapping %.1f%% of samples, mean speed %.0f px/s, sunk %d/%d\n",
           turnSum / std::max(1.0, samples), reversals / std::max(1.0, secs / 60.0), nnSum / std::max(1.0, nnN), 100.0 * overlapSamples / std::max(1.0, nnN), speedSum / std::max(1.0, samples), sunk[0], sunk[1]);
    return true;
}
