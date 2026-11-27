#include "ai.h"
#include <cstdio>
#include <cstdlib>

namespace {
bool aiDebugGhosts() { static const bool debug = std::getenv("ONEHOUR_DEBUG") != nullptr; return debug; }

bool knownTo(const Sim& S, const Entity& e, int p) {
    return S.explored(p, clampi(tileOf(e.pos.x), 0, MAP_W - 1), clampi(tileOf(e.pos.y), 0, MAP_H - 1))
        && S.visibleTo(e, p);
}

const float GHOST_SUB_SCALE = SUB_NUKE_SCALE * 4.0f, GHOST_JET_SCALE = NUKEBOMBER_SCALE * 9.0f;
constexpr int HELL_SAVE_START = 22000, HELL_CASH_BUFFER = 4000;
constexpr float HELL_SAVE_SPAN = 240.0f, HELL_SAVE_RAMP = 60.0f, HELL_SAVE_PAUSE = 300.0f;

int hellType(const Player& p) { return p.faction == F_CYBER ? B_C_HELLBUNKER : B_K_HELLBUNKER; }

int hellCount(const Sim& s, int player) {
    return s.countBuildings(player, B_C_HELLBUNKER) + s.countBuildings(player, B_K_HELLBUNKER);
}

bool wantsHell(float time, int count) {
    return count == 0 ? time >= 14 * 60 : count == 1 && time >= 30 * 60;
}

bool strikeSpot(int player, bool nostopme, Vec2& bestAt) {
    Sim& S = g_sim;
    Sim::Nuke sub, jet;
    sub.scale = GHOST_SUB_SCALE; jet.scale = GHOST_JET_SCALE;
    const float subR = S.nukeRadius(sub), jetR = S.nukeRadius(jet);
    const float R = std::max(subR, jetR);
    std::vector<Vec2> candidates;
    std::vector<float> value(S.ents.size(), 0.0f);
    std::vector<const Entity*> bunkers, friendlies;
    for (const Entity& e : S.ents) {
        if (!e.alive || e.kind == EK_RESOURCE || e.owner < 0 || e.owner >= S.numPlayers) continue;
        if (!S.enemies(player, e.owner)) {
            if (!e.carrier.valid()) friendlies.push_back(&e);
            continue;
        }
        if (!knownTo(S, e, player)) continue;
        if (e.isBuilding() && e.constructed && e.bt().role == BR_BUNKER) bunkers.push_back(&e);
        if (Sim::invulnerable(e) || Sim::districtRuin(e) || e.carrier.valid() || e.fall > 0) continue;
        const float worth = e.isBuilding() ? e.bt().cost : (e.ut().role == UR_COMBAT ? e.ut().cost * (0.5f + 0.5f * e.hp / e.maxHp) : 0.0f);
        if (worth <= 0) continue;
        value[&e - S.ents.data()] = worth;
        if (e.pos.x >= 0 && e.pos.y >= 0 && e.pos.x < WORLD_W && e.pos.y < WORLD_H) candidates.push_back(e.pos);
    }

    float best = 0;
    size_t friendlyVetoes = 0;
    auto scoreSpot = [&](Vec2 at) {
        // Both strikes are vetoed against own and allied positions at launch, including aircraft.
        // Scan the live list: new structures and just-unloaded units may not be in this tick's grid.
        // Embarked units are covered by their carrier, not by their stale pre-boarding position.
        if (std::any_of(friendlies.begin(), friendlies.end(), [&](const Entity* e) {
            const float reach = R + e->radius() * 0.5f;
            return dist2(at, e->pos) <= reach * reach || (e->isBuilding() && S.distToEntity(at, *e) <= R);
        })) { friendlyVetoes++; return; }
        float score = 0;
        S.forEachNearMask(at, R + 2.0f * TILE, S.enemyMask(player), [&](Entity& e) {
            const float worth = value[&e - S.ents.data()];
            if (worth <= 0) return;
            const float d2 = dist2(at, e.pos);
            const float reach = R + e.radius() * 0.5f;  // nukeBlast measures from half the entity radius
            if (d2 > reach * reach) return;
            const float d = std::max(0.0f, std::sqrt(d2) - e.radius() * 0.5f);
            score += worth * (1.0f - 0.75f * std::max(0.0f, d / R - 0.35f) / 0.65f);
        });
        if (score <= best) return;
        if (nostopme) {
            for (const Entity* b : bunkers) if (hellBunker(b->type) && dist2(at, b->pos) <= R * R) {
                score *= HELL_NOSTOPME_CUT; break;
            }
        } else {
            float subMiss = 1.0f - S.antiNukeChance(at, player, true, GHOST_SUB_SCALE, true);
            float jetMiss = 1.0f - S.antiNukeChance(at, player, true, GHOST_JET_SCALE);
            for (const Entity* b : bunkers) {
                if (b->disabledUntil > S.time) continue;
                const bool hell = hellBunker(b->type), laser = hell || Sim::bunkerHas(*b, BU_LASERS);
                const float cover = (hell ? HELL_ANTINUKE_COVER : laser ? BUNKER_LASER_COVER : BUNKER_ANTINUKE_COVER) * TILE;
                const float chance = hell ? HELL_ANTINUKE_CHANCE : laser ? BUNKER_LASER_NUKE : BUNKER_ANTINUKE_CHANCE;
                const float d2 = dist2(at, b->pos);
                if (laser && d2 <= (cover + subR) * (cover + subR)) subMiss *= 1.0f - chance * UNUKE_INTERCEPT_RESIST;
                if (d2 <= (cover + jetR) * (cover + jetR)) jetMiss *= 1.0f - chance;
            }
            score *= 0.2f + 0.8f * (subMiss + jetMiss) * 0.5f;
        }
        if (score > best) { best = score; bestAt = at; }
    };
    for (Vec2 at : candidates) scoreSpot(at);
    if (best > 0 || candidates.empty() || !nostopme) return best > 0;

    // The fast nostopme warhead can put known targets on the near side of the blast
    // if an enemy-centered shot is unsafe. Leave the longer-flight Ghost salvo's
    // target selection unchanged. Snap nearby samples together to bound the work.
    constexpr int stride = 4;
    constexpr int cols = (MAP_W + stride - 1) / stride, rows = (MAP_H + stride - 1) / stride;
    const Vec2 directions[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1},
                               {0.7071f, 0.7071f}, {0.7071f, -0.7071f}, {-0.7071f, 0.7071f}, {-0.7071f, -0.7071f}};
    std::vector<u8> tried(cols * rows, 0);
    const size_t centeredVetoes = friendlyVetoes;
    size_t offsets = 0;
    for (Vec2 target : candidates) for (float fraction : {0.30f, 0.60f, 0.90f}) for (Vec2 dir : directions) {
        Vec2 at = target + dir * (R * fraction);
        const int x = clampi(tileOf(at.x), 0, MAP_W - 1) / stride, y = clampi(tileOf(at.y), 0, MAP_H - 1) / stride;
        if (tried[y * cols + x]) continue;
        tried[y * cols + x] = 1;
        offsets++;
        scoreSpot(tileCenter(std::min(x * stride + stride / 2, MAP_W - 1), std::min(y * stride + stride / 2, MAP_H - 1)));
    }
    if (best > 0 && aiDebugGhosts())
        printf("[ai %d] t=%.0f STRIKE OFFSET %s: %zu/%zu centers vetoed, %zu/%zu offsets vetoed, best %.0f\n",
               player, S.time, nostopme ? "nostopme" : "ghost", centeredVetoes, candidates.size(), friendlyVetoes - centeredVetoes, offsets, best);
    return best > 0;
}

