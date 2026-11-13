// Differential gate for the split_equal expansion step.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <random>
extern "C" { int64_t fg237s_plan_expand(int64_t,int64_t,const int32_t*,const int32_t*,int32_t*); }
static int checks, failures;
static void ck(bool ok,const char*w,long long a,long long b,int at){++checks;
  if(!ok){if(failures<12)printf("FAIL %s at %d goose=%lld ref=%lld\n",w,at,a,b);++failures;}}

// transcription of the native loop
static int64_t ref_expand(int n_seqs, int64_t n_ubatch, const std::vector<int32_t>& len,
                          const std::vector<int32_t>& cur, std::vector<int32_t>& out) {
    std::vector<int32_t> c(cur.begin(), cur.begin()+n_seqs);
    int64_t iters = 0;
    while (true) {
        bool can = true;
        for (int s = 0; s < n_seqs; ++s) if (c[s] >= len[s]) { can = false; break; }
        if (!can) break;
        for (int s = 0; s < n_seqs; ++s) ++c[s];
        ++iters;
        if ((iters + 1) * n_seqs > n_ubatch) break;
    }
    out.assign(c.begin(), c.end());
    return iters;
}

int main(void){
    std::mt19937 rng(237011);
    auto run=[&](int ns,int64_t nub,std::vector<int32_t> len,std::vector<int32_t> cur,int t){
        len.resize(4,0); cur.resize(4,0);
        std::vector<int32_t> ref; const int64_t ri = ref_expand(ns,nub,len,cur,ref);
        std::vector<int32_t> got(5,-7);
        const int64_t gi = fg237s_plan_expand(ns,nub,len.data(),cur.data(),got.data());
        ck(gi==ri,"iters",gi,ri,t);
        for (int s=0;s<ns;++s) ck(got[s]==ref[s],"cursor",got[s],ref[s],t);
        ck(got[4]==-7,"no overrun",got[4],-7,t);
    };
    // edges: n_ubatch exactly filled, less than n_seqs, sets of unequal length, one empty set
    run(1,10,{3},{0},0);
    run(1,0,{5},{0},1);
    run(2,8,{9,9},{0,0},2);
    run(2,3,{9,9},{0,0},3);            // (iters+1)*2 > 3 after first iteration
    run(4,9,{9,9,9,9},{0,0,0,0},4);
    run(3,100,{5,2,7},{0,0,0},5);      // stops when the shortest set runs out
    run(2,100,{0,9},{0,0},6);          // one empty set -> zero iterations
    for (int t=0;t<20000;++t) {
        const int ns = 1 + (int)(rng()%4);
        const int64_t nub = (int64_t)(rng()%40);
        std::vector<int32_t> len(ns), cur(ns);
        for (int s=0;s<ns;++s){ len[s]=(int32_t)(rng()%9); cur[s]=(int32_t)(rng()%(len[s]+2)); }
        run(ns,nub,len,cur,100+t);
    }
    printf("FG237S_FIXTURE checks=%d failures=%d\n",checks,failures);
    if(failures){printf("EXP237_LOCAL_GATES_FAIL\n");return 1;}
    printf("EXP237_LOCAL_GATES_PASS\n"); return 0;
}
