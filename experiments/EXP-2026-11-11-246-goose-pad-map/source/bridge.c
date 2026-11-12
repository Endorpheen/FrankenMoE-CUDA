#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
__attribute__((destructor)) static void fg246p_report(void){
 if(!atomic_load(&g_calls))return;
 fprintf(stderr,"FG246P_PAD_MAP calls=%llu\n",atomic_load(&g_calls));}
static void g_fail(int64_t c){fprintf(stderr,"FG246P validation code %lld\n",(long long)c);abort();}
#define fg246p_fail g_fail
#define gg_init_globals fg246p_init_globals
#define main fg246p_generated_main
#include "pm-generated.c"
#undef main
int64_t fg246p_map(int64_t i0,int64_t i1,int64_t i2,int64_t i3,int64_t s00,int64_t s01,int64_t s02,int64_t s03,
                   int64_t lp0,int64_t rp0,int64_t lp1,int64_t rp1,int64_t lp2,int64_t rp2,
                   int64_t lp3,int64_t rp3,int64_t ne0,int64_t ne1,int64_t ne2,int64_t ne3,int64_t circ){
 atomic_fetch_add(&g_calls,1);
 return gg_function_1(i0,i1,i2,i3,s00,s01,s02,s03,lp0,rp0,lp1,rp1,lp2,rp2,lp3,rp3,ne0,ne1,ne2,ne3,circ,0);
}
int fg246p_enabled(void){const char*f=getenv("GGML_GOOSE_PAD_MAP");return !(f&&strcmp(f,"0")==0);}
