#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void fg252s_fail(int64_t c){fprintf(stderr,"FG252S_FAIL code=%lld\n",(long long)c);abort();}
static void fg252s_put(int64_t p,int64_t k,int64_t i,uint64_t v){((int64_t*)(uintptr_t)p)[k*16+i]=(int64_t)v;}
static uint64_t fg252s_get(int64_t p,int64_t k,int64_t i){return (uint64_t)((const int64_t*)(uintptr_t)p)[k*16+i];}
#define main fg252ds_generated_main
#include "host-qualified.c"
#undef main
int64_t fg252s_discard(int64_t n_keep,int64_t n_left,int64_t param){
    int64_t pkt[2*16]={0}; pkt[0]=n_keep; pkt[1]=n_left; pkt[2]=param;
    (void) fg252ds_gg_function_1((int64_t)(uintptr_t)pkt,1);
    return pkt[16+0];
}
