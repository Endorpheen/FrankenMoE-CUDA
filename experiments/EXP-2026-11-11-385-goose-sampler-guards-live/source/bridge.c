// Bridge for the sampler softmax (EXP385). Float access and expf stay on this side; the loop is Goose.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <math.h>
static _Atomic unsigned long long g_calls;
static void g_fail(int64_t c){ fprintf(stderr, "FG385S validation code %lld\n", (long long)c); abort(); }
static void   g_put(int64_t p,int64_t k,int64_t i,uint64_t v){ ((int64_t*)(uintptr_t)p)[k*16+i]=(int64_t)v; }
static uint64_t g_get(int64_t p,int64_t k,int64_t i){ return (uint64_t)((const int64_t*)(uintptr_t)p)[k*16+i]; }
static int64_t g_ld(uint64_t b,int64_t o){ return *(const int64_t*)(uintptr_t)(b+(uint64_t)o); }
static void   g_stw(uint64_t b,int64_t o,int64_t v){ *(int64_t*)(uintptr_t)(b+(uint64_t)o)=v; }
static float  g_ldf(uint64_t b,int64_t o){ return *(const float*)(uintptr_t)(b+(uint64_t)o); }
static void   g_stf(uint64_t b,int64_t o,float v){ *(float*)(uintptr_t)(b+(uint64_t)o)=v; }
static int64_t g_bits(float v){ int32_t u; memcpy(&u,&v,4); return (int64_t)u; }
#define fg385k_get  g_get
#define fg385k_put  g_put
#define fg385k_ld_i64 g_ld
#define fg385k_stw  g_stw
#define fg385k_ldf  g_ldf
#define fg385k_stf  g_stf
#define fg385k_bits g_bits
#define fg385k_fail g_fail
#define gg_init_globals fg385s_init_globals
#define main fg385s_generated_main
#include "host-qualified.c"
#undef main

// op 0: min_p skip (a=p*1e6, b=size); op 1: typical skip; op 2: top_p skip; op 3: top_k skip.
int64_t fg385s_decide(int64_t op, int64_t a, int64_t b) {
    const unsigned long long c = atomic_fetch_add(&g_calls, 1) + 1;
    if (c == 1) fprintf(stderr, "FG385S_SAMPLER_GUARDS first call calls=1\n");
    if ((c % 100) == 0) fprintf(stderr, "FG385S_SAMPLER_GUARDS calls=%llu\n", c);

    int64_t *pkt=(int64_t*)calloc(8*16,sizeof(int64_t));
    int64_t *outr=(int64_t*)calloc(4*4,sizeof(int64_t));
    int64_t *cnt=(int64_t*)calloc(4,sizeof(int64_t));
    if(!pkt||!outr||!cnt) abort();
    pkt[0*16+0]=op; pkt[0*16+1]=a; pkt[0*16+2]=b;
    pkt[1*16+0]=(int64_t)(uintptr_t)outr; pkt[2*16+0]=(int64_t)(uintptr_t)cnt;
    (void) fg385ds_gg_function_1((int64_t)(uintptr_t)pkt, 1);
    const int64_t ret = (int64_t) pkt[2*16+1];
    free(pkt); free(outr); free(cnt);
    return ret;
}
int fg385s_enabled(void){ const char*f=getenv("GGML_GOOSE_SAMPLER_SAMPLER_GUARDS"); return !(f&&strcmp(f,"0")==0); }
