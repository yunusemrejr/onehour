// Regression and measurement checks for placement, the expanded map and combat learning.
#include "ai.h"
#include <chrono>

// Independent flood-fill oracle for wouldPlug; deliberately keeps the original two-pass rule.
static bool plugReference(const Map& m, int tx, int ty, int w, int h) {
    int x0=std::max(0,tx-6), y0=std::max(0,ty-6);
    int x1=std::min(MAP_W-1,tx+w+5), y1=std::min(MAP_H-1,ty+h+5);
    auto count=[&](bool footprint) {
        std::vector<u8> seen(MAP_W*MAP_H); std::vector<int> stack; int n=0;
        auto open=[&](int x,int y) { return x>=x0 && y>=y0 && x<=x1 && y<=y1 && m.passable(x,y) &&
            !(footprint && x>=tx && y>=ty && x<tx+w && y<ty+h); };
        for(int y=y0;y<=y1;y++) for(int x=x0;x<=x1;x++) {
            int i=y*MAP_W+x; if(seen[i] || !open(x,y)) continue;
            ++n; stack.push_back(i); seen[i]=1;
            while(!stack.empty()) {
                int at=stack.back();stack.pop_back();
                const int dx[4]={1,-1,0,0},dy[4]={0,0,1,-1};
                for(int k=0;k<4;k++) {
                    int nx=at%MAP_W+dx[k],ny=at/MAP_W+dy[k];
                    if(!open(nx,ny)) continue;
                    int j=ny*MAP_W+nx;
                    if(!seen[j]) { seen[j]=1;stack.push_back(j); }
                }
            }
        }
        return n;
    };
    return count(true)>count(false);
}

bool placementTest(u64 seed) {
    Map m; Rng r(seed);
    for(int trial=0;trial<3000;trial++) {
        // Includes map edges, airfield-sized footprints, completely open rings,
        // narrow corridors, resources, terrain and existing buildings.
        int w=r.range(1,12),h=r.range(1,10),tx=r.range(0,MAP_W-w),ty=r.range(0,MAP_H-h);
        memset(m.blocked,0,sizeof m.blocked);
        for(int y=std::max(0,ty-7);y<std::min(MAP_H,ty+h+7);y++)
            for(int x=std::max(0,tx-7);x<std::min(MAP_W,tx+w+7);x++)
                if(r.range(0,99)<trial%55) m.blocked[y*MAP_W+x]=(u8)(1<<r.range(0,2));
        if(m.wouldPlug(tx,ty,w,h)!=plugReference(m,tx,ty,w,h)) {
            fprintf(stderr,"placementtest: mismatch at trial %d (%d,%d %dx%d)\n",trial,tx,ty,w,h); return false;
        }
    }
    printf("placementtest: 3000 obstacle/footprint cases match the flood-fill oracle\n");return true;
}

bool placementBench(u64 seed) {
    Faction f[4]={F_CYBER,F_CLANKER,F_CYBER,F_CLANKER}; bool a[4]={true,true,true,true};
    int d[4]={2,2,2,2},t[4]={0,1,2,3};g_sim.init(4,f,a,d,t,seed);g_ai.init(seed);
    for(auto& p:g_sim.players) { p.money=1000000;std::fill(p.explored.begin(),p.explored.end(),1); }
    auto start=std::chrono::steady_clock::now();u64 sum=0;int found=0;
    for(int i=0;i<240;i++) {
        int p=i%4,x=-1,y=-1;Vec2 at=g_sim.players[p].basePos+Vec2((i%13-6)*48.0f,(i%17-8)*44.0f);
        bool ok=g_ai.ais[p].findSpot(firstBuildOf(f[p])+(i%3==0?BR_AIRFIELD:BR_FACTORY),at,26,x,y);
        found+=ok;sum=sum*31+(u64)(x*131+y);
    }
    double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    printf("placementbench: %dx%d, 240 searches in %.3f ms, found %d, checksum %llu\n",MAP_W,MAP_H,ms,found,(unsigned long long)sum);
    return found==240;
}

