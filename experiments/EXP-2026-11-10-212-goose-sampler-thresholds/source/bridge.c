#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_minp,g_topp,g_topk;
__attribute__((destructor)) static void fg212s_report(void){
 if(!atomic_load(&g_minp)&&!atomic_load(&g_topp)&&!atomic_load(&g_topk))return;
 fprintf(stderr,"FG212S_SAMPLER min_p=%llu top_p=%llu top_k=%llu\n",
   atomic_load(&g_minp),atomic_load(&g_topp),atomic_load(&g_topk));}
static void g_fail(int64_t c){fprintf(stderr,"FG212S validation code %lld\n",(long long)c);abort();}
static float g_ld_f32(uint64_t d,int64_t b){return *(const float*)(uintptr_t)(d+(uint64_t)b);}
static void g_st_i64(uint64_t d,int64_t b,int64_t v){*(int64_t*)(uintptr_t)(d+(uint64_t)b)=v;}
static float g_logf(float x){return logf(x);}
#define fg212s_ld_f32 g_ld_f32
#define fg212s_st_i64 g_st_i64
#define fg212s_log    g_logf
#define fg212s_fail   g_fail
#define gg_init_globals fg212s_init_globals
#define main fg212s_generated_main
#include "samp-generated.c"
#undef main
int64_t fg212s_top_k_cut(int64_t k,int64_t n){atomic_fetch_add(&g_topk,1);return gg_function_1(k,n,0);}
int64_t fg212s_min_p_scan(uint64_t lg,int64_t n,float p,int64_t mk,uint64_t keep){
 atomic_fetch_add(&g_minp,1);return gg_function_4(lg,n,p,mk,keep,0);}
int64_t fg212s_top_p_scan(uint64_t pb,int64_t n,float p,int64_t mk){
 atomic_fetch_add(&g_topp,1);return gg_function_8(pb,n,p,mk,0);}
int fg212s_enabled(void){const char*f=getenv("GGML_GOOSE_SAMPLER");return !(f&&strcmp(f,"0")==0);}
