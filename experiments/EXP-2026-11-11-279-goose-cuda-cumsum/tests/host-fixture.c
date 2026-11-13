// Host planning gate for the Goose cumsum host module.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>

static uint64_t G[8][32];
static int g_launches, g_fail_code;
static jmp_buf g_jmp;

int64_t fg279h_plan(int64_t p){ return p; }
uint64_t fg279h_get(int64_t p,int64_t k,int64_t i){ return ((uint64_t*)(uintptr_t)p)[k*32+i]; }
void fg279h_put(int64_t p,int64_t k,int64_t i,uint64_t v){ ((uint64_t*)(uintptr_t)p)[k*32+i]=v; }
int64_t fg279h_launch(int64_t p){ (void)p; ++g_launches; return 0; }
void fg279h_fail(int64_t c){ g_fail_code=(int)c; longjmp(g_jmp,1); }
uint8_t fg279dh_run(int64_t p);
#include "host-unit.c"

static int checks, failures;
static void ck(int ok,const char*what){ ++checks; if(!ok){ ++failures; printf("FAIL %s\n",what); } }

static void reset(uint64_t dst,uint64_t src,int64_t ne00,int64_t ne01,int64_t ne02,int64_t ne03,int64_t ws){
    memset(G,0,sizeof G); g_launches=0; g_fail_code=0;
    G[0][0]=dst; G[0][1]=src; G[0][2]=(uint64_t)ne00; G[0][3]=(uint64_t)ne01;
    G[0][4]=(uint64_t)ne02; G[0][5]=(uint64_t)ne03; G[0][6]=(uint64_t)ws;
}

int main(void){
    const int64_t nes[6]={1,32,129,1000,4096,8193};
    for (int i=0;i<6;++i){
        const int64_t ne00=nes[i];
        const int64_t nw=(ne00+31)/32;
        int64_t blk=nw*32; if (blk>256) blk=256;
        const int64_t warps=blk/32;
        const int64_t shm=(blk+warps+2)*4;
        reset(0x1000,0x2000,ne00,2,1,1,32);
        if (setjmp(g_jmp)) { ck(0,"valid plan raised fail"); continue; }
        ck(fg279dh_run((int64_t)(uintptr_t)G)==1,"run returned true");
        ck(g_launches==1,"one launch");
        ck((int64_t)G[1][0]==blk,"block");
        ck((int64_t)G[1][1]==shm,"shmem bytes");
        ck((int64_t)G[1][2]==2&&(int64_t)G[1][3]==1&&(int64_t)G[1][4]==1,"grid dims");
    }

    reset(0,0x2000,10,1,1,1,32);
    if (!setjmp(g_jmp)) { fg279dh_run((int64_t)(uintptr_t)G); ck(0,"null dst accepted"); }
    else { ck(g_fail_code==20,"null dst code 20"); ck(g_launches==0,"no launch"); }

    reset(0x1000,0x2000,0,1,1,1,32);
    if (!setjmp(g_jmp)) { fg279dh_run((int64_t)(uintptr_t)G); ck(0,"zero ne00 accepted"); }
    else { ck(g_fail_code==21,"zero ne00 code 21"); ck(g_launches==0,"no launch"); }

    reset(0x1000,0x2000,10,1,1,1,0);
    if (!setjmp(g_jmp)) { fg279dh_run((int64_t)(uintptr_t)G); ck(0,"zero warp size accepted"); }
    else { ck(g_fail_code==22,"zero warp size code 22"); ck(g_launches==0,"no launch"); }

    printf("FG279H_HOST_FIXTURE checks=%d failures=%d\n",checks,failures);
    return failures?1:0;
}