#include "testutil.h"
#include <limits>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <csignal>

#define VERIFY11(c,m) do { if (!(c)) { fprintf(stderr,"frontiertest: %s\n",m); return false; } } while (0)

static void scene11(u64 seed, bool commander = true) {
    Faction f[2]={F_CYBER,F_CLANKER};bool ai[2]={commander,false};int d[2]={2,2},t[2]={0,1};
    g_sim.init(2,f,ai,d,t,seed);g_ai.init(seed);
    for (auto& p:g_sim.players) { p.money=0;std::fill(p.explored.begin(),p.explored.end(),1); }
}

bool frontierMapTest(u64 seed) {
    scene11(seed,false);
    VERIFY11(MAP_W==128 && MAP_H==128,"unexpected map scale");
    VERIFY11(g_map.supplies.size()==20,"supply count changed");
    VERIFY11(g_map.landmarks.size()==21,"a quadrant lost a settlement building");
    int kinds[5]={};for (auto& l:g_map.landmarks) { VERIFY11(l.kind>=0 && l.kind<5,"invalid landmark kind");kinds[l.kind]++; }
    VERIFY11(kinds[0]==4 && kinds[1]==4 && kinds[2]==5 && kinds[3]==4 && kinds[4]==4,"settlements are not distributed fairly");
    // Flood-fill real occupancy after HQs, resources and all landmarks are present.
    Vec2 start=g_map.nearestFree(g_sim.players[0].basePos+Vec2(0,6*TILE),10);
    std::vector<u8> seen(MAP_W*MAP_H);std::vector<int> q;
    int first=tileOf(start.y)*MAP_W+tileOf(start.x);seen[first]=1;q.push_back(first);
    for(size_t k=0;k<q.size();k++) {
        int at=q[k];const int dx[4]={1,-1,0,0},dy[4]={0,0,1,-1};
        for(int j=0;j<4;j++) { int x=at%MAP_W+dx[j],y=at/MAP_W+dy[j];
            if(!g_map.passable(x,y) || seen[y*MAP_W+x]) continue;
            seen[y*MAP_W+x]=1;q.push_back(y*MAP_W+x);
        }
    }
    auto reached=[&](int tx,int ty,int w,int h) {
        for(int y=ty-1;y<=ty+h;y++) for(int x=tx-1;x<=tx+w;x++) if(inMap(x,y) && seen[y*MAP_W+x]) return true;
        return false;
    };
    std::vector<Vec2> path;
    for(auto s:g_map.starts) {
        VERIFY11(reached(s.tx-2,s.ty-2,4,4),"a start has no connected staging ground");
        Vec2 to=g_map.nearestFree(tileCenter(s.tx,s.ty)+Vec2(0,6*TILE),10);
        VERIFY11(g_map.findPath(start,to,path),"default-budget pathfinding cannot cross the map");
        for(Vec2 p:path) VERIFY11(g_map.passable(tileOf(p.x),tileOf(p.y)),"a path waypoint crosses an obstacle");
    }
    for(auto s:g_map.supplies) VERIFY11(reached(s.tx,s.ty,1,1),"a supply pile is unreachable");
    for(auto& e:g_sim.ents) if(e.alive && e.isBuilding() && e.bt().role==BR_STRONGHOLD) {
        bool island=std::abs(e.tx-MAP_W/2)<=2 && std::abs(e.ty-MAP_H/2)<=2;
        VERIFY11(island || reached(e.tx,e.ty,e.bt().w,e.bt().h),"a mainland landmark cannot be entered");
        for(int y=e.ty;y<e.ty+e.bt().h;y++) for(int x=e.tx;x<e.tx+e.bt().w;x++)
            VERIFY11(g_map.terrainPassable(x,y) && !g_map.bridge[y*MAP_W+x],"a landmark overlaps terrain or a bridge");
    }
    VERIFY11(g_map.buildable(MAP_W/2+2,MAP_H/2),"the island lost its landing/building space");
    // The real commander claims a nearby cottage with four reserve infantry,
    // then leaves the garrison aboard while it surveys the field army.
    Ref cottage;
    for(auto& e:g_sim.ents) if(e.alive && e.isBuilding() && e.type==B_C_COTTAGE) { cottage=g_sim.refOf(e);break; }
    VERIFY11(cottage.valid(),"no cottage for commander test");
    g_sim.players[0].isAI=true;g_ai.ais[0].init(0,seed);
    g_ai.ais[0].rally=g_sim.get(cottage)->pos;
    Vec2 muster=g_map.nearestFree(g_sim.get(cottage)->pos+Vec2(0,4*TILE),6);
    for(int i=0;i<5;i++) g_sim.spawnUnit(U_C_INF1,0,muster+Vec2(i*8.0f,0));
    g_ai.ais[0].garrisonStrongholds();
    for(int k=0;k<12*SIM_HZ;k++) { g_sim.step();g_sim.events.clear(); }
    VERIFY11(g_sim.get(cottage)->owner==0 && g_sim.garrisonInfantry(*g_sim.get(cottage))==4,"commander failed to claim a reachable cottage with its reserve group");
    g_ai.ais[0].think();
    VERIFY11(g_sim.garrisonInfantry(*g_sim.get(cottage))==4,"commander pulled its garrison into the field army");
    // A failed escape from a completely closed room must never become a straight
    // fallback through a wall. A wall behind a vehicle must allow a lateral exit.
    Vec2 c=openGroundNear(tileCenter(16,16),7);flattenArena(c,8,8);
    Ref r=g_sim.spawnUnit(U_C_TANK,0,c);VERIFY11(r.valid(),"cannot spawn escape vehicle");
    int cx=tileOf(c.x),cy=tileOf(c.y);Vec2 dest;
    for(int y=cy-4;y<=cy+4;y++) g_map.blocked[y*MAP_W+cx+2]|=2;
    VERIFY11(g_sim.escapeDestination(*g_sim.get(r),c-Vec2(2*TILE,0),4,dest),"vehicle found no lateral escape from a wall");
    VERIFY11(dist(dest,c-Vec2(2*TILE,0))>2.4f*TILE,"escape did not increase threat distance");
    VERIFY11(g_map.findPath(c,dest,path),"escape selected an unreachable destination");
    for(int y=cy-1;y<=cy+1;y++) for(int x=cx-1;x<=cx+1;x++) if(x!=cx || y!=cy) g_map.blocked[y*MAP_W+x]=2;
    VERIFY11(!g_sim.escapeDestination(*g_sim.get(r),c-Vec2(2*TILE,0),4,dest),"boxed vehicle selected an unreachable escape");
    Map m;memset(m.blocked,1,sizeof m.blocked);memset(m.tiles,T_ROCK,sizeof m.tiles);
    VERIFY11(!m.findPath(Vec2(-999,-999),Vec2(WORLD_W+999,WORLD_H+999),path) && path.empty(),"blocked out-of-world path was accepted");
    VERIFY11(!m.findPath(Vec2(std::numeric_limits<float>::quiet_NaN(),0),c,path),"NaN path input was accepted");
    printf("frontiermaptest: 128x128, 20 supplies, 21 landmarks; starts, supplies and mainland entries connected; escape/path edge cases passed\n");
    return true;
}

