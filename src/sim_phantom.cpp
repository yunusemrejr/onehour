#include "sim.h"

// ------------------------------------------------------------ The Phantom Jeep
// One dark jeep per army at a time (a factory with a tech structure builds it), driven by hand. Nothing finds it but an aircraft close above it or a
// sniper's scope (see updateDetection), and no sniper can ever shoot it (canTarget). Ground weapons do PHANTOM_ARMOR of their damage, aircraft
// do more the more of them are on it, a warhead or fallout leaves it at PHANTOM_NUKE_FLOOR of its health at the least (phantomHit, from
// applyDamage). It mends itself, shoots soldiers with a rotary gun and aircraft, ships and vehicles with a missile mount, plants TNT on enemy
// structures, and drives over rock, trees, water and ordinary walls (Map::climbPassable: only Bastion Walls and other structures stop it).

Ref Sim::phantomOf(int player) const {
    for (auto& e : ents) if (e.alive && e.isUnit() && e.owner == player && phantomType(e.type)) return Ref{ (i32)(&e - &ents[0]), e.gen };
    return NOREF;
}

bool Sim::phantomInProduction(int player) const {
    for (auto& e : ents) {
        if (!e.alive || e.owner != player) continue;
        if (e.isUnit() && phantomType(e.type)) return true;
        if (e.isBuilding()) for (int t : e.queue) if (phantomType(t)) return true;
    }
    for (auto& a : airlifts) for (size_t k = a.next; k < a.load.size(); k++) if (a.owner == player && phantomType(a.load[k])) return true;   // (on its way down from a transport)
    return false;
}

bool Sim::phantomHit(Entity& p, const Entity* att, float& real, bool world) {
    if (world) return true;   // a warhead's blast and the fallout: all of it, but never to death
    float mult = PHANTOM_ARMOR;
    if (att && att->isUnit() && att->isAir()) {   // an aircraft (a drone, or the plane that dropped a bomb): the more of them on it within the window, the harder each one hits
        const Ref ar = refOf(*att);
        int n = 0, slot = -1, stalest = 0;
        for (int i = 0; i < PHANTOM_AIR_SLOTS; i++) {
            const bool used = p.airHit[i].idx >= 0, live = used && time - p.airHitAt[i] <= PHANTOM_AIR_WINDOW;
            if (live) { n++; if (p.airHit[i].idx == ar.idx && p.airHit[i].gen == ar.gen) slot = i; }
            if (!used ? p.airHit[stalest].idx >= 0 : (p.airHit[stalest].idx >= 0 && p.airHitAt[i] < p.airHitAt[stalest])) stalest = i;   // (an empty slot first)
        }
        if (slot < 0) { slot = stalest; n++; }   // (a new aircraft takes the stalest slot: a dead one, or the one hit longest ago)
        p.airHit[slot] = ar; p.airHitAt[slot] = time;
        mult = std::min(PHANTOM_AIR_CAP, PHANTOM_AIR_BASE + PHANTOM_AIR_STEP * (float)(n - 1));
    }
    real *= mult;
    return false;
}

