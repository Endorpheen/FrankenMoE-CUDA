// Host planning gate for the Goose arange host module.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <setjmp.h>

static uint64_t G[8][32];
static int g_launches;
static int g_fail_code;
static jmp_buf g_jmp;

int64_t fg274h_plan(int64_t p){ return p; }
uint64_t fg274h_get(int64_t p,int64_t k,int64_t i){ return ((uint64_t*)(uintptr_t)p)[k*32+i]; }
void fg274h_put(int64_t p,int64_t k,int64_t i,uint64_t v){ ((uint64_t*)(uintptr_t)p)[k*32+i]=v; }
int64_t fg274h_launch(int64_t p){ (void)p; ++g_launches; return 0; }
void fg274h_fail(int64_t c){ g_fail_code=(int)c; longjmp(g_jmp,1); }
float fg274h_u32f(uint32_t b){ float f; memcpy(&f,&b,4); return f; }
float fg274h_ceilf(float x){ return ceilf(x); }
uint8_t fg274dh_run(int64_t p);

#include "host-unit.c"

static int checks, failures;
static void ck(int ok,const char*what){ ++checks; if(!ok){ ++failures; printf("FAIL %s\n",what); } }

static uint32_t bits(float f){ uint32_t u; memcpy(&u,&f,4); return u; }

static void reset(uint64_t dst,uint64_t ne0,uint64_t ne,float start,float step,float stop){
    memset(G,0,sizeof G); g_launches=0; g_fail_code=0;
    G[0][0]=dst; G[0][1]=ne0; G[0][2]=ne;
    G[0][3]=bits(start); G[0][4]=bits(step); G[0][5]=bits(stop);
}

static void valid_case(uint64_t ne0,float start,float step,float stop,int expect_nb){
    int64_t steps=(int64_t)ceilf((stop-start)/step);
    reset(0x1000,ne0,(uint64_t)steps,start,step,stop);
    if (setjmp(g_jmp)) { ck(0,"valid plan raised fail"); return; }
    ck(fg274dh_run((int64_t)(uintptr_t)G)==1,"run returned true");
    ck(g_launches==1,"one launch");
    ck((int)G[1][0]==expect_nb,"num_blocks");
    ck((int)G[1][1]==256,"block 256");
}

int main(void){
    valid_case(1,0.0f,1.0f,1.0f,1);
    valid_case(256,0.0f,1.0f,256.0f,1);
    valid_case(257,0.0f,1.0f,257.0f,2);
    valid_case(1000,-3.5f,0.25f,246.5f,4);
    valid_case(100,0.1f,0.1f,10.0f,1);

    // step == 0 -> code 22
    reset(0x1000,10,10,0.0f,0.0f,10.0f);
    if (!setjmp(g_jmp)) { fg274dh_run((int64_t)(uintptr_t)G); ck(0,"zero step accepted"); }
    else { ck(g_fail_code==22,"zero step code 22"); ck(g_launches==0,"no launch on zero step"); }

    // steps mismatch -> code 23
    reset(0x1000,10,11,0.0f,1.0f,10.0f);
    if (!setjmp(g_jmp)) { fg274dh_run((int64_t)(uintptr_t)G); ck(0,"steps mismatch accepted"); }
    else { ck(g_fail_code==23,"steps mismatch code 23"); ck(g_launches==0,"no launch on mismatch"); }

    // null dst -> code 20
    reset(0,10,10,0.0f,1.0f,10.0f);
    if (!setjmp(g_jmp)) { fg274dh_run((int64_t)(uintptr_t)G); ck(0,"null dst accepted"); }
    else { ck(g_fail_code==20,"null dst code 20"); ck(g_launches==0,"no launch on null dst"); }

    // ne0 == 0 -> code 21
    reset(0x1000,0,0,0.0f,1.0f,0.0f);
    if (!setjmp(g_jmp)) { fg274dh_run((int64_t)(uintptr_t)G); ck(0,"zero ne0 accepted"); }
    else { ck(g_fail_code==21,"zero ne0 code 21"); ck(g_launches==0,"no launch on zero ne0"); }

    printf("FG274H_HOST_FIXTURE checks=%d failures=%d\n",checks,failures);
    return failures?1:0;
}
