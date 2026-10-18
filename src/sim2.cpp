// One Hour - simulation of the newer subsystems: jeeps (seats that shoot), protector drones, ships, anti-nuke defences and the Hotline Strike fighters.
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
    if (!j.unloading && (tick + j.gen) % 2 == 0) passengersFire(j, 12.5f);
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

Vec2 Sim::navalExit(const Entity& yard) const { return g_map.nearestWater(yard.pos, 10, true); }   // (always the lake: a ship put into a cut-off river could never get out)

// The legal spot nearest to 'near' for a structure that must touch the lake (a Naval Yard): scans the whole shore the player has explored.
bool Sim::shoreSite(int player, int buildType, Vec2 near, int& tx, int& ty) const {
    const BuildType& b = BUILDS[buildType];
    float best = 1e18f; bool found = false;
    for (int y = 1; y < MAP_H - b.h; y++) for (int x = 1; x < MAP_W - b.w; x++) {
        if (!canPlace(player, buildType, x, y)) continue;
        bool roadFree = true;   // never plug a road (the ring road runs right along the beach)
        for (int j = y - 1; j <= y + b.h && roadFree; j++) for (int i = x - 1; i <= x + b.w; i++) if (inMap(i, j) && (g_map.tile(i, j) == T_ROAD || g_map.bridge[j * MAP_W + i])) { roadFree = false; break; }
        if (!roadFree) continue;
        bool gapOk = true;   // keep the one-tile lane around the structure open, as the computer commander does for everything it builds
        for (int j = y - 1; j <= y + b.h && gapOk; j++) for (int i = x - 1; i <= x + b.w; i++) if (inMap(i, j) && (g_map.blocked[j * MAP_W + i] & 2)) { gapOk = false; break; }
        if (!gapOk) continue;
        float d = dist(buildingCenter(buildType, x, y), near);
        if (d < best) { best = d; tx = x; ty = y; found = true; }
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
    Nuke probe; probe.scale = scale;
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
float Sim::antiNukeChance(Vec2 at, int attacker, bool knownOnly, float scale, bool deep) const {
    float miss = 1.0f;
    Nuke probe; probe.owner = attacker;
    for (auto& b : ents) {
        if (!antiNukeWorking(b) || !nukeHurts(probe, b.owner) || (deep && !(b.bt().faction == F_CYBER || isAntiRocketWall(b)))) continue;
        if (knownOnly && attacker >= 0 && !explored(attacker, clampi(tileOf(b.pos.x), 0, MAP_W - 1), clampi(tileOf(b.pos.y), 0, MAP_H - 1))) continue;
        if (dist(at, b.pos) <= antiNukeCover(b, scale)) miss *= 1.0f - antiNukeWarheadShot(b);
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
        if (n.deep && !cy) continue;     // an Underground Nuke's warhead outruns interceptor missiles: only the lasers can burn it down
        if (time < b.siloTimer) break;   // (a laser that just fired at one warhead is still recharging)
        float left = NUKE_FLIGHT - n.t;
        int linked = cy ? antiNukeLinked(b) : 0;
        const float gap = cy ? 0.8f / (1.0f + ANTINUKE_LINK_TRACK * linked) : 1.5f, flight = cy ? 0.3f : 1.0f;
        if (left > (cy ? ANTINUKE_LASER_WINDOW : ANTINUKE_WINDOW) || left < flight + 0.2f) continue;
        if (std::find(n.engagedBy.begin(), n.engagedBy.end(), self) != n.engagedBy.end()) continue;
        n.engagedBy.push_back(self);
        int shots = cy ? 3 + linked : 2;
        while (shots > 1 && (shots - 1) * gap + flight > left - 0.2f) shots--;
        int winner = rng.f() < antiNukeWarheadShot(b) ? rng.range(0, shots - 1) : -1;
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

// ------------------------------------------------------------ the Hotline Strike fighters
// A fighter on loan for one run: it crosses the map at full speed, lines up on the called spot and lets a salvo of unguided rockets go on a pass,
// flies on, comes round, and does it again STRIKE_PASSES times; then it leaves over the nearest edge (or is withdrawn if its time is up).
void Sim::fireStrikeRocket(Entity& e) {
    const Weapon& w = WEAPONS[e.ut().weapon];
    float spread = STRIKE_SPREAD * TILE;
    Vec2 dest = e.target + Vec2(rng.f(-1, 1), rng.f(-1, 1)).norm() * (spread * std::sqrt(rng.f()));
    if (rng.f() < 0.65f) {   // most of the rockets go for something that stands in the circle, the rest rake the ground between
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
    if ((e.ammo & 3) == 0) emit(EV_SOUND, e.owner, SND_ROCKET, from);
}

void Sim::strikeJetRun(Entity& e) {
    const UnitType& ut = e.ut();
    if (e.salvoLeft > 0) {   // the salvo: one rocket every few hundredths of a second, flying dead straight
        e.actionTimer -= SIM_DT;
        while (e.salvoLeft > 0 && e.actionTimer <= 0) { fireStrikeRocket(e); e.salvoLeft--; if (e.ammo > 0) e.ammo--; e.actionTimer += 0.085f; }
        if (e.salvoLeft == 0) { e.strikePass++; e.jetBreak = 1.0f; }
        airSteer(e, e.angle, ut.speed);   // (the edge look-ahead still turns it inward near the border: the rockets fly to their own aim points either way)
        return;
    }
    bool leaving = e.strikePass >= STRIKE_PASSES || e.ammo <= 0 || (e.lifeUntil >= 0 && time >= e.lifeUntil - 10.0f);
    if (leaving) {   // out over the nearest edge, away from the target
        Vec2 away = e.pos - e.target;
        if (away.len2() < 1.0f) away = Vec2(std::cos(e.angle), std::sin(e.angle));
        float dl = std::min(std::min(e.pos.x, WORLD_W - e.pos.x), std::min(e.pos.y, WORLD_H - e.pos.y));
        if (e.jetBreak > 0 && dl > 90.0f) { e.jetBreak -= SIM_DT; airSteer(e, e.angle, ut.speed, false); }
        else airSteer(e, std::atan2(away.y, away.x), ut.speed, false);
        if (dl < 30.0f) destroy(e, false);   // gone over the edge
        return;
    }
    if (e.jetBreak > 0) { e.jetBreak -= SIM_DT; airSteer(e, e.angle, ut.speed); return; }   // flying on past the target before swinging round
    Vec2 aim = e.guardPos.len2() > 1.0f ? e.guardPos : e.target;   // each fighter has its own lane across the spot (see cmdPhoneStrike)
    Vec2 to = aim - e.pos;
    float D = to.len(), want = std::atan2(to.y, to.x);
    airSteer(e, want, ut.speed);
    if (D < 640.0f && D > 220.0f && std::abs(angDelta(e.angle, want)) < 0.12f) { e.salvoLeft = STRIKE_SALVO; e.actionTimer = 0; emit(EV_SOUND, e.owner, SND_JET, e.pos); }   // (a pass begins with the roar of the engines)
}

void Sim::updateStrikeMarks() {
    for (size_t i = 0; i < strikeMarks.size();) {
        StrikeMark& m = strikeMarks[i];
        float age = time - m.t0;
        if (age > STRIKE_MARK_LIFE) { strikeMarks.erase(strikeMarks.begin() + i); continue; }
        if (age < 30.0f && tick % 6 == 0) fx.push_back({FX_SMOKE, m.pos + Vec2(rng.f(-3, 3), rng.f(-3, 3)), Vec2(), 0, 3.4f, rgb(230, 70, 60), 7.0f, Vec2(rng.f(-4, 4), -34)});   // a red smoke flare over the spot
        i++;
    }
}
