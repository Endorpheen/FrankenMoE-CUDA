#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
__attribute__((destructor)) static void fg213p_report(void){
 if(!atomic_load(&g_calls))return;
 fprintf(stderr,"FG213P_PIECE_COPY calls=%llu\n",atomic_load(&g_calls));}
static void g_fail(int64_t c){fprintf(stderr,"FG213P validation code %lld\n",(long long)c);abort();}
static int64_t g_ld_u8(uint64_t d,int64_t b){return (int64_t)*(const uint8_t*)(uintptr_t)(d+(uint64_t)b);}
static void g_st_u8(uint64_t d,int64_t b,int64_t v){*(uint8_t*)(uintptr_t)(d+(uint64_t)b)=(uint8_t)v;}
#define fg213p_ld_u8 g_ld_u8
#define fg213p_st_u8 g_st_u8
#define fg213p_fail  g_fail
#define gg_init_globals fg213p_init_globals
#define main fg213p_generated_main
#include "piece-generated.c"
#undef main
int64_t fg213p_lstrip_count(uint64_t p,int64_t s,int64_t l){atomic_fetch_add(&g_calls,1);return gg_function_1(p,s,l,0);}
int64_t fg213p_try_copy(uint64_t src,int64_t size,int64_t lstrip,uint64_t dst,int64_t avail){
 atomic_fetch_add(&g_calls,1);return gg_function_6(src,size,lstrip,dst,avail,0);}
int fg213p_enabled(void){const char*f=getenv("GGML_GOOSE_PIECE");return !(f&&strcmp(f,"0")==0);}
