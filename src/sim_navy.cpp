// One Hour - nuclear submarines, battleships' cruise-missile cells and the warships' drone swarms (see SUB_*, VLS_*, SWARM_* in data.h)
#include "sim.h"

// ------------------------------------------------------------ submarines
int Sim::subsOf(int player, bool queued) const {
    int n = 0;
    for (auto& e : ents) {
        if (!e.alive || e.owner != player) continue;
        if (e.isUnit() && subType(e.type)) n++;
        else if (queued && e.isBuilding()) for (int t : e.queue) if (subType(t)) n++;
    }
    return n;
}

void Sim::cmdDive(const std::vector<Ref>& sel, bool dive) {
    for (Ref r : sel) {
        Entity* e = get(r);
        if (!e || !e->isUnit() || !subType(e->type) || e->owner < 0) continue;
        if (dive) {
            if (time < e->exposedUntil || e->nukeSalvo > 0) continue;   // (just after a launch it has to stay up; one about to launch comes up for it)
            if (e->subState == 0) { e->subState = 1; e->subT = time + SUB_DIVE_TIME; }
            else if (e->subState == 3) { e->subState = 1; e->subT = time + SUB_DIVE_TIME * 0.5f; }
            e->subAutoDive = false; e->subStayUp = false; e->subHunt = false;
            e->engaged = NOREF; if (e->order == O_ATTACK) { e->order = O_IDLE; e->targetEnt = NOREF; }   // (a dived submarine fires at nothing)
        } else {
            if (e->subState == 2) { e->subState = 3; e->subT = time + SUB_SURFACE_TIME; }
            else if (e->subState == 1) { e->subState = 3; e->subT = time + SUB_SURFACE_TIME * 0.5f; }
            e->subAutoDive = false; e->subStayUp = true; e->subHunt = false;   // (brought up on purpose: it stays up until the danger is serious)
        }
        if (e->subState == 1) for (int k = 0; k < 6; k++) fx.push_back({FX_RING, e->pos + Vec2(rng.f(-20, 20), rng.f(-10, 10)), e->pos, -k * 0.1f, 0.8f, rgb(220, 240, 250), 14.0f});
    }
}

bool Sim::cmdSubNuke(int player, Vec2 pos, bool force, Ref from) {
    if (player < 0 || player >= numPlayers) return false;
    force = force && !players[player].isAI;
    auto ready = [&](const Entity& e) { return e.alive && e.isUnit() && subType(e.type) && e.owner == player && time >= e.siloTimer && e.nukeSalvo == 0 && e.disabledUntil <= time; };
    Entity* s = get(from);
    if (!s || !ready(*s)) {
        s = nullptr; float best = 1e30f;
        for (auto& e : ents) if (ready(e)) { float d = dist2(e.pos, pos); if (d < best) { best = d; s = &e; } }   // (the nearest one with its missile ready)
    }
    if (!s) return false;
    s->nukeSalvo = 1; s->nukeSalvoPos = pos; s->nukeSalvoForce = force;
    if (s->subState == 2 || s->subState == 1) { s->subState = 3; s->subT = time + SUB_SURFACE_TIME; s->subAutoDive = true; }   // it has to come up to launch
    emit(EV_MSG, player, SND_NONE, s->pos, s->subState == 0 ? "Submarine launching" : "Submarine surfacing to launch");
    return true;
}

int Sim::subNukesReady(int player) const {
    int n = 0;
    for (auto& e : ents) if (e.alive && e.isUnit() && subType(e.type) && e.owner == player && time >= e.siloTimer && e.nukeSalvo == 0) n++;
    return n;
}
float Sim::subNukeWait(int player) const {
    float best = -1;
    for (auto& e : ents) if (e.alive && e.isUnit() && subType(e.type) && e.owner == player) {
        float w = std::max(0.0f, e.siloTimer - time);
        if (best < 0 || w < best) best = w;
    }
    return best;
}

