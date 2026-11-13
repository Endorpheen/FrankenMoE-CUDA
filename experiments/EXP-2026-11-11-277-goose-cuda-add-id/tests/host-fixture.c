// Host planning gate for the Goose add_id host module.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>

static uint64_t G[8][32];
static int g_launches, g_fail_code;
static jmp_buf g_jmp;

int64_t fg277h_plan(int64_t p){ return p; }
uint64_t fg277h_get(int64_t p,int64_t k,int64_t i){ return ((uint64_t*)(uintptr_t)p)[k*32+i]; }
void fg277h_put(int64_t p,int64_t k,int64_t i,uint64_t v){ ((uint64_t*)(uintptr_t)p)[k*32+i]=v; }
int64_t fg277h_launch(int64_t p){ (void)p; ++g_launches; return 0; }
void fg277h_fail(int64_t c){ g_fail_code=(int)c; longjmp(g_jmp,1); }
uint8_t fg277dh_run(int64_t p);
#include "host-unit.c"

static int checks, failures;
static void ck(int ok,const char*what){ ++checks; if(!ok){ ++failures; printf("FAIL %s\n",what); } }

static void reset(uint64_t dst,uint64_t s0,uint64_t s1,uint64_t s2,int64_t ne0,int64_t ne1,int64_t ne2,int64_t nb00){
    memset(G,0,sizeof G); g_launches=0; g_fail_code=0;
    G[0][0]=dst; G[0][1]=s0; G[0][2]=s1; G[0][3]=s2;
    G[0][4]=(uint64_t)ne0; G[0][5]=(uint64_t)ne1; G[0][6]=(uint64_t)ne2;
    G[0][7]=(uint64_t)nb00; G[0][8]=4; G[0][9]=4;
}

int main(void){
    const int64_t shapes[4][3]={{4,2,2},{768,3,1},{1000,2,4},{1,1,1}};
    for (int i=0;i<4;++i){
        const int64_t ne0=shapes[i][0],ne1=shapes[i][1],ne2=shapes[i][2];
        const int64_t thr=ne0<768?ne0:768;
        reset(0x1000,0x2000,0x3000,0x4000,ne0,ne1,ne2,4);
        if (setjmp(g_jmp)) { ck(0,"valid plan raised fail"); continue; }
        ck(fg277dh_run((int64_t)(uintptr_t)G)==1,"run returned true");
        ck(g_launches==1,"one launch");
        ck((int64_t)G[1][0]==thr,"threads");
        ck((int64_t)G[1][1]==ne1,"grid x = ne1");
        ck((int64_t)G[1][2]==ne2,"grid y = ne2");
    }

    reset(0x1000,0x2000,0x3000,0,4,2,2,4);
    if (!setjmp(g_jmp)) { fg277dh_run((int64_t)(uintptr_t)G); ck(0,"null src2 accepted"); }
    else { ck(g_fail_code==20,"null src2 code 20"); ck(g_launches==0,"no launch"); }

    reset(0x1000,0x2000,0x3000,0x4000,0,2,2,4);
    if (!setjmp(g_jmp)) { fg277dh_run((int64_t)(uintptr_t)G); ck(0,"zero ne0 accepted"); }
    else { ck(g_fail_code==21,"zero ne0 code 21"); ck(g_launches==0,"no launch"); }

    reset(0x1000,0x2000,0x3000,0x4000,4,2,2,8);
    if (!setjmp(g_jmp)) { fg277dh_run((int64_t)(uintptr_t)G); ck(0,"nb00!=4 accepted"); }
    else { ck(g_fail_code==22,"nb00!=4 code 22"); ck(g_launches==0,"no launch"); }

    printf("FG277H_HOST_FIXTURE checks=%d failures=%d\n",checks,failures);
    return failures?1:0;
}
