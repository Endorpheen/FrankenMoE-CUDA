#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
__attribute__((destructor)) static void fg210k_report(void){
 if(!atomic_load(&g_calls))return;
 fprintf(stderr,"FG210K_RECURRENT_SEQKEEP calls=%llu\n",atomic_load(&g_calls));}
static void g_fail(int64_t c){fprintf(stderr,"FG210K validation code %lld\n",(long long)c);abort();}
static int64_t g_ld_i64(uint64_t d,int64_t b){return *(const int64_t*)(uintptr_t)(d+(uint64_t)b);}
static void g_st_i64(uint64_t d,int64_t b,int64_t v){*(int64_t*)(uintptr_t)(d+(uint64_t)b)=v;}
#define fg210k_ld_i64 g_ld_i64
#define fg210k_st_i64 g_st_i64
#define fg210k_fail   g_fail
#define gg_init_globals fg210k_init_globals
#define main fg210k_generated_main
#include "seqkeep-generated.c"
#undef main
int64_t fg210k_seq_keep(uint64_t a,int64_t n,int64_t w,int64_t s,int64_t head,int64_t*used){
 atomic_fetch_add(&g_calls,1);
 return gg_function_1(a,n,w,s,head,(uint64_t)(uintptr_t)used,0);
}
int fg210k_enabled(void){const char*f=getenv("GGML_GOOSE_RECURRENT_SEQKEEP");return !(f&&strcmp(f,"0")==0);}
