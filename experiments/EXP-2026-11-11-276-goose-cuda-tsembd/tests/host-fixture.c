// Host planning gate for the Goose timestep embedding host module.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>

static uint64_t G[8][32];
static int g_launches, g_fail_code;
static jmp_buf g_jmp;

int64_t fg276h_plan(int64_t p){ return p; }
uint64_t fg276h_get(int64_t p,int64_t k,int64_t i){ return ((uint64_t*)(uintptr_t)p)[k*32+i]; }
void fg276h_put(int64_t p,int64_t k,int64_t i,uint64_t v){ ((uint64_t*)(uintptr_t)p)[k*32+i]=v; }
int64_t fg276h_launch(int64_t p){ (void)p; ++g_launches; return 0; }
void fg276h_fail(int64_t c){ g_fail_code=(int)c; longjmp(g_jmp,1); }
uint8_t fg276dh_run(int64_t p);
#include "host-unit.c"

static int checks, failures;
static void ck(int ok,const char*what){ ++checks; if(!ok){ ++failures; printf("FAIL %s\n",what); } }

static void reset(uint64_t dst,uint64_t src,int64_t ne00,int64_t nb1,int64_t dim,int64_t mp){
    memset(G,0,sizeof G); g_launches=0; g_fail_code=0;
    G[0][0]=dst; G[0][1]=src; G[0][2]=(uint64_t)ne00; G[0][3]=(uint64_t)nb1;
    G[0][4]=(uint64_t)dim; G[0][5]=(uint64_t)mp;
}

static void valid(int64_t ne00,int64_t dim,int64_t mp){
    const int64_t nb1=dim*4;
    const int64_t nb=( ((dim+1)/2) + 255 )/256;
    reset(0x1000,0x2000,ne00,nb1,dim,mp);
    if (setjmp(g_jmp)) { ck(0,"valid plan raised fail"); return; }
    ck(fg276dh_run((int64_t)(uintptr_t)G)==1,"run returned true");
    ck(g_launches==1,"one launch");
    ck((int64_t)G[1][0]==nb,"num_blocks");
    ck((int64_t)G[1][1]==256,"block 256");
    ck((int64_t)G[1][2]==ne00,"grid y = ne00");
}

int main(void){
    valid(3,8,10000);
    valid(7,257,1000);
    valid(1,1024,10000);
    valid(1,513,10000);

    reset(0,0x2000,3,32,8,10000);
    if (!setjmp(g_jmp)) { fg276dh_run((int64_t)(uintptr_t)G); ck(0,"null dst accepted"); }
    else { ck(g_fail_code==20,"null dst code 20"); ck(g_launches==0,"no launch"); }

    reset(0x1000,0x2000,3,32,0,10000);
    if (!setjmp(g_jmp)) { fg276dh_run((int64_t)(uintptr_t)G); ck(0,"dim 0 accepted"); }
    else { ck(g_fail_code==21,"dim 0 code 21"); ck(g_launches==0,"no launch"); }

    reset(0x1000,0x2000,3,32,8,0);
    if (!setjmp(g_jmp)) { fg276dh_run((int64_t)(uintptr_t)G); ck(0,"max_period 0 accepted"); }
    else { ck(g_fail_code==21,"max_period 0 code 21"); ck(g_launches==0,"no launch"); }

    reset(0x1000,0x2000,3,31,8,10000);
    if (!setjmp(g_jmp)) { fg276dh_run((int64_t)(uintptr_t)G); ck(0,"short nb1 accepted"); }
    else { ck(g_fail_code==22,"short nb1 code 22"); ck(g_launches==0,"no launch"); }

    printf("FG276H_HOST_FIXTURE checks=%d failures=%d\n",checks,failures);
    return failures?1:0;
}
