// One Hour - the commander's use of the October update (FEAT_FRONTIER): the Island Nuke Launchers out on the lake, the Nuclear Bomber call-in
// and the plain fighters.
//  * a launcher it does not hold (neutral, or the enemy's) is a prize: a cargo lifter flies an infiltrator out to the island, sets down beside the
//    launcher and the infiltrator takes it (with no lifter yet, one infiltrator swims out on its raft); a launcher it holds fires its Super Nuke at
//    the best target it knows
//  * a tech structure's Nuclear Bomber is called on a target worth a mid-sized warhead, not into a wall of anti-air
#include "ai.h"

namespace {

// Everything near p, riders included (Sim::forEachNear leaves out soldiers in a bunker or a jeep and vehicles in a cargo ship; these estimates always
// counted them where their carrier stands)
template <class F> void ridersOf(Entity& c, F& fn, int depth) {
    for (Ref r : c.passengers) if (Entity* q = g_sim.get(r)) { fn(*q); if (depth < 2 && !q->passengers.empty()) ridersOf(*q, fn, depth + 1); }
}
template <class F> void forNear(Vec2 p, float r, F&& fn) {
    g_sim.forEachNear(p, r, [&](Entity& e) { fn(e); if (!e.passengers.empty()) ridersOf(e, fn, 0); });
}

// Where a warhead of blast radius R would cost the enemy most, as far as this army has seen: units and small structures in the inner blast count in
// full, big structures half; friends and own units in the circle count against it; enemy anti-nuke cover and (for a bomber) flak on the way lower it.
Vec2 frontierTarget(int player, float R, float scale, bool bomber, float& best) {
    Sim& S = g_sim;
    std::vector<const Entity*> cand;
    for (auto& e : S.ents) {
        if (!e.alive || e.kind == EK_RESOURCE || !S.enemies(player, e.owner) || e.isAir() || Sim::invulnerable(e)) continue;
        if (Sim::districtRuin(e) || e.owner == RIOT_PLAYER) continue;   // (rubble of a town and a street crowd are not worth a warhead)
        if (!(S.explored(player, clampi(tileOf(e.pos.x), 0, MAP_W - 1), clampi(tileOf(e.pos.y), 0, MAP_H - 1)) && S.visibleTo(e, player))) continue;
        cand.push_back(&e);
    }
    // (what lies round each candidate is asked of the spatial grid - it was every candidate against every candidate and the whole map once or twice
    // more for each - and the anti-nuke cover and flak, which can only lower a score, only for a candidate that can still win)
    std::vector<char> isCand(S.ents.size(), 0);
    for (const Entity* c : cand) isCand[c - &S.ents[0]] = 1;
    std::vector<Vec2> flakAt;
    if (bomber) for (auto& o : S.ents) if (o.alive && o.isBuilding() && o.constructed && S.enemies(player, o.owner) && o.bt().role == BR_AATURRET) flakAt.push_back(o.pos);
    const float R2 = R * R, F2 = R * 1.05f * R * 1.05f;
    Vec2 bestPos; best = 0;
    for (const Entity* c : cand) {
        float score = 0;
        forNear(c->pos, R * 1.05f, [&](Entity& o) {
            float d2 = dist2(c->pos, o.pos);
            size_t oi = &o - &S.ents[0];
            if (oi < isCand.size() && isCand[oi]) {
                if (d2 > R2) return;
                float d = std::sqrt(d2);
                float f = d < R * 0.35f ? 1.0f : 1.0f - 0.75f * ((d - R * 0.35f) / (R * 0.65f));
                if (o.isUnit()) score += UNITS[o.type].cost * f;
                else { const BuildType& bt = o.bt(); float cost = bt.cost * (o.district >= 0 ? 0.1f : bt.role == BR_CIVIC ? 0.4f : 1.0f); score += bt.w * bt.h <= 6 ? cost * (f >= 0.62f ? 0.9f : 0.3f) : cost * 0.5f * f; }
                return;
            }
            if (o.isUnit() && o.owner >= 0 && !S.enemies(player, o.owner) && d2 < F2) score -= UNITS[o.type].cost * 1.2f;   // our own and our allies' people
        });
        if (score <= best) continue;
        const bool super = !bomber && scale >= ISLAND_NUKE_SCALE;   // (an island launcher's Super Nuke: only lasers try for it, at a fifth of their chance)
        score *= 1.0f - 0.8f * S.antiNukeChance(c->pos, player, true, scale, super, super);
        if (bomber) {   // the bomber flies in from our corner over whatever stands in the way: count the batteries round the target
            int flak = 0;
            for (Vec2 a : flakAt) if (dist2(a, c->pos) < 12.0f * TILE * 12.0f * TILE) flak++;
            score *= flak >= 5 ? 0.2f : (1.0f - 0.12f * flak);
        }
        if (score > best) { best = score; bestPos = c->pos; }
    }
    return bestPos;
}

// The free ground right beside a launcher, on its island (where a lifter sets down and an infiltrator stands to work)
Vec2 besideLauncher(const Entity& l) {
    for (float r = 1.4f; r <= 3.5f; r += 0.7f)
        for (int k = 0; k < 8; k++) {
            float a = k * 0.785f + 1.57f;
            Vec2 p = l.pos + Vec2(std::cos(a), std::sin(a)) * (l.bt().w * 0.5f + r) * TILE;
            int tx = tileOf(p.x), ty = tileOf(p.y);
            if (g_map.passable(tx, ty) && g_map.tile(tx, ty) != T_WATER) return tileCenter(tx, ty);
        }
    return g_map.nearestFree(l.pos, 4);
}

}  // namespace

