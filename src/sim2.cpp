// One Hour - simulation of the newer subsystems: jeeps (seats that shoot), protector drones, ships, anti-nuke defences, the Air Strike fighters and the aircraft carriers.
// The rest of the simulation is in sim.cpp; the two files share the Sim class.
#include "sim.h"

static inline float wrapAng(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
static inline float angDelta(float a, float b) { return wrapAng(b - a); }   // b - a wrapped to (-pi, pi]

// ------------------------------------------------------------ jeeps
// An airdrop brings DROP_JEEPS_MIN jeeps from the start and one more for every bit of the arms race the army has finished: the Advanced Program
// (+1) and each structure upgrade (+2 each), which tops out at DROP_JEEPS_MAX.
int Sim::dropJeeps(int player) const {
    if (player < 0 || player >= numPlayers) return 0;
    const Player& pl = players[player];
    int n = DROP_JEEPS_MIN + (pl.advTech ? 1 : 0);
    for (int u = 0; u < UPG_STRUCTURE_COUNT; u++) if (pl.upg[u]) n += 2;
    return clampi(n, DROP_JEEPS_MIN, DROP_JEEPS_MAX);
}

// The seven soldiers a dropped jeep carries: one of each kind that is worth a seat in a vehicle (a rifleman for bullets, a sniper for the long shot, a
// grenade or arc thrower for crowds) and rocket troopers in the rest, which is what a jeep needs against tanks and aircraft. An army that cannot
// field one of the three specialists yet (the tech structure or the Advanced Program is missing) fills the seat with a rocket trooper instead.
std::vector<int> Sim::jeepCrew(int player) const {
    std::vector<int> crew;
    if (player < 0 || player >= numPlayers) return crew;
    bool cy = players[player].faction == F_CYBER;
    int rifle = cy ? U_C_INF1 : U_K_INF1;
    int rocket = cy ? U_C_INF2 : U_K_INF2;       // Laser Trooper (anti-armour and anti-air) / RPG Trooper
    int sniper = cy ? U_C_SNIPER : U_K_SNIPER;
    int thrower = cy ? U_C_INF3 : U_K_ELITE;     // Shock Trooper's arc caster / Grenadier's grenade launcher
    int throwerB = cy ? U_C_INF3 : U_K_INF3;     // (the Grenadier needs the Advanced Program: the Gunner is the next best)
    auto pick = [&](int want, int fallback) { return unitAvailable(player, want) ? want : fallback; };
    crew.push_back(pick(rifle, rocket));
    crew.push_back(pick(sniper, rocket));
    crew.push_back(pick(thrower, pick(throwerB, rocket)));
    while ((int)crew.size() < JEEP_SEATS) crew.push_back(pick(rocket, rifle));
    return crew;
}

// The crew of a jeep: it mends nothing but it shoots. Every soldier aboard fires its own weapon at the best target it can hit, from wherever the
// jeep is (moving too); nothing aboard can be hit, and when the jeep goes up the crew is thrown out (see ejectAll).
bool Sim::passengersFire(Entity& host, float reachTiles) {
    static std::vector<Ref> cand; cand.clear();
    forEachNear(host.pos, reachTiles * TILE, [&](Entity& t) {
        if (t.kind == EK_RESOURCE || !enemies(host.owner, t.owner) || t.carrier.valid() || t.fall > 0 || !visibleTo(t, host.owner)) return;
        cand.push_back(refOf(t));
    });
    bool fired = false;
    for (Ref r : host.passengers) {
        Entity* u = get(r);
        if (!u) continue;
        if (u->cooldown > 0) u->cooldown -= 2 * SIM_DT;
        int wi = u->weapon();
        if (wi < 0 || u->cooldown > 0 || cand.empty()) continue;
        const Weapon& w = WEAPONS[wi];
        Entity* best = nullptr; float bs = 1e9f;
        for (Ref cr : cand) {
            Entity* t = get(cr);
            if (!t || !canTarget(*u, *t)) continue;
            float d = distToEntity(u->pos, *t) / TILE;
            if (d > w.range + 0.15f || d < w.minRange) continue;
            float score = d - 2.5f * w.mult[t->armor()];
            if (t->isUnit() && t->weapon() >= 0) score -= 1.5f;
            if (t->isBuilding() && !t->constructed) score += 1.0f;
            if (score < bs) { bs = score; best = t; }
        }
        if (best) { tryFire(*u, *best); if (u->cooldown > 0) fired = true; }
    }
    return fired;
}

void Sim::updateJeep(Entity& j) {
    Ref jr = refOf(j);
    for (size_t i = 0; i < j.passengers.size();) {
        Entity* u = get(j.passengers[i]);
        if (!u || u->carrier != jr) { j.passengers.erase(j.passengers.begin() + i); continue; }
        u->pos = j.pos; u->prevPos = j.prevPos;   // the crew rides with the jeep: its muzzle flashes come from where the jeep is
        i++;
    }
    if (j.unloading) updateCarrierUnload(j);
    if (j.passengers.empty()) { j.unloading = false; return; }
    if (!j.unloading && !j.ut().naval && (tick + j.gen) % 2 == 0) passengersFire(j, 12.5f);   // (a cargo ship's load rides below decks)
}

// ------------------------------------------------------------ protector drones
int Sim::droneCount(const Entity& e) const {
    if (!e.alive || !e.isUnit() || e.fall > 0 || e.carrier.valid() || e.disabledUntil > time) return 0;
    const UnitType& ut = e.ut();
    int n = ut.drones;
    if (hasUpgrade(e.owner, UPG_DRONES)) n += ut.naval ? SHIP_DRONES : (ut.cargoCap > 0 ? 1 : 0);
    return std::min(n, 3);
}

Vec2 Sim::dronePosAt(const Entity& e, int i, float t) const {
    int n = std::max(1, droneCount(e));
    float R = e.radius() + 15.0f;
    float a = t * 1.7f + 6.2831853f * (float)i / (float)n + (float)(e.gen % 7) * 0.9f;
    return e.pos + Vec2(std::cos(a), std::sin(a)) * R;
}

// A rocket or shell homing in on a unit with a drone screen: the first drone that has reloaded gets a shot at it as it comes within reach. Lasers and
// bullets are too quick to be engaged. Returns true if the projectile was shot down.
bool Sim::interceptProjectile(Projectile& p) {
    const Weapon& w = WEAPONS[p.weapon];
    if (w.proj != PJ_ROCKET && w.proj != PJ_SHELL && w.proj != PJ_BOMB) return false;   // (bombs are loosed, not fired, but a point defence burns them too)
    Entity* h = get(p.target);
    if (w.proj == PJ_ROCKET && h && h->isBuilding() && h->constructed && h->bt().role == BR_BUNKER && h->hatch > 0 && h->disabledUntil <= time) {   // the hatch guns and overdrive lasers of a bunker swat rockets
        float range = (h->surrounded ? BUNKER_CIWS_RANGE + 1.5f : BUNKER_CIWS_RANGE) * TILE + h->radius();
        if (time < h->ciwsAt || dist(p.pos, h->pos) > range) return false;
        h->ciwsAt = time + (h->surrounded ? BUNKER_CIWS_GAP * 0.65f : BUNKER_CIWS_GAP);
        int gi = rng.range(0, BUNKER_GUNS - 1);
        Vec2 g = h->surrounded ? bunkerOverdrivePos(*h, gi) : bunkerGunPos(*h, gi);
        h->gunAng[gi] = std::atan2(p.pos.y - g.y, p.pos.x - g.x);
        Color col = h->surrounded && h->bt().faction == F_CYBER ? WEAPONS[W_BUNKER_LASER].color : WEAPONS[W_BUNKER_MINIGUN].color;
        fx.push_back({FX_BEAM, g, p.pos, 0, 0.12f, col, h->surrounded ? 2.2f : 1.5f});
        float chance = h->surrounded ? 0.78f : BUNKER_CIWS_CHANCE;
        if (rng.f() >= chance) return false;
        fx.push_back({FX_EXPLODE, p.pos, p.pos, 0, 0.4f, rgb(255, 224, 150), 18.0f});
        for (int k = 0; k < 3; k++) fx.push_back({FX_SPARK, p.pos, p.pos, 0, rng.f(0.2f, 0.4f), col, 4});
        emit(EV_SOUND, h->owner, SND_EXPLODE_S, p.pos);
        return true;
    }
    // Q defences and Anti-Rocket walls burn incoming rockets, shells and bombs out of the air. They protect their own side: only an enemy
    // projectile that is not aimed at the defence's enemies is engaged, whichever one is closest and ready takes the shot.
    if (w.proj == PJ_ROCKET || w.proj == PJ_SHELL || w.proj == PJ_BOMB) {
        Entity* def = nullptr; float bd = 1e18f;
        forEachNear(p.pos, (WALL_PD_COVER + 3.0f) * TILE, [&](Entity& b) {
            if (!b.isBuilding() || !antiNukeWorking(b) || time < b.pdAt || !enemies(b.owner, p.owner)) return;
            Entity* t = get(p.target);
            if (t && enemies(b.owner, t->owner)) return;   // it is aimed at somebody on the defence's side only if that somebody is not its enemy
            float range = isAntiRocketWall(b) ? WALL_PD_COVER * TILE : (b.bt().faction == F_CYBER ? ANTINUKE_COVER_C : ANTINUKE_COVER_K) * TILE;
            float d = dist(p.pos, b.pos);
            if (d <= range && d < bd) { bd = d; def = &b; }
        });
        if (def) {
            bool wall = isAntiRocketWall(*def);
            bool cy = wall || def->bt().faction == F_CYBER;
            def->pdAt = time + (wall ? WALL_PD_RELOAD : 1.0f);
            Vec2 muzzle = def->pos + Vec2(0, wall ? -10.0f : (cy ? -22.0f : -8.0f));
            Color col = cy ? rgb(120, 236, 255) : rgb(255, 214, 120);
            fx.push_back({FX_BEAM, muzzle, p.pos, 0, 0.12f, col, wall ? 1.6f : 2.4f});
            fx.push_back({FX_FLASH, muzzle, muzzle, 0, 0.10f, col, wall ? 6.0f : 9.0f});
            if (rng.f() < antiNukeShot(*def)) {
                fx.push_back({FX_EXPLODE, p.pos, p.pos, 0, 0.4f, rgb(255, 224, 150), 20.0f});
                fx.push_back({FX_RING, p.pos, p.pos, 0, 0.35f, col, 26.0f});
                for (int k = 0; k < 4; k++) fx.push_back({FX_SPARK, p.pos, p.pos, 0, rng.f(0.2f, 0.4f), col, 4});
                emit(EV_SOUND, def->owner, SND_EXPLODE_S, p.pos);
                return true;
            }
            return false;   // a miss: the next ready defence gets the next tick
        }
    }
    if (!h || !h->isUnit()) return false;
    int n = droneCount(*h);
    if (n <= 0 || dist(p.pos, h->pos) > DRONE_RANGE + h->radius()) return false;
    for (int i = 0; i < n; i++) {
        if (h->droneAt[i] > time) continue;
        if (h->droneLoad[i] == 0) h->droneLoad[i] = (u8)DRONE_SHOTS;   // a drone whose reload has run out has a full magazine again
        h->droneLoad[i]--;
        h->droneAt[i] = time + (h->droneLoad[i] > 0 ? DRONE_GAP : DRONE_RELOAD);
        Vec2 d = dronePos(*h, i);
        Color col = players[h->owner].faction == F_CYBER ? rgb(120, 236, 255) : rgb(255, 214, 120);
        fx.push_back({FX_BEAM, d, p.pos, 0, 0.16f, col, 2});
        fx.push_back({FX_FLASH, d, d, 0, 0.12f, col, 6});
        if (rng.f() < DRONE_CHANCE) {
            fx.push_back({FX_EXPLODE, p.pos, p.pos, 0, 0.4f, rgb(255, 224, 150), 20.0f});
            fx.push_back({FX_RING, p.pos, p.pos, 0, 0.35f, col, 26.0f});
            for (int k = 0; k < 4; k++) fx.push_back({FX_SPARK, p.pos, p.pos, 0, rng.f(0.2f, 0.4f), col, 4});
            emit(EV_SOUND, h->owner, SND_EXPLODE_S, p.pos);
            return true;
        }
        return false;   // it slipped past this one: the next drone to be ready gets the next tick
    }
    return false;
}

// ------------------------------------------------------------ ships
// A ship has weight: it turns at a limited rate, builds up speed along its heading and eases off before a stop or a sharp turn, and it sails
// where it points (so the hull always lines up with its wake). It never leaves open water: a shore ahead stops it and it finds another way.
void Sim::moveShip(Entity& e, float speed) {
    const float turnRate = 1.05f;
    for (int guard = 0; guard < 3 && e.pathIdx < e.path.size(); guard++) {
        bool last = e.pathIdx + 1 >= e.path.size();
        if (dist(e.path[e.pathIdx], e.pos) > (last ? 10.0f : 28.0f)) break;
        e.pathIdx++;
    }
    float v = e.airspeed;
    bool steering = e.pathIdx < e.path.size();
    if (steering) {
        Vec2 d = e.path[e.pathIdx] - e.pos; float len = d.len();
        bool last = e.pathIdx + 1 >= e.path.size();
        float want = std::atan2(d.y, d.x), turn = turnRate * SIM_DT;
        e.angle = wrapAng(e.angle + clampf(angDelta(e.angle, want), -turn, turn));
        float vt = speed * clampf(1.15f - std::abs(angDelta(e.angle, want)) / 1.2f, 0.12f, 1.0f);
        if (last) vt = std::min(vt, std::max(speed * 0.12f, len * 1.1f));
        v = v < vt ? std::min(vt, v + speed * 0.7f * SIM_DT) : std::max(vt, v - speed * 1.2f * SIM_DT);
    } else v = std::max(0.0f, v - speed * 1.2f * SIM_DT);   // nothing to steer for: glide to a halt
    e.airspeed = v;
    if (v > 0.5f) {
        const Vec2 step = Vec2(std::cos(e.angle), std::sin(e.angle)) * (v * SIM_DT);
        const Vec2 np = e.pos + step, sx(np.x, e.pos.y), sy(e.pos.x, np.y);
        auto wet = [](Vec2 q) { return g_map.navTile(tileOf(q.x), tileOf(q.y)); };
        if (wet(np)) e.pos = np;
        else if (wet(sx) || wet(sy)) {   // a hull brushing the shore slides along it while it turns away (it never stops dead on a tile edge)
            e.pos = !wet(sy) ? sx : !wet(sx) ? sy : (std::abs(step.x) > std::abs(step.y) ? sx : sy);
            e.airspeed = v * 0.85f;
        } else {
            e.airspeed = 0;
            Vec2 ow = g_map.nearestWater(e.pos, 4);   // wedged into a corner: edge out toward the open water beside it
            float od = dist(ow, e.pos);
            if (od > 1.0f) { Vec2 q = e.pos + (ow - e.pos) * (std::min(od, 40.0f * SIM_DT) / od); if (wet(q)) e.pos = q; }
            if (steering && !e.path.empty()) requestPath(e, e.path.back());
        }
    }
}

Vec2 Sim::navalExit(const Entity& yard) const { return g_map.nearestWater(yard.pos, 12, true); }   // (always the lake or the ocean: a ship put into a cut-off river could never get out)

// The legal spot nearest to 'near' for a structure that must touch the lake (a Naval Yard): scans the whole shore the player has explored.
bool Sim::shoreSite(int player, int buildType, Vec2 near, int& tx, int& ty) const {
    const BuildType& b = BUILDS[buildType];
    // Rings outward from 'near' (the sea map has hundreds of tiles of coast, and every full placement test is costly): the nearest legal site of the
    // first ring that has one, compared with the next ring's (a footprint's centre one ring further out can still be a little closer)
    const int cx = tileOf(near.x) - b.w / 2, cy = tileOf(near.y) - b.h / 2;
    float best = 1e18f; bool found = false; int lastRing = MAP_W;
    for (int r = 0; r <= MAP_W && r <= lastRing; r++) {
        for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
            if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
            int x = cx + dx, y = cy + dy;
            if (x < 1 || y < 1 || x >= MAP_W - b.w || y >= MAP_H - b.h) continue;
            float d = dist(buildingCenter(buildType, x, y), near);
            if (d >= best) continue;
            bool wet = false;   // (cheap tests first)
            for (int i = x - 1; i <= x + b.w && !wet; i++) wet = g_map.bigWater(i, y - 1) || g_map.bigWater(i, y + b.h);
            for (int j = y; j < y + b.h && !wet; j++) wet = g_map.bigWater(x - 1, j) || g_map.bigWater(x + b.w, j);
            if (!wet) continue;
            bool roadFree = true;   // never plug a road (the ring road runs right along the beach)
            for (int j = y - 1; j <= y + b.h && roadFree; j++) for (int i = x - 1; i <= x + b.w; i++) if (inMap(i, j) && (g_map.tile(i, j) == T_ROAD || g_map.bridge[j * MAP_W + i])) { roadFree = false; break; }
            if (!roadFree) continue;
            bool gapOk = true;   // keep the one-tile lane around the structure open, as the computer commander does for everything it builds
            for (int j = y - 1; j <= y + b.h && gapOk; j++) for (int i = x - 1; i <= x + b.w; i++) if (inMap(i, j) && (g_map.blocked[j * MAP_W + i] & 2)) { gapOk = false; break; }
            if (!gapOk || !canPlace(player, buildType, x, y)) continue;
            best = d; tx = x; ty = y;
            if (!found) lastRing = r + 1;
            found = true;
        }
    }
    return found;
}

