// Differential gate for the 256-bit sequence-set construction.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <random>
extern "C" { int64_t fg240q_build_sets4(int64_t,const int32_t*,const int32_t*,int64_t*,int64_t*,int64_t); }
static int checks, failures;
static void ck(bool ok,const char*w,long long a,long long b,int at){++checks;
  if(!ok){if(failures<12)printf("FAIL %s at %d goose=%lld ref=%lld\n",w,at,a,b);++failures;}}

int main(void){
    std::mt19937 rng(240111);
    auto run=[&](const std::vector<std::vector<int>>& per,int nmax,int at){
        const int n=(int)per.size();
        std::vector<int32_t> flat, off(n+1,0);
        for (int i=0;i<n;++i){ for (int s : per[i]) flat.push_back(s); off[i+1]=(int32_t)flat.size(); }
        if (flat.empty()) flat.push_back(0);
        std::vector<int64_t> ref((size_t)n*4,0), unqref(4,0);
        for (int i=0;i<n;++i) for (int s : per[i]) {
            ref[(size_t)i*4 + (s/64)] |= (1LL<<(s%64));
            unqref[s/64] |= (1LL<<(s%64));
        }
        std::vector<int64_t> got((size_t)n*4+4,-7), unq(5,-7);
        const int64_t r=fg240q_build_sets4(n,flat.data(),off.data(),got.data(),unq.data(),nmax);
        ck(r==1,"ret",r,1,at);
        for (size_t k=0;k<(size_t)n*4;++k) ck(got[k]==ref[k],"word",got[k],ref[k],at);
        for (int w=0;w<4;++w) ck(unq[w]==unqref[w],"unq",unq[w],unqref[w],at);
        ck(got[(size_t)n*4]==-7,"no overrun",got[(size_t)n*4],-7,at);
        ck(unq[4]==-7,"unq no overrun",unq[4],-7,at);
    };
    run({},256,0);
    run({{0}},256,1);
    run({{63}},256,2);           // word 0 boundary
    run({{64}},256,3);           // word 1 first bit
    run({{127}},256,4);
    run({{128},{192}},256,5);
    run({{255}},256,6);          // top bit of word 3
    run({{0},{255},{64,128}},256,7);
    for (int t=0;t<20000;++t){
        const int nmax=1+(int)(rng()%256);
        const int n=(int)(rng()%5);
        std::vector<std::vector<int>> per(n);
        for (int i=0;i<n;++i){
            const int k=(int)(rng()%4);
            for (int j=0;j<k;++j) per[i].push_back((int)(rng()%(unsigned)nmax));
        }
        run(per,nmax,100+t);
    }
    printf("FG240Q_FIXTURE checks=%d failures=%d\n",checks,failures);
    if(failures){printf("EXP240Q_LOCAL_GATES_FAIL\n");return 1;}
    printf("EXP240Q_LOCAL_GATES_PASS\n"); return 0;
}