void AiPlayer::frontier() {
    Sim& S = g_sim;
    Player& pl = S.players[player];
    if (S.time < nextFrontier) return;
    nextFrontier = S.time + 1.5f;
    const float minutes = S.time / 60.0f;
    const float nukeBar = pl.difficulty >= 3 ? 2600.0f : (pl.difficulty == 2 ? 3200.0f : 4000.0f);

    // ---------- fire what we hold. A wave on its way in gets its objective softened up first: the defences round it are struck while the wave is still
    // clear of the blast, so it arrives as the dust settles. Otherwise only a target worth a warhead is struck.
    Vec2 objective; bool soften = false;
    if (attacking) if (const Entity* t = S.get(attackTarget)) {
        Vec2 c; int n = 0;
        for (Ref r : wave) if (const Entity* u = S.get(r)) { c += u->pos; n++; }
        if (n > 0) { c = c * (1.0f / n); float d = dist(c, t->pos) / TILE; soften = d > 16.0f && d < 45.0f; objective = t->pos; }
    }
    auto strikeSpot = [&](float scale, bool bomber, float bar, Vec2& at) {
        float R = NUKE_RADIUS * TILE * std::sqrt(scale), score;
        at = frontierTarget(player, R, scale, bomber, score);
        if (soften) {   // the objective's own surroundings, if they are worth anything at all
            float best = 0; Vec2 bp;
            for (auto& e : S.ents) {
                if (!e.alive || !e.isBuilding() || !S.enemies(player, e.owner) || Sim::invulnerable(e) || dist(e.pos, objective) > 7.0f * TILE) continue;
                float v = 0;
                forNear(e.pos, R, [&](Entity& o) { if (o.kind != EK_RESOURCE && S.enemies(player, o.owner) && dist2(o.pos, e.pos) < R * R) v += o.isUnit() ? UNITS[o.type].cost : BUILDS[o.type].cost * ((o.bt().role == BR_TURRET || o.bt().role == BR_AATURRET || wallRole(o.bt().role)) ? 1.5f : 0.6f); });
                if (v > best) { best = v; bp = e.pos; }
            }
            if (best >= bar * 0.5f) { at = bp; return true; }
        }
        return score >= bar;
    };
    Vec2 at;
    if (S.islandNukesReady(player) > 0 && strikeSpot(ISLAND_NUKE_SCALE, false, nukeBar * 0.5f, at)) S.cmdIslandNuke(player, at);
    if (pl.difficulty >= 1 && S.bombersReady(player) > 0 && strikeSpot(NUKEBOMBER_SCALE, true, nukeBar * 0.9f, at)) S.cmdNukeBomber(player, at);

    // ---------- a Mountain Fortress in reach of the army is worth holding: a squad of infantry claims and fills it (a sniper first, to see
    // infiltrators coming), while the army is not out on a wave
    if (pl.difficulty >= 1 && !attacking && minutes >= 4.0f) {
        const Entity* fort = nullptr; float fd = 75.0f * TILE;
        for (auto& e : S.ents) {
            if (!e.alive || e.type != B_C_FORTRESS || (e.owner >= 0 && e.owner != player) || S.inFallout(e.pos)) continue;
            if (S.garrisonInfantry(e) >= 12) continue;
            float d = dist(e.pos, pl.basePos); if (d < fd) { fd = d; fort = &e; }
        }
        if (fort) {
            Ref fr = S.refOf(*fort);
            int coming = S.garrisonInfantry(*fort);
            for (auto& e : S.ents) if (e.alive && e.owner == player && e.order == O_ENTER && e.targetEnt == fr) coming++;
            std::vector<Ref> pick;
            for (int pass = 0; pass < 2 && coming + (int)pick.size() < 12; pass++)
                for (auto& e : S.ents) {
                    if (coming + (int)pick.size() >= 12) break;
                    if (!e.alive || e.owner != player || !e.isUnit() || e.ut().kind != UK_INF || e.ut().role != UR_COMBAT || e.carrier.valid() || e.fall > 0) continue;
                    if ((pass == 0) != (e.ut().sniper != 0)) continue;   // the snipers go first
                    if (e.order != O_IDLE && e.order != O_GUARDAREA && e.order != O_GUARDPOS) continue;
                    if (std::find(wave.begin(), wave.end(), S.refOf(e)) != wave.end() || !S.canBoard(e, *fort)) continue;
                    if (dist(e.pos, pl.basePos) > 30.0f * TILE) continue;   // (the home reserve, not troops posted elsewhere)
                    pick.push_back(S.refOf(e));
                }
            int reserve = 0; for (auto& e : S.ents) if (e.alive && e.owner == player && e.isUnit() && e.ut().kind == UK_INF && e.ut().role == UR_COMBAT && !e.carrier.valid()) reserve++;
            if ((int)pick.size() >= 3 && reserve >= 10) S.cmdEnter(pick, fr);   // (only with an army to spare: a lone squad is not sent off)
        }
    }

    // ---------- the island launchers we do not hold
    if (pl.difficulty < 1 || minutes < 6.0f) return;
    const Entity* prize = nullptr; float prizeD = 1e30f;
    for (auto& e : S.ents) {
        if (!e.alive || !e.isBuilding() || e.bt().role != BR_ISLANDNUKE) continue;
        if (e.owner == player || (e.owner >= 0 && !S.enemies(player, e.owner))) continue;   // ours or a friend's
        float d = dist(e.pos, pl.basePos);
        if (d < prizeD) { prizeD = d; prize = &e; }
    }
    if (!prize) return;
    Ref prizeRef = S.refOf(*prize);
    Vec2 landing = besideLauncher(*prize);
    int spyT = pl.faction == F_CYBER ? U_C_SPY : U_K_SPY, liftT = pl.faction == F_CYBER ? U_C_CARGO : U_K_CARGO;
    Entity* lifter = nullptr; std::vector<Entity*> spies;
    for (auto& e : S.ents) {
        if (!e.alive || !e.isUnit() || e.owner != player) continue;
        if (e.type == liftT && !lifter) lifter = &e;
        if (e.type == spyT) spies.push_back(&e);
    }
    // an infiltrator already on the island: take the launcher
    for (Entity* s : spies) {
        if (s->carrier.valid()) continue;
        if (dist(s->pos, prize->pos) < 5.5f * TILE && !(s->order == O_CAPTURE && s->targetEnt == prizeRef)) S.cmdCapture({S.refOf(*s)}, prizeRef);
        if (dist(s->pos, prize->pos) < 5.5f * TILE) return;
    }
    // what we need for the trip: an infiltrator and a lifter (the tech structure unlocks both)
    bool tech = S.techOnline(player);
    if (!tech || pl.money < 1800) return;
    if (spies.empty()) {
        for (auto& b : S.ents) if (b.alive && b.isBuilding() && b.owner == player && b.constructed && b.bt().role == BR_BARRACKS && b.queue.empty()) { if (S.cmdTrain(S.refOf(b), spyT)) break; }
        return;
    }
    if (!lifter) {   // no lifter yet: one infiltrator paddles out on its raft (slower and exposed on the water, but it needs no airframe)
        bool going = false;
        for (Entity* s : spies) if (s->order == O_CAPTURE && s->targetEnt == prizeRef) going = true;
        if (!going) for (Entity* s : spies) if (!s->carrier.valid() && s->order != O_CAPTURE) { S.cmdCapture({S.refOf(*s)}, prizeRef); break; }
        if (pl.money < 4200) return;
        for (auto& a : S.ents) if (a.alive && a.isBuilding() && a.owner == player && a.constructed && a.bt().role == BR_AIRFIELD && a.queue.empty()) { if (S.cmdTrain(S.refOf(a), liftT)) break; }
        return;
    }
    Ref lref = S.refOf(*lifter);
    bool aboard = false;
    for (Ref r : lifter->passengers) if (const Entity* u = S.get(r)) if (u->type == spyT) aboard = true;
    if (aboard) {   // fly out and set down beside the launcher
        if (lifter->order != O_UNLOAD && !lifter->unloading && dist(lifter->pos, landing) > 1.5f * TILE) S.cmdUnload(lref, true, landing);
        else if (dist(lifter->pos, landing) <= 1.5f * TILE && !lifter->unloading) S.cmdUnload(lref, false, landing);
        return;
    }
    // no infiltrator aboard: the nearest idle one walks to the lifter (which waits where it is)
    Entity* pick = nullptr; float pd = 1e30f;
    for (Entity* s : spies) {
        if (s->carrier.valid() || s->order == O_CAPTURE) continue;
        float d = dist(s->pos, lifter->pos);
        if (d < pd) { pd = d; pick = s; }
    }
    if (!pick) return;
    if (lifter->order == O_IDLE && !(pick->order == O_ENTER && pick->targetEnt == lref)) {
        if (dist(lifter->pos, pick->pos) > 14.0f * TILE) S.cmdUnload(lref, true, g_map.nearestFree(pick->pos + Vec2(40, 0), 6));   // (the lifter comes to fetch it: an empty unload is just a flight)
        else S.cmdEnter({S.refOf(*pick)}, lref);
    }
}

