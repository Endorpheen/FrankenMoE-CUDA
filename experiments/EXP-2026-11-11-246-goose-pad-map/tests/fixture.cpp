#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <random>
extern "C" { int64_t fg246p_map(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,
                               int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,
                               int64_t,int64_t,int64_t,int64_t,int64_t); }
static int checks, failures;
static void ck(bool ok,const char*w,long long a,long long b,int at){++checks;
  if(!ok){if(failures<12)printf("FAIL %s at %d goose=%lld ref=%lld\n",w,at,a,b);++failures;}}
static int64_t wrap_around(int64_t c,int64_t s){return (c+s)%s;}

int main(void){
    std::mt19937_64 rng(246011);
    auto run=[&](int64_t i0,int64_t i1,int64_t i2,int64_t i3,
                 int64_t s00,int64_t s01,int64_t s02,int64_t s03,
                 int64_t lp0,int64_t rp0,int64_t lp1,int64_t rp1,
                 int64_t lp2,int64_t rp2,int64_t lp3,int64_t rp3,
                 int64_t ne0,int64_t ne1,int64_t ne2,int64_t ne3,int circ,int at){
        int64_t ref;
        if (!circ){
            const bool inside = (i0>=lp0 && i0<ne0-rp0) && (i1>=lp1 && i1<ne1-rp1) &&
                                (i2>=lp2 && i2<ne2-rp2) && (i3>=lp3 && i3<ne3-rp3);
            if (!inside) ref = -1;
            else ref = (i3-lp3)*s03 + (i2-lp2)*s02 + (i1-lp1)*s01 + (i0-lp0)*s00;
        } else {
            ref = wrap_around(i3-lp3,ne3-lp3-rp3)*s03 + wrap_around(i2-lp2,ne2-lp2-rp2)*s02 +
                  wrap_around(i1-lp1,ne1-lp1-rp1)*s01 + wrap_around(i0-lp0,ne0-lp0-rp0)*s00;
        }
        const int64_t g=fg246p_map(i0,i1,i2,i3,s00,s01,s02,s03,lp0,rp0,lp1,rp1,lp2,rp2,lp3,rp3,ne0,ne1,ne2,ne3,circ);
        ck(g==ref,"map",g,ref,at);
    };
    // 1-D: pad 1 each side over ne0=4, s00=1
    run(0,0,0,0,1,4,16,64, 1,1,0,0,0,0,0,0, 4,1,1,1,0,0);   // inside -> src 0
    run(1,0,0,0,1,4,16,64, 1,1,0,0,0,0,0,0, 4,1,1,1,0,1);   // still inside -> src 1
    run(3,0,0,0,1,4,16,64, 1,1,0,0,0,0,0,0, 4,1,1,1,0,2);   // i0=3 is in the right pad -> zero
    // circular wraps: i0=3 -> 3-1=2 -> 2 < ne0-lp-rp=2? no -> wraps to 0
    run(3,0,0,0,1,4,16,64, 1,1,0,0,0,0,0,0, 4,1,1,1,1,3);
    run(0,0,0,0,1,4,16,64, 1,1,0,0,0,0,0,0, 4,1,1,1,1,4);   // negative coord -> wrapped
    for (int t=0;t<20000;++t){
        const int64_t n0=2+(int64_t)(rng()%6), n1=1+(int64_t)(rng()%3);
        const int64_t lp0=(int64_t)(rng()%(unsigned)n0), rp0=(int64_t)(rng()%(unsigned)n0);
        const int64_t lp1=(int64_t)(rng()%(unsigned)n1), rp1=(int64_t)(rng()%(unsigned)n1);
        if (n0-lp0-rp0 <= 0 || n1-lp1-rp1 <= 0) continue;
        run((int64_t)(rng()%(unsigned)n0),(int64_t)(rng()%(unsigned)n1),0,0,
            (int64_t)(rng()%7)+1,(int64_t)(rng()%70)+8,(int64_t)(rng()%700)+80,(int64_t)(rng()%7000)+800,
            lp0,rp0,lp1,rp1,0,0,0,0,n0,n1,1,1,(int)(rng()%2),100+t);
    }
    printf("FG246P_FIXTURE checks=%d failures=%d\n",checks,failures);
    if(failures){printf("EXP246_LOCAL_GATES_FAIL\n");return 1;}
    printf("EXP246_LOCAL_GATES_PASS\n"); return 0;
}