void interceptGhosts(int player) {
    Sim& S = g_sim;
    for (int p = 0; p < S.numPlayers; p++) {
        const Sim::GhostPair& g = S.ghosts[p];
        const Sim::GhostSighting& sight = g.sighting;
        if (sight.team != S.players[player].team || sight.until <= S.time || !S.enemies(player, p)) continue;
        const Entity* jet = S.get(g.jet);
        const Entity* sub = S.get(g.sub);
        const float left = sight.until - S.time;
        const int sea = g_map.compAt(sight.at);
        std::vector<Ref> vsJet, vsSub;
        S.forEachNearMask(sight.at, 30.0f * TILE, 1u << (player + 1), [&](Entity& e) {
            if (e.owner != player || !e.isUnit() || !(e.isAir() || e.ut().naval) || e.ut().temp || e.ut().nukeBomb || e.carrier.valid() || e.fall > 0
                || e.disabledUntil > S.time || e.order == O_REARM || e.weapon() < 0 || (e.ut().ammo > 0 && e.ammo <= 0)) return;
            if (e.order == O_ATTACK && (e.targetEnt == g.jet || e.targetEnt == g.sub)) return;
            const bool diving = subType(e.type) && e.subState != 0;
            const Weapon& w = WEAPONS[e.weapon()];
            const float d = dist(sight.at, e.pos);
            const float range = w.range * TILE;
            if (d < w.minRange * TILE || (e.ut().naval && d > range && (!sea || g_map.compAt(e.pos) != sea))) return;
            const float eta = std::max(0.0f, d - range) / std::max(1.0f, e.ut().speed) + (diving ? SUB_SURFACE_TIME : 0.0f);
            if (eta >= left) return;
            if (jet && S.canTarget(e, *jet)) vsJet.push_back(S.refOf(e));
            else if (sub && (S.canTarget(e, *sub) || S.subCouldTarget(e, *sub))) vsSub.push_back(S.refOf(e));
        });
        if (!vsJet.empty()) S.cmdAttack(vsJet, g.jet);
        if (!vsSub.empty()) S.cmdAttack(vsSub, g.sub);
        if ((!vsJet.empty() || !vsSub.empty()) && aiDebugGhosts())
            printf("[ai %d] t=%.0f GHOST INTERCEPT enemy %d: %zu vs jet, %zu vs sub, %.1fs left\n", player, S.time, p, vsJet.size(), vsSub.size(), left);
    }
}
}

