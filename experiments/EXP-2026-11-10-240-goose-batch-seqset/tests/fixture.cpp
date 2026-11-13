// Differential gate for the sequence-set construction.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <random>
#include <map>
extern "C" { int64_t fg240s_build_sets(int64_t,const int32_t*,const int32_t*,int64_t*,int64_t); }
static int checks, failures;
static void ck(bool ok,const char*w,long long a,long long b,int at){++checks;
  if(!ok){if(failures<12)printf("FAIL %s at %d goose=%lld ref=%lld\n",w,at,a,b);++failures;}}

int main(void){
    std::mt19937 rng(240011);
    auto run=[&](const std::vector<std::vector<int>>& per,int nmax,int at){
        const int n=(int)per.size();
        std::vector<int32_t> flat, off(n+1,0);
        for (int i=0;i<n;++i){ for (int s : per[i]) flat.push_back(s); off[i+1]=(int32_t)flat.size(); }
        if (flat.empty()) flat.push_back(0);
        std::vector<int64_t> ref(n,0), got(n+2,-7);
        uint64_t refunq=0;
        for (int i=0;i<n;++i) for (int s : per[i]) { ref[i]|=(1LL<<s); refunq|=(1uLL<<s); }
        const int64_t gunq=fg240s_build_sets(n,flat.data(),off.data(),got.data(),nmax);
        ck(gunq==(int64_t)refunq,"union",gunq,(int64_t)refunq,at);
        for (int i=0;i<n;++i) ck(got[i]==ref[i],"set",got[i],ref[i],at);
        ck(got[n]==-7,"no overrun",got[n],-7,at);
    };
    run({},64,0);
    run({{0}},64,1);
    run({{0,1}},64,2);
    run({{0},{1},{0,1}},64,3);
    run({{63}},64,4);          // high bit must not be lost
    run({{0,63},{}},64,5);     // empty set is a legal token
    for (int t=0;t<20000;++t){
        // nmax up to 64 so seq ids above 31 are exercised: the first version stopped at 32 and a
        // 32-bit-shift mutation was caught by only 2 checks, both from the named high-bit case.
        const int nmax=1+(int)(rng()%64);
        const int n=(int)(rng()%6);
        std::vector<std::vector<int>> per(n);
        for (int i=0;i<n;++i){
            const int k=(int)(rng()%4);
            for (int j=0;j<k;++j) per[i].push_back((int)(rng()%(unsigned)nmax));
        }
        run(per,nmax,100+t);
    }
    printf("FG240S_FIXTURE checks=%d failures=%d\n",checks,failures);
    if(failures){printf("EXP240_LOCAL_GATES_FAIL\n");return 1;}
    printf("EXP240_LOCAL_GATES_PASS\n"); return 0;
}
