// The Ghosts: a submarine and its jet are trained as one pair, move as one, live and die together, and are found only by a Ghost2 sighting.
#include "sim.h"
#include "testutil.h"
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
void setup(u64 seed) {
    Faction factions[] = {F_CYBER, F_CLANKER};
    bool ai[] = {false, false};
    int difficulty[] = {1, 1}, teams[] = {0, 1};
    g_sim.init(2, factions, ai, difficulty, teams, seed);
    g_sim.rain = g_sim.rainTo = 0; g_sim.nextRainRoll = 1e9f;   // (a dry sky: no stray fire or rain in these scenes)
    for (int p = 0; p < 2; p++) {   // (a known map, as the other naval scenes use: canPlace/exploredRaw would refuse every unexplored coast)
        g_sim.players[p].money = 999999;
        std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1);
    }
}

void advance(int ticks) {
    for (int i = 0; i < ticks; ++i) { g_sim.step(); g_sim.events.clear(); }
}

int buildFor(Faction faction, BuildRole role) {
    for (int i = 0; i < B_COUNT; ++i) if (BUILDS[i].faction == faction && BUILDS[i].role == role) return i;
    return -1;
}

int ghostSubFor(Faction faction) { return faction == F_CYBER ? U_C_GHOSTSUB : U_K_GHOSTSUB; }
int ghostJetFor(Faction faction) { return faction == F_CYBER ? U_C_GHOSTJET : U_K_GHOSTJET; }
int ghost2For(Faction faction) { return faction == F_CYBER ? U_C_GHOST2 : U_K_GHOST2; }

// A finished structure of 'type' on the nearest legal spot to 'near' (ring by ring).
Ref placeNear(int player, int type, Vec2 near, int maxR = 40) {
    if (type < 0) return NOREF;
    const int cx = tileOf(near.x), cy = tileOf(near.y);
    for (int r = 0; r <= maxR; ++r)
        for (int dy = -r; dy <= r; ++dy) for (int dx = -r; dx <= r; ++dx) {
            if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
            const int tx = cx + dx, ty = cy + dy;
            if (g_sim.canPlace(player, type, tx, ty)) return g_sim.placeBuilding(type, player, tx, ty, true);
        }
    return NOREF;
}

struct Pair { Ref airfield, sub, jet; bool ok = false; };

// A normal two-army match, an Airfield and a coastal Naval Yard for player 0, and the Ghosts pair trained from it.
Pair makePair(u64 seed) {
    setup(seed);
    Pair p;
    const Faction f = g_sim.players[0].faction;
    const int air = buildFor(f, BR_AIRFIELD), naval = buildFor(f, BR_NAVAL);
    const int subType = ghostSubFor(f);
    if (air < 0 || naval < 0) return p;
    p.airfield = placeNear(0, air, g_sim.players[0].basePos);
    int tx = -1, ty = -1;
    if (!g_sim.get(p.airfield) || !g_sim.shoreSite(0, naval, g_sim.players[0].basePos, tx, ty)) return p;
    if (!g_sim.get(g_sim.placeBuilding(naval, 0, tx, ty, true))) return p;
    if (!g_sim.cmdTrain(p.airfield, subType)) return p;
    const int limit = (int)((UNITS[subType].buildTime * 2.5f + 10.0f) * SIM_HZ);   // (low power can halve the rate)
    for (int i = 0; i < limit; ++i) {
        g_sim.step(); g_sim.events.clear();
        if (g_sim.ghosts[0].sub.valid() && g_sim.ghosts[0].jet.valid()) break;
    }
    p.sub = g_sim.ghosts[0].sub;
    p.jet = g_sim.ghosts[0].jet;
    p.ok = g_sim.get(p.sub) && g_sim.get(p.jet);
    return p;
}

