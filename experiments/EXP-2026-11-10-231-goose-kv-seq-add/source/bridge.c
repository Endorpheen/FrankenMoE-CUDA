#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
__attribute__((destructor)) static void fg231a_report(void){
 if(!atomic_load(&g_calls))return;
 fprintf(stderr,"FG231A_KV_SEQ_ADD calls=%llu\n",atomic_load(&g_calls));}
static void g_fail(int64_t c){fprintf(stderr,"FG231A validation code %lld\n",(long long)c);abort();}
static void g_st_i64(uint64_t d,int64_t v){*(int64_t*)(uintptr_t)d=v;}
#define fg231a_st_i64 g_st_i64
#define fg231a_fail   g_fail
#define gg_init_globals fg231a_init_globals
#define main fg231a_generated_main
#include "sa-generated.c"
#undef main
int64_t fg231a_seq_add(int64_t shift,int64_t p0,int64_t p1,int64_t seq_id,int64_t size,
                       int64_t has_tail,int64_t tail_pos,int64_t tail_has_seq,int64_t*out){
 atomic_fetch_add(&g_calls,1);
 return gg_function_1(shift,p0,p1,seq_id,size,has_tail,tail_pos,tail_has_seq,(uint64_t)(uintptr_t)out,0);
}
int fg231a_enabled(void){const char*f=getenv("GGML_GOOSE_KV_RECR_SEQ_ADD");return !(f&&strcmp(f,"0")==0);}
