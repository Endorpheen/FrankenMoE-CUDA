#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
__attribute__((destructor)) static void fg241u_report(void){
 if(!atomic_load(&g_calls))return;
 fprintf(stderr,"FG241U_UBATCH_CLASSIFY calls=%llu\n",atomic_load(&g_calls));}
static void g_fail(int64_t c){fprintf(stderr,"FG241U validation code %lld\n",(long long)c);abort();}
static int64_t g_ld_i32(uint64_t d,int64_t i){return (int64_t)((const int32_t*)(uintptr_t)d)[i];}
static void g_st_i64(uint64_t d,int64_t i,int64_t v){((int64_t*)(uintptr_t)d)[i]=v;}
#define fg241u_ld_i32 g_ld_i32
#define fg241u_st_i64 g_st_i64
#define fg241u_fail   g_fail
#define gg_init_globals fg241u_init_globals
#define main fg241u_generated_main
#include "uc-generated.c"
#undef main
int64_t fg241u_classify(int64_t n_tokens,int64_t mixed_batch,const int32_t*is_embd,const int32_t*idxs,
                        int64_t has_token,int64_t has_embd,int64_t has_state,int64_t n_embd,
                        int64_t n_embd_state,int64_t n_pos_per_embd,int64_t*out){
 atomic_fetch_add(&g_calls,1);
 return gg_function_1(n_tokens,mixed_batch,(uint64_t)(uintptr_t)is_embd,(uint64_t)(uintptr_t)idxs,
                      has_token,has_embd,has_state,n_embd,n_embd_state,n_pos_per_embd,
                      (uint64_t)(uintptr_t)out,0);
}
int fg241u_enabled(void){const char*f=getenv("GGML_GOOSE_UBATCH_CLASSIFY");return !(f&&strcmp(f,"0")==0);}
