#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
__attribute__((destructor)) static void fg235v_report(void){
 if(!atomic_load(&g_calls))return;
 fprintf(stderr,"FG235V_MMQ_VEC_DOT_Q4K calls=%llu\n",atomic_load(&g_calls));}
static void g_fail(int64_t c){fprintf(stderr,"FG235V validation code %lld\n",(long long)c);abort();}
static int64_t g_ld_i32(uint64_t d,int64_t i){return (int64_t)((const int32_t*)(uintptr_t)d)[i];}
static int64_t g_ld_u8(uint64_t d,int64_t i){return (int64_t)((const uint8_t*)(uintptr_t)d)[i];}
static float g_ld_f32(uint64_t d,int64_t i){return ((const float*)(uintptr_t)d)[i];}
static float g_to_f32(int64_t v){return (float)v;}
#define fg235v_ld_i32 g_ld_i32
#define fg235v_ld_u8  g_ld_u8
#define fg235v_ld_f32 g_ld_f32
#define fg235v_to_f32 g_to_f32
#define fg235v_fail   g_fail
#define gg_init_globals fg235v_init_globals
#define main fg235v_generated_main
#include "vd-generated.c"
#undef main
float fg235v_vd_q4k(const int32_t*v,const int32_t*u,const uint8_t*sc,const uint8_t*m,
                    const float*dsx,const float*dsy,float dm4x,float dm4y,int64_t n_i){
 atomic_fetch_add(&g_calls,1);
 return gg_function_1((uint64_t)(uintptr_t)v,(uint64_t)(uintptr_t)u,(uint64_t)(uintptr_t)sc,
                      (uint64_t)(uintptr_t)m,(uint64_t)(uintptr_t)dsx,(uint64_t)(uintptr_t)dsy,
                      dm4x,dm4y,n_i,0);
}
int fg235v_enabled(void){const char*f=getenv("GGML_GOOSE_MMQ_VEC_DOT_Q4K");return !(f&&strcmp(f,"0")==0);}