void Sim::updateSub(Entity& e) {
    if (e.siloTimer <= 0) e.siloTimer = time + SUB_NUKE_ARM;   // (a new submarine: its missile is armed a while after it leaves the yard)
    if (e.subState == 1 && time >= e.subT) {   // under: out of every sight
        e.subState = 2; e.engaged = NOREF;
        if (e.order == O_ATTACK && !e.subHunt) { e.order = O_IDLE; e.targetEnt = NOREF; e.guardPos = e.pos; }   // (one sent at a ship keeps it: it closes in under water)
        for (int q = 0; q < MAX_PLAYERS; q++) if (e.owner < 0 || q != e.owner) e.detUntil[q] = 0;
        fx.push_back({FX_RING, e.pos, e.pos, 0, 1.1f, rgb(210, 232, 246), e.radius() * 1.6f});
        emit(EV_SOUND, -1, SND_SPLASH, e.pos);
    } else if (e.subState == 3 && time >= e.subT) {   // up
        e.subState = 0;
        fx.push_back({FX_RING, e.pos, e.pos, 0, 0.9f, rgb(230, 244, 255), e.radius() * 1.8f});
        for (int k = 0; k < 8; k++) fx.push_back({FX_SMOKE, e.pos + Vec2(rng.f(-30, 30), rng.f(-12, 12)), Vec2(), -rng.f(0, 0.3f), 1.4f, rgb(226, 238, 246), rng.f(8, 14), Vec2(rng.f(-10, 10), -rng.f(6, 16))});
        emit(EV_SOUND, -1, SND_SPLASH, e.pos);
    }
    // the launch it came up for: the hatch opens, the missile goes, and from now on it is out in the open for SUB_EXPOSED seconds
    if (e.nukeSalvo > 0 && e.subState == 0 && e.disabledUntil <= time) {
        Nuke n; n.from = e.pos; n.pos = e.nukeSalvoPos; n.owner = e.owner; n.t = 0; n.force = e.nukeSalvoForce; n.scale = SUB_NUKE_SCALE;
        n.deep = true; n.sub = true;   // (a silver-cased ballistic warhead: only lasers can stop it, and it hits harder than any other: SUB_NUKE_DAMAGE)
        nukes.push_back(n);
        e.nukeSalvo = 0; e.siloTimer = time + SUB_NUKE_COOLDOWN; e.exposedUntil = time + SUB_EXPOSED;
        for (int q = 0; q < numPlayers; q++) if (e.owner >= 0 && enemies(e.owner, q)) e.detUntil[q] = std::max(e.detUntil[q], time + SUB_EXPOSED);   // (the launch gives it away to everyone)
        for (int k = 0; k < 12; k++) fx.push_back({FX_SMOKE, e.pos + Vec2(rng.f(-14, 14), rng.f(-10, 10)), Vec2(), -rng.f(0, 0.3f), 2.6f, rgb(226, 230, 236), rng.f(10, 20), Vec2(rng.f(-20, 20), rng.f(-34, -8))});
        fx.push_back({FX_FLASH, e.pos, e.pos, 0, 0.3f, rgb(255, 230, 180), 40});
        emit(EV_SOUND, -1, SND_ROCKET, e.pos);
        emit(EV_SOUND, -1, SND_EXPLODE_S, e.pos);
        for (int q = 0; q < numPlayers; q++) if (e.owner >= 0 && enemies(e.owner, q)) {
            emit(EV_MSG, q, SND_ATTACKED, e.nukeSalvoPos, "SUBMARINE LAUNCH DETECTED");
            emit(EV_UNDER_ATTACK, q, SND_ATTACKED, e.pos, "An enemy submarine has surfaced to launch a nuke: it is exposed now");
        }
    }
    // it came up only to launch: down again as soon as it may
    if (e.subAutoDive && e.subState == 0 && e.nukeSalvo == 0 && time >= e.exposedUntil) { e.subAutoDive = false; e.subState = 1; e.subT = time + SUB_DIVE_TIME; }
    if ((tick + e.gen) % 4 == 0) subAutonomy(e);
}

// Down out of sight. crash: the emergency dive (SUB_CRASH_DIVE); a dive under way is not slowed down by it.
static void subGoDown(Sim& S, Entity& e, bool crash) {
    float t = crash ? SUB_CRASH_DIVE : SUB_DIVE_TIME;
    if (e.subState == 0) { e.subState = 1; e.subT = S.time + t; }
    else if (e.subState == 3) { e.subState = 1; e.subT = S.time + t * 0.5f; }
    else return;
    e.subAutoDive = false; e.subStayUp = false; e.subHunt = false; e.subUpSince = -1;
    e.engaged = NOREF; if (e.order == O_ATTACK) { e.order = O_IDLE; e.targetEnt = NOREF; e.guardPos = e.pos; }
    for (int k = 0; k < 6; k++) S.fx.push_back({FX_RING, e.pos + Vec2(S.rng.f(-20, 20), S.rng.f(-10, 10)), e.pos, -k * 0.1f, 0.8f, rgb(220, 240, 250), 14.0f});
}