bool learningTest(u64 seed) {
    scene11(seed);Brain brain;g_ai.ais[0].brain=&brain;
    Vec2 at=openGroundNear(tileCenter(17,17),8);flattenArena(at,10,10);
    Ref shooter=g_sim.spawnUnit(U_C_TANK,0,at),target=g_sim.spawnUnit(U_K_TANK,1,at+Vec2(3*TILE,0));
    g_sim.get(shooter)->disabledUntil=1e9f;g_sim.get(target)->disabledUntil=1e9f;
    // No deployment window may label a vehicle before its combat outcome exists.
    for(int k=0;k<90*SIM_HZ;k++) { g_sim.step();g_ai.update();g_sim.events.clear(); }
    VERIFY11(brain.unitSamples[U_C_TANK]==0,"idle/travelling vehicle produced a training label");
    AiPlayer& ai=g_ai.ais[0];ai.wave={shooter};ai.beginWaveLearning();
    float hp=g_sim.get(target)->hp;
    g_sim.applyDamagePublic(*g_sim.get(target),hp*0.5f,0,shooter);
    float credit=g_sim.get(shooter)->combatValue;
    VERIFY11(std::abs(credit-UNITS[U_K_TANK].cost*0.5f)<0.1f,"combat credit is not actual health removed");
    VERIFY11(std::abs(g_sim.players[0].waveCredit-credit)<0.1f,"wave member did not earn wave credit");
    Ref reserve=g_sim.spawnUnit(U_C_TANK,0,at+Vec2(0,2*TILE));
    g_sim.applyDamagePublic(*g_sim.get(target),1e7f,0,reserve);
    VERIFY11(std::abs(g_sim.players[0].waveCredit-credit)<0.1f,"reserve fire contaminated the wave label");
    VERIFY11(std::abs(g_sim.get(reserve)->combatValue-UNITS[U_K_TANK].cost*0.5f)<0.1f,"overkill earned extra credit");
    g_sim.applyDamagePublic(*g_sim.get(reserve),10,0,shooter);
    VERIFY11(g_sim.get(shooter)->combatValue==credit,"friendly fire earned learning credit");
    g_sim.applyDamagePublic(*g_sim.get(shooter),1e7f,1,NOREF);
    VERIFY11(g_sim.players[0].unitOutcomes.size()==1,"unit death did not close exactly one trial");
    ai.learnOutcomes();VERIFY11(brain.unitSamples[U_C_TANK]==1,"delayed combat evidence was not learned");
    g_sim.finishUnitExperience(*g_sim.get(reserve));g_sim.finishUnitExperience(*g_sim.get(reserve));
    ai.learnOutcomes();VERIFY11(brain.unitSamples[U_C_TANK]==2,"a surviving combat trial was lost or counted twice");
    // Damage stopped by a bunker's floor must not manufacture an efficiency gain.
    Ref bunker=g_sim.placeBuilding(B_K_BUNKER,1,tileOf(at.x)+6,tileOf(at.y)+3,true);
    Ref air=g_sim.spawnUnit(U_C_JET,0,at);g_sim.get(bunker)->hatch=0;
    g_sim.get(bunker)->hp=g_sim.get(bunker)->maxHp*BUNKER_FLOOR;
    g_sim.applyDamagePublic(*g_sim.get(bunker),100000,0,air);
    VERIFY11(g_sim.get(air)->combatValue==0,"bunker floor generated phantom damage credit");
    // Controlled held-out interpolation: a vehicle excels against armour but is
    // inefficient against infantry. This measures the regression, not match wins.
    Brain fit;float xi[UNIT_F],xv[UNIT_F],xt[UNIT_F];
    Brain::unitFeatures(1,0,0,xi);Brain::unitFeatures(0,1,0,xv);Brain::unitFeatures(.25f,.75f,0,xt);
    float before=std::abs(fit.unitEff(U_C_TANK,xt)-2.1f);
    for(int k=0;k<400;k++) { fit.learnUnit(U_C_TANK,xi,.3f);fit.learnUnit(U_C_TANK,xv,2.7f); }
    float after=std::abs(fit.unitEff(U_C_TANK,xt)-2.1f);
    VERIFY11(after<0.15f && after<before*.2f,"unit regression failed held-out combat-mix interpolation");
    int n=fit.unitSamples[U_C_TANK];
    fit.learnUnit(-1,xi,2);fit.learnUnit(U_C_TANK,xi,std::numeric_limits<float>::quiet_NaN());
    VERIFY11(fit.unitSamples[U_C_TANK]==n,"invalid evidence changed the model");
    char file[]="/tmp/onehour-learning-XXXXXX";int fd=mkstemp(file);VERIFY11(fd>=0,"cannot create brain fixture");close(fd);
    bool saved=fit.save(file);Brain restored;bool loaded=restored.load(file);
    VERIFY11(saved && loaded && restored.unitSamples[U_C_TANK]==n,"atomic brain round trip failed");
    // A child with a tiny file-size limit simulates an interrupted/full write.
    // The destination must still contain the previous complete brain afterwards.
    pid_t child=fork();VERIFY11(child>=0,"cannot fork save-failure check");
    if(child==0) {
        signal(SIGXFSZ,SIG_IGN);struct rlimit limit={128,128};
        if(setrlimit(RLIMIT_FSIZE,&limit)!=0) _exit(3);
        _exit(fit.save(file)?2:0);
    }
    int status=0;waitpid(child,&status,0);
    VERIFY11(WIFEXITED(status) && WEXITSTATUS(status)==0 && restored.load(file) && restored.unitSamples[U_C_TANK]==n,"failed write destroyed the last good brain");
    FILE* f=fopen(file,"r");VERIFY11(f,"cannot read brain fixture");
    std::string valid;char buf[512];while(fgets(buf,sizeof buf,f)) valid+=buf;fclose(f);
    size_t weights=valid.find('\n')+1, end=valid.find(' ',weights);
    std::string bad=valid;bad.replace(weights,end-weights,"nan");
    f=fopen(file,"w");VERIFY11(f,"cannot write corrupt brain fixture");fwrite(bad.data(),1,bad.size(),f);fclose(f);
    VERIFY11(!restored.load(file) && restored.unitSamples[U_C_TANK]==n,"nonfinite coefficient replaced good state");
    size_t row=valid.find('\n',weights)+1;end=valid.find(' ',row);bad=valid;bad.replace(row,end-row,"-3");
    f=fopen(file,"w");VERIFY11(f,"cannot write negative-count fixture");fwrite(bad.data(),1,bad.size(),f);fclose(f);
    VERIFY11(!restored.load(file) && restored.unitSamples[U_C_TANK]==n,"negative sample count replaced good state");remove(file);
    VERIFY11(!fit.save("/no-such-onehour-directory/brain.txt"),"invalid save destination reported success");
    printf("learningtest: delayed trials, overkill, friendly fire, wave membership, bunker floor, atomic persistence and corrupt input passed; held-out synthetic error %.3f -> %.3f\n",before,after);
    return true;
}

