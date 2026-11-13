// Differential gate for the ubatch_add classification.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <random>
extern "C" { int64_t fg241u_classify(int64_t,int64_t,const int32_t*,const int32_t*,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t*); }
static int checks, failures;
static void ck(bool ok,const char*w,long long a,long long b,int at){++checks;
  if(!ok){if(failures<12)printf("FAIL %s at %d goose=%lld ref=%lld\n",w,at,a,b);++failures;}}

struct Ref { int64_t rows; int64_t v[6]; };
static Ref ref(int64_t n,int mb,const std::vector<int>& ie,const std::vector<int>& idxs,
               int ht,int he,int hs,int64_t ne,int64_t nse,int64_t npe){
    int64_t rows=0;
    if (mb) for (int64_t i=0;i<n;++i) rows += ie[(size_t)idxs[(size_t)i]];
    const bool mixedb = mb!=0;
    Ref r{rows,{0,0,0,0,0,0}};
    r.v[0] = (mixedb && rows>0 && rows<n);
    r.v[1] = (ht && !(mixedb && rows==n));
    r.v[2] = (he && !(mixedb && rows==0));
    r.v[3] = r.v[2] ? n*ne : 0;
    r.v[4] = hs ? n*nse : 0;
    r.v[5] = n*npe;
    return r;
}

int main(void){
    std::mt19937 rng(241011);
    auto run=[&](int64_t n,int mb,std::vector<int> ie,std::vector<int> idxs,int ht,int he,int hs,
                 int64_t ne,int64_t nse,int64_t npe,int at){
        if ((int64_t)idxs.size()<n) idxs.resize((size_t)n,0);
        if (ie.empty()) ie.push_back(0);
        Ref r=ref(n,mb,ie,idxs,ht,he,hs,ne,nse,npe);
        std::vector<int64_t> got(7,-7);
        const int64_t gr=fg241u_classify(n,mb,ie.data(),idxs.data(),ht,he,hs,ne,nse,npe,got.data());
        ck(gr==r.rows,"rows",gr,r.rows,at);
        for (int k=0;k<6;++k) ck(got[k]==r.v[k],"field",got[k],r.v[k],at);
        ck(got[6]==-7,"no overrun",got[6],-7,at);
    };
    // all-token batch, all-embd batch, mixed batch, and the two suppression asymmetries
    run(3,0,{0},{0,1,2},1,0,0,8,4,1,0);        // token only
    run(3,0,{1},{0,1,2},0,1,0,8,4,1,1);        // embd only
    run(3,1,{1,1,1},{0,1,2},1,1,0,8,4,1,2);    // all embd rows -> use_token suppressed
    run(3,1,{0,0,0},{0,1,2},1,1,0,8,4,1,3);    // none embd -> use_embd suppressed
    run(4,1,{1,0,0,0},{0,1,2,3},1,1,1,8,4,2,4);// truly mixed
    run(1,0,{0},{0},1,0,1,8,4,3,5);            // n_pos_all unconditional
    for (int t=0;t<20000;++t){
        const int64_t n=1+(int64_t)(rng()%6);
        const int mb=(int)(rng()%2);
        std::vector<int> ie(n), idxs(n);
        for (auto& x:ie) x=(int)(rng()%2);
        for (int64_t i=0;i<n;++i) idxs[(size_t)i]=(int)(rng()%(unsigned)n);
        run(n,mb,ie,idxs,(int)(rng()%2),(int)(rng()%2),(int)(rng()%2),
            1+(int64_t)(rng()%16),1+(int64_t)(rng()%8),1+(int64_t)(rng()%4),100+t);
    }
    printf("FG241U_FIXTURE checks=%d failures=%d\n",checks,failures);
    if(failures){printf("EXP241_LOCAL_GATES_FAIL\n");return 1;}
    printf("EXP241_LOCAL_GATES_PASS\n"); return 0;
}
