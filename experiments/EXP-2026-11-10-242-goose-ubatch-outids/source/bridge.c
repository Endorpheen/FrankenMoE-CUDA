#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
__attribute__((destructor)) static void fg242o_report(void){
 if(!atomic_load(&g_calls))return;
 fprintf(stderr,"FG242O_UBATCH_OUTIDS calls=%llu\n",atomic_load(&g_calls));}
static void g_fail(int64_t c){fprintf(stderr,"FG242O validation code %lld\n",(long long)c);abort();}
static int64_t g_ld_i32(uint64_t d,int64_t i){return (int64_t)((const int32_t*)(uintptr_t)d)[i];}
static void g_st_i32(uint64_t d,int64_t i,int64_t v){((int32_t*)(uintptr_t)d)[i]=(int32_t)v;}
#define fg242o_ld_i32 g_ld_i32
#define fg242o_st_i32 g_st_i32
#define fg242o_fail   g_fail
#define gg_init_globals fg242o_init_globals
#define main fg242o_generated_main
#include "oi-generated.c"
#undef main
int64_t fg242o_pick_outids(int64_t n_tokens,const int32_t*idxs,const int32_t*output,int32_t*out){
 atomic_fetch_add(&g_calls,1);
 return gg_function_1(n_tokens,(uint64_t)(uintptr_t)idxs,(uint64_t)(uintptr_t)output,(uint64_t)(uintptr_t)out,0);
}
int64_t fg242o_map_seqs(int64_t n_max,int64_t u0,int64_t u1,int64_t u2,int64_t u3,
                        int32_t*out_seq_idx,int32_t*out_ids){
 atomic_fetch_add(&g_calls,1);
 return gg_function_6(n_max,u0,u1,u2,u3,(uint64_t)(uintptr_t)out_seq_idx,(uint64_t)(uintptr_t)out_ids,0);
}
int fg242o_enabled(void){const char*f=getenv("GGML_GOOSE_UBATCH_OUTIDS");return !(f&&strcmp(f,"0")==0);}
