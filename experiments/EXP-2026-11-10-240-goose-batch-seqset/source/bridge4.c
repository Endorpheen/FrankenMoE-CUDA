#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
__attribute__((destructor)) static void fg240q_report(void){
 if(!atomic_load(&g_calls))return;
 fprintf(stderr,"FG240Q_BATCH_SEQSET calls=%llu\n",atomic_load(&g_calls));}
static void g_fail(int64_t c){fprintf(stderr,"FG240Q validation code %lld\n",(long long)c);abort();}
static int64_t g_ld_i32(uint64_t d,int64_t i){return (int64_t)((const int32_t*)(uintptr_t)d)[i];}
static int64_t g_ld_i64(uint64_t d,int64_t i){return (int64_t)((const int64_t*)(uintptr_t)d)[i];}
static void g_st_i64(uint64_t d,int64_t i,int64_t v){((int64_t*)(uintptr_t)d)[i]=v;}
#define fg240q_ld_i32 g_ld_i32
#define fg240q_ld_i64 g_ld_i64
#define fg240q_st_i64 g_st_i64
#define fg240q_fail   g_fail
#define gg_init_globals fg240q_init_globals
#define main fg240q_generated_main
#include "ss4-generated.c"
#undef main
int64_t fg240q_build_sets4(int64_t n_tok,const int32_t*flat,const int32_t*off,int64_t*out,
                           int64_t*unq,int64_t n_max){
 atomic_fetch_add(&g_calls,1);
 return gg_function_1(n_tok,(uint64_t)(uintptr_t)flat,(uint64_t)(uintptr_t)off,
                      (uint64_t)(uintptr_t)out,(uint64_t)(uintptr_t)unq,n_max,0);
}
int fg240q_enabled(void){const char*f=getenv("GGML_GOOSE_BATCH_SEQSET");return !(f&&strcmp(f,"0")==0);}
