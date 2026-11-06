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
            e->subAutoDive = false;
            e->engaged = NOREF; if (e->order == O_ATTACK) { e->order = O_IDLE; e->targetEnt = NOREF; }   // (a dived submarine fires at nothing)
        } else {
            if (e->subState == 2) { e->subState = 3; e->subT = time + SUB_SURFACE_TIME; }
            else if (e->subState == 1) { e->subState = 3; e->subT = time + SUB_SURFACE_TIME * 0.5f; }
            e->subAutoDive = false;
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
        if (e.order == O_ATTACK) { e.order = O_IDLE; e.targetEnt = NOREF; e.guardPos = e.pos; }
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
