#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
__attribute__((destructor)) static void fg233c_report(void){
 if(!atomic_load(&g_calls))return;
 fprintf(stderr,"FG233C_KV_SCAN calls=%llu\n",atomic_load(&g_calls));}
static void g_fail(int64_t c){fprintf(stderr,"FG233C validation code %lld\n",(long long)c);abort();}
static int64_t g_ld_i32(uint64_t d,int64_t i){return (int64_t)((const int32_t*)(uintptr_t)d)[i];}
#define fg233c_ld_i32 g_ld_i32
#define fg233c_fail   g_fail
#define gg_init_globals fg233c_init_globals
#define main fg233c_generated_main
#include "sc-generated.c"
#undef main
int64_t fg233c_has_shift(int64_t n,const int32_t*flags){
 atomic_fetch_add(&g_calls,1);
 return gg_function_1(n,(uint64_t)(uintptr_t)flags,0);
}
int64_t fg233c_pos_min(int64_t n_pos,const int32_t*positions){
 atomic_fetch_add(&g_calls,1);
 return gg_function_5(n_pos,(uint64_t)(uintptr_t)positions,0);
}
int64_t fg233c_used_max_p1(int64_t n_used,const int32_t*used){
 atomic_fetch_add(&g_calls,1);
 return gg_function_6(n_used,(uint64_t)(uintptr_t)used,0);
}
int fg233c_enabled(void){const char*f=getenv("GGML_GOOSE_KV_SCAN");return !(f&&strcmp(f,"0")==0);}
