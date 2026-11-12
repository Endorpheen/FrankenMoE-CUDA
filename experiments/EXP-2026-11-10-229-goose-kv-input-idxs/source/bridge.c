#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
__attribute__((destructor)) static void fg229i_report(void){
 if(!atomic_load(&g_calls))return;
 fprintf(stderr,"FG229I_KV_INPUT_IDXS calls=%llu\n",atomic_load(&g_calls));}
static void g_fail(int64_t c){fprintf(stderr,"FG229I validation code %lld\n",(long long)c);abort();}
static int64_t g_ld_i32(uint64_t d,int64_t i){return (int64_t)((const int32_t*)(uintptr_t)d)[i];}
static int64_t g_ld_u32(uint64_t d,int64_t i){return (int64_t)*(const uint32_t*)(uintptr_t)(d+(uint64_t)(i*4));}
static int64_t g_ld_u64(uint64_t d,int64_t i){return (int64_t)((const uint64_t*)(uintptr_t)d)[i];}
static void g_st_i64(uint64_t d,int64_t b,int64_t v){*(int64_t*)(uintptr_t)(d+(uint64_t)b)=v;}
#define fg229i_ld_i32 g_ld_i32
#define fg229i_ld_u32 g_ld_u32
#define fg229i_ld_u64 g_ld_u64
#define fg229i_st_i64 g_st_i64
#define fg229i_fail   g_fail
#define gg_init_globals fg229i_init_globals
#define main fg229i_generated_main
#include "idx-generated.c"
#undef main
int64_t fg229i_fill_idxs(int64_t n_stream,int64_t size,const int32_t*strm,const uint32_t*const*idxs,
                         int64_t kv_size,int64_t*out){
 atomic_fetch_add(&g_calls,1);
 return gg_function_1(n_stream,size,(uint64_t)(uintptr_t)strm,(uint64_t)(uintptr_t)idxs,kv_size,
                      (uint64_t)(uintptr_t)out,0);
}
int fg229i_enabled(void){const char*f=getenv("GGML_GOOSE_KV_INPUT_IDXS");return !(f&&strcmp(f,"0")==0);}