// ------------------------------------------------------------ the relic bases, the canal's bridges and the submarines (F2_RELIC, F2_SUBS)
//  * a Hydra Silo it holds fires at the best target it knows; relic factories it holds build their war machines while the money is there
//  * a relic structure nobody holds is a prize: a squad of the home infantry walks over and claims it (the Command first), an infiltrator swims out to one
//    on an island, and one goes to look at the secret islands once the match has run a while
//  * a broken bridge near home is raised again by a dozer; once it holds a Relic Command it raises a few relic defences at home
//  * its submarines stay down, and one surfaces to launch whenever a target is worth a warhead
void AiPlayer::subOps() {
    Sim& S = g_sim;
    Player& pl = S.players[player];
    const float minutes = S.time / 60.0f;
    const int subT = pl.faction == F_CYBER ? U_C_SUB : U_K_SUB;
    std::vector<Entity*> subs;
    for (auto& e : S.ents) if (e.alive && e.isUnit() && e.owner == player && subType(e.type)) subs.push_back(&e);
    const int cap = pl.difficulty >= 3 ? 5 : (pl.difficulty == 2 ? 3 : (pl.difficulty == 1 ? 1 : 0));
    if (on2(F2_SUBS) && cap > 0 && minutes > 12.0f && S.subsOf(player) < cap && pl.money > UNITS[subT].cost + 9000 && S.unitAvailable(player, subT)) {
        for (auto& y : S.ents) if (y.alive && y.isBuilding() && y.owner == player && y.constructed && y.bt().role == BR_NAVAL && y.queue.empty() && g_map.compAt(S.navalExit(y)) == g_map.oceanComp) {
            if (S.cmdTrain(S.refOf(y), subT)) break;
        }
    }
    // down, out of every sight, unless it is up to launch or must stay up after one
    for (Entity* s : subs) if (s->subState == 0 && s->nukeSalvo == 0 && !s->subAutoDive && S.time >= s->exposedUntil && S.time - s->lastDamaged > 2.0f) S.cmdDive({S.refOf(*s)}, true);
    if (S.subNukesReady(player) > 0) {
        const float nukeBar = pl.difficulty >= 3 ? 2600.0f : (pl.difficulty == 2 ? 3200.0f : 4000.0f);
        float score; Vec2 at = frontierTarget(player, NUKE_RADIUS * TILE * std::sqrt(SUB_NUKE_SCALE), SUB_NUKE_SCALE, false, score);
        if (score >= nukeBar && S.cmdSubNuke(player, at)) subLaunches++;
    }
}

