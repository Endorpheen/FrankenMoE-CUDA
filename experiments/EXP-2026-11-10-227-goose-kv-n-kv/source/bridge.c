#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
__attribute__((destructor)) static void fg227n_report(void){
 if(!atomic_load(&g_calls))return;
 fprintf(stderr,"FG227N_KV_N_KV calls=%llu\n",atomic_load(&g_calls));}
static void g_fail(int64_t c){fprintf(stderr,"FG227N validation code %lld\n",(long long)c);abort();}
static int64_t g_ld_i32(uint64_t d,int64_t i){return (int64_t)((const int32_t*)(uintptr_t)d)[i];}
#define fg227n_ld_i32 g_ld_i32
#define fg227n_fail  g_fail
#define gg_init_globals fg227n_init_globals
#define main fg227n_generated_main
#include "kv-generated.c"
#undef main
int64_t fg227n_n_kv(int64_t n_streams,const int32_t*sizes,const int32_t*used,int64_t n_pad){
 atomic_fetch_add(&g_calls,1);
 return gg_function_1(n_streams,(uint64_t)(uintptr_t)sizes,(uint64_t)(uintptr_t)used,n_pad,0);
}
int fg227n_enabled(void){const char*f=getenv("GGML_GOOSE_KV_N_KV");return !(f&&strcmp(f,"0")==0);}
