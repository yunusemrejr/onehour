// One Hour - the newer commander: a personality of its own (FEAT_PERSONA), the towns round the lake (FEAT_TOWNS), team play with its allies
// (FEAT_TEAM). The riot handling (FEAT_RIOT) lives in think() itself, where the army is mustered.
//  * persona: the learned opening doctrine is the plan; the persona is the person carrying it out (raids, patience, towns, infiltrators)
//  * towns: an infiltrator takes a whole town at once, so the commander sends one at the richest town it can reach, with an escort when the town is
//    guarded or held by an enemy; it answers an attack on a town it holds, sends a dozer to rebuild one that was razed, and takes back what it lost
//  * team play: allied commanders agree on one enemy to go after (the weakest, the one in trouble with its own people, the one hitting us), join each
//    other's attacks, and tell a human teammate what they are doing
#include "ai.h"

static bool aiDebugC() { static int v = -1; if (v < 0) v = getenv("ONEHOUR_DEBUG") ? 1 : 0; return v == 1; }

const Persona PERSONAS[PE_COUNT] = {
    //  title           blurb                                                                                   raid  patience towns spies guard finisher
    { "Strategist",  "combined arms, joint attacks with its allies, holds the towns it can defend",               1.0f, 1.00f,   1.0f, 1.0f, 0.00f, 1.0f },
    { "Blitz",       "early raids on haulers and outposts, attacks before you are ready",                         1.8f, 0.86f,   0.7f, 0.6f, 0.00f, 1.4f },
    { "Iron Wall",   "a fortified base and a strong home guard: it counterattacks when your waves break",         0.6f, 1.15f,   0.9f, 0.6f, 0.25f, 0.8f },
    { "Sky Marshal", "air raids on your economy, its ground army kept close",                                     1.2f, 1.00f,   0.8f, 0.8f, 0.10f, 1.0f },
    { "Tycoon",      "grabs the towns round the lake with infiltrators and outspends you",                         0.8f, 1.06f,   1.8f, 1.2f, 0.10f, 0.9f },
    { "Shadow",      "infiltrators everywhere: towns and structures change hands under your nose",                1.1f, 1.00f,   1.5f, 1.9f, 0.00f, 1.2f },
};

// Who is in command: the doctrine (learned from match results) sets the broad plan, and a commander who suits it carries it out. A balanced
// opening is played by a strategist or by a commander who works through infiltrators.
void AiPlayer::choosePersona() {
    static const char* const CY_NAMES[8] = { "Commander Vega", "Director Lin", "Marshal Okoro", "Admiral Sato", "Overseer Kade", "Commander Ilves", "Director Amari", "Marshal Quell" };
    static const char* const CK_NAMES[8] = { "General Varga", "Colonel Brandt", "Warlord Krupp", "Major Hale", "General Ostrova", "Colonel Rusk", "Field Marshal Dorn", "Major Kessler" };
    switch (doctrine) {
    case DOC_RUSH:   persona = PE_BLITZ; break;
    case DOC_TURTLE: persona = PE_IRONWALL; break;
    case DOC_AIR:    persona = PE_SKYMARSHAL; break;
    case DOC_BOOM:   persona = PE_TYCOON; break;
    default:         persona = rng.f() < 0.5f ? PE_STRATEGIST : PE_SHADOW; break;
    }
    const char* const* pool = g_sim.players[player].faction == F_CYBER ? CY_NAMES : CK_NAMES;
    unsigned pick = (rng.next() + (unsigned)player * 3u) % 8u;
    for (int tries = 0; tries < 8; tries++, pick = (pick + 1) % 8u) {   // no two commanders of one match share a name
        bool taken = false;
        for (int q = 0; q < player; q++) if (g_ai.ais[q].player >= 0 && g_ai.ais[q].commander == pool[pick]) taken = true;
        if (!taken) break;
    }
    commander = pool[pick];
}

