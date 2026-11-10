#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <random>
extern "C" { int64_t fg245v_view(uint64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t*); }
static int checks, failures;
static void ck(bool ok,const char*w,long long a,long long b,int at){++checks;
  if(!ok){if(failures<12)printf("FAIL %s at %d goose=%lld ref=%lld\n",w,at,a,b);++failures;}}

int main(void){
    std::mt19937_64 rng(245011);
    auto run=[&](uint64_t base,int64_t off,int64_t es,int64_t nb1,int64_t nb2,int64_t nb3,
                 int64_t n0,int64_t n1,int64_t n2,int64_t n3,int at){
        std::vector<int64_t> got(11,-7);
        const int64_t r=fg245v_view(base,off,es,nb1,nb2,nb3,n0,n1,n2,n3,got.data());
        ck(r==1,"ret",r,1,at);
        ck(got[0]==(int64_t)(base+(uint64_t)off),"data",got[0],(long long)(base+off),at);
        ck(got[1]==n0,"ne0",got[1],n0,at); ck(got[2]==n1,"ne1",got[2],n1,at);
        ck(got[3]==n2,"ne2",got[3],n2,at); ck(got[4]==n3,"ne3",got[4],n3,at);
        ck(got[5]==es,"nb0",got[5],es,at);
        ck(got[6]==nb1,"nb1",got[6],nb1,at);
        ck(got[7]==nb2,"nb2",got[7],nb2,at);
        ck(got[8]==nb3,"nb3",got[8],nb3,at);
        ck(got[9]==-7,"no overrun",got[9],-7,at);
    };
    run(0,0,4,1,1,1,1,1,1,1,0);
    run(4096,256,4,128,512,2048,32,4,2,1,1);
    run(4096,0,4,128,512,2048,32,4,2,1,2);      // offset 0 must not shift
    run(4096,256,2,64,256,1024,32,4,2,1,3);     // nb0 from elem size, not from nb1
    for (int t=0;t<20000;++t){
        const uint64_t base=(uint64_t)(rng()%100000)*16;
        const int64_t es=1+(int64_t)(rng()%8);
        const int64_t n0=1+(int64_t)(rng()%64), n1=1+(int64_t)(rng()%8);
        const int64_t n2=1+(int64_t)(rng()%4),  n3=1+(int64_t)(rng()%3);
        run(base,(int64_t)(rng()%4096),es,n0*es,n1*n0*es,n2*n1*n0*es,n0,n1,n2,n3,100+t);
    }
    printf("FG245V_FIXTURE checks=%d failures=%d\n",checks,failures);
    if(failures){printf("EXP245_LOCAL_GATES_FAIL\n");return 1;}
    printf("EXP245_LOCAL_GATES_PASS\n"); return 0;
}