// Hash explicit state fields, never struct padding or pointer addresses.
static u64 stateHash11() {
    u64 h=1469598103934665603ull;
    auto bytes=[&](const void* p,size_t n) { const u8* b=(const u8*)p;for(size_t i=0;i<n;i++) h=(h^b[i])*1099511628211ull; };
    auto add=[&](const auto& v) { bytes(&v,sizeof v); };
    add(g_sim.tick);add(g_sim.rng.s);add(g_sim.gameOver);add(g_sim.winnerTeam);bytes(g_map.blocked,sizeof g_map.blocked);
    for(auto& p:g_sim.players) { add(p.money);add(p.mined);add(p.harvested);add(p.alive);bytes(p.valueDealt,sizeof p.valueDealt); }
    for(auto& e:g_sim.ents) {
        add(e.gen);add(e.alive);if(!e.alive) continue;
        add(e.kind);add(e.type);add(e.owner);add(e.pos.x);add(e.pos.y);add(e.hp);add(e.order);add(e.targetEnt.idx);add(e.targetEnt.gen);
        add(e.combatValue);add(e.ammo);add(e.cooldown);add(e.pathIdx);
        for(auto p:e.path) { add(p.x);add(p.y); }for(int t:e.queue) add(t);
    }
    for(auto& a:g_ai.ais) { add(a.rng.s);add(a.attacking);add(a.doctrine);add(a.nextThink); }
    bytes(g_brain.ww,sizeof g_brain.ww);bytes(g_brain.wu,sizeof g_brain.wu);bytes(g_brain.unitSamples,sizeof g_brain.unitSamples);
    return h;
}

