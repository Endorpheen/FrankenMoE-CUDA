#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
__attribute__((destructor)) static void fg215u_report(void){
 if(!atomic_load(&g_calls))return;
 fprintf(stderr,"FG215U_UNICODE_CPT calls=%llu\n",atomic_load(&g_calls));}
static void g_fail(int64_t c){fprintf(stderr,"FG215U validation code %lld\n",(long long)c);abort();}
static int64_t g_ld_u8(uint64_t d,int64_t b){return (int64_t)*(const uint8_t*)(uintptr_t)(d+(uint64_t)b);}
static void g_st_i64(uint64_t d,int64_t b,int64_t v){*(int64_t*)(uintptr_t)(d+(uint64_t)b)=v;}
#define fg215u_ld_u8 g_ld_u8
#define fg215u_st_i64 g_st_i64
#define fg215u_fail  g_fail
#define gg_init_globals fg215u_init_globals
#define main fg215u_generated_main
#include "cpt-generated.c"
#undef main
int64_t fg215u_cpt_from_utf8(const char*buf,int64_t size,int64_t off,int64_t*adv){
 atomic_fetch_add(&g_calls,1);
 return gg_function_1((uint64_t)(uintptr_t)buf,size,off,(uint64_t)(uintptr_t)adv,0);
}
int fg215u_enabled(void){const char*f=getenv("GGML_GOOSE_UNICODE_CPT");return !(f&&strcmp(f,"0")==0);}
