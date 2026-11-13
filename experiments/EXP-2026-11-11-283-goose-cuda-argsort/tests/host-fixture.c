// Host planning gate for the Goose argsort host module.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>

static uint64_t G[8][32];
static int g_launches, g_fail_code;
static jmp_buf g_jmp;

int64_t fg283h_plan(int64_t p){ return p; }
uint64_t fg283h_get(int64_t p,int64_t k,int64_t i){ return ((uint64_t*)(uintptr_t)p)[k*32+i]; }
void fg283h_put(int64_t p,int64_t k,int64_t i,uint64_t v){ ((uint64_t*)(uintptr_t)p)[k*32+i]=v; }
int64_t fg283h_launch(int64_t p){ (void)p; ++g_launches; return 0; }
void fg283h_fail(int64_t c){ g_fail_code=(int)c; longjmp(g_jmp,1); }
uint8_t fg283dh_run(int64_t p);
#include "host-unit.c"

static int checks, failures;
static void ck(int ok,const char*what){ ++checks; if(!ok){ ++failures; printf("FAIL %s\n",what); } }

static void reset(uint64_t dst,uint64_t x,int64_t ncols,int64_t nrows,int64_t smpb){
    memset(G,0,sizeof G); g_launches=0; g_fail_code=0;
    G[0][0]=dst; G[0][1]=x; G[0][2]=(uint64_t)ncols; G[0][3]=(uint64_t)nrows; G[0][4]=(uint64_t)smpb;
}

int main(void){
    const int64_t ns[8]={1,2,3,5,16,100,300,1024};
    for (int i=0;i<8;++i){
        const int64_t ncols=ns[i];
        int64_t pad=1; while (pad<ncols) pad*=2;
        const int64_t blk=pad<1024?pad:1024;
        const int64_t shm=pad*4;
        reset(0x1000,0x2000,ncols,3,49152);
        if (setjmp(g_jmp)) { ck(0,"valid plan raised fail"); continue; }
        ck(fg283dh_run((int64_t)(uintptr_t)G)==1,"run returned true");
        ck(g_launches==1,"one launch");
        ck((int64_t)G[1][0]==blk,"block");
        ck((int64_t)G[1][1]==shm,"shared bytes");
        ck((int64_t)G[1][2]==3,"grid nrows");
        ck((int64_t)G[1][3]==pad,"ncols_pad");
    }

    reset(0,0x2000,10,1,49152);
    if (!setjmp(g_jmp)) { fg283dh_run((int64_t)(uintptr_t)G); ck(0,"null dst accepted"); }
    else { ck(g_fail_code==20,"null dst code 20"); ck(g_launches==0,"no launch"); }

    reset(0x1000,0x2000,0,1,49152);
    if (!setjmp(g_jmp)) { fg283dh_run((int64_t)(uintptr_t)G); ck(0,"zero ncols accepted"); }
    else { ck(g_fail_code==21,"zero ncols code 21"); ck(g_launches==0,"no launch"); }

    reset(0x1000,0x2000,1024,1,1000);
    if (!setjmp(g_jmp)) { fg283dh_run((int64_t)(uintptr_t)G); ck(0,"shared overflow accepted"); }
    else { ck(g_fail_code==24,"shared overflow code 24"); ck(g_launches==0,"no launch"); }

    printf("FG283H_HOST_FIXTURE checks=%d failures=%d\n",checks,failures);
    return failures?1:0;
}