bool determinismTest(u64 seed, int seconds) {
    u64 result[2]={};int peak=0;
    Brain saved=g_brain;
    for(int run=0;run<2;run++) {
        g_brain.reset();Faction f[4]={F_CYBER,F_CLANKER,F_CLANKER,F_CYBER};bool ai[4]={true,true,true,true};int d[4]={3,3,2,1},t[4]={0,1,2,3};
        g_sim.init(4,f,ai,d,t,seed);g_ai.init(seed);
        for(int k=0;k<seconds*SIM_HZ && !g_sim.gameOver;k++) {
            g_sim.step();g_ai.update();g_sim.events.clear();
            if(k%SIM_HZ==0) {
                for(auto& e:g_sim.ents) if(e.alive) {
                    if(!std::isfinite(e.hp) || !std::isfinite(e.pos.x) || !std::isfinite(e.pos.y) || e.pos.x<0 || e.pos.y<0 || e.pos.x>WORLD_W || e.pos.y>WORLD_H) return false;
                }
                peak=std::max(peak,(int)g_sim.ents.size());result[run]=(result[run]*31)^stateHash11();
            }
        }
        result[run]^=stateHash11();
    }
    g_brain=saved;
    VERIFY11(result[0]==result[1],"repeated deterministic simulation diverged");
    printf("determinismtest: seed %llu, %d seconds x 2, digest %016llx, peak slots %d\n",(unsigned long long)seed,seconds,(unsigned long long)result[0],peak);return true;
}

