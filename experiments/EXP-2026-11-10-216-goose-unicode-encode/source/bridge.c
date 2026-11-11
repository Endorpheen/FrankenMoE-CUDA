#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
__attribute__((destructor)) static void fg216e_report(void){
 if(!atomic_load(&g_calls))return;
 fprintf(stderr,"FG216E_UNICODE_ENC calls=%llu\n",atomic_load(&g_calls));}
static void g_tick(void) { const unsigned long long n = atomic_fetch_add(&g_calls, 1) + 1;
    if (n == 1) fprintf(stderr, "FG216E_UNICODE_ENC first call calls=1\n");
    if ((n % 1000) == 0) fprintf(stderr, "FG216E_UNICODE_ENC calls=%llu\n", n); }
static void g_fail(int64_t c){fprintf(stderr,"FG216E validation code %lld\n",(long long)c);abort();}
static void g_st_u8(uint64_t d,int64_t b,int64_t v){*(uint8_t*)(uintptr_t)(d+(uint64_t)b)=(uint8_t)v;}
#define fg216e_st_u8 g_st_u8
#define fg216e_fail  g_fail
#define gg_init_globals fg216e_init_globals
#define main fg216e_generated_main
#include "enc-generated.c"
#undef main
int64_t fg216e_cpt_to_utf8(int64_t cpt,char*out,int64_t cap){ g_tick();
 atomic_fetch_add(&g_calls,1);
 return gg_function_1(cpt,(uint64_t)(uintptr_t)out,cap,0);
}
int fg216e_enabled(void){const char*f=getenv("GGML_GOOSE_UNICODE_ENC");return !(f&&strcmp(f,"0")==0);}
