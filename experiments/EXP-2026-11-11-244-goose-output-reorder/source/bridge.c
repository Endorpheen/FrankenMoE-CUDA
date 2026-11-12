#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
__attribute__((destructor)) static void fg244s_report(void){
 if(!atomic_load(&g_calls))return;
 fprintf(stderr,"FG244S_OUTPUT_REORDER calls=%llu\n",atomic_load(&g_calls));}
static void g_fail(int64_t c){fprintf(stderr,"FG244S validation code %lld\n",(long long)c);abort();}
static int64_t g_ld_i32(uint64_t d,int64_t i){return (int64_t)((const int32_t*)(uintptr_t)d)[i];}
static void g_st_i32(uint64_t d,int64_t i,int64_t v){((int32_t*)(uintptr_t)d)[i]=(int32_t)v;}
#define fg244s_ld_i32 g_ld_i32
#define fg244s_st_i32 g_st_i32
#define fg244s_fail   g_fail
#define gg_init_globals fg244s_init_globals
#define main fg244s_generated_main
#include "or-generated.c"
#undef main
int64_t fg244s_reorder(int64_t n_outputs,int32_t*out_ids,int32_t*swaps,int32_t*inv,int64_t inv_len){
 atomic_fetch_add(&g_calls,1);
 return gg_function_1(n_outputs,(uint64_t)(uintptr_t)out_ids,(uint64_t)(uintptr_t)swaps,
                      (uint64_t)(uintptr_t)inv,inv_len,0);
}
int fg244s_enabled(void){const char*f=getenv("GGML_GOOSE_OUTPUT_REORDER");return !(f&&strcmp(f,"0")==0);}
