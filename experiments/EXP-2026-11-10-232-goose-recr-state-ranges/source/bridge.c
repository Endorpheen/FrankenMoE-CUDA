#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
__attribute__((destructor)) static void fg232r_report(void){
 if(!atomic_load(&g_calls))return;
 fprintf(stderr,"FG232R_RECR_STATE_RANGES calls=%llu\n",atomic_load(&g_calls));}
static void g_fail(int64_t c){fprintf(stderr,"FG232R validation code %lld\n",(long long)c);abort();}
static int64_t g_ld_i32(uint64_t d,int64_t i){return (int64_t)((const int32_t*)(uintptr_t)d)[i];}
static void g_st_i32(uint64_t d,int64_t i,int64_t v){((int32_t*)(uintptr_t)d)[i]=(int32_t)v;}
#define fg232r_ld_i32 g_ld_i32
#define fg232r_st_i32 g_st_i32
#define fg232r_fail   g_fail
#define gg_init_globals fg232r_init_globals
#define main fg232r_generated_main
#include "rng-generated.c"
#undef main
int64_t fg232r_plan_ranges(int64_t size,const int32_t*match_,const int32_t*rs,const int32_t*src,
                           int32_t*out_ranges,int32_t*out_data,int32_t*counts){
 atomic_fetch_add(&g_calls,1);
 return gg_function_1(size,(uint64_t)(uintptr_t)match_,(uint64_t)(uintptr_t)rs,(uint64_t)(uintptr_t)src,
                      (uint64_t)(uintptr_t)out_ranges,(uint64_t)(uintptr_t)out_data,
                      (uint64_t)(uintptr_t)counts,0);
}
int fg232r_enabled(void){const char*f=getenv("GGML_GOOSE_RECR_STATE_RANGES");return !(f&&strcmp(f,"0")==0);}
