// Host planning gate for the Goose IQ4_XS row dequantizer.
#include <stdio.h>
#include <stdint.h>
#include <setjmp.h>
#include <string.h>

static uint64_t G[8][32];
static int g_launches;
static int g_fail_code;
static jmp_buf g_jmp;

int64_t fg253h_plan(int64_t p){ return p; }
uint64_t fg253h_get(int64_t p,int64_t k,int64_t i){ return ((uint64_t*)(uintptr_t)p)[k*32+i]; }
void fg253h_put(int64_t p,int64_t k,int64_t i,uint64_t v){ ((uint64_t*)(uintptr_t)p)[k*32+i]=v; }
int64_t fg253h_launch(int64_t p){ (void)p; ++g_launches; return 0; }
void fg253h_fail(int64_t c){ g_fail_code=(int)c; longjmp(g_jmp,1); }
uint8_t fg253dh_run(int64_t p);

#include "host-unit.c"

static int checks, failures;
static void ck(int ok,const char*what){ ++checks; if(!ok){ ++failures; printf("FAIL %s\n",what); } }

static void reset(uint64_t src,uint64_t k,uint64_t dst){
    memset(G,0,sizeof G); g_launches=0; g_fail_code=0;
    G[0][0]=src; G[0][1]=k; G[0][2]=dst;
}

int main(void){
    const uint64_t ks[] = { 256, 1024, 16384 };
    const uint64_t nbs[] = { 1, 4, 64 };
    for (int i = 0; i < 3; ++i) {
        reset(0x1000, ks[i], 0x2000);
        if (setjmp(g_jmp)) { ck(0,"valid plan raised fail"); continue; }
        ck(fg253dh_run((int64_t)(uintptr_t)G)==1,"run returned true");
        ck(g_launches==1,"one launch");
        ck(G[1][0]==nbs[i],"grid nb");
        ck(G[1][1]==32,"block 32");
    }
    reset(0,256,0x2000);
    if (!setjmp(g_jmp)) { fg253dh_run((int64_t)(uintptr_t)G); ck(0,"null src accepted"); }
    else { ck(g_fail_code==20,"null src code 20"); ck(g_launches==0,"no launch on null src"); }

    reset(0x1000,0,0x2000);
    if (!setjmp(g_jmp)) { fg253dh_run((int64_t)(uintptr_t)G); ck(0,"zero k accepted"); }
    else { ck(g_fail_code==21,"zero k code 21"); ck(g_launches==0,"no launch on zero k"); }

    reset(0x1000,257,0x2000);
    if (!setjmp(g_jmp)) { fg253dh_run((int64_t)(uintptr_t)G); ck(0,"k=257 accepted"); }
    else { ck(g_fail_code==30,"k=257 code 30"); ck(g_launches==0,"no launch on k=257"); }

    reset(0x1000,0x100000000ull,0x2000);
    if (!setjmp(g_jmp)) { fg253dh_run((int64_t)(uintptr_t)G); ck(0,"oversize k accepted"); }
    else { ck(g_fail_code==22,"oversize k code 22"); ck(g_launches==0,"no launch on oversize k"); }

    printf("FG253H_HOST_FIXTURE checks=%d failures=%d\n",checks,failures);
    return failures?1:0;
}
