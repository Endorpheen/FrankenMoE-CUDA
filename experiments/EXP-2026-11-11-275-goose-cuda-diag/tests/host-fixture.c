// Host planning gate for the Goose diag host module.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>

static uint64_t G[8][32];
static int g_launches, g_fail_code;
static jmp_buf g_jmp;

int64_t fg275h_plan(int64_t p){ return p; }
uint64_t fg275h_get(int64_t p,int64_t k,int64_t i){ return ((uint64_t*)(uintptr_t)p)[k*32+i]; }
void fg275h_put(int64_t p,int64_t k,int64_t i,uint64_t v){ ((uint64_t*)(uintptr_t)p)[k*32+i]=v; }
int64_t fg275h_launch(int64_t p){ (void)p; ++g_launches; return 0; }
void fg275h_fail(int64_t c){ g_fail_code=(int)c; longjmp(g_jmp,1); }
uint8_t fg275dh_run(int64_t p);
#include "host-unit.c"

static int checks, failures;
static void ck(int ok,const char*what){ ++checks; if(!ok){ ++failures; printf("FAIL %s\n",what); } }

static void reset(uint64_t dst,uint64_t src,int64_t ne0,int64_t ne1,int64_t ne2,int64_t ne3,int64_t ne){
    memset(G,0,sizeof G); g_launches=0; g_fail_code=0;
    G[0][0]=dst; G[0][1]=src;
    G[0][2]=(uint64_t)ne0; G[0][3]=(uint64_t)ne1; G[0][4]=(uint64_t)ne2; G[0][5]=(uint64_t)ne3;
    G[0][6]=(uint64_t)ne;
}

static void valid(int64_t ne0,int64_t ne1,int64_t ne2,int64_t ne3){
    const int64_t ne=ne0*ne1*ne2*ne3;
    reset(0x1000,0x2000,ne0,ne1,ne2,ne3,ne);
    if (setjmp(g_jmp)) { ck(0,"valid plan raised fail"); return; }
    ck(fg275dh_run((int64_t)(uintptr_t)G)==1,"run returned true");
    ck(g_launches==1,"one launch");
    ck((int64_t)G[1][0]==(ne+255)/256,"num_blocks");
    ck((int64_t)G[1][1]==256,"block 256");
}

int main(void){
    valid(4,4,1,1);
    valid(8,8,2,2);
    valid(7,7,1,3);
    valid(256,256,1,1);

    reset(0,0x2000,4,4,1,1,16);
    if (!setjmp(g_jmp)) { fg275dh_run((int64_t)(uintptr_t)G); ck(0,"null dst accepted"); }
    else { ck(g_fail_code==20,"null dst code 20"); ck(g_launches==0,"no launch"); }

    reset(0x1000,0x2000,4,5,1,1,20);
    if (!setjmp(g_jmp)) { fg275dh_run((int64_t)(uintptr_t)G); ck(0,"ne1!=ne0 accepted"); }
    else { ck(g_fail_code==21,"ne1!=ne0 code 21"); ck(g_launches==0,"no launch"); }

    reset(0x1000,0x2000,4,4,1,1,15);
    if (!setjmp(g_jmp)) { fg275dh_run((int64_t)(uintptr_t)G); ck(0,"product mismatch accepted"); }
    else { ck(g_fail_code==22,"product mismatch code 22"); ck(g_launches==0,"no launch"); }

    reset(0x1000,0x2000,4,4,1,1,17);
    if (!setjmp(g_jmp)) { fg275dh_run((int64_t)(uintptr_t)G); ck(0,"oversize element count accepted"); }
    else { ck(g_fail_code==22,"oversize count code 22"); ck(g_launches==0,"no launch"); }

    reset(0x1000,0x2000,0,0,1,1,0);
    if (!setjmp(g_jmp)) { fg275dh_run((int64_t)(uintptr_t)G); ck(0,"zero dim accepted"); }
    else { ck(g_fail_code==21,"zero dim code 21"); ck(g_launches==0,"no launch"); }

    printf("FG275H_HOST_FIXTURE checks=%d failures=%d\n",checks,failures);
    return failures?1:0;
}