// ------------------------------------------------------------ anti-nuke defences
// A defence that is switched on, standing and ready covers a circle around itself. Every defence covering the target of a warhead that would hurt
// its owner gets one chance at it (antiNukeShot), so n of them stop it with probability 1 - prod(1 - chance_i).
bool Sim::isAntiRocketWall(const Entity& b) const {
    return b.alive && b.isBuilding() && b.constructed && b.bt().role == BR_WALL && b.owner >= 0 &&
           hasUpgrade(b.owner, UPG_ANTIROCKET) && b.disabledUntil <= time;
}
bool Sim::antiNukeWorking(const Entity& b) const {
    if (!b.alive || !b.isBuilding() || !b.constructed || b.disabledUntil > time) return false;
    if (isAntiRocketWall(b)) return true;   // walls fly a laser and need no power
    if (b.bt().role != BR_ANTINUKE) return false;
    return !(players[b.owner].lowPower() && b.bt().power < 0);   // a defence without power is blind
}
float Sim::antiNukeCover(const Entity& b, float scale) const {
    Nuke probe; probe.scale = std::min(scale, UNUKE_SCALE);   // (a Super Nuke's blast spans most of the map: a defence still only answers for the ground an Underground Nuke would hit round it)
    if (isAntiRocketWall(b)) return WALL_PD_COVER * TILE + nukeRadius(probe);
    if (b.bt().faction != F_CYBER) return ANTINUKE_COVER_K * TILE;
    return ANTINUKE_COVER_C * TILE + nukeRadius(probe);   // a laser has no blind ring at the edge: a blast that would reach into its circle is engaged
}
int Sim::antiNukeLinked(const Entity& b) const {
    if (isAntiRocketWall(b)) {
        int n = 0;
        for (auto& o : ents) {
            if (&o == &b || o.owner != b.owner || !isAntiRocketWall(o) || dist(o.pos, b.pos) > ANTINUKE_LINK * TILE) continue;
            if (++n >= ANTINUKE_LINK_MAX) break;
        }
        return n;
    }
    if (b.bt().faction != F_CYBER) return 0;
    int n = 0;
    for (auto& o : ents) {
        if (&o == &b || o.owner != b.owner || !antiNukeWorking(o) || o.bt().faction != F_CYBER || dist(o.pos, b.pos) > ANTINUKE_LINK * TILE) continue;
        if (++n >= ANTINUKE_LINK_MAX) break;
    }
    return n;
}
float Sim::antiNukeShot(const Entity& b) const {
    if (isAntiRocketWall(b)) return WALL_PD_CHANCE + ANTINUKE_LINK_BONUS * antiNukeLinked(b);
    return ANTINUKE_CHANCE + ANTINUKE_LINK_BONUS * antiNukeLinked(b);
}
// What a defence's warhead interception is actually worth after the sighting nerf: the Anti-Nuke Laser is 40% weaker against nuclear warheads
// and an Anti-Rocket wall's laser is 70% weaker; the Clanker missile silo is untouched and so is point-defence against rockets, shells and
// aircraft (those still roll antiNukeShot, see interceptProjectile and updatePointDefenseAir).
float Sim::antiNukeWarheadShot(const Entity& b) const {
    float c = antiNukeShot(b);
    if (isAntiRocketWall(b)) return c * WALL_PD_NUKE_MUL;
    if (b.bt().faction == F_CYBER) return c * ANTINUKE_LASER_NUKE_MUL;
    return c;
}
int Sim::antiNukeCovering(Vec2 at, int attacker, bool knownOnly, float scale, bool deep) const {
    int n = 0;
    Nuke probe; probe.owner = attacker;
    for (auto& b : ents) {
        if (!antiNukeWorking(b) || !nukeHurts(probe, b.owner) || (deep && !(b.bt().faction == F_CYBER || isAntiRocketWall(b)))) continue;   // (only a laser stops an Underground Nuke)
        if (knownOnly && attacker >= 0 && !explored(attacker, clampi(tileOf(b.pos.x), 0, MAP_W - 1), clampi(tileOf(b.pos.y), 0, MAP_H - 1))) continue;
        if (dist(at, b.pos) <= antiNukeCover(b, scale)) n++;
    }
    return n;
}
float Sim::antiNukeChance(Vec2 at, int attacker, bool knownOnly, float scale, bool deep, bool super) const {
    deep = deep || super;   // (only lasers try for either)
    float miss = 1.0f;
    Nuke probe; probe.owner = attacker;
    for (auto& b : ents) {
        if (!antiNukeWorking(b) || !nukeHurts(probe, b.owner) || (deep && !(b.bt().faction == F_CYBER || isAntiRocketWall(b)))) continue;
        if (knownOnly && attacker >= 0 && !explored(attacker, clampi(tileOf(b.pos.x), 0, MAP_W - 1), clampi(tileOf(b.pos.y), 0, MAP_H - 1))) continue;
        if (dist(at, b.pos) <= antiNukeCover(b, scale)) {
            float one = antiNukeWarheadShot(b);
            if (deep) one *= super ? ISLAND_NUKE_INTERCEPT_RESIST : UNUKE_INTERCEPT_RESIST;   // the armoured silver casing: half the chance to stop it (a Super Nuke: a tenth)
            miss *= 1.0f - one;
        }
    }
    return 1.0f - miss;
}