void Sim::updatePhantom(Entity& e) {
    // self-repair: slow under fire, quicker once it has been left alone
    const float rate = time - e.lastDamaged >= PHANTOM_CALM ? PHANTOM_REGEN : PHANTOM_REGEN_HOT;
    e.hp = std::min(e.maxHp, e.hp + e.maxHp * rate * SIM_DT);
    // TNT: one charge back every PHANTOM_TNT_RECHARGE seconds while it carries fewer than PHANTOM_TNT_MAX
    if (e.tntLeft >= PHANTOM_TNT_MAX) e.tntNext = 0;
    else if (e.tntNext <= 0) e.tntNext = time + PHANTOM_TNT_RECHARGE;
    else if (time >= e.tntNext) { e.tntLeft++; e.tntNext = e.tntLeft < PHANTOM_TNT_MAX ? time + PHANTOM_TNT_RECHARGE : 0; }
    if (e.disabledUntil > time || e.fall > 0 || e.carrier.valid()) return;
    // the missile mount fires on its own: aircraft first, then ships (a surfaced submarine too), then vehicles
    const Weapon& wp = WEAPONS[W_PHANTOM_MISSILE];
    if (e.gunCd[0] > 0) e.gunCd[0] -= SIM_DT;
    if (e.gunCd[0] > 0 || (tick + e.gen) % 4 != 0) return;
    Entity* t = get(e.gunTgt[0]);
    if (!t || !canTarget(e, *t, W_PHANTOM_MISSILE) || distToEntity(e.pos, *t) > wp.range * TILE) {
        t = nullptr; float best = 1e9f;
        forEachNearMask(e.pos, (wp.range + 1.0f) * TILE, enemyMask(e.owner), [&](Entity& c) {
            if (!c.isUnit() || !enemies(e.owner, c.owner) || !canTarget(e, c, W_PHANTOM_MISSILE)) return;
            const float d = distToEntity(e.pos, c);
            if (d > wp.range * TILE) return;
            const int tier = c.isAir() ? 0 : c.ut().naval ? 1 : c.ut().kind == UK_VEH ? 2 : 3;   // (soldiers are the rotary gun's: the missile leaves them be)
            if (tier == 3) return;
            const float score = tier * 10000.0f + d;
            if (score < best) { best = score; t = &c; }
        });
    }
    if (!t) { e.gunTgt[0] = NOREF; return; }
    e.gunTgt[0] = refOf(*t);
    const Vec2 dir = (t->pos - e.pos).norm();
    e.gunAng[0] = std::atan2(dir.y, dir.x);
    const Vec2 muzzle = e.pos + dir * (e.radius() * 0.9f);
    fireWeapon(e, *t, wp, &muzzle);
    e.gunCd[0] = wp.cooldown;
}

