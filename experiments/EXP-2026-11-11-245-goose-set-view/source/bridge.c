#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
__attribute__((destructor)) static void fg245v_report(void){
 if(!atomic_load(&g_calls))return;
 fprintf(stderr,"FG245V_SET_VIEW calls=%llu\n",atomic_load(&g_calls));}
static void g_fail(int64_t c){fprintf(stderr,"FG245V validation code %lld\n",(long long)c);abort();}
static void g_st_i64(uint64_t d,int64_t i,int64_t v){((int64_t*)(uintptr_t)d)[i]=v;}
#define fg245v_st_i64 g_st_i64
#define fg245v_fail   g_fail
#define gg_init_globals fg245v_init_globals
#define main fg245v_generated_main
#include "sv-generated.c"
#undef main
int64_t fg245v_view(uint64_t dst_data,int64_t offset,int64_t elem_size,int64_t nb1,int64_t nb2,int64_t nb3,
                    int64_t ne0,int64_t ne1,int64_t ne2,int64_t ne3,int64_t*out){
 atomic_fetch_add(&g_calls,1);
 return gg_function_1((int64_t)dst_data,offset,elem_size,nb1,nb2,nb3,ne0,ne1,ne2,ne3,(uint64_t)(uintptr_t)out,0);
}
int fg245v_enabled(void){const char*f=getenv("GGML_GOOSE_SET_VIEW");return !(f&&strcmp(f,"0")==0);}