// Q defences and Anti-Rocket walls also look for enemy aircraft in reach. They fire a short, bright laser burst (or an interceptor for the
// Clanker missile silo) and the aircraft takes the hit immediately; a target that survives simply gets engaged again after the reload.
void Sim::updatePointDefenseAir(Entity& b) {
    if (time < b.pdAt || b.disabledUntil > time) return;
    bool wall = isAntiRocketWall(b);
    if (!wall && b.bt().role != BR_ANTINUKE) return;
    if (!wall && players[b.owner].lowPower() && b.bt().power < 0) return;
    float range = wall ? WALL_PD_COVER * TILE : (b.bt().faction == F_CYBER ? ANTINUKE_COVER_C : ANTINUKE_COVER_K) * TILE;
    Entity* best = nullptr; float bd = range;
    forEachNear(b.pos, range, [&](Entity& e) {
        if (!e.isUnit() || !e.isAir() || !enemies(b.owner, e.owner) || !visibleTo(e, b.owner)) return;
        float d = dist(e.pos, b.pos);
        if (d < bd) { bd = d; best = &e; }
    });
    if (!best) return;
    b.pdAt = time + PD_AIRCRAFT_RELOAD;
    bool cy = wall || b.bt().faction == F_CYBER;
    Vec2 muzzle = b.pos + Vec2(0, wall ? -10.0f : (cy ? -22.0f : -8.0f));
    Color col = cy ? rgb(120, 236, 255) : rgb(255, 214, 120);
    fx.push_back({FX_BEAM, muzzle, best->pos, 0, 0.14f, col, wall ? 1.8f : 2.6f});
    fx.push_back({FX_FLASH, muzzle, muzzle, 0, 0.10f, col, wall ? 7.0f : 10.0f});
    fx.push_back({FX_EXPLODE, best->pos, best->pos, 0, 0.30f, rgb(255, 224, 150), 16.0f});
    applyDamage(*best, wall ? WALL_PD_AA : ANTINUKE_PD_AA, b.owner, refOf(b), nullptr);
    emit(EV_SOUND, b.owner, cy ? SND_LASER_HEAVY : SND_ROCKET, b.pos);
}

// A defence watches the warheads in flight. One heading for a spot it covers opens fire some seconds from impact (a laser tower earlier, with three
// beams and more when linked to its neighbours; a missile silo two interceptors): the whole salvo is rolled now - this defence either has the warhead
// in it or misses with every shot - and the shots are laid out in time so they can be drawn; the warhead is shot down when the winning shot arrives.
void Sim::updateAntiNuke(Entity& b) {
    if (time < b.siloTimer || !antiNukeWorking(b)) return;
    bool cy = isAntiRocketWall(b) || b.bt().faction == F_CYBER;
    const int self = (int)(&b - &ents[0]);
    for (auto& n : nukes) {
        if (n.broken || n.killT >= 0 || !nukeHurts(n, b.owner) || dist(n.pos, b.pos) > antiNukeCover(b, n.scale)) continue;
        if ((n.deep || n.super) && !cy) continue;     // an Underground Nuke's or a Super Nuke's warhead outruns interceptor missiles: only the lasers can burn it down
        if (time < b.siloTimer) break;   // (a laser that just fired at one warhead is still recharging)
        float left = flightOf(n) - n.t;
        int linked = cy ? antiNukeLinked(b) : 0;
        const float gap = cy ? 0.8f / (1.0f + ANTINUKE_LINK_TRACK * linked) : 1.5f, flight = cy ? 0.3f : 1.0f;
        if (left > (cy ? ANTINUKE_LASER_WINDOW : ANTINUKE_WINDOW) || left < flight + 0.2f) continue;
        if (std::find(n.engagedBy.begin(), n.engagedBy.end(), self) != n.engagedBy.end()) continue;
        n.engagedBy.push_back(self);
        int shots = cy ? 3 + linked : 2;
        while (shots > 1 && (shots - 1) * gap + flight > left - 0.2f) shots--;
        float resist = n.super ? ISLAND_NUKE_INTERCEPT_RESIST : (n.deep ? UNUKE_INTERCEPT_RESIST : 1.0f);   // (the silver casing is armoured: harder to shoot down; a Super Nuke five times harder still)
        int winner = rng.f() < antiNukeWarheadShot(b) * resist ? rng.range(0, shots - 1) : -1;
        b.siloTimer = time + (cy ? (shots - 1) * gap + flight + ANTINUKE_LASER_RECHARGE : ANTINUKE_RELOAD);
        Vec2 muzzle = b.pos + Vec2(0, cy ? -22.0f : -8.0f);
        for (int k = 0; k < shots; k++) {
            Interceptor it;
            it.from = muzzle; it.owner = b.owner; it.nukeId = n.id; it.laser = cy; it.hit = (k == winner);
            it.launchT = time + k * gap; it.dur = flight;
            float tMeet = n.t + k * gap + flight;
            it.aim = nukeAirPos(n, tMeet);
            if (!it.hit) { Vec2 off(rng.f(-1, 1), rng.f(-1, 1)); it.aim += off.norm() * rng.f(44.0f, 110.0f); }   // a miss passes close by
            interceptors.push_back(it);
            if (it.hit) n.killT = tMeet;
        }
        Vec2 air = nukeAirPos(n, n.t + flight);
        b.turret2 = std::atan2(air.y - b.pos.y, air.x - b.pos.x);
        b.cooldown2 = (shots - 1) * gap + flight + 0.4f;   // the firing animation of the head
        if (n.engagedBy.size() == 1) {   // the first defence to open fire tells both sides
            emit(EV_MSG, b.owner, SND_NONE, n.pos, "Anti-nuke defences are engaging the warhead");
            for (int p = 0; p < numPlayers; p++) if (p != b.owner && n.owner == p) emit(EV_MSG, p, SND_ATTACKED, n.pos, "Enemy anti-nuke defences are engaging our warhead");
        }
        emit(EV_SOUND, -1, cy ? SND_LASER_HEAVY : SND_ROCKET, b.pos);
    }
}

