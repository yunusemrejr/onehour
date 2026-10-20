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
