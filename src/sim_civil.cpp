// One Hour - the towns and cities round the lake, and the riots (see DISTRICT_* and RIOT_* in data.h)
#include "sim.h"

// ------------------------------------------------------------ the civilian areas
// The map lays them out (Map::districts); here every block becomes a civic building of nobody's, tied to its area by Entity.district. The cities use
// the glass-and-steel civic designs, the towns the old brick ones. A block is tougher than the same building in a player's own town.
void Sim::spawnDistricts() {
    districts.clear();
    for (size_t d = 0; d < g_map.districts.size() && (int)d < DISTRICT_MAX; d++) {
        const DistrictSpot& s = g_map.districts[d];
        District D; D.name = s.name; D.city = s.city;
        Vec2 sum; int n = 0;
        for (auto& b : s.blocks) {
            int type = (s.city ? B_C_HOUSE : B_K_HOUSE) + b.kind;
            Ref r = placeBuilding(type, -1, b.tx, b.ty, true);
            Entity* e = get(r);
            if (!e) continue;
            e->district = (int)d; e->constructed = true; e->progress = 1.0f;
            e->maxHp = e->hp = BUILDS[type].hp * DISTRICT_HP;
            D.blocks.push_back(r); sum += e->pos; n++;
            D.pay += (int)(CIVICS[b.kind].income * DISTRICT_PAY_SHARE);
            D.people += CIVICS[b.kind].people;
        }
        D.center = n ? sum * (1.0f / n) : Vec2(s.cx * TILE, s.cy * TILE);
        for (Ref r : D.blocks) if (const Entity* e = get(r)) D.radius = std::max(D.radius, dist(e->pos, D.center) + e->radius());
        D.payAt = DISTRICT_PAY_INTERVAL;
        districts.push_back(D);
    }
}

// A dozer helps where it may: its own army's sites and repairs (the right kind of dozer for each), and any block of a town round the lake that is not
// held by an enemy - its own side's, an ally's, or a neutral one's (raising a town before taking it is a fair bet). Any kind of dozer works on a town.
bool Sim::dozerMayWork(const Entity& dz, const Entity& b) const {
    if (!b.isBuilding() || !dz.isUnit() || dz.ut().role != UR_DOZER) return false;
    if (b.district >= 0) return b.owner < 0 || b.owner == dz.owner || !enemies(dz.owner, b.owner);
    if (b.owner != dz.owner) return false;
    return b.constructed ? canMend(dz, b.type) : canRaise(dz, b.type);
}

// A block looks after itself: once the shooting has stopped its people patch it up, and a razed block is cleared and raised again from its ruins.
// A dozer working on it (O_BUILD) adds its own build rate on top, several times as fast.
void Sim::updateDistrictBlock(Entity& b) {
    if (!b.constructed) {
        if (time - b.lastDamaged < DISTRICT_REBUILD_DELAY || inFallout(b.pos)) return;
        b.progress += SIM_DT / DISTRICT_REBUILD;
        b.hp = std::max(b.hp, b.maxHp * (0.1f + 0.9f * clampf(b.progress, 0, 1)));
        if ((tick + b.gen) % 12 == 0) fx.push_back({FX_SPARK, b.pos + Vec2(rng.f(-b.bt().w * 13.0f, b.bt().w * 13.0f), rng.f(-b.bt().h * 13.0f, b.bt().h * 13.0f)), Vec2(), 0, 0.25f, rgb(255, 226, 150), 3});
        if (b.progress >= 1.0f) finishBuilding(b);
        return;
    }
    if (b.hp < b.maxHp && time - b.lastDamaged >= DISTRICT_MEND_DELAY && !inFallout(b.pos))
        b.hp = std::min(b.maxHp, b.hp + b.maxHp * DISTRICT_MEND * SIM_DT);
}

// A block that is razed does not vanish: it falls in (the same fire, smoke and rubble as any structure) and stands as a ruin that its people
// start to raise again once the shooting stops. A ruin is nothing to shoot at until it has been raised a way (see canTarget).
void Sim::collapseBlock(Entity& b, int attackerOwner) {
    deathFx(b);
    b.constructed = false; b.progress = 0; b.hp = b.maxHp * 0.1f; b.lastDamaged = time; b.dmgLoad = 0;
    b.engaged = NOREF; b.engaged2 = NOREF;
    District& D = districts[b.district];
    if (b.owner >= 0 && attackerOwner >= 0 && enemies(attackerOwner, b.owner)) { players[attackerOwner].structuresKilled++; players[b.owner].structuresLost++; }
    if (b.owner >= 0) {
        char msg[112]; snprintf(msg, sizeof msg, "%s: a %s was razed (its people will rebuild it)", D.name, b.bt().name);
        emit(EV_MSG, b.owner, SND_NONE, b.pos, msg);
    }
}