// The nearest open-water tile at least 'minTiles' away, of the same body of water as 'from'.
Vec2 farWater(Vec2 from, float minTiles) {
    const int comp = g_map.compAt(from);
    const float minD = minTiles * TILE;
    float best = 1e30f; Vec2 out;
    bool found = false;
    for (int y = 1; y < MAP_H - 1; ++y) for (int x = 1; x < MAP_W - 1; ++x) {
        if (!g_map.navOpen(x, y)) continue;
        const Vec2 c = tileCenter(x, y);
        if (comp && g_map.compAt(c) != comp) continue;
        const float d = dist(c, from);
        if (d < minD || d >= best) continue;
        best = d; out = c; found = true;
    }
    return found ? out : from;
}

bool isWater(Vec2 p) { return g_map.navOpen(tileOf(p.x), tileOf(p.y)); }

bool distFromBases(Vec2 pos, float tiles) {
    for (int p = 0; p < g_sim.numPlayers; ++p) if (dist(pos, g_sim.players[p].basePos) < tiles * TILE) return false;
    return true;
}
}

bool ghostTest(u64 seed) {
    bool ok = true;
    auto check = [&](bool pass, const char* text) { printf("ghosttest %s: %s\n", pass ? "PASS" : "FAIL", text); ok &= pass; };
    const Faction f = F_CYBER;

    // 1. one pair per army, and only when a finished Airfield and a finished Naval Yard both stand
    {
        setup(seed);
        const int air = buildFor(f, BR_AIRFIELD), naval = buildFor(f, BR_NAVAL), subType = ghostSubFor(f);
        check(air >= 0 && naval >= 0, "the default map offers an Airfield and a Naval Yard build");
        check(g_sim.ghostTrainBlockReason(0) != nullptr && !g_sim.unitAvailable(0, subType),
              "without an Airfield the Ghosts are blocked and unavailable");
        Ref af = placeNear(0, air, g_sim.players[0].basePos);
        check(g_sim.get(af) != nullptr, "a finished Airfield was placed for the scene");
        check(g_sim.ghostTrainBlockReason(0) != nullptr && !g_sim.cmdTrain(af, subType),
              "the Airfield refuses the Ghosts without a finished Naval Yard");
        int tx = -1, ty = -1;
        const bool shore = g_sim.shoreSite(0, naval, g_sim.players[0].basePos, tx, ty);
        Ref yard = shore ? g_sim.placeBuilding(naval, 0, tx, ty, true) : NOREF;
        check(g_sim.get(yard) != nullptr, "a finished Naval Yard was placed on the shore");
        check(g_sim.ghostTrainBlockReason(0) == nullptr && g_sim.unitAvailable(0, subType),
              "with both structures finished the Ghosts are available");
        check(g_sim.cmdTrain(af, subType), "the Airfield accepts the Ghosts pair");
        check(!g_sim.cmdTrain(af, subType) && g_sim.ghostTrainBlockReason(0) != nullptr,
              "a second pair is refused while the first is queued");
        advance((int)((UNITS[subType].buildTime * 2.5f + 10.0f) * SIM_HZ));
        check(g_sim.get(g_sim.ghosts[0].sub) && g_sim.get(g_sim.ghosts[0].jet),
              "the queued pair is trained after its build time");
        check(g_sim.ghostTrainBlockReason(0) != nullptr && !g_sim.cmdTrain(af, subType),
              "a second pair is refused while the first is alive");
    }

    // 2. where and how the pair is born
    {
        Pair p = makePair(seed);
        check(p.ok, "the Ghosts pair exists after training");
        Entity* s = g_sim.get(p.sub);
        Entity* j = g_sim.get(p.jet);
        check(s && j && s->owner == 0 && j->owner == 0, "both halves belong to the training army");
        check(s && j && s->partner == p.jet && j->partner == p.sub, "the halves reference each other both ways");
        check(s && j && s->type == ghostSubFor(F_CYBER) && j->type == ghostJetFor(F_CYBER), "the pair has the right sub and jet types");
        check(s && j && distFromBases(s->pos, 60.0f) && distFromBases(j->pos, 60.0f),
              "the pair spawns at least 60 tiles from every base");
        check(s && isWater(s->pos) && g_map.openSea(tileOf(s->pos.x), tileOf(s->pos.y)), "the sub spawns in open sea");
    }

    // 3. the jet follows the sub; a land order becomes a move onto water
    {
        Pair p = makePair(seed);
        check(p.ok, "the pair exists for the move check");
        Entity* s = g_sim.get(p.sub);
        if (p.ok && s) {
            const Vec2 from = s->pos;
            const Vec2 dest = farWater(from, 15.0f);
            check(isWater(dest) && dist(dest, from) >= 15.0f * TILE, "a water destination 15+ tiles away was found");
            g_sim.cmdMove({p.sub}, dest, false);
            Entity* s2 = g_sim.get(p.sub);
            check(s2 && !s2->path.empty() && isWater(s2->target), "the sub's path leads toward open water");
            bool together = true;
            int ticks = 0;
            for (; ticks < 40 * SIM_HZ; ++ticks) {
                g_sim.step(); g_sim.events.clear();
                Entity* a = g_sim.get(p.sub); Entity* b = g_sim.get(p.jet);
                if (!a || !b) { together = false; break; }
                together &= dist(a->pos, b->pos) < 0.01f;
                if (dist(a->pos, a->target) < 2.0f * TILE) break;
            }
            check(together, "the jet sits on the sub on every tick of the move");
            Entity* a = g_sim.get(p.sub);
            check(a && isWater(a->pos) && dist(a->pos, dest) < 4.0f * TILE, "the pair arrives on the destination water");
            g_sim.cmdMove({p.sub}, g_sim.players[1].basePos, false);
            Entity* b = g_sim.get(p.sub);
            check(b && isWater(b->target), "an order onto land snaps the sub's goal to water");
            advance(2 * SIM_HZ);
            b = g_sim.get(p.sub);
            check(b && isWater(b->pos), "the sub stays on water after a land order");
            Ref building = placeNear(1, buildFor(F_CLANKER, BR_BARRACKS), g_sim.players[1].basePos);
            check(g_sim.get(building) != nullptr, "a land structure was placed for the attack order");
            if (g_sim.get(building)) {
                g_sim.cmdAttack({p.jet}, building);
                Entity* c = g_sim.get(p.sub);
                check(c && isWater(c->target), "an attack on a land structure ordered on the jet becomes a move to water");
                advance(2 * SIM_HZ);
                c = g_sim.get(p.sub);
                check(c && isWater(c->pos), "the pair stays on water after the attack order");
            } else {
                check(false, "an attack on a land structure ordered on the jet becomes a move to water");
            }
        }
    }

    // 4. the submarine stays dived through a move and a strike
    {
        Pair p = makePair(seed);
        check(p.ok, "the pair exists for the dive check");
        Entity* s = g_sim.get(p.sub);
        if (p.ok && s) {
            const Vec2 dest = farWater(s->pos, 15.0f);
            g_sim.cmdMove({p.sub}, dest, false);
            advance(3 * SIM_HZ);
            s = g_sim.get(p.sub);
            check(s && s->subState == 2, "the sub is dived after moving");
            g_sim.time = g_sim.ghosts[0].strikeReadyAt;
            const bool struck = g_sim.cmdGhostStrike(0, g_sim.players[1].basePos);
            advance(1);
            s = g_sim.get(p.sub);
            check(struck && s && s->subState == 2, "the sub is still dived after a strike");
        }
    }

    // 5. snipers, spy drones, AA and bunkers never find the pair; only a Ghost2 can
    {
        Pair p = makePair(seed);
        check(p.ok, "the pair exists for the detection check");
        Entity* s = g_sim.get(p.sub);
        if (p.ok && s) {
            Ref sniper = g_sim.spawnUnit(U_K_SNIPER, 1, s->pos + Vec2(3.0f * TILE, 0));
            Ref drone = g_sim.spawnUnit(U_K_SDRONE, 1, s->pos + Vec2(-3.0f * TILE, 0));
            // the pair sits in open sea, where a structure has no legal ground: fall back to standing the two enemy structures on the water
            // beside it (this check is about detection, not placement)
            const int aaType = buildFor(F_CLANKER, BR_AATURRET), bunkerType = buildFor(F_CLANKER, BR_BUNKER);
            const int sx = tileOf(s->pos.x), sy = tileOf(s->pos.y);
            const int fx = clampi(sx + 4, 1, MAP_W - 4), fy = clampi(sy + 4, 1, MAP_H - 4);
            Ref aa = placeNear(1, aaType, s->pos, 24);
            if (!g_sim.get(aa)) aa = g_sim.placeBuilding(aaType, 1, fx, sy, true);
            Ref bunker = placeNear(1, bunkerType, s->pos, 24);
            if (!g_sim.get(bunker)) bunker = g_sim.placeBuilding(bunkerType, 1, sx, fy, true);
            Ref rpg = g_sim.spawnUnit(U_K_INF2, 1, s->pos + Vec2(0, 4.0f * TILE));
            check(g_sim.get(sniper) && g_sim.get(drone), "an enemy sniper and spy drone stand beside the pair");
            check(g_sim.get(aa) && g_sim.get(bunker), "an enemy AA turret and bunker stand near the pair");
            advance(3 * SIM_HZ);
            Entity* sub = g_sim.get(p.sub);
            Entity* jet = g_sim.get(p.jet);
            Entity* r = g_sim.get(rpg);
            check(sub && jet && !g_sim.visibleTo(*sub, 1) && !g_sim.visibleTo(*jet, 1),
                  "enemy snipers, drones, AA and bunkers do not reveal the pair");
            check(r && sub && jet && !g_sim.canTarget(*r, *sub, -1) && !g_sim.canTarget(*r, *jet, -1),
                  "an enemy soldier cannot target either half without a Ghost2 sighting");
        }
    }

    // 6. a Ghost2 sighting reveals the pair, then it slips away and cannot be re-sighted while cloaked
    {
        Pair p = makePair(seed);
        check(p.ok, "the pair exists for the sighting check");
        Entity* s = g_sim.get(p.sub);
        if (p.ok && s) {
            Ref g2 = g_sim.spawnUnit(ghost2For(F_CLANKER), 1, s->pos + Vec2(3.0f * TILE, 0));
            advance(2);
            check(g_sim.get(g2) != nullptr && g_sim.ghosts[0].sighting.until > g_sim.time,
                  "a Ghost2 within 9 tiles creates a sighting");
            Entity* sub = g_sim.get(p.sub);
            Entity* jet = g_sim.get(p.jet);
            check(sub && jet && g_sim.visibleTo(*sub, 1) && g_sim.visibleTo(*jet, 1),
                  "the sighting reveals both halves");
            Ref rpg = g_sim.spawnUnit(U_K_INF2, 1, s->pos + Vec2(5.0f * TILE, 0));
            Entity* r = g_sim.get(rpg);
            if (r) r->disabledUntil = 1e9f;   // it may aim (canTarget) but must not shoot the pair apart during the 10 s sighting
            check(r && sub && jet && g_sim.canTarget(*r, *sub, -1) && g_sim.canTarget(*r, *jet, -1),
                  "an enemy armed unit may target both halves during the sighting");
            Entity* j = g_sim.get(p.jet);
            if (j) {
                const float hp0 = j->hp;
                g_sim.applyDamagePublic(*j, 100.0f, 1, NOREF);
                const float loss = hp0 - j->hp;
                check(std::fabs(loss - 30.0f) < 8.0f, "the jet takes about 0.3x of an ordinary hit");
            } else {
                check(false, "the jet takes about 0.3x of an ordinary hit");
            }
            Entity* before = g_sim.get(p.sub);
            const Vec2 from = before ? before->pos : Vec2();
            int ticks = 0;
            while (g_sim.ghosts[0].sighting.until > g_sim.time && ticks < 15 * SIM_HZ) { g_sim.step(); g_sim.events.clear(); ++ticks; }
            advance(2);
            Entity* after = g_sim.get(p.sub);
            check(after && dist(after->pos, from) >= 40.0f * TILE, "after the sighting the pair slips 40+ tiles");
            check(after && !g_sim.visibleTo(*after, 1) && g_sim.get(p.jet) && !g_sim.visibleTo(*g_sim.get(p.jet), 1),
                  "the pair is hidden again once the sighting ends");
            const float cloak = g_sim.ghosts[0].cloakUntil - g_sim.time;
            check(cloak >= 9.0f && cloak <= 60.0f, "the cloak then lasts between 10 and 60 s");
            Entity* d = g_sim.get(g2);
            if (d && after) d->pos = after->pos + Vec2(3.0f * TILE, 0);
            advance(2);
            Entity* end = g_sim.get(p.sub);
            check(end && g_sim.ghosts[0].sighting.until <= g_sim.time && !g_sim.visibleTo(*end, 1),
                  "a Ghost2 beside the pair cannot re-sight it during the cloak");
        }
    }

    // 7. the joint strike: two warheads, no earlier than 120 s after the pair appeared, then a 900 s cooldown
    {
        Pair p = makePair(seed);
        check(p.ok, "the pair exists for the strike check");
        Entity* s = g_sim.get(p.sub);
        if (p.ok && s) {
            const Vec2 at = g_sim.players[1].basePos;
            const float ready = g_sim.ghosts[0].strikeReadyAt;
            g_sim.time = ready - 1.0f;
            const size_t n0 = g_sim.nukes.size();
            check(!g_sim.cmdGhostStrike(0, at), "a strike is refused before GHOST_FIRST_STRIKE");
            check(g_sim.nukes.size() == n0, "a refused strike adds no warheads");
            g_sim.time = ready;
            check(g_sim.cmdGhostStrike(0, at), "a strike is accepted once the pair has waited 120 s");
            check(g_sim.nukes.size() == n0 + 2, "a strike adds exactly two warheads");
            if (g_sim.nukes.size() >= n0 + 2) {
                const Sim::Nuke& a = g_sim.nukes[n0];
                const Sim::Nuke& b = g_sim.nukes[n0 + 1];
                check(a.scale == SUB_NUKE_SCALE * 4 && a.dmgMul == 2 && a.sub && a.deep,
                      "the submarine warhead is four sub scales, double damage, sub and deep");
                check(b.scale == NUKEBOMBER_SCALE * 9 && b.dmgMul == 5 && !b.sub && !b.deep,
                      "the jet warhead is nine bomber scales, five times damage");
                check(std::fabs(Sim::nukeDamageFactor(a) - SUB_NUKE_DAMAGE * 2.0f) < 1e-4f
                      && std::fabs(Sim::nukeDamageFactor(b) - 5.0f) < 1e-4f,
                      "the two warheads' damage factors are SUB_NUKE_DAMAGE*2 and 5");
            }
            check(!g_sim.cmdGhostStrike(0, at), "a second strike is refused during the 900 s cooldown");
            g_sim.time = g_sim.ghosts[0].strikeReadyAt;
            check(g_sim.cmdGhostStrike(0, at), "a strike is accepted after GHOST_STRIKE_COOLDOWN");
            Sim::Nuke plain{}, subN{}, deepN{}, superN{};
            subN.sub = true; deepN.deep = true; superN.super = true;
            auto oldFactor = [](const Sim::Nuke& n) {
                return n.super ? ISLAND_NUKE_DAMAGE : (n.sub ? SUB_NUKE_DAMAGE : (n.deep ? UNUKE_DAMAGE : 1.0f));
            };
            check(std::fabs(Sim::nukeDamageFactor(plain) - oldFactor(plain)) < 1e-4f
                  && std::fabs(Sim::nukeDamageFactor(subN) - oldFactor(subN)) < 1e-4f
                  && std::fabs(Sim::nukeDamageFactor(deepN) - oldFactor(deepN)) < 1e-4f
                  && std::fabs(Sim::nukeDamageFactor(superN) - oldFactor(superN)) < 1e-4f,
                  "nukeDamageFactor still matches the old formula for plain, sub, deep and super warheads");
        }
    }

    // 8. the pair lives and dies together, and a new pair can then be trained
    {
        Pair p = makePair(seed);
        check(p.ok, "the pair exists for the death check");
        const int subType = ghostSubFor(F_CYBER);
        Entity* s = g_sim.get(p.sub);
        if (s) g_sim.destroy(*s, true);
        advance(3);
        check(!g_sim.get(p.jet), "destroying the sub takes the jet with it");
        check(!g_sim.ghosts[0].sub.valid() && !g_sim.ghosts[0].jet.valid(), "the lost pair's record is cleared");
        check(!g_sim.ghostsInProduction(0), "no Ghosts are left in production");
        check(g_sim.ghostTrainBlockReason(0) == nullptr && g_sim.cmdTrain(p.airfield, subType),
              "a new pair may be trained after the loss");

        Pair q = makePair(seed);
        check(q.ok, "the pair exists for the jet-first death check");
        Entity* j = g_sim.get(q.jet);
        if (j) g_sim.destroy(*j, true);
        advance(3);
        check(!g_sim.get(q.sub), "destroying the jet takes the sub with it");
        check(!g_sim.ghosts[0].sub.valid() && !g_sim.ghosts[0].jet.valid(), "the jet-first loss also clears the pair");
        check(!g_sim.ghostsInProduction(0), "no Ghosts remain after the jet-first loss");
    }

    // 9. a sighting that ends with no sea 20 tiles away leaves the pair where it is, uncloaked
    {
        Pair p = makePair(seed);
        check(p.ok, "the pair exists for the short-hop check");
        Entity* s = g_sim.get(p.sub);
        if (p.ok && s) {
            std::vector<u8> tiles0(g_map.tiles, g_map.tiles + MAP_W * MAP_H);
            std::vector<u8> blocked0(g_map.blocked, g_map.blocked + MAP_W * MAP_H);
            const Vec2 at = s->pos;
            for (int y = 1; y < MAP_H - 1; ++y) for (int x = 1; x < MAP_W - 1; ++x) {
                if (!g_map.openSea(x, y)) continue;
                if (dist(tileCenter(x, y), at) < 20.0f * TILE) continue;
                g_map.tiles[y * MAP_W + x] = T_ROCK; g_map.blocked[y * MAP_W + x] |= 1;
            }
            g_map.touch();
            g_sim.ghosts[0].sighting = {1, g_sim.time, at};
            advance(2);
            Entity* after = g_sim.get(p.sub);
            check(after && dist(after->pos, at) < 1.0f, "with no sea 20 tiles away the pair stays put");
            check(g_sim.ghosts[0].cloakUntil <= g_sim.time, "and is not cloaked");
            std::memcpy(g_map.tiles, tiles0.data(), sizeof(u8) * MAP_W * MAP_H);
            std::memcpy(g_map.blocked, blocked0.data(), sizeof(u8) * MAP_W * MAP_H);
            g_map.touch();
        }
    }

    // 10. a Ghost2 hunts on its own when its hold ends, but a player's order wins until it arrives and the hold ends
    {
        setup(seed);
        const Vec2 water = g_map.nearestWater(tileCenter(MAP_W / 2, MAP_H / 2), 200, true);
        const Ref g2 = g_sim.spawnUnit(ghost2For(F_CLANKER), 1, water);
        Entity* d = g_sim.get(g2);
        check(d != nullptr, "a Ghost2 was spawned for the hunt check");
        if (d) {
            g_sim.time = d->autoHoldUntil + 0.1f;
            advance(2);
            d = g_sim.get(g2);
            check(d && d->order != O_IDLE && isWater(d->target), "an idle Ghost2 past its hold picks a patrol on its own");
            const Vec2 dest = d ? g_map.nearestWater(d->pos, 8) : water;   // (the drone's own water: it arrives within the hold)
            g_sim.cmdMove({g2}, dest, false);
            d = g_sim.get(g2);
            check(d && d->order == O_MOVE && dist(d->target, dest) < 2.0f * TILE, "a player's move order is taken");
            advance(SIM_HZ);
            d = g_sim.get(g2);
            check(d && g_sim.time < d->autoHoldUntil && (d->order == O_IDLE || dist(d->target, dest) < 3.0f * TILE),
                  "the player's order is not overridden while its hold lasts");
            if (d) g_sim.time = d->autoHoldUntil + 0.1f;
            advance(2);
            d = g_sim.get(g2);
            check(d && d->order != O_IDLE && dist(d->target, dest) > 5.0f * TILE,
                  "once the hold ends the Ghost2 hunts again on its own");
        }
    }

    return ok;
}