void AiPlayer::announce(const char* what, Vec2 at) {
    Sim& S = g_sim;
    for (int q = 0; q < S.numPlayers; q++)
        if (q != player && !S.players[q].isAI && S.players[q].alive && S.players[q].team == S.players[player].team) {
            char msg[160]; snprintf(msg, sizeof msg, "Ally %s: %s", commander, what);
            S.emit(EV_MSG, q, SND_NONE, at, msg);
        }
}

// ---------------------------------------------------------------- team play
// Every fifteen seconds each team of computer commanders agrees on one enemy army to go after: the one hitting the team's bases right now if any,
// otherwise the weakest for its distance - and an army that has lost its grip on its own people (riots, a burning base) is weaker than it looks.
void AiManager::updateTeamFocus() {
    Sim& S = g_sim;
    if (S.time < focusAt) return;
    focusAt = S.time + 15.0f;
    float str[MAX_PLAYERS] = {}, pressure[MAX_PLAYERS][MAX_PLAYERS] = {};
    for (auto& e : S.ents) {
        if (!e.alive || e.owner < 0 || e.owner >= S.numPlayers || e.district >= 0) continue;
        if (e.isUnit() && e.ut().role == UR_COMBAT && !e.ut().temp) {
            str[e.owner] += UNITS[e.type].cost;
            for (int t = 0; t < S.numPlayers; t++) if (t != e.owner && S.enemies(t, e.owner) && dist(e.pos, S.players[t].basePos) < 28.0f * TILE) pressure[t][e.owner] += UNITS[e.type].cost;   // its fighters at t's door
        } else if (e.isBuilding() && e.constructed && e.bt().weapon >= 0) str[e.owner] += BUILDS[e.type].cost * 0.6f;
    }
    for (int t = 0; t < MAX_PLAYERS; t++) focus[t] = -1;
    for (int team = 0; team < MAX_PLAYERS; team++) {
        Vec2 home; int members = 0;
        for (int p = 0; p < S.numPlayers; p++) if (S.players[p].alive && S.players[p].team == team) { home += S.players[p].basePos; members++; }
        if (!members) continue;
        home = home * (1.0f / members);
        int best = -1; float bs = 1e30f;
        for (int q = 0; q < S.numPlayers; q++) {
            if (!S.players[q].alive || S.players[q].team == team) continue;
            float press = 0; for (int p = 0; p < S.numPlayers; p++) if (S.players[p].alive && S.players[p].team == team) press += pressure[p][q];
            float sc = str[q] * (0.55f + 0.45f * S.control[q]) + dist(S.players[q].basePos, home) / TILE * 40.0f - press * 1.5f;
            if (sc < bs) { bs = sc; best = q; }
        }
        focus[team] = best;
    }
}

int AiPlayer::teamFocus() const {
    if (!on(FEAT_TEAM)) return -1;
    int t = g_sim.players[player].team;
    return t >= 0 && t < MAX_PLAYERS ? g_ai.focus[t] : -1;
}

// A teammate's attack is under way: an army that is ready enough goes in beside it, on the same objective, instead of hitting somewhere else on
// its own (two half-strength waves at once beat one full wave twice).
bool AiPlayer::teamJoin(const std::vector<Ref>& army, float armyValue, float waveThreshold) {
    Sim& S = g_sim;
    if (!on(FEAT_TEAM) || attacking || recovering || army.size() < 5 || armyValue < waveThreshold * 0.45f || S.time - lastTeamJoin < 40.0f) return false;
    for (int q = 0; q < S.numPlayers; q++) {
        if (q == player || !S.players[q].alive || !S.players[q].isAI || S.players[q].team != S.players[player].team) continue;
        const AiPlayer& o = g_ai.ais[q];
        if (o.player < 0 || !o.attacking || S.time - o.attackStarted > 45.0f) continue;
        Entity* tgt = S.get(o.attackTarget);
        if (!tgt || !S.enemies(player, tgt->owner) || Sim::districtRuin(*tgt)) continue;
        lastTeamJoin = S.time; teamJoins++;
        attacking = true; waveValue = armyValue; attackStarted = S.time; attackTarget = o.attackTarget; wave = army; waveHasSample = false; beginWaveLearning();
        S.cmdMove(army, tgt->pos, true);
        nextOrderTime = S.time + 6;
        if (aiDebugC()) fprintf(stderr, "[ai%d t=%.0f] TEAM JOIN ai%d's attack on %s with %d units\n", player, S.time, q, tgt->isUnit() ? tgt->ut().name : tgt->bt().name, (int)army.size());
        if (S.time - lastAnnounce > 30.0f) { lastAnnounce = S.time; char m[120]; snprintf(m, sizeof m, "joining %s's attack on the %s", o.commander, tgt->isUnit() ? tgt->ut().name : tgt->bt().name); announce(m, tgt->pos); }
        return true;
    }
    return false;
}