// The warhead is hit: an airburst high over the ground, and from here on only its pieces come down on the target (see nukeBlast: power).
void Sim::breakNuke(Nuke& n) {
    n.broken = true;
    n.power = ANTINUKE_FALLOFF;
    Vec2 at = nukeAirPos(n, n.killT >= 0 ? n.killT : n.t);
    fx.push_back({FX_EXPLODE, at, at, 0, 1.0f, rgb(255, 244, 214), 110.0f});
    fx.push_back({FX_RING, at, at, 0, 1.1f, rgb(255, 214, 150), 190.0f});
    fx.push_back({FX_RING, at, at, -0.15f, 1.3f, rgb(255, 255, 255), 120.0f});
    fx.push_back({FX_EMP, at, at, 0, 0.9f, rgb(255, 230, 180), 150.0f});
    for (int k = 0; k < 14; k++) fx.push_back({FX_EXPLODE, at + Vec2(rng.f(-50, 50), rng.f(-40, 40)), at, -rng.f(0, 0.5f), rng.f(0.5f, 0.9f), rgb(255, 170, 80), rng.f(18, 44)});
    for (int k = 0; k < 18; k++) fx.push_back({FX_DEBRIS, at, at, 0, rng.f(0.8f, 1.6f), rgb(92, 90, 90), rng.f(2, 5), Vec2(rng.f(-260, 260), rng.f(-180, 80))});
    emit(EV_SOUND, -1, SND_EXPLODE_L, at);
    for (int p = 0; p < numPlayers; p++) {
        if (p == n.owner) emit(EV_MSG, p, SND_ATTACKED, n.pos, "Our warhead was shot down: only its pieces will fall");
        else if (enemies(n.owner, p) || (n.owner >= 0 && !players[n.owner].isAI)) emit(EV_MSG, p, SND_NONE, n.pos, "Warhead shot down: its pieces and radiation will still fall");
    }
}

void Sim::updateInterceptors() {
    for (auto& it : interceptors) {
        if (it.done || time < it.launchT + it.dur) continue;
        it.done = true;
        Color col = it.laser ? rgb(130, 240, 255) : rgb(255, 214, 140);
        if (it.hit) continue;   // the hit is handled by the warhead's own bookkeeping (updateNukes)
        fx.push_back({FX_FLASH, it.aim, it.aim, 0, 0.15f, col, 10});
    }
    interceptors.erase(std::remove_if(interceptors.begin(), interceptors.end(), [&](const Interceptor& it) { return time > it.launchT + it.dur + 0.6f; }), interceptors.end());
}

// ------------------------------------------------------------ the Air Strike fighters
// A wave comes in as a V: the leader at the apex flies the middle lane over the called spot, the wingmen behind it to either side fly lanes spread
// across the target circle, so the wave's sticks of bombs carpet a strip. Every wave is put on the map at the edge it comes in over (it was
// flying out at sea before that).
void Sim::launchStrikeWave(const StrikeWave& w) {
    if (w.owner < 0 || w.owner >= numPlayers || !players[w.owner].alive) return;
    Vec2 dir = w.dir.norm(), perp(-dir.y, dir.x);
    const float m = 3.0f * TILE, RANK = 58.0f, WING = 46.0f;
    // back along the heading from the spot to the edge of the map, then in again by the depth of the V so every wingman starts over the map
    float t = 1e9f;
    if (dir.x > 1e-4f) t = std::min(t, (w.target.x - m) / dir.x); else if (dir.x < -1e-4f) t = std::min(t, (w.target.x - (WORLD_W - m)) / dir.x);
    if (dir.y > 1e-4f) t = std::min(t, (w.target.y - m) / dir.y); else if (dir.y < -1e-4f) t = std::min(t, (w.target.y - (WORLD_H - m)) / dir.y);
    int ranks = (w.n) / 2;
    Vec2 apex = w.target - dir * std::max(700.0f, t - ranks * RANK);
    const float lane = STRIKE_SPREAD * TILE / std::max(1, (STRIKE_WAVE) / 2);
    for (int i = 0; i < w.n; i++) {
        int rank = (i + 1) / 2, side = (i & 1) ? 1 : -1;
        Vec2 from = apex - dir * (rank * RANK) + perp * (side * rank * WING);
        from.x = clampf(from.x, m, WORLD_W - m); from.y = clampf(from.y, m, WORLD_H - m);
        Ref r = spawnUnit(w.type, w.owner, from);
        Entity& e = ents[r.idx];
        const UnitType& ut = e.ut();
        e.pos = e.prevPos = e.lastPos = from;
        e.alt = 1; e.airspeed = ut.speed; e.angle = e.turret = std::atan2(dir.y, dir.x);
        e.order = O_STRIKERUN; e.target = w.target; e.guardPos = w.target + perp * (side * rank * lane); e.runDir = dir;
        e.strikePass = 0; e.salvoLeft = STRIKE_ROCKETS; e.ammo = ut.ammo; e.bombsLeft = STRIKE_BOMBS; e.bombTimer = 0; e.actionTimer = 0.1f * i;
        e.jetMode = side > 0 ? 1 : 0; e.jetBreak = 0; e.roll = 0; e.fade = 1; e.lifeUntil = time + STRIKE_LIFE;
    }
    emit(EV_SOUND, w.owner, SND_JET, apex);
}

// A rocket of the run-in: most go for something that stands in the circle, the rest rake the ground between.
void Sim::fireStrikeRocket(Entity& e) {
    const Weapon& w = WEAPONS[e.ut().weapon];
    float spread = STRIKE_SPREAD * TILE;
    Vec2 dest = e.target + Vec2(rng.f(-1, 1), rng.f(-1, 1)).norm() * (spread * std::sqrt(rng.f()));
    if (rng.f() < 0.75f) {
        static std::vector<Vec2> spots; spots.clear();
        forEachNear(e.target, spread * 1.5f, [&](Entity& t) {
            if (t.kind == EK_RESOURCE || !enemies(e.owner, t.owner) || t.isAir() || t.carrier.valid() || t.fall > 0 || !visibleTo(t, e.owner)) return;
            spots.push_back(t.pos);
        });
        if (!spots.empty()) dest = spots[rng.range(0, (int)spots.size() - 1)] + Vec2(rng.f(-10, 10), rng.f(-10, 10));
    }
    Vec2 fwd(std::cos(e.angle), std::sin(e.angle)), side(-fwd.y, fwd.x);
    Vec2 from = e.pos + fwd * 16.0f + side * ((e.ammo & 1) ? 11.0f : -11.0f);
    Projectile p;
    p.pos = from; p.prevPos = from; p.target = NOREF; p.weapon = (int)(&w - WEAPONS); p.owner = e.owner; p.shooter = refOf(e);
    Vec2 dir = (dest - from).norm();
    p.vel = dir * w.projSpeed; p.dest = dest; p.life = dist(from, dest) / w.projSpeed + 1.5f;
    projs.push_back(p);
    fx.push_back({FX_FLASH, from, from, 0, 0.1f, w.color, 8});
    emit(EV_SOUND, e.owner, SND_ROCKET, from);
}

// One pass, then away. On the run-in it holds its wave's heading and slides onto its own lane, fires its rockets from long range and lays its stick
// of bombs centred on its lane point; then it breaks to its side of the V in a hard turn with a full barrel roll and a spray of flares, and runs
// for the nearest edge ahead of it, fading out over the sea.
void Sim::strikeJetRun(Entity& e) {
    const UnitType& ut = e.ut();
    const float top = ut.speed;
    Vec2 fwd(std::cos(e.angle), std::sin(e.angle));
    if (e.strikePass < 2) {   // heading out over the map's edge on the run or the break (a spot called right by the border): straight on out, fading
        Vec2 out(0, 0); float dl = 1e9f;
        if (e.pos.x < dl) { dl = e.pos.x; out = Vec2(-1, 0); }
        if (WORLD_W - e.pos.x < dl) { dl = WORLD_W - e.pos.x; out = Vec2(1, 0); }
        if (e.pos.y < dl) { dl = e.pos.y; out = Vec2(0, -1); }
        if (WORLD_H - e.pos.y < dl) { dl = WORLD_H - e.pos.y; out = Vec2(0, 1); }
        if (dl < 140.0f && fwd.x * out.x + fwd.y * out.y > 0.2f) { e.strikePass = 2; e.bombsLeft = 0; }
    }
    if (e.strikePass == 0) {
        Vec2 lane = e.guardPos, to = lane - e.pos;
        float along = to.x * e.runDir.x + to.y * e.runDir.y;   // how far ahead along its heading the lane point still is
        Vec2 aim = lane - e.runDir * std::max(0.0f, along - 260.0f);   // (a point on its lane line a little ahead: it converges onto the lane, then holds it)
        float want = along > 80.0f ? std::atan2(aim.y - e.pos.y, aim.x - e.pos.x) : std::atan2(e.runDir.y, e.runDir.x);
        airSteer(e, want, top, false);
        e.actionTimer -= SIM_DT;
        if (e.salvoLeft > 0 && along < 900.0f && along > 420.0f && e.actionTimer <= 0) { fireStrikeRocket(e); e.salvoLeft--; if (e.ammo > 0) e.ammo--; e.actionTimer = 0.22f; }
        // the stick: a bomb lands 'carry' ahead of the aircraft, the first one half a stick short of the lane point
        const float carry = top * 0.45f, half = (STRIKE_BOMBS - 1) * STRIKE_BOMB_GAP * 0.5f;
        Vec2 impact = e.pos + fwd * carry;
        float lead = (lane.x - impact.x) * e.runDir.x + (lane.y - impact.y) * e.runDir.y;
        if (e.bombsLeft > 0 && lead <= half + 4.0f) {
            e.bombTimer -= SIM_DT;
            if (e.bombTimer <= 0) { dropBomb(e, impact); e.bombsLeft--; e.bombTimer += STRIKE_BOMB_GAP / top; if (e.bombsLeft == STRIKE_BOMBS - 1) emit(EV_SOUND, e.owner, SND_AIR, e.pos); }
        }
        if (e.bombsLeft == 0 || along < -400.0f) { e.strikePass = 1; e.jetBreak = 1.3f; e.bombsLeft = 0; emit(EV_SOUND, e.owner, SND_JET, e.pos); }
        return;
    }
    if (e.strikePass == 1) {   // the break
        float side = e.jetMode ? 1.0f : -1.0f;
        e.jetBreak -= SIM_DT;
        airSteer(e, e.angle + side * 1.4f, top, false);
        e.roll = side * 6.2832f * clampf(1.0f - e.jetBreak / 1.3f, 0, 1) * 1.15f;   // one full roll and a bit, rolling into the turn
        if (e.jetBreak > 0.5f && tick % 2 == 0) fx.push_back({FX_SPARK, e.pos - fwd * 18.0f + Vec2(rng.f(-6, 6), rng.f(-6, 6)), Vec2(), 0, 0.7f, rgb(255, 236, 170), 5});   // flares
        if (e.jetBreak <= 0) { e.strikePass = 2; e.roll = side * 0.6f; }
        return;
    }
    // out: on ahead, toward the nearest edge in front of it
    Vec2 edge(e.pos.x < WORLD_W * 0.5f ? -1.0f : 1.0f, 0);
    if (std::min(e.pos.y, WORLD_H - e.pos.y) < std::min(e.pos.x, WORLD_W - e.pos.x)) edge = Vec2(0, e.pos.y < WORLD_H * 0.5f ? -1.0f : 1.0f);
    Vec2 away = (Vec2(std::cos(e.angle), std::sin(e.angle)) * 1.2f + e.runDir * 0.6f + edge * 0.9f).norm();
    leaveMap(e, away);
}