// The whole area changes hands at once: every block, ruins included. 'to' -1 frees it (its army is gone).
void Sim::captureDistrict(int d, int to) {
    if (d < 0 || d >= (int)districts.size()) return;
    District& D = districts[d];
    int from = D.owner;
    if (from == to) return;
    D.owner = to; D.takenAt = time; D.payAt = time + DISTRICT_PAY_INTERVAL;
    Color fc = to < 0 ? rgb(230, 224, 200) : (players[to].faction == F_CYBER ? rgb(120, 236, 255) : rgb(255, 214, 120));
    for (Ref r : D.blocks) {
        Entity* b = get(r);
        if (!b) continue;
        b->owner = to; b->attacker = NOREF; b->engaged = NOREF; b->engaged2 = NOREF; b->forceTarget = NOREF; b->visKey = -1;
        for (int q = 0; q < MAX_PLAYERS; q++) b->detUntil[q] = 0;
        if (to >= 0) {
            for (int k = 0; k < 4; k++) fx.push_back({FX_SPARK, b->pos + Vec2(rng.f(-b->radius(), b->radius()), rng.f(-b->radius(), b->radius())), Vec2(), -rng.f(0, 0.4f), 0.5f, fc, 5});
            fx.push_back({FX_RING, b->pos, b->pos, 0, 0.9f, fc, b->radius() * 1.2f});
        }
    }
    char msg[112];
    if (to >= 0) {
        snprintf(msg, sizeof msg, "We took %s: the %s pays us now (enemies will come for it)", D.name, D.city ? "city" : "town");
        emit(EV_MSG, to, SND_NONE, D.center, msg);
        emit(EV_SOUND, to, SND_BUILD_DONE, D.center);
        for (int q = 0; q < numPlayers; q++) if (q != to && players[q].team == players[to].team) { snprintf(msg, sizeof msg, "Our ally took %s", D.name); emit(EV_MSG, q, SND_NONE, D.center, msg); }
    }
    if (from >= 0 && to >= 0) {
        snprintf(msg, sizeof msg, enemies(from, to) ? "An enemy infiltrator took %s" : "Our ally took over %s", D.name);
        emit(enemies(from, to) ? EV_UNDER_ATTACK : EV_MSG, from, enemies(from, to) ? SND_ATTACKED : SND_NONE, D.center, msg);
    }
    updatePower();
}

// Twice a second: how intact every area is, what it pays, how many people are about.
void Sim::updateDistricts() {
    for (auto& D : districts) {
        float hp = 0, mx = 0, people = 0; int ruins = 0;
        for (Ref r : D.blocks) {
            const Entity* b = get(r);
            if (!b) continue;
            hp += b->hp; mx += b->maxHp;
            int k = civicKind(b->type);
            if (!b->constructed) { ruins++; people += 0.4f * b->progress * CIVICS[k].people; }   // the ones raising it again
            else people += CIVICS[k].people * (0.5f + 0.5f * b->hp / b->maxHp);
        }
        D.integrity = mx > 0 ? hp / mx : 0;
        D.ruins = ruins;
        D.people = (int)(people + 0.5f);
        if (D.owner < 0 || !players[D.owner].alive) { D.payAt = time + DISTRICT_PAY_INTERVAL; continue; }
        if (time < D.payAt) continue;
        D.payAt += DISTRICT_PAY_INTERVAL;
        if (D.integrity < DISTRICT_PAY_MIN) continue;   // too badly damaged to pay until it is rebuilt
        Player& pl = players[D.owner];
        int amt = (int)(D.pay * D.integrity * pl.econMul());
        pl.money += amt; pl.mined += amt;
        fx.push_back({FX_SPARK, D.center, D.center, 0, 0.7f, rgb(255, 224, 90), 9});
    }
}

// ------------------------------------------------------------ riots
int Sim::riotersAlive() const {
    int n = 0;
    for (auto& e : ents) if (e.alive && e.owner == RIOT_PLAYER && e.isUnit()) n++;
    return n;
}