// ---------------------------------------------------------------- the towns round the lake
void AiPlayer::civilAffairs(const std::vector<Ref>& army, const std::vector<Ref>& spies, bool threat, int dozers) {
    Sim& S = g_sim;
    Player& pl = S.players[player];
    if (!on(FEAT_TOWNS) || pl.difficulty < 1 || S.districts.empty() || S.time < nextCivil) return;
    nextCivil = S.time + 2.0f;
    const Persona& P = pers();
    auto inWave = [&](Ref r) { return attacking && std::find(wave.begin(), wave.end(), r) != wave.end(); };
    // the reserve: ground fighters at home, not in a wave, not already sent somewhere
    auto reserveNear = [&](Vec2 at, float wantValue, int maxN, std::vector<Ref>& out) {
        std::vector<std::pair<float, Ref>> c;
        for (Ref r : army) {
            Entity* e = S.get(r);
            if (!e || e->isAir() || e->ut().naval || e->weapon() < 0 || inWave(r) || e->carrier.valid()) continue;
            if (e->order != O_IDLE && e->order != O_GUARDAREA && e->order != O_GUARDPOS) continue;
            c.push_back({dist(e->pos, at), r});
        }
        std::sort(c.begin(), c.end(), [](const std::pair<float, Ref>& a, const std::pair<float, Ref>& b) { return a.first < b.first; });
        float v = 0;
        for (auto& x : c) { if ((int)out.size() >= maxN || v >= wantValue) break; out.push_back(x.second); Entity* e = S.get(x.second); v += UNITS[e->type].cost; }
    };
    auto armedNear = [&](Vec2 at, float r) {   // enemy fighting strength round a spot (rioters included: they shoot too)
        float v = 0;
        S.forEachNear(at, r, [&](Entity& g) {
            if (g.kind == EK_RESOURCE || !S.enemies(player, g.owner) || g.weapon() < 0 || g.carrier.valid()) return;
            if (g.isBuilding() ? !g.constructed : g.ut().role != UR_COMBAT) return;
            if (!S.visibleTo(g, player)) return;
            v += g.isUnit() ? UNITS[g.type].cost : BUILDS[g.type].cost * 1.6f;
        });
        return v;
    };
    int armyN = (int)army.size();
    // 1. a town we hold is under attack: the reserve goes to its defence (a town pays every ten seconds, and steadies our own people)
    if (!threat && S.time - lastTownGuard > 12.0f) {
        for (int d = 0; d < (int)S.districts.size(); d++) {
            const District& D = S.districts[d];
            if (D.owner != player || S.time - D.lastHit > 6.0f) continue;
            float danger = armedNear(D.center, D.radius + 7.0f * TILE);
            if (danger <= 0) continue;
            std::vector<Ref> pick;
            reserveNear(D.center, danger * 1.5f + 600.0f, std::max(4, (int)(armyN * 0.4f * P.towns)), pick);
            if (pick.size() < 2) continue;
            S.cmdMove(pick, D.center, true);
            lastTownGuard = S.time; townDefences++;
            if (aiDebugC()) fprintf(stderr, "[ai%d t=%.0f] TOWN DEFENCE %s: %d units vs %.0f\n", player, S.time, D.name, (int)pick.size(), danger);
            if (S.time - lastAnnounce > 40.0f) { lastAnnounce = S.time; char m[120]; snprintf(m, sizeof m, "sending troops to defend %s", D.name); announce(m, D.center); }
            break;
        }
    }
    // 2. a town of ours that was razed: a dozer goes to raise it again once nobody is shooting there (the townsfolk alone take much longer)
    if (!threat && !recovering && S.time - lastTownRebuild > 18.0f) {
        for (int d = 0; d < (int)S.districts.size(); d++) {
            const District& D = S.districts[d];
            if (D.owner != player || (D.ruins == 0 && D.integrity > 0.8f) || S.time - D.lastHit < 8.0f || armedNear(D.center, D.radius + 6.0f * TILE) > 0) continue;
            Entity* job = nullptr; float worst = 2.0f;
            for (Ref r : D.blocks) { Entity* b = S.get(r); if (!b) continue; float f = b->constructed ? b->hp / b->maxHp : -1.0f + b->progress; if (f < worst) { worst = f; job = b; } }
            if (!job) continue;
            Entity* dz = nullptr; float dd = 1e30f;
            for (auto& e : S.ents) {   // a civilian dozer first (it has nothing else to do), else a military one that is idle, only if more than one is left at home
                if (!e.alive || !e.isUnit() || e.owner != player || e.ut().role != UR_DOZER || e.carrier.valid() || e.order != O_IDLE) continue;
                if (!civicDozerType(e.type) && dozers < 2) continue;
                float de = dist(e.pos, D.center) * (civicDozerType(e.type) ? 0.5f : 1.0f);
                if (de < dd) { dd = de; dz = &e; }
            }
            if (!dz) continue;
            S.cmdAssist({S.refOf(*dz)}, S.refOf(*job));
            lastTownRebuild = S.time; townRebuilds++;
            if (aiDebugC()) fprintf(stderr, "[ai%d t=%.0f] TOWN REBUILD %s (%d ruins, %.0f%%)\n", player, S.time, D.name, D.ruins, D.integrity * 100);
            break;
        }
    }
    // 3. take a town: the richest one we can reach that is not ours or a friend's
    int best = -1; float bs = -1e30f;
    for (int d = 0; d < (int)S.districts.size(); d++) {
        const District& D = S.districts[d];
        if (D.owner == player || (D.owner >= 0 && !S.enemies(player, D.owner))) continue;
        if (!S.explored(player, clampi(tileOf(D.center.x), 0, MAP_W - 1), clampi(tileOf(D.center.y), 0, MAP_H - 1))) continue;
        float danger = armedNear(D.center, D.radius + 6.0f * TILE);
        float sc = D.pay * D.integrity * 10.0f * P.towns * (D.owner >= 0 ? 1.3f : 1.0f) - danger * 0.8f - dist(D.center, pl.basePos) / TILE * 25.0f;
        if (sc > bs) { bs = sc; best = d; }
    }
    townTarget = best;
    if (best < 0) return;
    const District& T = S.districts[best];
    // an infiltrator to take it: trained for the purpose (the tech structure unlocks them), one more for the commanders who live by them
    int spyType = pl.faction == F_CYBER ? U_C_SPY : U_K_SPY, queued = 0;
    for (auto& b : S.ents) if (b.alive && b.isBuilding() && b.owner == player && b.bt().role == BR_BARRACKS) for (int t : b.queue) if (t == spyType) queued++;
    int wantSpies = 1 + (P.towns >= 1.4f ? 1 : 0);
    if ((int)spies.size() + queued < wantSpies && S.unitAvailable(player, spyType) && pl.money > 2400 && armyN >= 6 && !threat)
        for (auto& b : S.ents) if (b.alive && b.isBuilding() && b.owner == player && b.constructed && b.bt().role == BR_BARRACKS && b.queue.size() < 2) { if (S.cmdTrain(S.refOf(b), spyType)) break; }
    Entity* spy = nullptr; float sd = 1e30f;
    bool going = false;
    for (Ref r : spies) {
        Entity* s = S.get(r);
        if (!s || s->carrier.valid()) continue;
        const Entity* tgt = s->order == O_CAPTURE ? S.get(s->targetEnt) : nullptr;
        if (tgt && tgt->district == best) { going = true; spy = s; break; }
        if (s->order == O_CAPTURE) continue;   // (busy with a structure or an island launcher)
        float d = dist(s->pos, T.center);
        if (d < sd) { sd = d; spy = s; }
    }
    if (!spy) return;
    if (!going) {
        Entity* blk = nullptr; float bd = 1e30f;
        for (Ref r : T.blocks) { Entity* b = S.get(r); if (b && dist(b->pos, spy->pos) < bd) { bd = dist(b->pos, spy->pos); blk = b; } }
        if (!blk) return;
        S.cmdCapture({S.refOf(*spy)}, S.refOf(*blk));
        if (aiDebugC()) fprintf(stderr, "[ai%d t=%.0f] TOWN CAPTURE: infiltrator -> %s (owner %d)\n", player, S.time, T.name, T.owner);
        if (S.time - lastAnnounce > 40.0f) { lastAnnounce = S.time; char m[120]; snprintf(m, sizeof m, "an infiltrator is on the way to take %s", T.name); announce(m, T.center); }
    }
    // the escort: a guarded town, or one an enemy holds, gets a squad that clears the way for the infiltrator
    float danger = armedNear(T.center, T.radius + 7.0f * TILE);
    if ((danger > 0 || T.owner >= 0) && S.time - lastEscort > 15.0f && dist(spy->pos, T.center) < 70.0f * TILE) {
        escort.erase(std::remove_if(escort.begin(), escort.end(), [&](Ref r) { return S.get(r) == nullptr || inWave(r); }), escort.end());
        float have = 0; for (Ref r : escort) have += UNITS[S.get(r)->type].cost;
        std::vector<Ref> more;
        if (have < danger * 1.4f + 900.0f) reserveNear(T.center, danger * 1.4f + 900.0f - have, std::max(3, (int)(armyN * 0.35f * P.towns)), more);
        for (Ref r : more) escort.push_back(r);
        if (!escort.empty()) {
            S.cmdMove(escort, T.center, true);
            lastEscort = S.time;
            if (aiDebugC()) fprintf(stderr, "[ai%d t=%.0f] TOWN ESCORT %s: %d units vs %.0f\n", player, S.time, T.name, (int)escort.size(), danger);
        }
    } else if (!escort.empty() && T.owner == player) {   // the town is ours: the escort stays a while as its guard, then rejoins the army
        escort.erase(std::remove_if(escort.begin(), escort.end(), [&](Ref r) { return S.get(r) == nullptr; }), escort.end());
        if (!escort.empty()) S.cmdGuardArea(escort, T.center, T.radius + 2.0f * TILE);
        escort.clear();
    }
}