// A loan on its way out: a lazy weave out to the edge, fading into the haze over the ocean, then gone (no wreck, no loss)
void Sim::leaveMap(Entity& e, Vec2 away) {
    float dl = std::min(std::min(e.pos.x, WORLD_W - e.pos.x), std::min(e.pos.y, WORLD_H - e.pos.y));
    e.fade = clampf((dl - 40.0f) / STRIKE_FADE, 0.0f, 1.0f);
    if (dl < 40.0f) { destroy(e, false); return; }
    float weave = std::sin(time * 2.4f + (float)(e.gen % 97));
    airSteer(e, std::atan2(away.y, away.x) + 0.25f * weave, airTopSpeed(e), false);
    e.roll = 0.45f * weave;
}

// ------------------------------------------------------------ cargo ships
// The beach a cargo ship lands its load on: the free land tile nearest the hull within CARGOSHIP_LAND tiles, leaning toward the spot the player picked
// (so a ship lying between two shores lands on the one that was meant). False if there is no land within reach.
bool Sim::shipLanding(const Entity& c, Vec2* land) const {
    const int R = (int)CARGOSHIP_LAND + 1;
    int cx = tileOf(c.pos.x), cy = tileOf(c.pos.y);
    Vec2 aim = c.unloadSpot.len2() > 1.0f ? c.unloadSpot : c.pos;
    float best = 1e18f; int bx = -1, by = -1;
    for (int y = cy - R; y <= cy + R; y++) for (int x = cx - R; x <= cx + R; x++) {
        if (!g_map.passable(x, y)) continue;
        float d = dist(tileCenter(x, y), c.pos);
        if (d > c.radius() + CARGOSHIP_LAND * TILE) continue;
        float sc = d + dist(tileCenter(x, y), aim) * 0.25f;
        if (sc < best) { best = sc; bx = x; by = y; }
    }
    if (bx < 0) return false;
    if (land) *land = tileCenter(bx, by);
    return true;
}

// ------------------------------------------------------------ aircraft carriers
static inline bool isCarrierType(int t) { return t == U_C_CARRIER || t == U_K_CARRIER; }

static inline bool nearBorder(Vec2 p) { const float m = 5.0f * TILE; return p.x < m || p.y < m || p.x > WORLD_W - m || p.y > WORLD_H - m; }

// The enemy a carrier's jets should go after: aircraft first, then warships, then armed units and the rest, the nearest first, within CARRIER_RANGE of
// the carrier and seen (never a town: its blocks fight nobody)
Entity* Sim::carrierTarget(const Entity& c, const Entity* jet) {
    Entity* best = nullptr; float bd = 1e18f;
    const float R = CARRIER_RANGE * TILE;
    forEachNear(c.pos, R, [&](Entity& t) {
        if (t.kind == EK_RESOURCE || !enemies(c.owner, t.owner) || (t.isBuilding() && t.district >= 0)) return;
        if (nearBorder(t.pos)) return;   // (a jet's attack run would carry it off the map: a called fleet still coming in is left to the guns)
        if (!canTarget(jet ? *jet : c, t)) return;
        if (!explored(c.owner, clampi(tileOf(t.pos.x), 0, MAP_W - 1), clampi(tileOf(t.pos.y), 0, MAP_H - 1))) return;
        float d = dist(t.pos, c.pos);
        if (d > R) return;
        float score = d - (t.isAir() ? 9.0f * TILE : 0.0f) - (t.isUnit() && !t.isAir() && t.ut().naval ? 5.0f * TILE : 0.0f) - (t.isUnit() && t.weapon() >= 0 ? 2.0f * TILE : 0.0f);
        if (score < bd) { bd = score; best = &t; }
    });
    return best;
}

// Every quarter second: a lost jet is replaced after CARRIER_REPLACE, and a ready jet takes off (one every CARRIER_LAUNCH_GAP) while there is an
// enemy to fight near the carrier. The jet does not appear in the air: it leaves its parking spot on the deck (the last ready one, so the others stay
// where they are), taxis to a bow catapult and is shot off (see carrierJetRun). (Spawning may grow the entity array: nothing touches 'c' after it.)
void Sim::updateCarrier(Entity& c) {
    if ((tick + c.gen) % 5 != 0 || c.owner < 0) return;
    const int jetT = c.type == U_C_CARRIER ? U_C_CARRIERJET : U_K_CARRIERJET;
    const Ref cr = refOf(c);
    int up = 0;
    for (auto& e : ents) if (e.alive && e.isUnit() && e.type == jetT && e.home == cr) up++;
    if ((int)c.deck.size() + up < CARRIER_WING) {
        if (c.deckReplace < 0) c.deckReplace = time + CARRIER_REPLACE;
        else if (time >= c.deckReplace) { c.deck.push_back(time); c.deckReplace = -1; }
    } else c.deckReplace = -1;
    if (c.deck.empty() || time < c.deckLaunch || c.disabledUntil > time) return;
    int ready = -1;
    for (int i = (int)c.deck.size() - 1; i >= 0; i--) if (c.deck[i] <= time) { ready = i; break; }
    if (ready < 0 || !carrierTarget(c, nullptr)) return;
    const int slot = std::min(ready, CARRIER_WING - 1);
    c.deck.erase(c.deck.begin() + ready);
    c.deckLaunch = time + CARRIER_LAUNCH_GAP;
    const float side = c.catSide ? CARRIER_CAT_Y : -CARRIER_CAT_Y;
    c.catSide ^= 1;
    const Vec2 park(CARRIER_PARK[slot][0], CARRIER_PARK[slot][1]);
    const float parkAng = c.angle + CARRIER_PARK[slot][2];
    const Vec2 at = carrierPoint(c, park);
    const int owner = c.owner;
    Ref r = spawnUnit(jetT, owner, at);
    Entity& j = ents[r.idx];
    j.pos = j.prevPos = j.lastPos = at;
    j.alt = 0; j.airspeed = 0; j.angle = j.turret = parkAng;
    j.home = cr; j.order = O_CARRIERJET; j.ammo = UNITS[jetT].ammo; j.targetEnt = NOREF;
    j.deckPhase = 1; j.deckT = 0; j.deckFrom = park; j.deckTo = Vec2(CARRIER_CAT_X0, side);
}