float Sim::armyControl(int p) const { return p >= 0 && p < MAX_PLAYERS ? control[p] : 1.0f; }

// Where a crowd gathers against an army: open ground a little way out from one of its structures (its economy most of all), as far as it can get
// from the army's guns and troops, and on ground from which that structure can be reached.
bool Sim::riotSpawnSpot(int victim, Vec2& out) {
    std::vector<const Entity*> mine, guns;
    for (auto& e : ents) {
        if (!e.alive || e.owner != victim || e.district >= 0) continue;
        if (e.isBuilding() && e.constructed && !invulnerable(e)) { mine.push_back(&e); if (e.bt().weapon >= 0) guns.push_back(&e); }
        else if (e.isUnit() && e.ut().role == UR_COMBAT && e.weapon() >= 0 && !e.isAir() && !e.ut().naval) guns.push_back(&e);
    }
    if (mine.empty()) return false;
    if (g_map.regionDirty) g_map.buildRegions();
    float bestScore = -1e18f; bool found = false;
    for (int tries = 0; tries < 28; tries++) {
        const Entity* b = mine[rng.range(0, (int)mine.size() - 1)];
        BuildRole role = b->bt().role;
        float lure = (role == BR_SUPPLY || role == BR_POWER || role == BR_INCOME || role == BR_CIVIC || role == BR_FACTORY || role == BR_BARRACKS) ? 6.0f : 0.0f;
        float a = rng.f(0, 6.2832f), r = rng.f(9.0f, 16.0f) * TILE;
        Vec2 p = b->pos + Vec2(std::cos(a), std::sin(a)) * r;
        int tx = tileOf(p.x), ty = tileOf(p.y);
        if (tx < 2 || ty < 2 || tx > MAP_W - 3 || ty > MAP_H - 3 || !g_map.passable(tx, ty) || inFallout(p, TILE)) continue;
        Vec2 door = g_map.nearestFree(b->pos + (p - b->pos).norm() * (b->radius() + TILE), 6);
        int dx = tileOf(door.x), dy = tileOf(door.y);
        if (!inMap(dx, dy) || g_map.region[ty * MAP_W + tx] == 0 || g_map.region[ty * MAP_W + tx] != g_map.region[dy * MAP_W + dx]) continue;   // (it must be able to walk to it)
        float nearest = 1e9f;
        for (const Entity* g : guns) nearest = std::min(nearest, distToEntity(p, *g) - (g->weapon() >= 0 ? WEAPONS[g->weapon()].range * TILE : 0.0f));
        float score = std::min(nearest, 10.0f * TILE) / TILE + lure + rng.f(0, 2.0f);
        if (score > bestScore) { bestScore = score; out = p; found = true; }
    }
    return found;
}

// The crowd's next mark: the victim's structure that is worth wrecking and guarded least (its economy and power first, never into a nest of guns
// if anything softer is near), or - for a crowd marching on a town - the nearest standing block of a town the victim holds.
Entity* Sim::riotPickTarget(Riot& r, Vec2 from) {
    Entity* best = nullptr; float bs = 1e18f;
    for (auto& e : ents) {
        if (!e.alive || !e.isBuilding() || e.owner != r.victim || invulnerable(e)) continue;
        if (r.city != (e.district >= 0)) continue;
        if (districtRuin(e) && e.progress < DISTRICT_RUIN_SAFE) continue;
        if (Sim::hardened(e) && e.hatch <= 0 && e.hp <= e.maxHp * (BUNKER_FLOOR + BUNKER_FLOOR_MARGIN) + 1.0f) continue;
        float s = dist(e.pos, from) / TILE;
        if (!r.city) {
            BuildRole role = e.bt().role;
            if (role == BR_SUPPLY || role == BR_POWER || role == BR_INCOME || role == BR_CIVIC) s -= 10;
            else if (role == BR_FACTORY || role == BR_BARRACKS || role == BR_AIRFIELD || role == BR_TECH || role == BR_NAVAL) s -= 6;
            else if (role == BR_HQ) s += 4;
            else if (role == BR_TURRET || role == BR_AATURRET) s += 10;
            else if (wallRole(role)) s += 16;
            if (!e.constructed) s -= 3;   // a site is soft
            int guns = 0;
            forEachNear(e.pos, 7.0f * TILE, [&](Entity& g) { if (g.owner == r.victim && g.weapon() >= 0 && (g.isBuilding() ? g.constructed : g.ut().role == UR_COMBAT)) guns++; });
            s += guns * 4.0f;
        }
        if (s < bs) { bs = s; best = &e; }
    }
    return best;
}

