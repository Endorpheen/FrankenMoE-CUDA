// Host planning gate for the Goose snake/softcap host module.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>

static uint64_t G[8][32];
static int g_launches, g_fail_code;
static jmp_buf g_jmp;

int64_t fg286h_plan(int64_t p){ return p; }
uint64_t fg286h_get(int64_t p,int64_t k,int64_t i){ return ((uint64_t*)(uintptr_t)p)[k*32+i]; }
void fg286h_put(int64_t p,int64_t k,int64_t i,uint64_t v){ ((uint64_t*)(uintptr_t)p)[k*32+i]=v; }
int64_t fg286h_launch(int64_t p){ (void)p; ++g_launches; return 0; }
void fg286h_fail(int64_t c){ g_fail_code=(int)c; longjmp(g_jmp,1); }
uint8_t fg286dh_run(int64_t p);
#include "host-unit.c"

static int checks, failures;
static void ck(int ok,const char*what){ ++checks; if(!ok){ ++failures; printf("FAIL %s\n",what); } }

static void reset(uint64_t dst,uint64_t x,int64_t n,int64_t mode){
    memset(G,0,sizeof G); g_launches=0; g_fail_code=0;
    G[0][0]=dst; G[0][1]=x; G[0][2]=(uint64_t)n; G[0][3]=(uint64_t)mode;
}

int main(void){
    const int64_t ns[4]={1,256,257,100000};
    for (int i=0;i<4;++i){
        reset(0x1000,0x2000,ns[i],0);
        if (setjmp(g_jmp)) { ck(0,"valid snake plan raised fail"); continue; }
        ck(fg286dh_run((int64_t)(uintptr_t)G)==1,"snake run");
        ck(g_launches==1,"snake launch");
        ck((int64_t)G[1][0]==256,"block 256");
        ck((int64_t)G[1][1]==(ns[i]+255)/256,"snake grid");
    }
    reset(0x1000,0x2000,1000,1);
    if (setjmp(g_jmp)) { ck(0,"valid softcap plan raised fail"); }
    else { ck(fg286dh_run((int64_t)(uintptr_t)G)==1,"softcap run"); ck((int64_t)G[1][1]==(1000+255)/256,"softcap grid"); }

    reset(0,0x2000,10,0);
    if (!setjmp(g_jmp)) { fg286dh_run((int64_t)(uintptr_t)G); ck(0,"null dst accepted"); }
    else { ck(g_fail_code==20,"null dst code 20"); ck(g_launches==0,"no launch"); }
    reset(0x1000,0x2000,0,0);
    if (!setjmp(g_jmp)) { fg286dh_run((int64_t)(uintptr_t)G); ck(0,"zero n accepted"); }
    else { ck(g_fail_code==21,"zero n code 21"); ck(g_launches==0,"no launch"); }
    reset(0x1000,0x2000,10,2);
    if (!setjmp(g_jmp)) { fg286dh_run((int64_t)(uintptr_t)G); ck(0,"bad mode accepted"); }
    else { ck(g_fail_code==22,"bad mode code 22"); ck(g_launches==0,"no launch"); }

    printf("FG286H_HOST_FIXTURE checks=%d failures=%d\n",checks,failures);
    return failures?1:0;
}
