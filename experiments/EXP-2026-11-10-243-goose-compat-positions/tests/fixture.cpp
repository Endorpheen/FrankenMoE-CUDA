// Differential gate for the compat position decision.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <random>
extern "C" { int64_t fg243c_positions(int64_t,int64_t,int64_t,int64_t,const int64_t*,const int32_t*,const int32_t*,int64_t*,int64_t*,int64_t); }
static int checks, failures;
static void ck(bool ok,const char*w,long long a,long long b,int at){++checks;
  if(!ok){if(failures<12)printf("FAIL %s at %d goose=%lld ref=%lld\n",w,at,a,b);++failures;}}

int main(void){
    std::mt19937 rng(243011);
    // sid_lists: one vector of seq ids per token. The first version of this fixture used a single id per
    // token, and that made it blind to the sids[0]-vs-sids[n_sid-1] trap -- the mutation passed with zero
    // failures. Multi-id entries are the only way to see the difference.
    auto run=[&](int64_t n,int ht,int hp,int64_t np,std::vector<std::vector<int>> sid_lists,int64_t nseq,int at){
        std::vector<int32_t> flat, off(n+1,0);
        for (int64_t i=0;i<n;++i){
            for (int s : sid_lists[(size_t)i]) flat.push_back(s);
            off[(size_t)i+1]=(int32_t)flat.size();
        }
        std::vector<int64_t> pos((size_t)np*n+1,0), pn(nseq+1,0), out((size_t)n*np+2,-7);
        for (size_t k=0;k<(size_t)np*(size_t)n;++k) pos[k]=(int64_t)(rng()%1000);
        for (int64_t s=0;s<nseq;++s) pn[(size_t)s]=100+ s*7;
        std::vector<int64_t> pn_ref(pn);
        std::vector<int64_t> ref((size_t)n*np,0);
        for (int64_t i=0;i<n;++i){
            if (hp){
                if (ht) { ref[(size_t)(i*np)] = pos[(size_t)i];
                          for (int64_t j=1;j<np;++j) ref[(size_t)(i*np+j)]=0; }
                else    { for (int64_t j=0;j<np;++j) ref[(size_t)(i*np+j)] = pos[(size_t)(j*n+i)]; }
            } else {
                const int s0 = flat[(size_t)off[(size_t)i]];
                ref[(size_t)(i*np)] = pn_ref[(size_t)s0];
                pn_ref[(size_t)s0] += 1;
                for (int64_t j=1;j<np;++j) ref[(size_t)(i*np+j)]=0;
            }
        }
        const int64_t r=fg243c_positions(n,ht,hp,np,pos.data(),flat.data(),off.data(),pn.data(),out.data(),nseq);
        ck(r==1,"ret",r,1,at);
        for (size_t k=0;k<(size_t)n*np;++k) ck(out[k]==ref[k],"pos",out[k],ref[k],at);
        for (int64_t s=0;s<nseq;++s) ck(pn[(size_t)s]==pn_ref[(size_t)s],"counter",pn[(size_t)s],pn_ref[(size_t)s],at);
        ck(out[(size_t)n*np]==-7,"no overrun",out[(size_t)n*np],-7,at);
    };
    run(1,1,1,1,{{0}},4,0);              // token, explicit pos
    run(1,1,0,1,{{2}},4,1);              // token, auto -> counter for seq 2
    run(3,1,0,1,{{0},{0},{1}},4,2);      // two entries same seq -> consecutive
    run(2,0,1,4,{{0},{1}},4,3);          // embd, section-major with np=4
    run(2,0,1,1,{{0},{1}},4,4);          // embd, np=1
    run(3,1,1,2,{{0},{1},{2}},4,5);      // token with np=2 -> slots 1.. must be 0
    run(2,1,0,1,{{1,0},{3,3,2}},4,6);    // FIRST id is the counter, not the last and not the largest
    run(3,1,0,1,{{2,5},{5,2},{1,2,5}},6,7);
    for (int t=0;t<20000;++t){
        const int64_t n=1+(int64_t)(rng()%5);
        const int64_t np=1+(int64_t)(rng()%4);
        const int ht=(int)(rng()%2), hp=(int)(rng()%2);
        const int64_t nseq=1+(int64_t)(rng()%6);
        std::vector<std::vector<int>> sl((size_t)n);
        for (int64_t i=0;i<n;++i){
            const int k=1+(int)(rng()%3);           // 1..3 seq ids per entry
            for (int j=0;j<k;++j) sl[(size_t)i].push_back((int)(rng()%(unsigned)nseq));
        }
        run(n,ht,hp,np,sl,nseq,100+t);
    }
    printf("FG243C_FIXTURE checks=%d failures=%d\n",checks,failures);
    if(failures){printf("EXP243_LOCAL_GATES_FAIL\n");return 1;}
    printf("EXP243_LOCAL_GATES_PASS\n"); return 0;
}
