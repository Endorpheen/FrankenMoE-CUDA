#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
__attribute__((destructor)) static void fg218t_report(void){
 if(!atomic_load(&g_calls))return;
 fprintf(stderr,"FG218T_UNICODE_TOLOWER calls=%llu\n",atomic_load(&g_calls));}
static void g_fail(int64_t c){fprintf(stderr,"FG218T validation code %lld\n",(long long)c);abort();}
static int64_t g_ld_u32(uint64_t d,int64_t i){return (int64_t)*(const uint32_t*)(uintptr_t)(d+(uint64_t)i);}
#define fg218t_ld_u32 g_ld_u32
#define fg218t_fail   g_fail
#define gg_init_globals fg218t_init_globals
#define main fg218t_generated_main
#include "tolower-generated.c"
#undef main
// The table is the project's existing unicode_map_lowercase, passed in by the caller. Its entries
// are {uint32_t first, uint32_t second} pairs, which is what the Goose module walks.
int64_t fg218t_tolower(int64_t cpt,const void*tab,int64_t n){
 atomic_fetch_add(&g_calls,1);
 return gg_function_1(cpt,(uint64_t)(uintptr_t)tab,n,0);
}
int fg218t_enabled(void){const char*f=getenv("GGML_GOOSE_UNICODE_TOLOWER");return !(f&&strcmp(f,"0")==0);}