// The stretch of its own sea, 8-26 tiles off, furthest from everything that threatens it (and out of a warhead's blast): 48 candidates scored
// against at most 32 threats, only when it has to run, at most every 2.5 s.
bool Sim::subSafeSpot(const Entity& e, const std::vector<Vec2>& threats, const Nuke* hot, Vec2& out) const {
    const int comp = g_map.compAt(e.pos);
    Vec2 away(0, 0);
    for (const Vec2& t : threats) { Vec2 d = e.pos - t; float l = d.len(); if (l > 1) away = away + d * (1.0f / l); }
    if (hot) { Vec2 d = e.pos - hot->pos; float l = d.len(); if (l > 1) away = away + d * (3.0f / l); }
    float al = away.len(); if (al > 0.01f) away = away * (1.0f / al);
    float best = -1e30f; bool found = false;
    static const float RAD[3] = {8.0f, 15.0f, 26.0f};
    for (int ri = 0; ri < 3; ri++) for (int k = 0; k < 16; k++) {
        float a = k * (6.2831853f / 16) + ri * 0.2f;
        Vec2 dir(std::cos(a), std::sin(a));
        Vec2 c = e.pos + dir * (RAD[ri] * TILE);
        int tx = tileOf(c.x), ty = tileOf(c.y);
        if (!g_map.navOpen(tx, ty) || g_map.compAt(c) != comp) continue;
        float near = 30.0f * TILE;
        for (const Vec2& t : threats) near = std::min(near, dist(c, t));
        float sc = near - 0.25f * RAD[ri] * TILE + 40.0f * (dir.x * away.x + dir.y * away.y);
        if (hot) { float R = nukeRadius(*hot) + 2.0f * TILE; float dn = dist(c, hot->pos); if (dn < R) sc -= (R - dn) * 3.0f; }
        if (sc > best) { best = sc; out = c; found = true; }
    }
    return found;
}

// A submarine looks after itself. Every fourth tick (staggered): is a warhead coming down on it, is it being shot at, is an enemy gun that can see
// it aimed at it? Up and in danger it crash-dives and runs; up with nothing to do it dives; ordered at a ship while dived it closes in under water
// and surfaces only within torpedo range. A dived submarine is safe, so all this costs nothing for the boats that stay down.
void Sim::subAutonomy(Entity& e) {
    if (e.owner < 0 || !e.alive) return;
    const Ref me = refOf(e);
    // ---- dived with an attack order on a ship: close in under water, up only within reach of the torpedoes
    if (e.subState == 2) {
        Entity* t = e.order == O_ATTACK ? get(e.targetEnt) : nullptr;
        if (t && e.weapon() >= 0 && e.nukeSalvo == 0 && e.disabledUntil <= time) {
            float reach = WEAPONS[e.weapon()].range * TILE;
            if (distToEntity(e.pos, *t) <= reach + 0.15f * TILE) { e.subState = 3; e.subT = time + SUB_SURFACE_TIME; e.subHunt = true; e.subStayUp = false; }
        }
        return;
    }
    // ---- what is coming at it
    const Nuke* hot = nullptr; float hotLeft = 1e9f;
    for (auto& n : nukes) {
        if (n.broken || !nukeHurts(n, e.owner)) continue;
        float R = nukeRadius(n) + 2.0f * TILE;
        if (dist2(n.pos, e.pos) >= R * R) continue;
        float left = flightOf(n) - n.t;
        if (left < hotLeft) { hotLeft = left; hot = &n; }
    }
    const bool hurt = time - e.lastDamaged < 1.5f;
    std::vector<Vec2> threats; bool aimed = false;
    if (e.subState != 2) {
        const u8 mask = enemyMask(e.owner);
        forEachNearMask(e.pos, SUB_THREAT_R * TILE, mask, [&](Entity& w) {
            if (w.kind == EK_RESOURCE || w.owner < 0 || !enemies(e.owner, w.owner) || w.weapon() < 0) return;
            if (w.isBuilding() && !w.constructed) return;
            if (w.isUnit() && (w.carrier.valid() || submerged(w))) return;
            if (threats.size() < 32) threats.push_back(w.pos);
            if (aimed) return;
            if (w.engaged == me || w.targetEnt == me || w.forceTarget == me) aimed = true;
            else if (dist(w.pos, e.pos) <= WEAPONS[w.weapon()].range * TILE + e.radius() + TILE && canTarget(w, e)) aimed = true;
        });
    }
    // ---- up (or coming up), in danger: down at once, and away
    // (brought up on purpose, or up to torpedo a ship: it stands and fights until a warhead comes or half its hull is gone)
    const bool fighting = e.subStayUp || e.subHunt;
    const bool serious = hot || e.hp < e.maxHp * (e.subHunt ? 0.6f : 0.5f);
    const bool danger = hot || hurt || aimed;
    const bool react = danger && (!fighting || serious);
    if ((e.subState == 0 || e.subState == 3) && react) {
        if (hot && e.nukeSalvo > 0 && e.subState == 3) {   // a warhead is coming down where it is surfacing to launch: it keeps its missile for later
            e.nukeSalvo = 0;
            emit(EV_MSG, e.owner, SND_NONE, e.pos, "Submarine aborted its launch: a warhead is coming down on it");
        }
        bool mayDive = e.nukeSalvo == 0 && (time >= e.exposedUntil || hot);
        if (mayDive) {
            bool wasUp = e.subState == 0;
            subGoDown(*this, e, true);
            if (wasUp && !players[e.owner].isAI) emit(EV_MSG, e.owner, SND_NONE, e.pos, hot ? "Submarine crash-diving: warhead incoming" : "Submarine under fire: crash-diving");
        }
    }
    if ((react || (e.subState == 1 && !threats.empty() && !fighting)) && e.subState != 2 && time >= e.subEvadeAt && e.nukeSalvo == 0 && e.disabledUntil <= time) {
        e.subEvadeAt = time + 2.5f;
        Vec2 safe;
        if ((!threats.empty() || hot) && subSafeSpot(e, threats, hot, safe)) {
            std::vector<Vec2> path;
            if (g_map.findNavalPath(e.pos, safe, path, 6000) && !path.empty()) {
                e.order = O_MOVE; e.postOrder = O_IDLE; e.target = safe; e.targetEnt = NOREF; e.engaged = NOREF; e.actionTimer = 0;
                e.path = path; e.pathIdx = 0; e.repathTimer = 2.0f; e.stuckTimer = 0;
            }
        }
    }
    // ---- up with nothing to do: down again after a moment (a fresh boat out of the yard, a hunt that is over, a launch it has finished)
    if (e.subState == 0 && !e.subStayUp && e.nukeSalvo == 0 && !e.subAutoDive && time >= e.exposedUntil && e.order != O_ATTACK) {
        if (e.subUpSince < 0) e.subUpSince = time;
        else if (time - e.subUpSince >= SUB_IDLE_DIVE) subGoDown(*this, e, false);
    } else e.subUpSince = -1;
}

