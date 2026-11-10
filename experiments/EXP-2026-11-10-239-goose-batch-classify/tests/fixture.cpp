// Differential gate for the batch entry classification in llama_batch_allocr::init.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <random>
extern "C" { int64_t fg239b_classify(int64_t,const int32_t*,const int32_t*,int64_t,int32_t*); }
static int checks, failures;
static void ck(bool ok,const char*w,long long a,long long b,int at){++checks;
  if(!ok){if(failures<12)printf("FAIL %s at %d goose=%lld ref=%lld\n",w,at,a,b);++failures;}}

struct Ref { int64_t code; int32_t nto, neo, nb, mixed, ht, he; };
static Ref ref(const std::vector<int>& t,const std::vector<int>& e,int allow){
    Ref r{0,0,0,0,0,0,0};
    const int n=(int)t.size();
    for (int i=0;i<n;++i){
        const bool it=t[i], ie=e[i];
        if (!it && !ie) { r.code=1; return r; }       // immediate, before the both test
        r.nto += it && !ie;
        r.neo += ie && !it;
        r.nb  += it &&  ie;
    }
    if (r.nb>0 && r.nb!=n) { r.code=2; return r; }
    r.mixed = (r.nto>0 && r.neo>0);
    if (r.mixed && !allow) { r.code=3; return r; }
    r.ht = (r.nto>0 || r.nb>0);
    r.he = (r.neo>0 || r.nb>0);
    return r;
}

int main(void){
    std::mt19937 rng(239011);
    auto run=[&](std::vector<int> t,std::vector<int> e,int allow,int at){
        // native asserts n_tok > 0; skip empties
        if (t.empty()) return;
        Ref r=ref(t,e,allow);
        std::vector<int32_t> got(7,-7);
        const int64_t c=fg239b_classify((int64_t)t.size(),t.data(),e.data(),allow,got.data());
        ck(c==r.code,"code",c,r.code,at);
        if (r.code==0){
            ck(got[0]==r.nto,"nto",got[0],r.nto,at);
            ck(got[1]==r.neo,"neo",got[1],r.neo,at);
            ck(got[2]==r.nb,"nb",got[2],r.nb,at);
            ck(got[3]==r.mixed,"mixed",got[3],r.mixed,at);
            ck(got[4]==r.ht,"has_token",got[4],r.ht,at);
            ck(got[5]==r.he,"has_embd",got[5],r.he,at);
        }
        ck(got[6]==-7,"no overrun",got[6],-7,at);
    };
    // named: token only, embd only, all-both, both+token (err 2), mixed allowed, mixed refused (err 3),
    // and the precedence case: an empty entry BEFORE a both+token batch must report 1, not 2.
    run({1,0},{0,1},0,0);          // mixed, refused -> 3
    run({1,0},{0,1},1,1);          // mixed, allowed -> 0
    run({1,1},{1,1},0,2);          // all both -> 0
    run({1,1,1},{1,0,1},0,3);      // both mixed with token-only -> 2
    run({0,0},{0,0},0,4);          // neither -> 1
    run({0,1,1},{0,1,0},0,5);      // neither FIRST then both+token -> must be 1, not 2
    run({1},{0},0,6);              // single token
    run({0},{1},0,7);              // single embd
    for (int it=0;it<20000;++it){
        const int n=1+(int)(rng()%5);
        std::vector<int> t(n), e(n);
        for (int i=0;i<n;++i){ t[i]=(int)(rng()%2); e[i]=(int)(rng()%2); }
        run(t,e,(int)(rng()%2),100+it);
    }
    printf("FG239B_FIXTURE checks=%d failures=%d\n",checks,failures);
    if(failures){printf("EXP239_LOCAL_GATES_FAIL\n");return 1;}
    printf("EXP239_LOCAL_GATES_PASS\n"); return 0;
}