int AiPlayer::updateHellReserve(bool blocked) {
    const Sim& s = g_sim;
    const Player& pl = s.players[player];
    if (!hellSaveUntil && (blocked || s.time < 14 * 60 || s.time < hellSaveRetryAt || pl.money < HELL_SAVE_START)) return 0;

    const int type = hellType(pl);
    const bool timedOut = hellSaveUntil && s.time >= hellSaveUntil;
    const bool eligible = !blocked && !timedOut && wantsHell(s.time, hellCount(s, player)) && s.buildAvailable(player, type);
    if (!eligible) {
        if (hellSaveUntil) {
            if (aiDebugGhosts()) printf("[ai %d] t=%.0f HELL SAVE STOP ($%d, %s)\n", player, s.time, pl.money,
                                        blocked ? "base threatened or recovering" : timedOut ? "timeout" : "prerequisites lost");
            hellSaveUntil = 0;
            hellSaveRetryAt = s.time + HELL_SAVE_PAUSE;
        }
        return 0;
    }
    if (!hellSaveUntil) {
        hellSaveUntil = s.time + HELL_SAVE_SPAN;
        if (aiDebugGhosts()) printf("[ai %d] t=%.0f HELL SAVE START ($%d)\n", player, s.time, pl.money);
    }
    const float progress = clampf((s.time - (hellSaveUntil - HELL_SAVE_SPAN)) / HELL_SAVE_RAMP, 0.0f, 1.0f);
    return HELL_SAVE_START + (int)((BUILDS[type].cost + HELL_CASH_BUFFER - HELL_SAVE_START) * progress);
}