// A carrier's jet: it fights the carrier's targets while it has missiles, then comes back and lands on the deck (it goes back into the carrier's deck
// list). On deck and on the glide path it moves with the carrier, along a scripted track in the carrier's own frame. If the carrier is gone it has
// nowhere to land: it flies off the map like a loan.
void Sim::carrierJetRun(Entity& e) {
    const UnitType& ut = e.ut();
    Entity* c = get(e.home);
    if (!c || !c->alive || !c->isUnit() || !isCarrierType(c->type)) {
        if (e.deckPhase != 0) { e.deckPhase = 0; e.alt = std::max(e.alt, 0.6f); e.airspeed = std::max(e.airspeed, ut.speed * 0.6f); }
        Vec2 out(e.pos.x < WORLD_W * 0.5f ? -1.0f : 1.0f, e.pos.y < WORLD_H * 0.5f ? -1.0f : 1.0f);
        leaveMap(e, out.norm());
        return;
    }
    const float cspd = dist(c->pos, c->prevPos) / SIM_DT;   // the carrier's own speed (a jet on deck rides along with it)
    auto smooth = [](float u) { u = clampf(u, 0.0f, 1.0f); return u * u * (3.0f - 2.0f * u); };
    auto place = [&](Vec2 local, float rel) { e.pos = carrierPoint(*c, local); e.angle = e.turret = c->angle + rel; };
    auto angTo = [](Vec2 d) { return std::atan2(d.y, d.x); };
    switch (e.deckPhase) {
    case 1: {   // taxiing from the parking spot to the catapult, turning onto the track
        e.deckT += SIM_DT;
        float u = smooth(e.deckT / CARRIER_TAXI_T);
        Vec2 l = e.deckFrom + (e.deckTo - e.deckFrom) * u;
        // its heading: from the parked angle round onto the taxi line, then straight down the catapult (relative to the hull)
        const float parked = e.deckFrom.y < 0 ? CARRIER_PARK[0][2] : CARRIER_PARK[CARRIER_WING - 1][2];
        float a0 = angTo(e.deckTo - e.deckFrom), rel = u < 0.3f ? parked + (a0 - parked) * (u / 0.3f) : (u < 0.7f ? a0 : a0 * (1.0f - (u - 0.7f) / 0.3f));
        place(l, rel);
        e.alt = 0; e.airspeed = cspd;
        if (e.deckT >= CARRIER_TAXI_T) { e.deckPhase = 2; e.deckT = 0; e.deckFrom = e.deckTo; c->catAt = time; }
        return;
    }
    case 2: {   // the catapult stroke: from standstill to flying speed off the bow
        e.deckT += SIM_DT;
        float u = clampf(e.deckT / CARRIER_CAT_T, 0.0f, 1.0f);
        place(Vec2(CARRIER_CAT_X0 + (CARRIER_CAT_X1 - CARRIER_CAT_X0) * u * u, e.deckFrom.y), 0.0f);
        e.alt = 0; e.airspeed = cspd + ut.speed * 0.62f * u;
        if (e.deckT >= CARRIER_CAT_T) {
            e.deckPhase = 3; e.deckT = 0; e.alt = 0.05f; e.airspeed = std::max(ut.speed * 0.62f, cspd + ut.speed * 0.5f);
            fx.push_back({FX_SMOKE, e.pos, e.pos, 0, 1.0f, rgb(236, 240, 244), 7.0f, Vec2(-std::cos(e.angle) * 30, -std::sin(e.angle) * 30)});
            emit(EV_SOUND, e.owner, SND_JET, e.pos);
        }
        return;
    }
    case 3: {   // the climb-out: straight ahead, gaining height and speed
        e.deckT += SIM_DT;
        airSteer(e, e.angle, airTopSpeed(e));   // (straight ahead, unless the map's edge is coming up: then it pulls round already)
        e.alt = clampf(0.05f + 0.95f * e.deckT / CARRIER_CLIMB_T, 0.0f, 1.0f);
        if (e.deckT >= CARRIER_CLIMB_T) { e.deckPhase = 0; e.deckT = 0; e.alt = 1; }
        return;
    }
    case 5: {   // the glide path: from where it lined up down to the wires, riding with the carrier
        e.deckT += SIM_DT;
        float u = clampf(e.deckT / CARRIER_FINAL_T, 0.0f, 1.0f);
        Vec2 l = e.deckFrom + (e.deckTo - e.deckFrom) * u;
        place(l, CARRIER_LAND_ANG + (e.deckAng - CARRIER_LAND_ANG) * (1.0f - smooth(u / 0.3f)));
        e.alt = std::max(0.0f, 1.0f - u);
        e.airspeed = cspd + ut.speed * 0.42f * (1.0f - 0.3f * u);
        if (e.deckT >= CARRIER_FINAL_T) {   // down: the hook takes a wire
            e.deckPhase = 6; e.deckT = 0; e.alt = 0; e.deckFrom = e.deckTo; e.deckTo = Vec2(CARRIER_ROLL_X, CARRIER_ROLL_Y);
            fx.push_back({FX_SMOKE, e.pos, e.pos, 0, 0.8f, rgb(214, 214, 210), 5.0f, Vec2(-std::cos(e.angle) * 12, -std::sin(e.angle) * 12)});
            fx.push_back({FX_SPARK, e.pos, Vec2(), 0, 0.25f, rgb(255, 220, 160), 4});
        }
        return;
    }
    case 6: {   // the roll-out: the wire pulls it up short
        e.deckT += SIM_DT;
        float u = clampf(e.deckT / CARRIER_ROLL_T, 0.0f, 1.0f), k = 1.0f - (1.0f - u) * (1.0f - u);
        place(e.deckFrom + (e.deckTo - e.deckFrom) * k, CARRIER_LAND_ANG);
        e.alt = 0; e.airspeed = cspd;
        if (e.deckT >= CARRIER_ROLL_T) {
            int slot = std::min((int)c->deck.size(), CARRIER_WING - 1);
            e.deckPhase = 7; e.deckT = 0; e.deckFrom = e.deckTo; e.deckTo = Vec2(CARRIER_PARK[slot][0], CARRIER_PARK[slot][1]);
        }
        return;
    }
    case 7: {   // taxiing to its parking spot; there it is struck down for rearming (back into the deck list)
        e.deckT += SIM_DT;
        float T = CARRIER_TAXI_T * 1.3f, u = smooth(e.deckT / T);
        Vec2 d = e.deckTo - e.deckFrom;
        place(e.deckFrom + d * u, angTo(d));
        e.alt = 0; e.airspeed = cspd;
        if (e.deckT >= T) { c->deck.push_back(time + CARRIER_REARM); destroy(e, false); }
        return;
    }
    default: break;
    }
    if (e.ammo > 0) {
        Entity* t = get(e.targetEnt);
        if (!t || !canTarget(e, *t) || dist(t->pos, c->pos) > CARRIER_RANGE * TILE * 1.35f || nearBorder(t->pos)) {
            t = carrierTarget(*c, &e);
            e.targetEnt = t ? refOf(*t) : NOREF; e.jetBreak = 0; e.stuckTimer = 0;
        }
        if (t) { if (e.deckPhase == 4) e.deckPhase = 0; e.deckT = 0; e.goRounds = 0; jetAttack(e, *t); return; }
    }
    // coming home: line up astern of the angled deck (going round first when it would arrive from the wrong side), then fly the glide path. The
    // points it steers by stay inside the map (a carrier off the border - a called fleet coming in, say - would put them outside it), it never
    // slows below a safe approach speed on the way (flyTo is a landing approach: it bleeds speed near its spot), and a jet that has gone round
    // twice, or has been at it for a long while, comes in from where it is
    e.deckT += SIM_DT;   // (in this phase: seconds since it started for home)
    const Vec2 touch(CARRIER_TOUCH_X, CARRIER_TOUCH_Y);
    const float landAng = c->angle + CARRIER_LAND_ANG;
    const Vec2 ld(std::cos(landAng), std::sin(landAng));
    const float edge = 2.5f * TILE;
    auto inside = [&](Vec2 q) { return Vec2(clampf(q.x, edge, WORLD_W - edge), clampf(q.y, edge, WORLD_H - edge)); };
    auto steerTo = [&](Vec2 q, float spd) { Vec2 d = q - e.pos; airSteer(e, std::atan2(d.y, d.x), spd); };
    const Vec2 gate = inside(carrierPoint(*c, touch) - ld * 190.0f);
    const float approach = std::max(airTopSpeed(e) * AIR_STALL * 1.3f, ut.speed * 0.55f);
    const bool anyway = e.goRounds >= 2 || e.deckT > 20.0f;
    if (e.deckPhase == 4) {
        Vec2 wide = inside(gate - ld * 260.0f);
        if (dist(e.pos, wide) < 140.0f) { e.deckPhase = 0; if (e.goRounds < 255) e.goRounds++; }
        else { steerTo(wide, ut.speed * 0.8f); return; }
    }
    if (dist(e.pos, gate) < (anyway ? 150.0f : 80.0f)) {
        Vec2 h(std::cos(e.angle), std::sin(e.angle));
        if (h.x * ld.x + h.y * ld.y > (anyway ? -1.1f : 0.88f)) {   // lined up (or past caring): onto the glide path
            e.goRounds = 0;
            float rel = e.angle - c->angle;
            while (rel > 3.14159265f) rel -= 6.2831853f;
            while (rel < -3.14159265f) rel += 6.2831853f;
            e.deckPhase = 5; e.deckT = 0; e.deckFrom = carrierLocal(*c, e.pos); e.deckTo = touch; e.deckAng = rel;
            e.targetEnt = NOREF;
            carrierJetRun(e);   // (the first step of the glide path this tick: it never stops dead in the air)
            return;
        }
        e.deckPhase = 4;   // the wrong way round: out astern and back in
    }
    steerTo(gate, dist(e.pos, gate) > 600.0f ? ut.speed : approach);
}

// ------------------------------------------------------------ indestructible structures and the Island Nuke Launchers
void Sim::heatUp(Entity& b, float amount) {
    if (amount <= 0) return;
    float before = b.heat;
    b.heat = std::min(HEAT_MAX, b.heat + amount);
    if (before < 0.25f && b.heat >= 0.25f) fx.push_back({FX_SMOKE, b.pos, b.pos, 0, 2.4f, rgb(70, 62, 54), b.radius() * 0.6f, Vec2(rng.f(-6, 6), -26)});   // it catches
}

void Sim::updateIslandNuke(Entity& b) {
    // A neutral launcher sleeps. A held one keeps its clock (ISLAND_NUKE_COOLDOWN, the bunkers' cooldown) whatever burns round it; the renderer shows the
    // laser chamber flaring when something hits it (lastDamaged) and the flames (heat).
    if (b.owner < 0) return;
    if (time >= b.siloTimer && (tick + b.gen) % 200 == 0) fx.push_back({FX_SPARK, b.pos, b.pos, 0, 0.5f, rgb(255, 120, 90), 6});   // armed: a red light blinks
}

bool Sim::cmdIslandNuke(int player, Vec2 pos, bool force, Ref from) {
    if (player < 0 || player >= numPlayers) return false;
    force = force && !players[player].isAI;
    auto ready = [&](const Entity& e) { return e.alive && e.isBuilding() && e.owner == player && e.constructed && e.bt().role == BR_ISLANDNUKE && time >= e.siloTimer; };
    Entity* l = get(from);
    if (!l || !ready(*l)) {
        l = nullptr;
        for (auto& e : ents) if (ready(e)) { l = &e; break; }
    }
    if (!l) return false;
    l->siloTimer = time + ISLAND_NUKE_COOLDOWN;
    players[player].islandLaunches++;
    Nuke n; n.from = l->pos; n.pos = pos; n.owner = player; n.t = 0; n.force = force; n.scale = ISLAND_NUKE_SCALE; n.super = true;
    nukes.push_back(n);
    emit(EV_SOUND, -1, SND_ROCKET, l->pos);
    emit(EV_SOUND, -1, SND_EXPLODE_S, l->pos);
    for (int k = 0; k < 12; k++) fx.push_back({FX_SMOKE, l->pos + Vec2(rng.f(-24, 24), rng.f(-16, 16)), Vec2(), -rng.f(0, 0.3f), 2.8f, rgb(224, 214, 220), rng.f(12, 22), Vec2(rng.f(-26, 26), rng.f(-34, -8))});
    fx.push_back({FX_FLASH, l->pos, l->pos, 0, 0.35f, ISLAND_NUKE_GLOW, 46});
    for (int p = 0; p < numPlayers; p++) if (enemies(player, p)) {
        emit(EV_MSG, p, SND_ATTACKED, pos, "SUPER NUKE LAUNCH DETECTED"); emit(EV_UNDER_ATTACK, p, SND_ATTACKED, pos, "Super Nuke from the island incoming: it is almost unstoppable!");
    }
    return true;
}
int Sim::islandNukesReady(int player) const {
    int n = 0;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.bt().role == BR_ISLANDNUKE && time >= e.siloTimer) n++;
    return n;
}
float Sim::islandNukeWait(int player) const {
    float best = -1;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.bt().role == BR_ISLANDNUKE) {
        float w = std::max(0.0f, e.siloTimer - time);
        if (best < 0 || w < best) best = w;
    }
    return best;
}

