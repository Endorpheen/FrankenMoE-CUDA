#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_ins,g_rem,g_bound;
__attribute__((destructor)) static void fg209i_report(void){
 if(!atomic_load(&g_ins)&&!atomic_load(&g_rem)&&!atomic_load(&g_bound))return;
 fprintf(stderr,"FG209I_KV_SEQPOS ins=%llu rem=%llu bound=%llu\n",
   atomic_load(&g_ins),atomic_load(&g_rem),atomic_load(&g_bound));}
static void g_fail(int64_t c){fprintf(stderr,"FG209I validation code %lld\n",(long long)c);abort();}
static int64_t g_ld_i64(uint64_t d,int64_t b){return *(const int64_t*)(uintptr_t)(d+(uint64_t)b);}
static void g_st_i64(uint64_t d,int64_t b,int64_t v){*(int64_t*)(uintptr_t)(d+(uint64_t)b)=v;}
#define fg209i_ld_i64 g_ld_i64
#define fg209i_st_i64 g_st_i64
#define fg209i_fail   g_fail
#define gg_init_globals fg209i_init_globals
#define main fg209i_generated_main
#include "incr-generated.c"
#undef main
int64_t fg209i_on_insert(uint64_t a,int64_t s,int64_t n,int64_t p){atomic_fetch_add(&g_ins,1);return gg_function_1(a,s,n,p,0);}
int64_t fg209i_on_remove(uint64_t a,int64_t s,int64_t n,int64_t p){atomic_fetch_add(&g_rem,1);return gg_function_10(a,s,n,p,0);}
int64_t fg209i_commit_bound(uint64_t a,int64_t s,int64_t n,int64_t lo,int64_t hi){atomic_fetch_add(&g_bound,1);return gg_function_11(a,s,n,lo,hi,0);}

int fg209i_enabled(void){const char*f=getenv("GGML_GOOSE_KV_SEQPOS");return !(f&&strcmp(f,"0")==0);}
