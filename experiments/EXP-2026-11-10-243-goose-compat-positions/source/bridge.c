#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
__attribute__((destructor)) static void fg243c_report(void){
 if(!atomic_load(&g_calls))return;
 fprintf(stderr,"FG243C_COMPAT_POSITIONS calls=%llu\n",atomic_load(&g_calls));}
static void g_fail(int64_t c){fprintf(stderr,"FG243C validation code %lld\n",(long long)c);abort();}
static int64_t g_ld_i32(uint64_t d,int64_t i){return (int64_t)((const int32_t*)(uintptr_t)d)[i];}
static int64_t g_ld_i64(uint64_t d,int64_t i){return ((const int64_t*)(uintptr_t)d)[i];}
static void g_st_i64(uint64_t d,int64_t i,int64_t v){((int64_t*)(uintptr_t)d)[i]=v;}
#define fg243c_ld_i32 g_ld_i32
#define fg243c_ld_i64 g_ld_i64
#define fg243c_st_i64 g_st_i64
#define fg243c_fail   g_fail
#define gg_init_globals fg243c_init_globals
#define main fg243c_generated_main
#include "cp-generated.c"
#undef main
int64_t fg243c_positions(int64_t n_tokens,int64_t has_token,int64_t has_pos,int64_t n_pos_per_embd,
                         const int64_t*pos,const int32_t*sid_flat,const int32_t*sid_off,
                         int64_t*pos_next,int64_t*out,int64_t n_seq_max){
 atomic_fetch_add(&g_calls,1);
 return gg_function_1(n_tokens,has_token,has_pos,n_pos_per_embd,(uint64_t)(uintptr_t)pos,
                      (uint64_t)(uintptr_t)sid_flat,(uint64_t)(uintptr_t)sid_off,
                      (uint64_t)(uintptr_t)pos_next,(uint64_t)(uintptr_t)out,n_seq_max,0);
}
int fg243c_enabled(void){const char*f=getenv("GGML_GOOSE_COMPAT_POSITIONS");return !(f&&strcmp(f,"0")==0);}
