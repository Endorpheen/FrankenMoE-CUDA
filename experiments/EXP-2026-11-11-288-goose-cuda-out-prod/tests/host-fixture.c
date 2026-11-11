// Host policy gate for the Goose out-prod plan.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>

static uint64_t G[8][32];
static int g_fail_code;
static jmp_buf g_jmp;

int64_t fg288h_plan(int64_t p){ return p; }
uint64_t fg288h_get(int64_t p,int64_t k,int64_t i){ return ((uint64_t*)(uintptr_t)p)[k*32+i]; }
void fg288h_put(int64_t p,int64_t k,int64_t i,uint64_t v){ ((uint64_t*)(uintptr_t)p)[k*32+i]=v; }
void fg288h_fail(int64_t c){ g_fail_code=(int)c; longjmp(g_jmp,1); }
uint8_t fg288dh_run(int64_t p);
#include "host-unit.c"

static int checks, failures;
static void ck(int ok,const char*what){ ++checks; if(!ok){ ++failures; printf("FAIL %s\n",what); } }

struct In {
    int64_t ne0,ne1,ne2,ne3, ne00,ne01,ne02,ne03, ne10,ne11,ne12,ne13,
            nb01,nb10,nb11, nb02,nb03,nb12,nb13, nb1,nb2,nb3, tsz;
};
static void load(struct In in){
    memset(G,0,sizeof G); g_fail_code=0;
    uint64_t v[23]={ (uint64_t)in.ne0,(uint64_t)in.ne1,(uint64_t)in.ne2,(uint64_t)in.ne3,
        (uint64_t)in.ne00,(uint64_t)in.ne01,(uint64_t)in.ne02,(uint64_t)in.ne03,
        (uint64_t)in.ne10,(uint64_t)in.ne11,(uint64_t)in.ne12,(uint64_t)in.ne13,
        (uint64_t)in.nb01,(uint64_t)in.nb10,(uint64_t)in.nb11,
        (uint64_t)in.nb02,(uint64_t)in.nb03,(uint64_t)in.nb12,(uint64_t)in.nb13,
        (uint64_t)in.nb1,(uint64_t)in.nb2,(uint64_t)in.nb3,(uint64_t)in.tsz };
    for (int i=0;i<23;++i) G[0][i]=v[i];
}
static int64_t out(int i){ return (int64_t)G[1][i]; }

int main(void){
    // single GEMM: ne2 == ne3 == 1, contiguous src1
    struct In a = {4,3,1,1, 4,3,1,1, 3,3,1,1, 12,4,12, 12,12,12,12, 12,12,12, 4};
    load(a);
    if (setjmp(g_jmp)) { printf("code=%d\n",g_fail_code); ck(0,"single gemm rejected"); }
    else {
        ck(fg288dh_run((int64_t)(uintptr_t)G)==1,"run true");
        ck(out(0)==2,"branch single"); ck(out(1)==3,"lda"); ck(out(2)==3,"ldc"); ck(out(3)==3,"ldb");
        ck(out(4)==1,"op T"); ck(out(5)==1,"batch 1");
        ck(out(6)==3&&out(7)==3&&out(8)==3&&out(9)==3&&out(10)==3&&out(11)==3,"strides");
        ck(out(12)==1&&out(13)==1,"dps");
        ck(out(14)==1&&out(15)==1,"grid");
    }
    // strided batched: dps2 == 1 and ne2 > 1
    struct In b = {4,3,8,1, 4,3,8,1, 3,3,8,1, 12,4,12, 12,12,12,12, 12,12,12, 4};
    load(b);
    if (setjmp(g_jmp)) { ck(0,"strided batched rejected"); }
    else { fg288dh_run((int64_t)(uintptr_t)G);
        ck(out(0)==0,"branch strided"); ck(out(5)==8,"batch ne2"); ck(out(12)==1,"dps2 1");
        ck(out(14)==1,"grid x 1"); }
    // batched pointers because dps2 > 1
    struct In c = {4,3,8,2, 4,3,2,2, 3,3,8,2, 12,4,12, 12,12,12,12, 12,12,12, 4};
    load(c);
    if (setjmp(g_jmp)) { ck(0,"batched ptrs rejected"); }
    else { fg288dh_run((int64_t)(uintptr_t)G);
        ck(out(0)==1,"branch ptrs"); ck(out(5)==16,"batch ne2*ne3"); ck(out(12)==4,"dps2 4");
        ck(out(13)==1,"dps3 1"); ck(out(14)==1&&out(15)==1,"grid"); }
    // batched because ne3 > 1 while ne2 == 1
    struct In d = {4,3,1,5, 4,3,1,1, 3,3,1,5, 12,4,12, 12,12,12,12, 12,12,12, 4};
    load(d);
    if (setjmp(g_jmp)) { ck(0,"batched ne3 rejected"); }
    else { fg288dh_run((int64_t)(uintptr_t)G);
        ck(out(0)==1,"branch ne3"); ck(out(5)==5,"batch ne3"); ck(out(13)==5,"dps3 5"); }
    // transposed src1: nb10 == 12, nb11 == 4
    struct In e = {4,3,8,1, 4,3,8,1, 3,3,8,1, 12,12,4, 12,12,12,12, 12,12,12, 4};
    load(e);
    if (setjmp(g_jmp)) { ck(0,"transposed rejected"); }
    else { fg288dh_run((int64_t)(uintptr_t)G);
        ck(out(0)==0,"branch strided"); ck(out(3)==3,"ldb from nb10"); ck(out(4)==0,"op N"); }
    // rejections
    struct In f=a; f.ne11=4;   load(f); if(!setjmp(g_jmp)){ fg288dh_run((int64_t)(uintptr_t)G); ck(0,"ne01!=ne11 accepted"); } else ck(g_fail_code==40,"code 40");
    struct In h=a; h.ne02=3;   load(h); if(!setjmp(g_jmp)){ fg288dh_run((int64_t)(uintptr_t)G); ck(0,"ne2%ne02 accepted"); } else ck(g_fail_code==43,"code 43");
    struct In i2=b; i2.nb10=8; load(i2); if(!setjmp(g_jmp)){ fg288dh_run((int64_t)(uintptr_t)G); ck(0,"bad src1 stride accepted"); } else ck(g_fail_code==47,"code 47");
    printf("FG288H_HOST_FIXTURE checks=%d failures=%d\n",checks,failures);
    return failures?1:0;
}