void AiPlayer::hellConstruction(int reserve, bool blocked) {
    Sim& s = g_sim;
    const Player& pl = s.players[player];
    if (blocked || s.time < 14 * 60 || s.time < hellSaveRetryAt || s.time < nextHellBuild) return;
    const int type = hellType(pl);
    if (pl.money < BUILDS[type].cost + std::max(reserve, HELL_CASH_BUFFER)) return;
    const int count = hellCount(s, player);
    if (!wantsHell(s.time, count)) return;
    nextHellBuild = s.time + 12.0f;
    const Vec2 side(-enemyDir.y, enemyDir.x);
    // The late base can fill the inner rings. Keep the core preference but search out to its perimeter.
    if (!tryBuild(type, pl.basePos + side * ((count ? -5.0f : 5.0f) * TILE))) {
        if (hellSaveUntil && aiDebugGhosts())
            printf("[ai %d] t=%.0f HELL BUILD WAIT ($%d, %s)\n", player, s.time, pl.money,
                   !s.buildAvailable(player, type) ? "prerequisites" : idleDozer() ? "no legal site" : "no free dozer");
        return;
    }
    hellSaveUntil = 0;
    if (aiDebugGhosts()) printf("[ai %d] t=%.0f HELL BUILD (%d/2)\n", player, s.time, count + 1);
}

void AiPlayer::ghostProduction(int reserve) {
    Sim& S = g_sim;
    const Player& pl = S.players[player];
    if (S.time < nextGhostBuy) return;
    nextGhostBuy = S.time + 2.0f;
    const bool cyber = pl.faction == F_CYBER;
    const int drone = cyber ? U_C_GHOST2 : U_K_GHOST2, pair = cyber ? U_C_GHOSTSUB : U_K_GHOSTSUB;
    int drones = unitsOf(drone);
    bool enemyYard = false;
    Entity* field = nullptr;
    for (Entity& e : S.ents) {
        if (!e.alive || !e.isBuilding()) continue;
        if (e.owner != player) {
            if (!enemyYard && e.constructed && e.bt().role == BR_NAVAL && S.enemies(player, e.owner) && knownTo(S, e, player)) enemyYard = true;
            continue;
        }
        if (e.bt().role != BR_AIRFIELD) continue;
        drones += (int)std::count(e.queue.begin(), e.queue.end(), drone);
        if (e.constructed && e.disabledUntil <= S.time && e.queue.size() < 2 && (!field || e.queue.size() < field->queue.size())) field = &e;
    }
    if (recovering) return;
    if (field) {
        const Ref airfield = S.refOf(*field);
        const int want = (pl.difficulty >= 2 ? 3 : 2) + (enemyYard ? 1 : 0);
        // Take a surplus-funded pair before spending down the bank on replacement drones.
        if (S.time >= 9 * 60 && pl.money >= UNITS[pair].cost + std::max(reserve, 2500)
            && !S.ghostTrainBlockReason(player) && S.unitAvailable(player, pair) && S.cmdTrain(airfield, pair)) {
            if (aiDebugGhosts()) printf("[ai %d] t=%.0f GHOSTS BUY\n", player, S.time);
        } else if (drones < want && pl.money >= UNITS[drone].cost + std::max(reserve, 2000)
                   && S.unitAvailable(player, drone) && S.cmdTrain(airfield, drone)) {
            if (aiDebugGhosts()) printf("[ai %d] t=%.0f GHOST2 BUY (%d/%d)\n", player, S.time, drones + 1, want);
        }
    }
}

bool AiPlayer::isHellGuard(Ref r) const {
    for (const HellGuard& g : hellGuards) if (std::find(g.troops.begin(), g.troops.end(), r) != g.troops.end()) return true;
    return false;
}

