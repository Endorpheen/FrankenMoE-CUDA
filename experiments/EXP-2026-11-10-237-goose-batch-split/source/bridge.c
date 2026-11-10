#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
__attribute__((destructor)) static void fg237s_report(void){
 if(!atomic_load(&g_calls))return;
 fprintf(stderr,"FG237S_BATCH_SPLIT calls=%llu\n",atomic_load(&g_calls));}
static void g_fail(int64_t c){fprintf(stderr,"FG237S validation code %lld\n",(long long)c);abort();}
static int64_t g_ld_i32(uint64_t d,int64_t i){return (int64_t)((const int32_t*)(uintptr_t)d)[i];}
static void g_st_i32(uint64_t d,int64_t i,int64_t v){((int32_t*)(uintptr_t)d)[i]=(int32_t)v;}
#define fg237s_ld_i32 g_ld_i32
#define fg237s_st_i32 g_st_i32
#define fg237s_fail   g_fail
#define gg_init_globals fg237s_init_globals
#define main fg237s_generated_main
#include "sp-generated.c"
#undef main
int64_t fg237s_plan_expand(int64_t n_seqs,int64_t n_ubatch,const int32_t*set_len,
                           const int32_t*cursor,int32_t*out_cursor){
 atomic_fetch_add(&g_calls,1);
 return gg_function_1(n_seqs,n_ubatch,(uint64_t)(uintptr_t)set_len,(uint64_t)(uintptr_t)cursor,
                      (uint64_t)(uintptr_t)out_cursor,0);
}
int fg237s_enabled(void){const char*f=getenv("GGML_GOOSE_BATCH_SPLIT");return !(f&&strcmp(f,"0")==0);}