bool crowdTest(u64 seed) {
    scene11(seed,false);std::vector<Ref> units[2];
    // Distribute a large combined force over real land on the expanded map.
    for(int y=3;y<MAP_H-3 && units[0].size()+units[1].size()<2000;y+=2)
        for(int x=3;x<MAP_W-3 && units[0].size()+units[1].size()<2000;x+=2) {
            if(!g_map.passable(x,y)) continue;
            int p=x<MAP_W/2?0:1;int type=(units[p].size()%4==0)?(p?U_K_TANK:U_C_TANK):(p?U_K_INF1:U_C_INF1);
            units[p].push_back(g_sim.spawnUnit(type,p,tileCenter(x,y)));
        }
    VERIFY11(units[0].size()+units[1].size()==2000,"could not stage 2000 land units");
    // Force the next production to cross the vector's capacity boundary. Entity
    // references must be reacquired after spawning, even under heavy occupancy.
    Ref hq;
    for(auto& e:g_sim.ents) if(e.alive && e.owner==0 && e.isBuilding() && e.bt().role==BR_HQ) hq=g_sim.refOf(e);
    g_sim.players[0].money=100000;VERIFY11(g_sim.cmdTrain(hq,U_C_DOZER),"cannot queue capacity-boundary production");
    g_sim.get(hq)->queueProgress=1;g_sim.ents.shrink_to_fit();
    g_sim.cmdMove(units[0],g_sim.players[1].basePos,true);g_sim.cmdMove(units[1],g_sim.players[0].basePos,true);
    auto start=std::chrono::steady_clock::now();
    for(int k=0;k<60*SIM_HZ;k++) {
        g_sim.step();g_sim.events.clear();
        for(auto& e:g_sim.ents) if(e.alive) {
            VERIFY11(std::isfinite(e.hp) && std::isfinite(e.pos.x) && std::isfinite(e.pos.y),"nonfinite state in crowd");
            VERIFY11(e.pos.x>=0 && e.pos.y>=0 && e.pos.x<WORLD_W && e.pos.y<WORLD_H,"crowd escaped world bounds");
            if(e.isUnit() && !e.isAir() && !e.carrier.valid()) VERIFY11(g_map.terrainPassable(tileOf(e.pos.x),tileOf(e.pos.y)),"crowd entered impassable terrain");
        }
    }
    double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    printf("crowdtest: 2000 starting units, 1200 ticks in %.0f ms (%.3f ms/tick), %zu entity slots; finite state, world/terrain bounds and capacity-boundary production passed\n",ms,ms/1200,g_sim.ents.size());return true;
}
