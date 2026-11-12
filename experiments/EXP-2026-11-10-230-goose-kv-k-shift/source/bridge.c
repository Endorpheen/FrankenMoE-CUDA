#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
__attribute__((destructor)) static void fg230s_report(void){
 if(!atomic_load(&g_calls))return;
 fprintf(stderr,"FG230S_KV_K_SHIFT calls=%llu\n",atomic_load(&g_calls));}
static void g_fail(int64_t c){fprintf(stderr,"FG230S validation code %lld\n",(long long)c);abort();}
static int64_t g_ld_i32(uint64_t d,int64_t i){return (int64_t)((const int32_t*)(uintptr_t)d)[i];}
static int64_t g_ld_u64(uint64_t d,int64_t i){return (int64_t)((const uint64_t*)(uintptr_t)d)[i];}
static void g_st_i32(uint64_t d,int64_t i,int64_t v){((int32_t*)(uintptr_t)d)[i]=(int32_t)v;}
#define fg230s_ld_i32 g_ld_i32
#define fg230s_ld_u64 g_ld_u64
#define fg230s_st_i32 g_st_i32
#define fg230s_fail   g_fail
#define gg_init_globals fg230s_init_globals
#define main fg230s_generated_main
#include "sh-generated.c"
#undef main
int64_t fg230s_fill_shift(int64_t n_stream,const int32_t*sizes,const int32_t*const*shifts,
                          const int32_t*const*empties,int32_t*out){
 atomic_fetch_add(&g_calls,1);
 return gg_function_1(n_stream,(uint64_t)(uintptr_t)sizes,(uint64_t)(uintptr_t)shifts,
                      (uint64_t)(uintptr_t)empties,(uint64_t)(uintptr_t)out,0);
}
int fg230s_enabled(void){const char*f=getenv("GGML_GOOSE_KV_K_SHIFT");return !(f&&strcmp(f,"0")==0);}
