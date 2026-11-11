// Host planning gate for the Goose cooperative softmax host module.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>

static uint64_t G[8][32];
static int g_launches, g_fail_code;
static jmp_buf g_jmp;

int64_t fg285h_plan(int64_t p){ return p; }
uint64_t fg285h_get(int64_t p,int64_t k,int64_t i){ return ((uint64_t*)(uintptr_t)p)[k*32+i]; }
void fg285h_put(int64_t p,int64_t k,int64_t i,uint64_t v){ ((uint64_t*)(uintptr_t)p)[k*32+i]=v; }
int64_t fg285h_launch(int64_t p){ (void)p; ++g_launches; return 0; }
void fg285h_fail(int64_t c){ g_fail_code=(int)c; longjmp(g_jmp,1); }
float fg285h_u32f(uint32_t b){ float f; memcpy(&f,&b,4); return f; }
uint8_t fg285dh_run(int64_t p);
#include "host-unit.c"

static int checks, failures;
static void ck(int ok,const char*what){ ++checks; if(!ok){ ++failures; printf("FAIL %s\n",what); } }
static uint32_t bits(float f){ uint32_t u; memcpy(&u,&f,4); return u; }

static void reset(uint64_t dst,uint64_t x,int64_t ncols,int64_t nrows,int64_t smpbo,int64_t nsm,
                  int64_t coop,float scale,float maxb){
    memset(G,0,sizeof G); g_launches=0; g_fail_code=0;
    G[0][0]=dst; G[0][1]=x; G[0][2]=(uint64_t)ncols; G[0][3]=(uint64_t)nrows;
    G[0][4]=(uint64_t)smpbo; G[0][5]=(uint64_t)nsm; G[0][6]=(uint64_t)coop;
    G[0][7]=bits(scale); G[0][8]=bits(maxb);
}

int main(void){
    reset(0x1000,0x2000,20000,1,1000,46,1,1.0f,0.0f);
    if (setjmp(g_jmp)) { ck(0,"valid plan raised fail"); }
    else {
        ck(fg285dh_run((int64_t)(uintptr_t)G)==1,"run returned true");
        ck(g_launches==1,"one launch");
        ck((int64_t)G[1][0]==256,"block 256");
        ck((int64_t)G[1][1]==46,"grid nsm");
        ck((int64_t)G[1][2]==256,"shared 256");
        ck((int64_t)G[1][3]==1,"coop flag");
    }

    // shared would fit: not the cooperative branch
    reset(0x1000,0x2000,20000,1,200000,46,1,1.0f,0.0f);
    if (!setjmp(g_jmp)) { fg285dh_run((int64_t)(uintptr_t)G); ck(0,"fitting shared accepted"); }
    else { ck(g_fail_code==25,"fitting shared code 25"); ck(g_launches==0,"no launch"); }

    reset(0x1000,0x2000,20000,1,1000,46,0,1.0f,0.0f);
    if (!setjmp(g_jmp)) { fg285dh_run((int64_t)(uintptr_t)G); ck(0,"no coop support accepted"); }
    else { ck(g_fail_code==26,"no coop support code 26"); ck(g_launches==0,"no launch"); }

    reset(0x1000,0x2000,8000,1,1000,46,1,1.0f,0.0f);
    if (!setjmp(g_jmp)) { fg285dh_run((int64_t)(uintptr_t)G); ck(0,"ratio<=8192 accepted"); }
    else { ck(g_fail_code==27,"ratio code 27"); ck(g_launches==0,"no launch"); }

    reset(0x1000,0x2000,20000,1,1000,46,1,0.5f,0.0f);
    if (!setjmp(g_jmp)) { fg285dh_run((int64_t)(uintptr_t)G); ck(0,"scale!=1 accepted"); }
    else { ck(g_fail_code==28,"scale code 28"); ck(g_launches==0,"no launch"); }

    reset(0x1000,0x2000,20000,1,1000,46,1,1.0f,8.0f);
    if (!setjmp(g_jmp)) { fg285dh_run((int64_t)(uintptr_t)G); ck(0,"max_bias!=0 accepted"); }
    else { ck(g_fail_code==29,"max_bias code 29"); ck(g_launches==0,"no launch"); }

    printf("FG285H_HOST_FIXTURE checks=%d failures=%d\n",checks,failures);
    return failures?1:0;
}
