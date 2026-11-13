// Host planning gate for the Goose Q6_K row dequantizer.
//
// The generated host module owns the element-count contract and the launch
// geometry: nb > 0, nb <= INT32_MAX, grid = nb, block = 64. The bridge functions
// below stand in for the CUDA runtime so the plan can be checked without a DSO.
#include <stdio.h>
#include <stdint.h>
#include <setjmp.h>
#include <string.h>

static uint64_t G[8][32];
static int g_launches;
static int g_fail_code;
static jmp_buf g_jmp;

int64_t fg248h_plan(int64_t p){ return p; }
uint64_t fg248h_get(int64_t p,int64_t k,int64_t i){ return ((uint64_t*)(uintptr_t)p)[k*32+i]; }
void fg248h_put(int64_t p,int64_t k,int64_t i,uint64_t v){ ((uint64_t*)(uintptr_t)p)[k*32+i]=v; }
int64_t fg248h_launch(int64_t p){ (void)p; ++g_launches; return 0; }
void fg248h_fail(int64_t c){ g_fail_code=(int)c; longjmp(g_jmp,1); }
uint8_t fg248dh_run(int64_t p);

#include "host-unit.c"

static int checks, failures;
static void ck(int ok,const char*what){ ++checks; if(!ok){ ++failures; printf("FAIL %s\n",what); } }

static void reset(uint64_t src,uint64_t dst,uint64_t nb){
    memset(G,0,sizeof G); g_launches=0; g_fail_code=0;
    G[0][0]=src; G[0][1]=nb; G[0][2]=dst;
}

int main(void){
    // Valid plan: grid = nb, block = 64, one launch.
    for (uint64_t nb = 1; nb <= 4; ++nb) {
        reset(0x1000,0x2000,nb);
        if (setjmp(g_jmp)) { ck(0,"valid plan raised fail"); continue; }
        ck(fg248dh_run((int64_t)(uintptr_t)G)==1,"run returned true");
        ck(g_launches==1,"one launch");
        ck(G[1][0]==nb,"grid nb");
        ck(G[1][1]==64,"block 64");
    }
    // Rejections before any launch.
    reset(0,0x2000,1);
    if (!setjmp(g_jmp)) { fg248dh_run((int64_t)(uintptr_t)G); ck(0,"null src accepted"); }
    else { ck(g_fail_code==20,"null src code 20"); ck(g_launches==0,"no launch on null src"); }

    reset(0x1000,0x2000,0);
    if (!setjmp(g_jmp)) { fg248dh_run((int64_t)(uintptr_t)G); ck(0,"zero nb accepted"); }
    else { ck(g_fail_code==21,"zero nb code 21"); ck(g_launches==0,"no launch on zero nb"); }

    reset(0x1000,0x2000,0x80000000ull);
    if (!setjmp(g_jmp)) { fg248dh_run((int64_t)(uintptr_t)G); ck(0,"oversize nb accepted"); }
    else { ck(g_fail_code==22,"oversize nb code 22"); ck(g_launches==0,"no launch on oversize nb"); }

    printf("FG248H_HOST_FIXTURE checks=%d failures=%d\n",checks,failures);
    return failures?1:0;
}