// ------------------------------------------------------------ battleships and the drone swarms
void Sim::updateWarship(Entity& e) {
    // a battleship's cruise-missile cells: a salvo of W_VLS at the best target on the ground or the sea in VLS_RANGE, on their own reload
    if (battleshipType(e.type) && time >= e.vlsCd && (tick + e.gen) % 5 == 0) {
        const Weapon& w = WEAPONS[W_VLS];
        Entity* t = get(e.engaged);
        if (!t || t->isAir() || !canTarget(e, *t, W_VLS) || distToEntity(e.pos, *t) > w.range * TILE || distToEntity(e.pos, *t) < w.minRange * TILE) t = acquireTarget(e, w.range, W_VLS);
        if (t && !t->isAir() && distToEntity(e.pos, *t) >= w.minRange * TILE) {
            Vec2 fw(std::cos(e.angle), std::sin(e.angle)), sd(-fw.y, fw.x);
            for (int k = 0; k < w.burst; k++) {   // out of the two blocks of cells, fore and aft of the bridge
                float along = (k < w.burst / 2 ? 0.27f : -0.08f) * e.radius() * 2.2f, across = ((k % 3) - 1) * 5.0f;
                Vec2 m = e.pos + fw * along + sd * across;
                fireWeapon(e, *t, w, &m);
            }
            e.vlsCd = time + w.cooldown;
        } else e.vlsCd = time + 1.0f;
    }
    // the swarm stings enemy aircraft that come near the ship: every ready drone gets a burst in, twice a second
    if (time >= e.swarmCd && (tick + e.gen) % 4 == 0) {
        int n = droneCount(e);
        if (n <= 0) return;
        e.swarmCd = time + WEAPONS[W_DRONE_ZAP].cooldown;
        const float reach = (SWARM_REACH + 1.5f) * TILE + e.radius();
        Entity* best = nullptr; float bd = 1e30f;
        forEachNearMask(e.pos, reach, enemyMask(e.owner), [&](Entity& t) {
            if (!t.isAir() || !enemies(e.owner, t.owner) || !visibleTo(t, e.owner) || t.fall > 0 || t.fade < 0.5f) return;
            float d = dist2(t.pos, e.pos);
            if (d < bd) { bd = d; best = &t; }
        });
        if (!best) return;
        const Weapon& zap = WEAPONS[W_DRONE_ZAP];
        Ref self = refOf(e), tr = refOf(*best);
        for (int i = 0; i < n; i++) {
            Entity* t = get(tr);
            if (!t) break;
            Vec2 dp = dronePos(e, i);
            if (dist(dp, t->pos) > SWARM_REACH * TILE + t->radius()) continue;
            fx.push_back({FX_BEAM, dp, t->pos, 0, 0.07f, zap.color, 1});
            applyDamage(*t, zap.dmg, e.owner, self, &zap);
        }
    }
}
