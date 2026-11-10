// Differential gate for out_ids selection and seq_idx mapping.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <random>
extern "C" { int64_t fg242o_pick_outids(int64_t,const int32_t*,const int32_t*,int32_t*);
             int64_t fg242o_map_seqs(int64_t,int64_t,int64_t,int64_t,int64_t,int32_t*,int32_t*); }
static int checks, failures;
static void ck(bool ok,const char*w,long long a,long long b,int at){++checks;
  if(!ok){if(failures<12)printf("FAIL %s at %d goose=%lld ref=%lld\n",w,at,a,b);++failures;}}

int main(void){
    std::mt19937 rng(242011);
    // --- out_ids ---
    auto run1=[&](std::vector<int> idxs,std::vector<int> outp,int at){
        if (idxs.empty()) return;
        const int n=(int)idxs.size();
        std::vector<int> ref;
        for (int i=0;i<n;++i) if (outp[(size_t)i]) ref.push_back(idxs[(size_t)i]);
        std::vector<int32_t> got(n+2,-7);
        const int64_t g=fg242o_pick_outids(n,idxs.data(),outp.data(),got.data());
        ck(g==(int64_t)ref.size(),"n",g,(long long)ref.size(),at);
        for (size_t k=0;k<ref.size();++k) ck(got[k]==ref[k],"idx",got[k],ref[k],at);
        ck(got[ref.size()]==-7,"no overrun",got[ref.size()],-7,at);
    };
    run1({0,1,2},{0,0,0},0);
    run1({0,1,2},{1,0,1},1);         // order must follow i, not value
    run1({5,3,9},{1,1,1},2);
    run1({2},{1},3);
    for (int t=0;t<10000;++t){
        const int n=1+(int)(rng()%8);
        std::vector<int> ix(n), o(n);
        for (auto& x:ix) x=(int)(rng()%16);
        for (auto& x:o)  x=(int)(rng()%2);
        run1(ix,o,100+t);
    }
    // --- seq_idx ---
    auto run2=[&](std::vector<int> present,int nmax,int at){
        int64_t u[4]={0,0,0,0};
        for (int s : present) u[s/64] |= (1LL<<(s%64));
        std::vector<int32_t> ref_idx(nmax,-1);
        std::vector<int> ref_ids;
        for (int s=0;s<nmax;++s) if ((u[s/64]>>(s%64))&1) { ref_idx[(size_t)s]=(int32_t)ref_ids.size(); ref_ids.push_back(s); }
        // Pre-state must match native: udata->seq_idx is resized to LLAMA_MAX_SEQ with -1, and the loop
        // only writes the present entries. The module replaces the LOOP, not the initialisation, so an
        // absent entry keeping -1 is the correct expectation and not an omission.
        std::vector<int32_t> gi(nmax+2,-1), gid(nmax+2,-7);
        const int64_t g=fg242o_map_seqs(nmax,u[0],u[1],u[2],u[3],gi.data(),gid.data());
        ck(g==(int64_t)ref_ids.size(),"nseq",g,(long long)ref_ids.size(),at);
        for (size_t k=0;k<ref_ids.size();++k) ck(gid[k]==ref_ids[k],"uid",gid[k],ref_ids[k],at);
        for (int s=0;s<nmax;++s) ck(gi[(size_t)s]==ref_idx[(size_t)s],"seqidx",gi[(size_t)s],ref_idx[(size_t)s],at);
        ck(gid[ref_ids.size()]==-7,"no overrun",gid[ref_ids.size()],-7,at);
    };
    run2({},64,0);
    run2({0},64,1);
    run2({0,1,2},64,2);             // first present gets 0
    run2({63},256,3);
    run2({64},256,4);               // word 1
    run2({255},256,5);              // word 3, must not shift
    run2({0,64,128,192},256,6);
    for (int t=0;t<10000;++t){
        const int nmax=64*(1+(int)(rng()%4));
        std::vector<int> pr;
        for (int k=0;k<(int)(rng()%5);++k) pr.push_back((int)(rng()%(unsigned)nmax));
        run2(pr,nmax,200+t);
    }
    printf("FG242O_FIXTURE checks=%d failures=%d\n",checks,failures);
    if(failures){printf("EXP242_LOCAL_GATES_FAIL\n");return 1;}
    printf("EXP242_LOCAL_GATES_PASS\n"); return 0;
}