void AiPlayer::ghostOps(const Entity* threat) {
    Sim& S = g_sim;
    interceptGhosts(player);
    if (threat) hellDefendUntil = S.time + 15.0f;
    hellGuards.erase(std::remove_if(hellGuards.begin(), hellGuards.end(), [&](const HellGuard& g) {
        const Entity* b = S.get(g.bunker);
        return !b || b->owner != player;
    }), hellGuards.end());

    Ref readyHell;
    for (Entity& b : S.ents) {
        if (!b.alive || b.owner != player || !b.isBuilding() || !hellBunker(b.type) || !b.constructed || !b.hellGarrisoned) continue;
        const Ref bunker = S.refOf(b);
        if (!readyHell.valid() && b.disabledUntil <= S.time && S.nostopmeCooldownLeft(b) == 0) readyHell = bunker;
        // Include the perimeter of a crowded home but not bunkers left at a former base.
        if (!threat || dist2(S.players[player].basePos, b.pos) > (32 * TILE) * (32 * TILE)
            || b.unloading || b.passengers.empty() || !S.visibleTo(*threat, player)) continue;
        auto it = std::find_if(hellGuards.begin(), hellGuards.end(), [&](const HellGuard& g) { return g.bunker == bunker; });
        if (it == hellGuards.end()) { hellGuards.push_back({bunker, {}}); it = hellGuards.end() - 1; }
        for (Ref r : b.passengers) if (std::find(it->troops.begin(), it->troops.end(), r) == it->troops.end()) it->troops.push_back(r);
        S.cmdUnload(bunker, false, b.pos);
        if (aiDebugGhosts()) printf("[ai %d] t=%.0f HELL UNLOAD %zu\n", player, S.time, b.passengers.size());
    }
    for (HellGuard& g : hellGuards) {
        const Entity* b = S.get(g.bunker);
        const bool defending = S.time < hellDefendUntil;
        std::vector<Ref> orders;
        g.troops.erase(std::remove_if(g.troops.begin(), g.troops.end(), [&](Ref r) {
            const Entity* e = S.get(r);
            if (!e || e->owner != player) return true;
            if (e->carrier.valid()) return e->carrier != g.bunker || (!defending && !b->unloading);
            if (defending) {
                if (e->weapon() >= 0 && (e->order == O_IDLE || e->order == O_ENTER)) orders.push_back(r);
                return false;
            }
            if (!b->unloading && S.canBoard(*e, *b) && (e->order != O_ENTER || e->targetEnt != g.bunker)) orders.push_back(r);
            return false;
        }), g.troops.end());
        if (orders.empty()) continue;
        if (defending) {
            Vec2 post = b->pos;
            if (threat && S.visibleTo(*threat, player)) post += (threat->pos - post).norm() * std::min(5.0f * TILE, dist(post, threat->pos));
            S.cmdGuardArea(orders, post, 10.0f * TILE);
        } else {
            S.cmdEnter(orders, g.bunker);
            if (aiDebugGhosts()) printf("[ai %d] t=%.0f HELL REBOARD %zu\n", player, S.time, orders.size());
        }
    }
    hellGuards.erase(std::remove_if(hellGuards.begin(), hellGuards.end(), [](const HellGuard& g) { return g.troops.empty(); }), hellGuards.end());

    const Sim::GhostPair& g = S.ghosts[player];
    const Entity* sub = S.get(g.sub);
    const Entity* jet = S.get(g.jet);
    const bool ghostReady = sub && jet && S.time >= g.strikeReadyAt && sub->disabledUntil <= S.time && jet->disabledUntil <= S.time;
    if ((!ghostReady && !readyHell.valid()) || S.time < nextGhostStrikeLook) return;
    nextGhostStrikeLook = S.time + 4.0f;
    Vec2 at;
    if (ghostReady && strikeSpot(player, false, at) && S.cmdGhostStrike(player, at)) {
        nextGhostStrikeLook = S.time + Sim::NUKE_FLIGHT + 1.0f;  // re-score after the salvo lands before committing the next warhead
        if (aiDebugGhosts()) printf("[ai %d] t=%.0f GHOST STRIKE (%d,%d)\n", player, S.time, tileOf(at.x), tileOf(at.y));
        return;
    }
    if (readyHell.valid() && strikeSpot(player, true, at) && S.cmdNostopme(player, readyHell, at) && aiDebugGhosts())
        printf("[ai %d] t=%.0f HELL NOSTOPME (%d,%d)\n", player, S.time, tileOf(at.x), tileOf(at.y));
}