void AiPlayer::relicOps() {
    Sim& S = g_sim;
    Player& pl = S.players[player];
    if (S.time < nextRelic) return;
    nextRelic = S.time + 2.0f;
    const float minutes = S.time / 60.0f;
    const float nukeBar = pl.difficulty >= 3 ? 2600.0f : (pl.difficulty == 2 ? 3200.0f : 4000.0f);
    subOps();
    if (!on2(F2_RELIC)) return;
    // ---------- a Hydra we hold: five warheads over a wide ring, so the target must be worth it
    if (S.relicNukesReady(player) > 0) {
        float R = (RELIC_SILO_SPREAD * 0.7f + NUKE_RADIUS * std::sqrt(RELIC_SILO_SCALE)) * TILE, score;
        Vec2 at = frontierTarget(player, R, RELIC_SILO_SCALE, false, score);
        if (score >= nukeBar * 1.2f && S.cmdRelicNuke(player, at)) hydras++;
    }
    // ---------- relic factories we hold: their war machines while there is money to spare
    if (pl.money > 7000) for (auto& b : S.ents) {
        if (!b.alive || !b.isBuilding() || b.owner != player || !b.constructed || !b.queue.empty() || b.disabledUntil > S.time) continue;
        if (b.bt().role == BR_RELICFOUNDRY) S.cmdTrain(S.refOf(b), rng.f() < 0.55f ? U_R_JUGGERNAUT : U_R_STRIDER);
        else if (b.bt().role == BR_RELICPAD) S.cmdTrain(S.refOf(b), U_R_THUNDERHAWK);
    }
    // ---------- a broken bridge near home: the nearest free dozer raises it again (unless one of ours is already at it)
    if (minutes >= 2.0f && !recovering) {
        for (auto& b : S.ents) {
            if (!b.alive || !Sim::bridgeEnt(b) || b.constructed || dist(b.pos, pl.basePos) > 95.0f * TILE || S.time - b.lastDamaged < 6.0f) continue;
            Ref br = S.refOf(b); bool working = false; int dozers = 0;
            for (auto& u : S.ents) if (u.alive && u.isUnit() && u.owner == player && u.ut().role == UR_DOZER && !civicDozerType(u.type)) { dozers++; if (u.order == O_BUILD && u.targetEnt == br) working = true; }
            if (working || dozers < 2) break;
            if (Entity* d = idleDozer()) { S.cmdAssist({S.refOf(*d)}, br); bridgeRepairs++; }
            break;
        }
    }
    // ---------- relic defences at home once we hold a Relic Command (its flak and lancer towers are far tougher than our own)
    if (S.relicBuildable(player) && pl.money > 9000 && S.time >= nextRelicBuild && !recovering) {
        nextRelicBuild = S.time + 45.0f;
        int have = 0;
        for (auto& b : S.ents) if (b.alive && b.isBuilding() && b.owner == player && (b.type == B_R_FLAK || b.type == B_R_LASER || b.type == B_R_CANNON) && dist(b.pos, pl.basePos) < 30.0f * TILE) have++;
        if (have < 2 + pl.difficulty) tryBuild(have % 3 == 2 ? B_R_CANNON : (have % 2 ? B_R_FLAK : B_R_LASER), pl.basePos + enemyDir * (8.0f * TILE), 18);
    }
    // ---------- relic structures nobody holds: claim them
    if (pl.difficulty < 1 || minutes < 3.0f || recovering) return;
    const Entity* goal = S.get(relicGoal);
    if (goal && (goal->owner >= 0 || !goal->constructed)) { goal = nullptr; relicGoal = NOREF; }
    Vec2 home = g_map.nearestFree(pl.basePos + enemyDir * (4.0f * TILE), 8);
    if (!goal) {
        float best = -1e30f;
        for (auto& e : S.ents) {
            if (!e.alive || !Sim::relicEnt(e) || e.owner >= 0 || !e.constructed || e.bt().role == BR_STRONGHOLD || e.bt().role == BR_WALL) continue;
            if (!S.visibleTo(e, player) || !S.explored(player, clampi(tileOf(e.pos.x), 0, MAP_W - 1), clampi(tileOf(e.pos.y), 0, MAP_H - 1))) continue;
            if (S.time - relicBadAt < 90.0f && S.refOf(e) == relicBad) continue;
            Vec2 door = g_map.nearestFree(e.pos + Vec2(0, (e.bt().h * 0.5f + 1.0f) * TILE), 4);
            bool walk = g_map.sameGround(home, door);
            float v = e.type == B_R_COMMAND ? 10.0f : (e.type == B_R_SILO ? 8.0f : ((e.type == B_R_FOUNDRY || e.type == B_R_AIRSTRIP) ? 6.0f : (e.type == B_R_REACTOR ? 5.0f : 3.5f)));
            float d = dist(e.pos, pl.basePos) / TILE;
            if (d > 140.0f) continue;
            float score = v * 100.0f - d * 3.0f - (walk ? 0.0f : 120.0f) - enemyStrengthNear(e.pos, 10.0f) * 0.05f;
            if (score > best) { best = score; goal = &e; }
        }
        if (goal) relicGoal = S.refOf(*goal);
    }
    if (!goal) {   // nothing known to claim: once in a while an infiltrator goes to look at a secret island we have not found yet
        if (minutes >= 8.0f && S.time >= nextSecretLook && g_map.seaIslands.size() >= 4) {
            nextSecretLook = S.time + 240.0f;
            const auto& isl = g_map.seaIslands[2 + (int)(rng.next() % 2)];
            Vec2 spot(isl.cx * TILE, isl.cy * TILE);
            int spyT = pl.faction == F_CYBER ? U_C_SPY : U_K_SPY;
            for (auto& e : S.ents) if (e.alive && e.isUnit() && e.owner == player && e.type == spyT && !e.carrier.valid() && e.order == O_IDLE) { S.cmdMove({S.refOf(e)}, spot, false); break; }
        }
        return;
    }
    Ref gref = S.refOf(*goal);
    Vec2 door = g_map.nearestFree(goal->pos + Vec2(0, (goal->bt().h * 0.5f + 1.0f) * TILE), 4);
    const bool walk = g_map.sameGround(home, door);
    // the squad: whoever of it is still alive and free, topped up with idle home infantry (never the wave, snipers or infiltrators)
    relicSquad.erase(std::remove_if(relicSquad.begin(), relicSquad.end(), [&](Ref r) { const Entity* u = S.get(r); return !u || u->carrier.valid(); }), relicSquad.end());
    if (walk) {
        if ((int)relicSquad.size() < 4) for (auto& e : S.ents) {
            if ((int)relicSquad.size() >= 4) break;
            if (!e.alive || e.owner != player || !e.isUnit() || e.carrier.valid() || e.fall > 0 || !S.relicClaimable(e, *goal)) continue;
            if (e.order != O_IDLE && e.order != O_GUARDAREA && e.order != O_GUARDPOS) continue;
            if (std::find(wave.begin(), wave.end(), S.refOf(e)) != wave.end() || std::find(relicSquad.begin(), relicSquad.end(), S.refOf(e)) != relicSquad.end()) continue;
            if (dist(e.pos, pl.basePos) > 40.0f * TILE || !g_map.sameGround(e.pos, door)) continue;   // (not one boxed in somewhere it cannot walk out of)
            relicSquad.push_back(S.refOf(e));
        }
        if (relicSquad.size() < 2) return;   // (not a lone soldier)
        std::vector<Ref> send;
        for (Ref r : relicSquad) if (const Entity* u = S.get(r)) if (!(u->order == O_CAPTURE && u->targetEnt == gref)) send.push_back(r);
        if (!send.empty()) { S.cmdCapture(send, gref); relicClaims++; }
        // a squad that has been at it for long without getting there gives up on that one for a while
        if (relicSince <= 0) relicSince = S.time;
        if (S.time - relicSince > 150.0f) { relicBad = gref; relicBadAt = S.time; relicGoal = NOREF; relicSince = 0; for (Ref r : relicSquad) S.cmdStop({r}); relicSquad.clear(); }
        return;
    }
    relicSince = 0;
    // out on an island: an infiltrator swims out and takes it
    int spyT = pl.faction == F_CYBER ? U_C_SPY : U_K_SPY;
    bool going = false;
    for (auto& e : S.ents) if (e.alive && e.isUnit() && e.owner == player && e.type == spyT && e.order == O_CAPTURE && e.targetEnt == gref) going = true;
    if (going) return;
    for (auto& e : S.ents) if (e.alive && e.isUnit() && e.owner == player && e.type == spyT && !e.carrier.valid() && e.order != O_CAPTURE) { S.cmdCapture({S.refOf(e)}, gref); relicClaims++; return; }
    if (S.techOnline(player) && pl.money > 2500) for (auto& b : S.ents) if (b.alive && b.isBuilding() && b.owner == player && b.constructed && b.bt().role == BR_BARRACKS && b.queue.empty()) { if (S.cmdTrain(S.refOf(b), spyT)) break; }
}
