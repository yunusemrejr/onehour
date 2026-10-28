// One Hour - headless tests 19: the unit micro update (Player::micro, see MicroBit)
#include "ai.h"
#include "testutil.h"

// --micromatrix: do the smarter unit behaviours win fights? Mirrored scenes, one army with Player::micro (ONEHOUR_MICRO=bits, all by default) and
// one without any, then the sides swap; both armies, several seeds. Two kinds of scene:
//  - field: two equal armies meet on open ground (attack-move at each other), several compositions;
//  - assault: an army attack-moves into a position held by turrets behind a wall and a garrison; once the attacker plays smart, once the defender.
// Prints wins and the value each side has left (units at their cost times their health share, turrets included in the assault).
bool microMatrix(u64 seed) {
    const int smartMask = getenv("ONEHOUR_MICRO") ? atoi(getenv("ONEHOUR_MICRO")) : MICRO_ALL;
    const int seeds = getenv("ONEHOUR_MICROSEEDS") ? atoi(getenv("ONEHOUR_MICROSEEDS")) : 6;
    struct Comp { const char* name; int off[6]; int n[6]; };
    static const Comp COMPS[] = {
        { "infantry", { 2, 3, 4, -1 }, { 10, 6, 6 } },
        { "armour",   { 5, 6, 7, 3, -1 }, { 6, 4, 3, 6 } },
        { "combined", { 2, 3, 5, 6, 8, -1 }, { 8, 5, 4, 3, 3 } },
        { "ranged",   { 7, 9, 5, 2, -1 }, { 4, 4, 4, 8 } },
    };
    const int NC = (int)(sizeof(COMPS) / sizeof(COMPS[0]));
    int wins[2][2] = {}, draws[2] = {};   // [scene kind][0 smart, 1 plain]
    double left[2][2] = {};
    int compWins[8][2] = {};
    int atkWins[2] = {}, defWins[2] = {}; double atkLeft[2] = {}, defLeft[2] = {};   // [0: the attacker plays smart, 1: the defender does]
    auto setup = [&](Faction fa, Faction fb, u64 sd) {
        Faction fac[2] = { fa, fb }; bool ai[2] = { false, false }; int diff[2] = { 1, 1 }, team[2] = { 0, 1 };
        g_sim.init(2, fac, ai, diff, team, sd);
        g_sim.riotsOn = false;
        for (int p = 0; p < 2; p++) { std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); g_sim.players[p].money = 0; }
    };
    auto value = [&](const std::vector<Ref>& v) { double s = 0; for (Ref r : v) if (Entity* e = g_sim.get(r)) s += (e->isUnit() ? UNITS[e->type].cost : BUILDS[e->type].cost) * e->hp / e->maxHp; return s; };
    for (u64 sd = 0; sd < (u64)seeds; sd++) for (int fp = 0; fp < 4; fp++) for (int swap = 0; swap < 2; swap++) {
        Faction fa = (Faction)(fp & 1), fb = (Faction)((fp >> 1) & 1);
        const int smartSide = swap;
        // ---- field battles
        for (int ci = 0; ci < NC; ci++) {
            const Comp& c = COMPS[ci];
            setup(fa, fb, seed + sd * 31 + ci);
            g_sim.players[smartSide].micro = smartMask; g_sim.players[1 - smartSide].micro = 0;
            Vec2 mid = openGroundNear(Vec2(WORLD_W * 0.5f, ISLE_X0 * TILE + 40.0f * TILE), 14);
            flattenArena(mid, 22, 16);
            std::vector<Ref> side[2];
            for (int s = 0; s < 2; s++) {
                int base = firstUnitOf(g_sim.players[s].faction), k = 0;
                for (int i = 0; c.off[i] >= 0 && i < 6; i++) for (int j = 0; j < c.n[i]; j++, k++) {
                    Vec2 at = mid + Vec2((s ? 1.0f : -1.0f) * (11.0f * TILE + (k / 8) * 30.0f), ((k % 8) - 3.5f) * 34.0f);
                    Ref r = g_sim.spawnUnit(base + c.off[i], s, at);
                    g_sim.get(r)->angle = s ? 3.14159f : 0.0f;
                    side[s].push_back(r);
                }
            }
            for (int s = 0; s < 2; s++) g_sim.cmdMove(side[s], mid + Vec2((s ? -1.0f : 1.0f) * 8.0f * TILE, 0), true);
            for (int t = 0; t < 120 * SIM_HZ; t++) {
                g_sim.step(); g_sim.events.clear();
                if (t % 20 == 0 && (value(side[0]) <= 0 || value(side[1]) <= 0)) break;
                if (t % (20 * SIM_HZ) == 0 && t > 0) for (int s = 0; s < 2; s++) { std::vector<Ref> idle; for (Ref r : side[s]) if (Entity* e = g_sim.get(r)) if (e->order == O_IDLE) idle.push_back(r); if (!idle.empty()) g_sim.cmdMove(idle, mid + Vec2((s ? -1.0f : 1.0f) * 10.0f * TILE, 0), true); }
            }
            double vs = value(side[smartSide]), vp = value(side[1 - smartSide]);
            left[0][0] += vs; left[0][1] += vp;
            if (vs > 0 && vp <= 0) { wins[0][0]++; compWins[ci][0]++; } else if (vp > 0 && vs <= 0) { wins[0][1]++; compWins[ci][1]++; } else draws[0]++;
        }
        // ---- assault on a held position: the attacker is player 'swap'; in one run it plays smart against a plain defence, in the other the defence
        // plays smart against a plain attacker
        for (int role = 0; role < 2; role++) {
            setup(fa, fb, seed + sd * 53 + role * 7 + 1000);
            const int att = swap, def = 1 - swap;
            const bool attSmart = role == 0;
            g_sim.players[att].micro = attSmart ? smartMask : 0; g_sim.players[def].micro = attSmart ? 0 : smartMask;
            Vec2 mid = openGroundNear(Vec2(WORLD_W * 0.5f, ISLE_X0 * TILE + 40.0f * TILE), 16);
            flattenArena(mid, 24, 18);
            const float dir = att == 0 ? 1.0f : -1.0f;   // the attacker comes from the west when it is player 0, from the east otherwise
            std::vector<Ref> A, D;
            // the defence: a power plant, two gun turrets and an anti-air battery behind a short wall, a garrison of mixed units
            int bd = firstBuildOf(g_sim.players[def].faction), ud = firstUnitOf(g_sim.players[def].faction), ua = firstUnitOf(g_sim.players[att].faction);
            int ctx = tileOf(mid.x) + (int)(dir * 6), cty = tileOf(mid.y);
            g_sim.placeBuilding(bd + BR_POWER, def, ctx + (int)(dir * 5) - 1, cty - 1, true);
            for (int k = 0; k < 2; k++) { Ref r = g_sim.placeBuilding(bd + BR_TURRET, def, ctx + (int)dir, cty - 5 + k * 8, true); if (g_sim.get(r)) D.push_back(r); }
            { Ref r = g_sim.placeBuilding(bd + BR_AATURRET, def, ctx + (int)(dir * 2), cty - 1, true); if (g_sim.get(r)) D.push_back(r); }
            int wallT = g_sim.players[def].faction == F_CYBER ? B_C_WALL : B_K_WALL;
            for (int y = -3; y <= 3; y++) if (y != 0) g_sim.placeBuilding(wallT, def, ctx - (int)dir, cty + y, true);
            static const int DOFF[] = { 2, 3, 5, 6, 2, 3, 4, 5 };
            for (int k = 0; k < 8; k++) { Ref r = g_sim.spawnUnit(ud + DOFF[k], def, g_map.nearestFree(mid + Vec2(dir * TILE * (8.0f + (k % 2) * 2), TILE * (k / 2 - 1.5f) * 2.0f), 4)); D.push_back(r); }
            static const int AOFF[] = { 2, 2, 3, 3, 4, 5, 5, 6, 7, 7, 3, 2, 5, 9, 2, 3, 5, 6, 7, 4, 2, 5, 5, 7, 6, 3, 3, 5, 9, 7, 2, 5 };
            for (int k = 0; k < 32; k++) { Vec2 at = mid + Vec2(-dir * TILE * (12.0f + (k / 8) * 2), TILE * ((k % 8) - 3.5f) * 1.2f); Ref r = g_sim.spawnUnit(ua + AOFF[k], att, g_map.nearestFree(at, 4)); A.push_back(r); }
            const Vec2 goal = mid + Vec2(dir * TILE * 14.0f, 0);
            g_sim.cmdMove(A, goal, true);
            for (int t = 0; t < 150 * SIM_HZ; t++) {
                g_sim.step(); g_sim.events.clear();
                if (t % 20 == 0 && (value(A) <= 0 || value(D) <= 0)) break;
                if (t % (15 * SIM_HZ) == 0 && t > 0) { std::vector<Ref> idle; for (Ref r : A) if (Entity* e = g_sim.get(r)) if (e->order == O_IDLE) idle.push_back(r); if (!idle.empty()) g_sim.cmdMove(idle, goal, true); }
            }
            double va = value(A), vd = value(D);
            int k = attSmart ? 0 : 1;
            atkLeft[k] += va; defLeft[k] += vd;
            if (va > 0 && vd <= 0) atkWins[k]++; else if (vd > 0 && va <= 0) defWins[k]++; else draws[1]++;
        }
    }
    printf("micromatrix (mask %d): field battles: smart won %d, plain won %d, unresolved %d; value left: smart %.0f, plain %.0f\n", smartMask, wins[0][0], wins[0][1], draws[0], left[0][0], left[0][1]);
    for (int ci = 0; ci < NC; ci++) printf("    %-9s smart %d plain %d\n", COMPS[ci].name, compWins[ci][0], compWins[ci][1]);
    printf("micromatrix (mask %d): assaults, smart attacker vs plain defence: attacker won %d, defence held %d (value left %.0f / %.0f)\n", smartMask, atkWins[0], defWins[0], atkLeft[0], defLeft[0]);
    printf("micromatrix (mask %d): assaults, plain attacker vs smart defence: attacker won %d, defence held %d (value left %.0f / %.0f); unresolved %d\n", smartMask, atkWins[1], defWins[1], atkLeft[1], defLeft[1], draws[1]);
    return true;
}
