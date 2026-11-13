#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
__attribute__((destructor)) static void fg238q_report(void){
 if(!atomic_load(&g_calls))return;
 fprintf(stderr,"FG238Q_BATCH_SPLIT_SEQ calls=%llu\n",atomic_load(&g_calls));}
static void g_fail(int64_t c){fprintf(stderr,"FG238Q validation code %lld\n",(long long)c);abort();}
static int64_t g_ld_i32(uint64_t d,int64_t i){return (int64_t)((const int32_t*)(uintptr_t)d)[i];}
static int64_t g_ld_u64(uint64_t d,int64_t i){return (int64_t)((const uint64_t*)(uintptr_t)d)[i];}
static void g_st_i32(uint64_t d,int64_t i,int64_t v){((int32_t*)(uintptr_t)d)[i]=(int32_t)v;}
#define fg238q_ld_i32 g_ld_i32
#define fg238q_ld_u64 g_ld_u64
#define fg238q_st_i32 g_st_i32
#define fg238q_fail   g_fail
#define gg_init_globals fg238q_init_globals
#define main fg238q_generated_main
#include "sq-generated.c"
#undef main
int64_t fg238q_plan_seq(int64_t n_tokens,int64_t n_ubatch,const int32_t*used,
                        const uint64_t*set_lo,const uint64_t*set_hi,int32_t*out){
 atomic_fetch_add(&g_calls,1);
 return gg_function_1(n_tokens,n_ubatch,(uint64_t)(uintptr_t)used,(uint64_t)(uintptr_t)set_lo,
                      (uint64_t)(uintptr_t)set_hi,(uint64_t)(uintptr_t)out,0);
}
int fg238q_enabled(void){const char*f=getenv("GGML_GOOSE_BATCH_SPLIT_SEQ");return !(f&&strcmp(f,"0")==0);}