// The rotary gun: every volley is spread over up to PHANTOM_GUN_TARGETS soldiers (snipers and other hidden ones first), fired together
bool Sim::phantomVolley(Entity& e, Entity& tgt) {
    const Weapon& wp = WEAPONS[W_PHANTOM_GUN];
    if (distToEntity(e.pos, tgt) / TILE > wp.range + 0.15f) return false;
    e.turret = std::atan2(tgt.pos.y - e.pos.y, tgt.pos.x - e.pos.x);
    if (e.cooldown > 0) return true;
    Entity* pick[PHANTOM_GUN_TARGETS] = { &tgt, nullptr, nullptr };
    int n = 1;
    if (phantomSeekable(tgt)) {
        std::vector<std::pair<float, Entity*>> others;
        forEachNearMask(e.pos, (wp.range + 0.5f) * TILE, enemyMask(e.owner), [&](Entity& c) {
            if (&c == &tgt || !phantomSeekable(c) || !enemies(e.owner, c.owner) || !canTarget(e, c, W_PHANTOM_GUN)) return;
            const float d = distToEntity(e.pos, c);
            if (d > wp.range * TILE) return;
            others.push_back({ ((c.ut().sniper || stealthOf(c)) ? 0.0f : 100000.0f) + d, &c });
        });
        std::sort(others.begin(), others.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        for (size_t i = 0; i < others.size() && n < PHANTOM_GUN_TARGETS; i++) pick[n++] = others[i].second;
    }
    for (int k = 0; k < wp.burst; k++) fireWeapon(e, *pick[k % n], wp);
    e.cooldown = wp.cooldown;
    return true;
}

// Idle: it holds its post and fires at what is in range; it steps up to PHANTOM_LEASH tiles off the post to reach a target, never farther, and goes back
void Sim::phantomIdle(Entity& e) {
    const Weapon& wp = WEAPONS[W_PHANTOM_GUN];
    Entity* t = get(e.engaged);
    if (!t || !canTarget(e, *t) || distToEntity(e.pos, *t) > (wp.range + 0.3f) * TILE) {
        t = nullptr; e.engaged = NOREF;
        if ((tick + e.gen) % 5 == 0) { t = acquireTarget(e, wp.range + 0.3f); if (t) e.engaged = refOf(*t); }
    }
    if (t) { tryFire(e, *t); e.path.clear(); e.pathIdx = 0; return; }
    if ((tick + e.gen) % 5 == 0 && e.pathIdx >= e.path.size()) {
        const float leash = PHANTOM_LEASH * TILE;
        Entity* far = acquireTarget(e, wp.range + PHANTOM_LEASH);
        Vec2 want = e.guardPos;
        if (far) {   // a spot within gun range of it, never more than the leash from the post
            want = far->pos + (e.pos - far->pos).norm() * std::max(0.0f, (wp.range - 0.7f) * TILE + far->radius());
            if (dist(want, e.guardPos) > leash) want = e.guardPos + (want - e.guardPos).norm() * leash;
        }
        if (dist(want, e.pos) > 0.4f * TILE && walkable(e, tileOf(want.x), tileOf(want.y))) { e.path.assign(1, want); e.pathIdx = 0; }
    }
    if (e.pathIdx < e.path.size()) moveAlong(e, e.ut().speed);
}

// ---- TNT
bool Sim::plantable(int player, const Entity& site) const {
    return site.isBuilding() && site.constructed && site.owner >= 0 && site.owner != player && enemies(player, site.owner) && !invulnerable(site)
        && site.bt().role != BR_FWALL && visibleTo(site, player);
}

// O_PLANT: drive to the footprint, plant (PHANTOM_TNT_PLANT s within a tile), then it is free again; the charge blows PHANTOM_TNT_FUSE s later
void Sim::plantRun(Entity& e) {
    Entity* b = get(e.targetEnt);
    if (!b || e.tntLeft == 0 || !plantable(e.owner, *b)) {   // the structure is gone or no longer a target, or the charges are spent: the guns take over
        const bool gun = b && b->alive && canTarget(e, *b);
        e.order = gun ? O_ATTACK : O_IDLE; e.postOrder = O_IDLE; e.actionTimer = 0; e.path.clear(); e.pathIdx = 0;
        if (!gun) { e.targetEnt = NOREF; e.guardPos = e.pos; }
        return;
    }
    if (distToEntity(e.pos, *b) <= TILE) {
        e.path.clear(); e.pathIdx = 0; e.airspeed = 0;
        e.turret = std::atan2(b->pos.y - e.pos.y, b->pos.x - e.pos.x);
        e.actionTimer += SIM_DT;
        if (e.actionTimer < PHANTOM_TNT_PLANT) return;
        tnts.push_back({ refOf(*b), refOf(e), e.owner, time + PHANTOM_TNT_FUSE });
        e.tntLeft--; if (e.tntNext <= 0) e.tntNext = time + PHANTOM_TNT_RECHARGE;
        for (int q = 0; q < numPlayers; q++) if (players[q].team == players[b->owner].team) e.detUntil[q] = std::max(e.detUntil[q], time + PHANTOM_TNT_REVEAL);   // the planting shows it to the structure's army
        emit(EV_UNDER_ATTACK, b->owner, SND_ATTACKED, b->pos, "Explosives planted on our structure");
        e.order = O_IDLE; e.targetEnt = NOREF; e.actionTimer = 0; e.guardPos = e.pos;
        return;
    }
    e.actionTimer = 0;
    if (e.pathIdx >= e.path.size() || e.repathTimer <= 0) requestSidePath(e, *b);
    moveAlong(e, e.ut().speed);
}

void Sim::updateTnt() {
    for (size_t i = 0; i < tnts.size();) {
        TntCharge c = tnts[i];
        Entity* b = get(c.site);
        if (!b) { tnts.erase(tnts.begin() + i); continue; }   // the structure fell first: the charge goes with it
        if (time < c.blowAt) { i++; continue; }
        tnts.erase(tnts.begin() + i);
        const float dmg = PHANTOM_TNT_DAMAGE + PHANTOM_TNT_SHARE * b->maxHp;
        const float r = std::max(b->bt().w, b->bt().h) * TILE * 0.5f;
        fx.push_back({FX_EXPLODE, b->pos, b->pos, 0, 1.0f, rgb(255, 196, 96), r * 1.6f});
        fx.push_back({FX_EXPLODE, b->pos + Vec2(r * 0.4f, -r * 0.2f), b->pos, -0.15f, 0.8f, rgb(255, 150, 60), r});
        emit(EV_SOUND, c.owner, SND_EXPLODE_L, b->pos);
        applyDamage(*b, dmg, c.owner, c.by, nullptr);
    }
}