// ------------------------------------------------------------ the Nuclear Bomber call-in
// The bomber comes in from the map corner behind the caller's base (the corner nearest it), holds a straight line at altitude to the called
// spot, lets its one bomb go just short of it so the bomb's fall carries it on, and flies straight on out over the far side of the map.
Vec2 Sim::bomberEntry(int player, Vec2 target) const {
    Vec2 base = player >= 0 && player < numPlayers ? players[player].basePos : target;
    const float m = 40.0f;
    Vec2 corners[4] = { Vec2(m, m), Vec2(WORLD_W - m, m), Vec2(m, WORLD_H - m), Vec2(WORLD_W - m, WORLD_H - m) };
    int best = 0; float bd = 1e30f;
    for (int i = 0; i < 4; i++) { float d = dist2(corners[i], base); if (d < bd) { bd = d; best = i; } }
    if (dist(corners[best], target) < 14.0f * TILE) {   // the spot is right by that corner: come in from the next one along so the run has room
        float far = -1;
        for (int i = 0; i < 4; i++) { float d = dist2(corners[i], target); if (i != best && (far < 0 || d < far) && dist(corners[i], target) >= 14.0f * TILE) { far = d; best = i; } }
    }
    return corners[best];
}

int Sim::bombersReady(int player) const {
    if (player < 0 || player >= numPlayers || time < NUKEBOMBER_OPEN) return 0;
    int n = 0;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.bt().role == BR_TECH && e.disabledUntil <= time && time >= e.bomberTimer) n++;
    return n;
}
float Sim::bomberWait(int player) const {
    float best = -1;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.bt().role == BR_TECH) {
        float w = std::max(0.0f, std::max(e.bomberTimer, NUKEBOMBER_OPEN) - time);
        if (best < 0 || w < best) best = w;
    }
    return best;
}

bool Sim::cmdNukeBomber(int player, Vec2 pos) {
    if (player < 0 || player >= numPlayers || time < NUKEBOMBER_OPEN) return false;
    Entity* tech = nullptr;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.bt().role == BR_TECH && e.disabledUntil <= time && time >= e.bomberTimer) { tech = &e; break; }
    if (!tech) return false;
    tech->bomberTimer = time + NUKEBOMBER_COOLDOWN;
    players[player].bomberCalls++;
    pos = Vec2(clampf(pos.x, TILE, WORLD_W - TILE), clampf(pos.y, TILE, WORLD_H - TILE));
    Vec2 from = bomberEntry(player, pos);
    int type = players[player].faction == F_CYBER ? U_C_NUKEBOMBER : U_K_NUKEBOMBER;
    Ref r = spawnUnit(type, player, from);
    Entity& b = ents[r.idx];
    b.pos = b.prevPos = b.lastPos = from;
    b.alt = 1; b.airspeed = UNITS[type].speed;
    Vec2 d = pos - from;
    b.angle = b.turret = std::atan2(d.y, d.x);
    b.order = O_NUKERUN; b.target = pos; b.bombDropped = false; b.lifeUntil = time + NUKEBOMBER_LIFE;
    strikeMarks.push_back({pos, player, time});
    emit(EV_SOUND, player, SND_JET, from);
    emit(EV_MSG, player, SND_NONE, pos, (std::string(UNITS[type].name) + " inbound with a nuclear bomb").c_str());
    for (int p = 0; p < numPlayers; p++) if (enemies(player, p)) { emit(EV_MSG, p, SND_ATTACKED, pos, "ENEMY NUCLEAR BOMBER INBOUND"); emit(EV_UNDER_ATTACK, p, SND_ATTACKED, pos, "A nuclear bomber is heading for this spot!"); }
    return true;
}

void Sim::nukeBomberRun(Entity& e) {
    const UnitType& ut = e.ut();
    if (!e.bombDropped) {
        Vec2 to = e.target - e.pos;
        float D = to.len();
        airSteer(e, std::atan2(to.y, to.x), ut.speed, false);
        // the bomb keeps most of the bomber's speed while it falls: let it go a fall's worth of flight short of the spot (and never past it)
        float lead = ut.speed * NUKEBOMBER_FALL * 0.55f;
        if (D <= lead || D < 24.0f) {
            Projectile p;
            p.pos = e.pos; p.prevPos = e.pos; p.vel = e.pos;
            p.dest = e.target; p.target = NOREF; p.owner = e.owner; p.shooter = refOf(e);
            p.weapon = W_NUKE_BOMB; p.nukeScale = NUKEBOMBER_SCALE;
            p.arcLen = NUKEBOMBER_FALL; p.arcT = 0; p.life = p.arcLen + 0.3f;
            projs.push_back(p);
            e.bombDropped = true;
            emit(EV_SOUND, -1, SND_AIR, e.pos);
        }
        return;
    }
    // bomb gone: on straight out over the far side of the map
    airSteer(e, e.angle, ut.speed, false);
    float dl = std::min(std::min(e.pos.x, WORLD_W - e.pos.x), std::min(e.pos.y, WORLD_H - e.pos.y));
    if (dl < 12.0f) destroy(e, false);
}

void Sim::updateStrikeMarks() {
    for (size_t i = 0; i < strikeWaves.size();) {   // the waves of an Air Strike come in one after another
        if (time < strikeWaves[i].at) { i++; continue; }
        StrikeWave w = strikeWaves[i];
        strikeWaves.erase(strikeWaves.begin() + i);
        launchStrikeWave(w);
    }
    for (size_t i = 0; i < strikeMarks.size();) {
        StrikeMark& m = strikeMarks[i];
        float age = time - m.t0;
        if (age > STRIKE_MARK_LIFE) { strikeMarks.erase(strikeMarks.begin() + i); continue; }
        if (age < 30.0f && tick % 6 == 0) fx.push_back({FX_SMOKE, m.pos + Vec2(rng.f(-3, 3), rng.f(-3, 3)), Vec2(), 0, 3.4f, rgb(230, 70, 60), 7.0f, Vec2(rng.f(-4, 4), -34)});   // a red smoke flare over the spot
        i++;
    }
}

// ------------------------------------------------------------ Waymakers
// Explored land, none of it water, a bridge deck, a structure or a supply pile, and something on it worth clearing (a tree, rock, a ridge)
bool Sim::waymakerFits(int player, int tx, int ty, int w, int h) const {
    int rough = 0;
    for (int j = ty; j < ty + h; j++) for (int i = tx; i < tx + w; i++) {
        if (!g_map.wayTile(i, j) || !exploredRaw(player, i, j)) return false;
        if (g_map.blocked[j * MAP_W + i] & 1) rough++;
    }
    return rough > 0;
}

// The machine grinds its patch flat (dust and grit fly while the bar fills), then the ground is open earth and the Waymaker is gone
void Sim::updateWaymaker(Entity& b) {
    b.progress = std::min(1.0f, b.progress + SIM_DT / WAYMAKER_TIME);
    b.hp = std::max(b.hp, b.maxHp * (0.1f + 0.9f * b.progress));
    const BuildType& bt = b.bt();
    if (tick % 4 == 0) {
        Vec2 at = b.pos + Vec2(rng.f(-1, 1) * bt.w * TILE * 0.45f, rng.f(-1, 1) * bt.h * TILE * 0.45f);
        fx.push_back({FX_SMOKE, at, at, 0, 1.6f, rgb(156, 138, 110), rng.f(8, 13), Vec2(rng.f(-10, 10), -rng.f(6, 18))});
        if (tick % 8 == 0) fx.push_back({FX_SPARK, at, Vec2(), 0, 0.3f, rgb(255, 214, 140), 3});
    }
    if (b.progress < 1.0f) return;
    int tx = b.tx, ty = b.ty, owner = b.owner;
    Vec2 at = b.pos;
    destroy(b, false);   // (lifts its structure mark off the ground first)
    int opened = g_map.clearWay(tx, ty, bt.w, bt.h);
    for (int k = 0; k < 8; k++) { Vec2 q = at + Vec2(rng.f(-40, 40), rng.f(-34, 34)); fx.push_back({FX_SMOKE, q, q, -rng.f(0, 0.3f), 2.2f, rgb(170, 152, 124), rng.f(12, 20), Vec2(rng.f(-12, 12), -rng.f(4, 14))}); }
    if (owner >= 0) emit(EV_MSG, owner, SND_BUILD_DONE, at, opened > 0 ? "Waymaker: the way is clear" : "Waymaker: nothing left to clear");
}

// ------------------------------------------------------------ Healing Antennas
// Every half second: everything of its army and its allies in reach gets a share of its health back. Antennas do not stack: a thing mended in the
// last half second (by this antenna or another) waits for the next pulse. The scrubbing of fallout is done with the trucks' (see updateFallout).
void Sim::updateHealAntenna(Entity& b) {
    const u32 PULSE = (u32)(0.5f * SIM_HZ);
    if ((tick + b.gen) % PULSE != 0) return;
    const float R = HEALANT_RADIUS * TILE, dt = PULSE * SIM_DT;
    int healed = 0;
    forEachNear(b.pos, R + 2 * TILE, [&](Entity& t) {
        if (t.kind == EK_RESOURCE || t.owner < 0 || t.hp >= t.maxHp || t.carrier.valid() || (t.owner != b.owner && enemies(b.owner, t.owner))) return;
        if (t.isBuilding() && (!t.constructed || invulnerable(t))) return;
        if (dist(t.pos, b.pos) > R + t.radius() * 0.5f || tick - t.mendTick < PULSE) return;
        t.mendTick = tick;
        t.hp = std::min(t.maxHp, t.hp + t.maxHp * HEALANT_RATE * (t.isBuilding() ? HEALANT_STRUCT : 1.0f) * dt);
        if (((tick / PULSE) + (u32)(&t - &ents[0])) % 3 == 0) fx.push_back({FX_SPARK, t.pos + Vec2(rng.f(-6, 6), rng.f(-8, 2)), Vec2(), 0, 0.5f, rgb(120, 255, 170), 4});
        healed++;
    });
    if (healed && ((tick / PULSE) + b.gen) % 4 == 0) fx.push_back({FX_RING, b.pos, b.pos, 0, 1.1f, rgb(120, 255, 180), R * 0.85f});
}

