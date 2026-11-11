#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static void fg256s_fail(int64_t c){fprintf(stderr,"FG256S_FAIL code=%lld\n",(long long)c);abort();}
static void fg256s_put(int64_t p,int64_t k,int64_t i,uint64_t v){((int64_t*)(uintptr_t)p)[k*16+i]=(int64_t)v;}
static uint64_t fg256s_get(int64_t p,int64_t k,int64_t i){return (uint64_t)((const int64_t*)(uintptr_t)p)[k*16+i];}
static int64_t fg256s_f2b(float x){int32_t u;memcpy(&u,&x,4);return (int64_t)u;}
static float fg256s_b2f(int64_t x){float f;int32_t u=(int32_t)x;memcpy(&f,&u,4);return f;}
static float fg256s_fma(float a,float b,float c){return fmaf(a,b,c);}
static float fg256s_mul(float a,float b){return a*b;}
#define main fg256s_generated_main
#include "host-qualified.c"
#undef main
void fg256s_coeff(int64_t ax,int64_t ay,int64_t bx,int64_t by,int64_t out[2]){
    int64_t pkt[2*16]={0}; pkt[0]=ax; pkt[1]=ay; pkt[2]=bx; pkt[3]=by;
    (void) fg256s_gg_function_1((int64_t)(uintptr_t)pkt,1);
    out[0]=pkt[16+0]; out[1]=pkt[16+1];
}
