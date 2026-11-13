#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <random>
extern "C" { int64_t fg247r_map(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t); }
static int checks, failures;
static void ck(bool ok,const char*w,long long a,long long b,int at){++checks;
  if(!ok){if(failures<12)printf("FAIL %s at %d goose=%lld ref=%lld\n",w,at,a,b);++failures;}}
static int64_t wrap_index(int64_t v,int64_t ne){ if(v<0)return v+ne; if(v>=ne)return v-ne; return v; }

int main(void){
    std::mt19937_64 rng(247011);
    auto run=[&](int64_t n0,int64_t n1,int64_t n2,int64_t n3,int64_t s0,int64_t s1,int64_t s2,int64_t s3,int at){
        const int64_t tot=n0*n1*n2*n3;
        for (int64_t idx=0; idx<tot; ++idx){
            const int64_t i0=idx%n0, i1=(idx/n0)%n1, i2=(idx/(n0*n1))%n2, i3=(idx/(n0*n1*n2))%n3;
            const int64_t d0=wrap_index(i0-s0,n0), d1=wrap_index(i1-s1,n1);
            const int64_t d2=wrap_index(i2-s2,n2), d3=wrap_index(i3-s3,n3);
            const int64_t ref=d3*n0*n1*n2+d2*n0*n1+d1*n0+d0;
            ck(fg247r_map(idx,n0,n1,n2,n3,s0,s1,s2,s3)==ref,"map",
               fg247r_map(idx,n0,n1,n2,n3,s0,s1,s2,s3),ref,at);
        }
    };
    run(1,1,1,1,0,0,0,0,0);
    run(4,3,2,1,0,0,0,0,1);      // identity
    run(4,3,2,1,1,2,0,0,2);      // positive shift wraps down
    run(4,3,2,1,-1,-2,0,0,3);    // negative shift wraps up
    run(4,3,2,1,3,2,1,0,4);      // maximum legal shift
    run(4,3,2,1,-3,-2,-1,0,5);   // minimum legal shift
    for (int t=0;t<2000;++t){
        const int64_t n0=1+(int64_t)(rng()%5), n1=1+(int64_t)(rng()%4);
        const int64_t n2=1+(int64_t)(rng()%3), n3=1+(int64_t)(rng()%2);
        auto sh=[](std::mt19937_64&r,int64_t n){ return (int64_t)(r() % (unsigned long long)(2*n-1)) - (n-1); };
        run(n0,n1,n2,n3,sh(rng,n0),sh(rng,n1),sh(rng,n2),sh(rng,n3),100+t);
    }
    printf("FG247R_FIXTURE checks=%d failures=%d\n",checks,failures);
    if(failures){printf("EXP247_LOCAL_GATES_FAIL\n");return 1;}
    printf("EXP247_LOCAL_GATES_PASS\n"); return 0;
}