// Once a second. Every army's grip on its own people (control[]) follows how it is doing: its strength against its enemies, how much of its base
// still stands against the best it had lately, and what it lost in the last minute or two; the towns it holds round the lake steady it. Unrest
// fills at a rate between RIOT_CALM_RATE (in full control) and RIOT_ANGRY_RATE (none left), and each time it is full a crowd rises - bigger the
// less control there is. The crowds themselves are steered here too: they wreck one structure after another and melt away after a while.
void Sim::updateRiots() {
    const float dt = 1.0f;
    float S[MAX_PLAYERS] = {}, baseNow[MAX_PLAYERS] = {};
    int structs[MAX_PLAYERS] = {};
    for (auto& e : ents) {
        if (!e.alive || e.owner < 0 || e.owner >= numPlayers || e.district >= 0 || e.carrier.valid()) continue;
        float hf = e.maxHp > 0 ? clampf(e.hp / e.maxHp, 0, 1) : 0;
        if (e.isUnit()) { const UnitType& ut = e.ut(); if (ut.role == UR_COMBAT && !ut.temp && ut.weapon >= 0) S[e.owner] += ut.cost * (0.5f + 0.5f * hf); }
        else if (e.isBuilding() && e.constructed && !invulnerable(e)) {
            const BuildType& bt = e.bt();
            baseNow[e.owner] += std::max(bt.cost, 300) * hf; structs[e.owner]++;
            if (bt.weapon >= 0) S[e.owner] += bt.cost * 0.6f * hf;
        }
    }
    for (int p = 0; p < numPlayers; p++) {
        Player& pl = players[p];
        lossHeat[p] *= 0.9923f;   // (a half-life of 90 s at one call a second)
        basePeak[p] = std::max(baseNow[p], basePeak[p] * (1.0f - 0.0025f * dt));
        if (!pl.alive) { control[p] = 1; unrest[p] = 0; continue; }
        float foes = 0; int nf = 0;
        for (int q = 0; q < numPlayers; q++) if (players[q].alive && enemies(p, q)) { foes += S[q]; nf++; }
        float ratio = nf ? (S[p] + 2500.0f) / (foes / nf + 2500.0f) : 2.0f;   // (the constant keeps two small armies at the start of a match even)
        float weak = clampf((1.0f - ratio) / 0.8f, 0, 1);
        float damage = basePeak[p] > 0 ? clampf((1.0f - baseNow[p] / basePeak[p]) * 1.6f, 0, 1) : 0;
        float losses = clampf(lossHeat[p] / (S[p] + 4000.0f), 0, 1);
        float c = 1.0f - (0.45f * weak + 0.35f * damage + 0.20f * losses);
        for (auto& D : districts) if (D.owner == p) c += D.integrity >= DISTRICT_PAY_MIN ? 0.04f : -0.03f;   // a thriving town it holds steadies its people
        control[p] = clampf(c, 0, 1);
        if (!riotsOn || time < RIOT_GRACE || structs[p] == 0) continue;
        float anger = std::pow(1.0f - control[p], 1.5f);
        unrest[p] += dt * (RIOT_CALM_RATE + (RIOT_ANGRY_RATE - RIOT_CALM_RATE) * anger);
        if (unrest[p] < 1.0f) continue;
        int crowds = 0; for (auto& r : riots) if (r.victim == p) crowds++;
        int alive = riotersAlive();
        if (crowds >= 3 || alive >= RIOT_CAP) { unrest[p] = 0.85f; continue; }   // (enough trouble on the streets already: it waits)
        unrest[p] -= 1.0f;
        // a crowd rises: against the base, or - now and then, when the army holds a town - marching on that town
        Riot R; R.id = ++riotSeq; R.victim = p; R.until = time + RIOT_LIFE;
        std::vector<int> held;
        for (int d = 0; d < (int)districts.size(); d++) if (districts[d].owner == p && districts[d].integrity > 0.3f) held.push_back(d);
        Vec2 at;
        bool ok = false;
        if (!held.empty() && rng.f() < 0.35f) {
            const District& D = districts[held[rng.range(0, (int)held.size() - 1)]];
            for (int k = 0; k < 16 && !ok; k++) {
                float a = rng.f(0, 6.2832f);
                Vec2 c2 = D.center + Vec2(std::cos(a), std::sin(a)) * (D.radius + rng.f(3.0f, 7.0f) * TILE);
                int tx = tileOf(c2.x), ty = tileOf(c2.y);
                if (inMap(tx, ty) && g_map.passable(tx, ty) && !inFallout(c2, TILE)) { at = c2; ok = true; R.city = true; }
            }
        }
        if (!ok) ok = riotSpawnSpot(p, at);
        if (!ok) continue;
        R.origin = at;
        int n = RIOT_MIN_GROUP + (int)std::lround((RIOT_MAX_GROUP - RIOT_MIN_GROUP) * std::pow(1.0f - control[p], 1.2f)) + rng.range(-1, 1);
        n = clampi(std::min(n, RIOT_CAP - alive), 1, RIOT_MAX_GROUP);
        float fireShare = rng.f(0.25f, 0.45f);
        for (int k = 0; k < n; k++) {
            int type = (k > 0 && rng.f() < fireShare) ? U_FIREBOMBER : U_RIOTER;
            Ref u = spawnUnit(type, RIOT_PLAYER, at + Vec2(rng.f(-1.5f, 1.5f), rng.f(-1.5f, 1.5f)) * TILE);
            if (Entity* e = get(u)) { e->maxHp = e->hp = UNITS[type].hp * RIOT_HP; e->riot = R.id; R.members.push_back(u); }
        }
        riotsRaised[p]++;
        riots.push_back(R);
        if (time - riotNoteAt[p] > 20.0f) {
            riotNoteAt[p] = time;
            char msg[112];
            if (R.city) snprintf(msg, sizeof msg, "Riot! %d armed civilians are marching on %s", n, districts[held.empty() ? 0 : held[0]].name);
            else snprintf(msg, sizeof msg, "Riot! %d armed civilians are rising against us near the base", n);
            emit(EV_UNDER_ATTACK, p, SND_ATTACKED, at, msg);
            for (int q = 0; q < numPlayers; q++) if (q != p && !players[q].isAI && players[q].team == pl.team) emit(EV_MSG, q, SND_NONE, at, "Riots are breaking out against our ally");
        }
    }
    // steer the crowds
    for (size_t i = 0; i < riots.size();) {
        Riot& R = riots[i];
        R.members.erase(std::remove_if(R.members.begin(), R.members.end(), [&](Ref m) { return get(m) == nullptr; }), R.members.end());
        if (R.members.empty()) { riots.erase(riots.begin() + i); continue; }
        Vec2 cen; for (Ref m : R.members) cen += get(m)->pos;
        cen = cen * (1.0f / R.members.size());
        bool victimGone = R.victim < 0 || !players[R.victim].alive;
        if (time >= R.until || victimGone) {   // the survivors melt back into the streets (whoever is still fighting finishes first)
            for (size_t k = 0; k < R.members.size();) {
                Entity* e = get(R.members[k]);
                if (time - e->lastDamaged > 6.0f || victimGone || time > R.until + 60.0f) {
                    fx.push_back({FX_SMOKE, e->pos, e->pos, 0, 1.2f, rgb(120, 116, 108), 8, Vec2(0, -14)});
                    destroy(*e, false);
                    R.members.erase(R.members.begin() + k);
                } else k++;
            }
            if (R.members.empty()) { riots.erase(riots.begin() + i); continue; }
            i++; continue;
        }
        Entity* t = get(R.target);
        bool stale = !t || t->owner != R.victim || (districtRuin(*t) && t->progress < DISTRICT_RUIN_SAFE);
        if (stale) {
            t = riotPickTarget(R, cen);
            R.target = t ? refOf(*t) : NOREF;
            R.nextOrder = 0;
            if (!t && R.city) { R.city = false; t = riotPickTarget(R, cen); R.target = t ? refOf(*t) : NOREF; }   // the town fell to somebody else: back to the base
            if (!t) { R.until = std::min(R.until, time); i++; continue; }
        }
        if (time >= R.nextOrder) {
            R.nextOrder = time + 4.0f;
            std::vector<Ref> go;
            for (Ref m : R.members) { Entity* e = get(m); if (e && !(e->order == O_ATTACK && e->targetEnt == R.target) && !(e->autoTask && time - e->lastDamaged < 3.0f)) go.push_back(m); }
            if (!go.empty()) cmdAttack(go, R.target);
        }
        i++;
    }
}
