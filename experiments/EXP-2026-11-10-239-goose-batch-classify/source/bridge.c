#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
__attribute__((destructor)) static void fg239b_report(void){
 if(!atomic_load(&g_calls))return;
 fprintf(stderr,"FG239B_BATCH_CLASSIFY calls=%llu\n",atomic_load(&g_calls));}
static void g_fail(int64_t c){fprintf(stderr,"FG239B validation code %lld\n",(long long)c);abort();}
static int64_t g_ld_i32(uint64_t d,int64_t i){return (int64_t)((const int32_t*)(uintptr_t)d)[i];}
static void g_st_i32(uint64_t d,int64_t i,int64_t v){((int32_t*)(uintptr_t)d)[i]=(int32_t)v;}
#define fg239b_ld_i32 g_ld_i32
#define fg239b_st_i32 g_st_i32
#define fg239b_fail   g_fail
#define gg_init_globals fg239b_init_globals
#define main fg239b_generated_main
#include "cl-generated.c"
#undef main
int64_t fg239b_classify(int64_t n_tok,const int32_t*is_tok,const int32_t*is_emb,int64_t allow_mixed,
                        int32_t*out){
 atomic_fetch_add(&g_calls,1);
 return gg_function_1(n_tok,(uint64_t)(uintptr_t)is_tok,(uint64_t)(uintptr_t)is_emb,allow_mixed,
                      (uint64_t)(uintptr_t)out,0);
}
int fg239b_enabled(void){const char*f=getenv("GGML_GOOSE_BATCH_CLASSIFY");return !(f&&strcmp(f,"0")==0);}