// ------------------------------------------------------------ Fleet Call
// The tech structure's naval power: a battle fleet comes in from the open sea. Nothing appears in the middle of the map: the called spot is moved onto
// the nearest open ocean, and the ships come out of the haze at the stretch of map border nearest it, lane after lane, then sail in and form up round
// it (patrol boats as a screen ahead, destroyers and cruisers round the carrier). From then on they are the army's own ships.
static inline bool isPatrolType(int t) { return t == U_C_PATROL || t == U_K_PATROL; }
static inline bool isDestroyerType(int t) { return t == U_C_DESTROYER || t == U_K_DESTROYER; }

int Sim::fleetsReady(int player) const {
    if (player < 0 || player >= numPlayers || !players[player].alive || time < players[player].dropReady) return 0;
    int n = 0;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.disabledUntil <= time && e.bt().role == BR_TECH && time >= e.fleetTimer) n++;
    return n;
}
float Sim::fleetWait(int player) const {
    if (player < 0 || player >= numPlayers) return -1;
    float best = -1;
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.bt().role == BR_TECH) {
        float w = std::max(0.0f, std::max(e.fleetTimer, players[player].dropReady) - time);
        if (best < 0 || w < best) best = w;
    }
    return best;
}

std::vector<int> Sim::fleetManifest(int player) const {
    std::vector<int> out;
    if (player < 0 || player >= numPlayers) return out;
    const bool cy = players[player].faction == F_CYBER;
    for (int k = 0; k < FLEET_PATROLS; k++) out.push_back(cy ? U_C_PATROL : U_K_PATROL);
    for (int k = 0; k < FLEET_WARSHIPS; k++) out.push_back(cy ? U_C_DESTROYER : U_K_DESTROYER);
    for (int k = 0; k < FLEET_WARSHIPS; k++) out.push_back(cy ? U_C_CRUISER : U_K_CRUISER);
    for (int k = 0; k < FLEET_CARRIERS; k++) out.push_back(cy ? U_C_CARRIER : U_K_CARRIER);
    return out;
}

bool Sim::fleetEntry(Vec2 pos, Vec2& goal, Vec2& entry, Vec2& inward) const {
    const int ocean = g_map.oceanComp;
    if (ocean == 0) return false;
    pos = Vec2(clampf(pos.x, TILE, WORLD_W - TILE), clampf(pos.y, TILE, WORLD_H - TILE));
    goal = g_map.nearestWater(pos, (int)FLEET_REACH, true, ocean);
    if (!g_map.navTile(tileOf(goal.x), tileOf(goal.y)) || g_map.compAt(goal) != ocean) return false;
    // the four borders, nearest first; on each, slide from the point level with the spot until the whole column of lanes (and a couple of ship
    // lengths in from the border) is open ocean
    const float depth = 1.6f * TILE;
    const int half = 3;
    struct Side { float d; Vec2 n; } sides[4] = { { goal.x, Vec2(1, 0) }, { WORLD_W - goal.x, Vec2(-1, 0) }, { goal.y, Vec2(0, 1) }, { WORLD_H - goal.y, Vec2(0, -1) } };
    std::sort(sides, sides + 4, [](const Side& a, const Side& b) { return a.d < b.d; });
    for (const Side& sd : sides) {
        const Vec2 n = sd.n, along(-n.y, n.x);
        Vec2 base = n.x > 0 ? Vec2(depth, goal.y) : n.x < 0 ? Vec2(WORLD_W - depth, goal.y) : n.y > 0 ? Vec2(goal.x, depth) : Vec2(goal.x, WORLD_H - depth);
        for (int k = 0; k <= 50; k++) for (int sgn = -1; sgn <= 1; sgn += 2) {
            if (k == 0 && sgn > 0) continue;
            Vec2 c = base + along * (sgn * k * 2.0f * TILE);
            bool ok = true;
            for (int l = -half; l <= half && ok; l++) for (int dd = 0; dd <= 3 && ok; dd++) {
                Vec2 q = c + along * (l * FLEET_LANE * TILE) + n * (dd * 2.0f * TILE);
                int x = tileOf(q.x), y = tileOf(q.y);
                if (!inMap(x, y) || !g_map.navOpen(x, y) || g_map.waterComp[y * MAP_W + x] != ocean) ok = false;
            }
            if (ok) { entry = c; inward = n; return true; }
        }
    }
    return false;
}

bool Sim::cmdFleetCall(int player, Vec2 pos) {
    if (player < 0 || player >= numPlayers) return false;
    Player& pl = players[player];
    if (!pl.alive || time < pl.dropReady) return false;
    Entity* tech = nullptr;   // like the paradrops: each Data Center / Arms Lab calls one fleet per cooldown
    for (auto& e : ents) if (e.alive && e.isBuilding() && e.owner == player && e.constructed && e.disabledUntil <= time && e.bt().role == BR_TECH && time >= e.fleetTimer) { tech = &e; break; }
    if (!tech) return false;
    Vec2 goal, entry, n;
    if (!fleetEntry(pos, goal, entry, n)) return false;
    tech->fleetTimer = time + DROP_TIERS[dropTier(player)].cooldown;   // the paradrops' own cooldown
    const Vec2 along(-n.y, n.x);
    const int ocean = g_map.oceanComp;
    auto slot = [&](float fwd, float side) {   // a place in the formation round the goal (tiles ahead along the way in, tiles to the side)
        Vec2 q = goal + n * (fwd * TILE) + along * (side * TILE);
        q = Vec2(clampf(q.x, 2.0f * TILE, WORLD_W - 2.0f * TILE), clampf(q.y, 2.0f * TILE, WORLD_H - 2.0f * TILE));
        Vec2 w = g_map.nearestWater(q, 9, true, ocean);
        return g_map.compAt(w) == ocean && g_map.navTile(tileOf(w.x), tileOf(w.y)) ? w : goal;
    };
    int pi = 0, di = 0, ci = 0, total = 0;
    for (int t : fleetManifest(player)) {
        FleetShip f; f.owner = player; f.type = t; f.dir = n;
        int lane; float row, fwd, side;
        if (isPatrolType(t)) {   // the screen: lanes two and three out on either side, three rows deep; they spread into a line well ahead of the goal
            static const int LANES[4] = { -2, 2, -3, 3 };
            lane = LANES[pi % 4]; int r = pi / 4; row = r * 0.7f;
            side = (lane < 0 ? -1.0f : 1.0f) * (std::abs(lane) == 3 ? 9.0f - 2.0f * r : 1.0f + 2.0f * r);
            fwd = std::abs(side) <= 5.0f ? 8.0f : (std::abs(side) <= 7.0f ? 6.5f : 5.0f);
            pi++;
        } else if (isDestroyerType(t)) {   // the first row in the middle lanes; they take the places ahead of the carrier
            lane = di - 1; row = 0;
            fwd = lane == 0 ? 5.5f : 4.0f; side = lane * 4.0f;
            di++;
        } else if (t == U_C_CRUISER || t == U_K_CRUISER) {   // the second row: the flanks of the carrier, one just ahead of it
            lane = ci - 1; row = 1;
            fwd = lane == 0 ? 1.0f : -2.0f; side = lane * 6.0f;
            ci++;
        } else {   // the carrier last, down the middle lane, to the centre of the group a little behind the goal
            lane = 0; row = 2.3f;
            fwd = -3.5f; side = 0;
        }
        f.from = entry + along * (lane * FLEET_LANE * TILE);
        f.at = time + 0.4f + row * FLEET_GAP + 0.22f * std::abs(lane);
        f.to = slot(fwd, side);
        fleetArrivals.push_back(f);
        total++;
    }
    fleetMarks.push_back({entry, goal, player, time});
    const char* edge = n.x > 0 ? "west" : n.x < 0 ? "east" : n.y > 0 ? "north" : "south";
    char msg[160];
    snprintf(msg, sizeof msg, "%s: %d ships inbound from the %s edge of the map", FLEETS[pl.faction].name, total, edge);
    emit(EV_MSG, player, SND_NONE, goal, msg);
    for (int p = 0; p < numPlayers; p++) if (enemies(player, p)) emit(EV_MSG, p, SND_ATTACKED, entry, "ENEMY FLEET APPROACHING FROM THE OPEN SEA");
    return true;
}

void Sim::updateFleetArrivals() {
    for (size_t i = 0; i < fleetArrivals.size();) {
        if (time < fleetArrivals[i].at) { i++; continue; }
        FleetShip f = fleetArrivals[i];
        fleetArrivals.erase(fleetArrivals.begin() + i);
        if (f.owner < 0 || f.owner >= numPlayers || !players[f.owner].alive) continue;   // (the army fell before its fleet came in)
        Ref r = spawnUnit(f.type, f.owner, f.from);
        Entity& e = ents[r.idx];
        e.angle = e.turret = std::atan2(f.dir.y, f.dir.x);
        e.fade = 0.02f;   // out of the haze (see updateUnit)
        e.guardPos = f.to;
        cmdMove({r}, f.to, true);
        bool last = true;
        for (auto& o : fleetArrivals) if (o.owner == f.owner) { last = false; break; }
        if (last) emit(EV_MSG, f.owner, SND_UNIT_READY, f.to, "The fleet has arrived: it is yours to command");
    }
    for (size_t i = 0; i < fleetMarks.size();) { if (time - fleetMarks[i].t0 > 30.0f) fleetMarks.erase(fleetMarks.begin() + i); else i++; }
}
