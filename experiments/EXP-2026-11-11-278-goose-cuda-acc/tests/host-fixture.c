// Host planning gate for the Goose acc host module.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>

static uint64_t G[8][32];
static int g_launches, g_fail_code;
static jmp_buf g_jmp;

int64_t fg278h_plan(int64_t p){ return p; }
uint64_t fg278h_get(int64_t p,int64_t k,int64_t i){ return ((uint64_t*)(uintptr_t)p)[k*32+i]; }
void fg278h_put(int64_t p,int64_t k,int64_t i,uint64_t v){ ((uint64_t*)(uintptr_t)p)[k*32+i]=v; }
int64_t fg278h_launch(int64_t p){ (void)p; ++g_launches; return 0; }
void fg278h_fail(int64_t c){ g_fail_code=(int)c; longjmp(g_jmp,1); }
uint8_t fg278dh_run(int64_t p);
#include "host-unit.c"

static int checks, failures;
static void ck(int ok,const char*what){ ++checks; if(!ok){ ++failures; printf("FAIL %s\n",what); } }

static void reset(uint64_t dst,uint64_t x,uint64_t y,int64_t ne,int64_t s11){
    memset(G,0,sizeof G); g_launches=0; g_fail_code=0;
    G[0][0]=dst; G[0][1]=x; G[0][2]=y; G[0][3]=(uint64_t)ne;
    G[0][4]=(uint64_t)s11; G[0][5]=12; G[0][6]=24;
}

int main(void){
    const int64_t nes[3]={1,256,1000};
    for (int i=0;i<3;++i){
        const int64_t ne=nes[i];
        reset(0x1000,0x2000,0x3000,ne,4);
        if (setjmp(g_jmp)) { ck(0,"valid plan raised fail"); continue; }
        ck(fg278dh_run((int64_t)(uintptr_t)G)==1,"run returned true");
        ck(g_launches==1,"one launch");
        ck((int64_t)G[1][0]==(ne+255)/256,"num_blocks");
        ck((int64_t)G[1][1]==256,"block 256");
    }

    reset(0,0x2000,0x3000,10,4);
    if (!setjmp(g_jmp)) { fg278dh_run((int64_t)(uintptr_t)G); ck(0,"null dst accepted"); }
    else { ck(g_fail_code==20,"null dst code 20"); ck(g_launches==0,"no launch"); }

    reset(0x1000,0x2000,0x3000,0,4);
    if (!setjmp(g_jmp)) { fg278dh_run((int64_t)(uintptr_t)G); ck(0,"zero ne accepted"); }
    else { ck(g_fail_code==21,"zero ne code 21"); ck(g_launches==0,"no launch"); }

    reset(0x1000,0x2000,0x3000,10,0);
    if (!setjmp(g_jmp)) { fg278dh_run((int64_t)(uintptr_t)G); ck(0,"zero stride accepted"); }
    else { ck(g_fail_code==22,"zero stride code 22"); ck(g_launches==0,"no launch"); }

    printf("FG278H_HOST_FIXTURE checks=%d failures=%d\n",checks,failures);
    return failures?1:0;
}
