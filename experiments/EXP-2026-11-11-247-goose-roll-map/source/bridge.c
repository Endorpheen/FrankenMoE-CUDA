#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
__attribute__((destructor)) static void fg247r_report(void){
 if(!atomic_load(&g_calls))return;
 fprintf(stderr,"FG247R_ROLL_MAP calls=%llu\n",atomic_load(&g_calls));}
static void g_fail(int64_t c){fprintf(stderr,"FG247R validation code %lld\n",(long long)c);abort();}
#define fg247r_fail g_fail
#define gg_init_globals fg247r_init_globals
#define main fg247r_generated_main
#include "rm-generated.c"
#undef main
int64_t fg247r_map(int64_t idx,int64_t n0,int64_t n1,int64_t n2,int64_t n3,
                   int64_t s0,int64_t s1,int64_t s2,int64_t s3){
 atomic_fetch_add(&g_calls,1);
 return gg_function_1(idx,n0,n1,n2,n3,s0,s1,s2,s3,0);
}
int fg247r_enabled(void){const char*f=getenv("GGML_GOOSE_ROLL_MAP");return !(f&&strcmp(f,"0")==0);}
