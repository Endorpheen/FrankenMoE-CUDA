// Host planning gate for the Goose mmvf host module.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>

static uint64_t G[8][32];
static int g_launches, g_fail_code;
static jmp_buf g_jmp;

int64_t fg287h_plan(int64_t p){ return p; }
uint64_t fg287h_get(int64_t p,int64_t k,int64_t i){ return ((uint64_t*)(uintptr_t)p)[k*32+i]; }
void fg287h_put(int64_t p,int64_t k,int64_t i,uint64_t v){ ((uint64_t*)(uintptr_t)p)[k*32+i]=v; }
int64_t fg287h_launch(int64_t p){ (void)p; ++g_launches; return 0; }
void fg287h_fail(int64_t c){ g_fail_code=(int)c; longjmp(g_jmp,1); }
uint8_t fg287dh_run(int64_t p);
#include "host-unit.c"

static int checks, failures;
static void ck(int ok,const char*what){ ++checks; if(!ok){ ++failures; printf("FAIL %s\n",what); } }

static void reset(uint64_t dst,uint64_t x,int64_t ncols,int64_t nrows,int64_t nch,int64_t ns,int64_t warp,int64_t maxb){
    memset(G,0,sizeof G); g_launches=0; g_fail_code=0;
    G[0][0]=dst; G[0][1]=x; G[0][2]=(uint64_t)ncols; G[0][3]=(uint64_t)nrows;
    G[0][4]=(uint64_t)nch; G[0][5]=(uint64_t)ns; G[0][6]=(uint64_t)warp; G[0][7]=(uint64_t)maxb;
}

static int64_t ref_best(int64_t ncols,int64_t warp,int64_t maxb){
    int64_t best=warp, nb=(ncols+2*warp-1)/(2*warp);
    for (int64_t bs=2*warp; bs<=maxb; bs+=warp){ int64_t ni=(ncols+2*bs-1)/(2*bs); if (ni<nb){nb=ni;best=bs;} }
    return best;
}

int main(void){
    const int64_t ns[5]={4,256,512,2048,8192};
    for (int i=0;i<5;++i){
        const int64_t ncols=ns[i];
        reset(0x1000,0x2000,ncols,8,2,3,32,256);
        if (setjmp(g_jmp)) { ck(0,"valid plan raised fail"); continue; }
        ck(fg287dh_run((int64_t)(uintptr_t)G)==1,"run returned true");
        ck(g_launches==1,"one launch");
        ck((int64_t)G[1][0]==ref_best(ncols,32,256),"block best");
        ck((int64_t)G[1][1]==128,"shared 128");
        ck((int64_t)G[1][2]==8&&(int64_t)G[1][3]==2&&(int64_t)G[1][4]==3,"grid dims");
    }
    reset(0,0x2000,256,8,1,1,32,256);
    if (!setjmp(g_jmp)) { fg287dh_run((int64_t)(uintptr_t)G); ck(0,"null dst accepted"); }
    else { ck(g_fail_code==20,"null dst code 20"); ck(g_launches==0,"no launch"); }
    reset(0x1000,0x2000,0,8,1,1,32,256);
    if (!setjmp(g_jmp)) { fg287dh_run((int64_t)(uintptr_t)G); ck(0,"zero ncols accepted"); }
    else { ck(g_fail_code==21,"zero ncols code 21"); ck(g_launches==0,"no launch"); }
    reset(0x1000,0x2000,256,8,1,1,32,32);
    if (!setjmp(g_jmp)) { fg287dh_run((int64_t)(uintptr_t)G); ck(0,"small max_block accepted"); }
    else { ck(g_fail_code==22,"small max_block code 22"); ck(g_launches==0,"no launch"); }
    printf("FG287H_HOST_FIXTURE checks=%d failures=%d\n",checks,failures);
    return failures?1:0;
}
