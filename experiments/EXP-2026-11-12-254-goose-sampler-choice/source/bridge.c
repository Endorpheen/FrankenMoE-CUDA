#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void fg254s_fail(int64_t c){fprintf(stderr,"FG254S_FAIL code=%lld\n",(long long)c);abort();}
static void fg254s_put(int64_t p,int64_t k,int64_t i,uint64_t v){((int64_t*)(uintptr_t)p)[k*16+i]=(int64_t)v;}
static uint64_t fg254s_get(int64_t p,int64_t k,int64_t i){return (uint64_t)((const int64_t*)(uintptr_t)p)[k*16+i];}
static int64_t fg254s_flt_bits(int64_t p,int64_t k,int64_t d){(void)p;(void)k;return d;}
#define main fg254ds_generated_main
#include "host-qualified.c"
#undef main
int64_t fg254s_greedy(int64_t n_probs,int64_t n_samplers,int64_t last,int64_t top_k){
    int64_t pkt[2*16]={0}; pkt[0]=n_probs; pkt[1]=n_samplers; pkt[2]=last; pkt[3]=top_k; pkt[4]=0; pkt[5]=0;
    (void) fg254ds_gg_function_1((int64_t)(uintptr_t)pkt,1);
    return pkt[16+0];
}
