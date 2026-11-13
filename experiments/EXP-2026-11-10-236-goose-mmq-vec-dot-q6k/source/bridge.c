#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
__attribute__((destructor)) static void fg236k_report(void){
 if(!atomic_load(&g_calls))return;
 fprintf(stderr,"FG236K_MMQ_VEC_DOT_Q6K calls=%llu\n",atomic_load(&g_calls));}
static void g_fail(int64_t c){fprintf(stderr,"FG236K validation code %lld\n",(long long)c);abort();}
static int64_t g_ld_i32(uint64_t d,int64_t i){return (int64_t)((const int32_t*)(uintptr_t)d)[i];}
static int64_t g_ld_i8(uint64_t d,int64_t i){return (int64_t)((const int8_t*)(uintptr_t)d)[i];}
static float g_ld_f32(uint64_t d,int64_t i){return ((const float*)(uintptr_t)d)[i];}
static float g_to_f32(int64_t v){return (float)v;}
static int64_t g_dp4a(int64_t a,int64_t b,int64_t c){
 const int8_t*as=(const int8_t*)&a; const int8_t*bs=(const int8_t*)&b;
 return c+as[0]*bs[0]+as[1]*bs[1]+as[2]*bs[2]+as[3]*bs[3];}
#define fg236k_ld_i32 g_ld_i32
#define fg236k_ld_i8  g_ld_i8
#define fg236k_ld_f32 g_ld_f32
#define fg236k_to_f32 g_to_f32
#define fg236k_dp4a   g_dp4a
#define fg236k_fail   g_fail
#define gg_init_globals fg236k_init_globals
#define main fg236k_generated_main
#include "vd6-generated.c"
#undef main
float fg236k_vd_q6k(const int32_t*v,const int32_t*u,const uint8_t*sc,float d6,const float*d8,int64_t n){
 atomic_fetch_add(&g_calls,1);
 return gg_function_1((uint64_t)(uintptr_t)v,(uint64_t)(uintptr_t)u,(uint64_t)(uintptr_t)sc,d6,
                      (uint64_t)(uintptr_t)d8,n,0);
}
int fg236k_enabled(void){const char*f=getenv("GGML_GOOSE_MMQ_VEC_DOT_Q6K");return !(f&&strcmp(f,"0")==0);}
