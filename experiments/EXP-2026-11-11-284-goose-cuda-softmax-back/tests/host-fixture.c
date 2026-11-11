// Host planning gate for the Goose softmax-back host module.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>

static uint64_t G[8][32];
static int g_launches, g_fail_code;
static jmp_buf g_jmp;

int64_t fg284h_plan(int64_t p){ return p; }
uint64_t fg284h_get(int64_t p,int64_t k,int64_t i){ return ((uint64_t*)(uintptr_t)p)[k*32+i]; }
void fg284h_put(int64_t p,int64_t k,int64_t i,uint64_t v){ ((uint64_t*)(uintptr_t)p)[k*32+i]=v; }
int64_t fg284h_launch(int64_t p){ (void)p; ++g_launches; return 0; }
void fg284h_fail(int64_t c){ g_fail_code=(int)c; longjmp(g_jmp,1); }
uint8_t fg284dh_run(int64_t p);
#include "host-unit.c"

static int checks, failures;
static void ck(int ok,const char*what){ ++checks; if(!ok){ ++failures; printf("FAIL %s\n",what); } }

static void reset(uint64_t dst,uint64_t grad,uint64_t dstf,int64_t ncols,int64_t nrows){
    memset(G,0,sizeof G); g_launches=0; g_fail_code=0;
    G[0][0]=dst; G[0][1]=grad; G[0][2]=dstf; G[0][3]=(uint64_t)ncols; G[0][4]=(uint64_t)nrows;
}

int main(void){
    const int64_t ns[3]={1,100,4096};
    for (int i=0;i<3;++i){
        reset(0x1000,0x2000,0x3000,ns[i],5);
        if (setjmp(g_jmp)) { ck(0,"valid plan raised fail"); continue; }
        ck(fg284dh_run((int64_t)(uintptr_t)G)==1,"run returned true");
        ck(g_launches==1,"one launch");
        ck((int64_t)G[1][0]==32,"block 32");
        ck((int64_t)G[1][1]==5,"grid nrows");
    }
    reset(0,0x2000,0x3000,10,1);
    if (!setjmp(g_jmp)) { fg284dh_run((int64_t)(uintptr_t)G); ck(0,"null dst accepted"); }
    else { ck(g_fail_code==20,"null dst code 20"); ck(g_launches==0,"no launch"); }
    reset(0x1000,0x2000,0x3000,0,1);
    if (!setjmp(g_jmp)) { fg284dh_run((int64_t)(uintptr_t)G); ck(0,"zero ncols accepted"); }
    else { ck(g_fail_code==21,"zero ncols code 21"); ck(g_launches==0,"no launch"); }
    printf("FG284H_HOST_FIXTURE checks=%d failures=%d\n",checks,failures);
    return failures?1:0;
}
