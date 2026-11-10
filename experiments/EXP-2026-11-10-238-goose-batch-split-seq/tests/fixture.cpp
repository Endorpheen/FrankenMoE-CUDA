// Differential gate for split_seq selection.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <random>
extern "C" { int64_t fg238q_plan_seq(int64_t,int64_t,const int32_t*,const uint64_t*,const uint64_t*,int32_t*); }
static int checks, failures;
static void ck(bool ok,const char*w,long long a,long long b,int at){++checks;
  if(!ok){if(failures<12)printf("FAIL %s at %d goose=%lld ref=%lld\n",w,at,a,b);++failures;}}

static bool subset(uint64_t a0,uint64_t a1,uint64_t b0,uint64_t b1){
    return (a0 & b0) == a0 && (a1 & b1) == a1;
}
static int64_t ref_seq(int n_tokens,int64_t n_ubatch,const std::vector<int32_t>& used,
                       const std::vector<uint64_t>& lo,const std::vector<uint64_t>& hi,
                       std::vector<int32_t>& out) {
    int64_t cur = 0;
    while (cur < n_tokens && used[(size_t)cur]) ++cur;
    if (cur >= n_tokens) { out.clear(); return 0; }
    uint64_t clo = lo[(size_t)cur], chi = hi[(size_t)cur];
    out.clear();
    while (true) {
        out.push_back((int32_t)cur);
        if ((int64_t)out.size() >= n_ubatch) break;
        int64_t next = cur + 1;
        bool found = false;
        while (next < n_tokens) {
            if (!used[(size_t)next] && subset(clo,chi,lo[(size_t)next],hi[(size_t)next])) { found = true; break; }
            ++next;
        }
        if (!found) break;
        cur = next; clo = lo[(size_t)cur]; chi = hi[(size_t)cur];
    }
    return (int64_t)out.size();
}

int main(void){
    std::mt19937 rng(238011);
    auto run=[&](int n,int64_t nub,std::vector<int32_t> used,std::vector<uint64_t> lo,
                 std::vector<uint64_t> hi,int t){
        used.resize(n>0?n:1,0); lo.resize(n>0?n:1,0); hi.resize(n>0?n:1,0);
        std::vector<int32_t> ref; const int64_t ri=ref_seq(n,nub,used,lo,hi,ref);
        std::vector<int32_t> got(80,-7);
        const int64_t gi=fg238q_plan_seq(n,nub,used.data(),lo.data(),hi.data(),got.data());
        ck(gi==ri,"count",gi,ri,t);
        for (size_t k=0;k<(size_t)ri;++k) ck(got[k]==ref[k],"idx",got[k],ref[k],t);
        ck(got[(size_t)ri]==-7,"no overrun",got[(size_t)ri],-7,t);
    };
    // 1: all used -> 0.  2: n_ubatch 1 -> single.  3: no subset match -> single.
    // 4: subset chain.   5: superset must NOT match (the trap).
    run(3,8,{1,1,1},{0,0,0},{0,0,0},0);
    run(3,1,{0,0,0},{7,7,7},{0,0,0},1);
    run(3,8,{0,0,0},{1,2,4},{0,0,0},2);
    run(4,8,{0,0,0,0},{3,1,3,0xFF},{0,0,0,0},3);
    run(3,8,{0,0,0},{1,2,4},{0,0,0},4);   // 4 is not a subset of 1 -> stop after first
    run(0,8,{},{},{},5);
    for (int t=0;t<20000;++t) {
        const int n = (int)(rng()%8);
        const int64_t nub = 1 + (int64_t)(rng()%8);
        std::vector<int32_t> used(n); std::vector<uint64_t> lo(n), hi(n);
        for (int i=0;i<n;++i){ used[i]=(int32_t)(rng()%3==0); lo[i]=rng()%8; hi[i]=rng()%3; }
        run(n,nub,used,lo,hi,100+t);
    }
    printf("FG238Q_FIXTURE checks=%d failures=%d\n",checks,failures);
    if(failures){printf("EXP238_LOCAL_GATES_FAIL\n");return 1;}
    printf("EXP238_LOCAL_GATES_PASS\n"); return 0;
}
