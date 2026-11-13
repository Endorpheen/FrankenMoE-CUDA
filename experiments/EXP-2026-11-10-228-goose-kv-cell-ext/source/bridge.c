#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
__attribute__((destructor)) static void fg228e_report(void){
 if(!atomic_load(&g_calls))return;
 fprintf(stderr,"FG228E_KV_CELL_EXT calls=%llu\n",atomic_load(&g_calls));}
static void g_fail(int64_t c){fprintf(stderr,"FG228E validation code %lld\n",(long long)c);abort();}
static void g_st_i32(uint64_t d,int64_t i,int64_t v){((int32_t*)(uintptr_t)d)[i]=(int32_t)v;}
#define fg228e_st_i32 g_st_i32
#define fg228e_fail   g_fail
#define gg_init_globals fg228e_init_globals
#define main fg228e_generated_main
#include "ext-generated.c"
#undef main
int64_t fg228e_build_ext(int64_t has_ext,int64_t pos_2d,int64_t px,int64_t py,int64_t has_token,
                         int64_t tok_i,int64_t is_mixed,int64_t type_i,int64_t ple_n_heads,
                         int64_t ple_image_id,int64_t ple_eos_id,int32_t*out){
 atomic_fetch_add(&g_calls,1);
 return gg_function_1(has_ext,pos_2d,px,py,has_token,tok_i,is_mixed,type_i,ple_n_heads,
                      ple_image_id,ple_eos_id,(uint64_t)(uintptr_t)out,0);
}
int fg228e_enabled(void){const char*f=getenv("GGML_GOOSE_KV_CELL_EXT");return !(f&&strcmp(f,"0")==0);}