// ---------------------------------------------------------------- difficulty raised mid-match
// The human may raise any computer army's difficulty from the pause menu, one step at a time and never back down. What the difficulty decided at the
// start of the match follows it: the income structures it aims for and when, the thinking cadence and the spending engine (read live in think()),
// and a Brutal commander's edge - the faster production, richer income and harder hits (Player::brutal()) and its bigger war chest.
bool AiManager::raiseDifficulty(int p) {
    Sim& S = g_sim;
    if (p < 0 || p >= S.numPlayers || !S.players[p].isAI || !S.players[p].alive || S.players[p].difficulty >= 3) return false;
    Player& pl = S.players[p];
    pl.difficulty++;
    AiPlayer& a = ais[p];
    const int d = pl.difficulty;
    int incomes = d <= 0 ? 2 : (d == 1 ? 3 : 4);
    if (a.doctrine == DOC_RUSH) incomes = std::max(2, incomes - 1);
    if (a.doctrine == DOC_BOOM) incomes = std::min(INCOME_MAX, incomes + 1);
    a.style.incomes = std::max(a.style.incomes, incomes);
    a.style.incomeFrom = std::min(a.style.incomeFrom, (d >= 2 ? 1.2f : 1.8f) - (a.doctrine == DOC_BOOM ? 0.4f : 0.0f));
    if (d >= 3) pl.money += 6000;   // the Brutal war chest it would have started with (an Easy commander raised to Normal introduces itself at its next think)
    return true;
}
