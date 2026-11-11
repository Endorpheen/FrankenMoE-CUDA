// Bridge for the sampler softmax (EXP384). Float access and expf stay on this side; the loop is Goose.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <math.h>
static _Atomic unsigned long long g_calls;
static void g_fail(int64_t c){ fprintf(stderr, "FG384S validation code %lld\n", (long long)c); abort(); }
static void   g_put(int64_t p,int64_t k,int64_t i,uint64_t v){ ((int64_t*)(uintptr_t)p)[k*16+i]=(int64_t)v; }
static uint64_t g_get(int64_t p,int64_t k,int64_t i){ return (uint64_t)((const int64_t*)(uintptr_t)p)[k*16+i]; }
static int64_t g_ld(uint64_t b,int64_t o){ return *(const int64_t*)(uintptr_t)(b+(uint64_t)o); }
static void   g_stw(uint64_t b,int64_t o,int64_t v){ *(int64_t*)(uintptr_t)(b+(uint64_t)o)=v; }
static float  g_ldf(uint64_t b,int64_t o){ return *(const float*)(uintptr_t)(b+(uint64_t)o); }
static void   g_stf(uint64_t b,int64_t o,float v){ *(float*)(uintptr_t)(b+(uint64_t)o)=v; }
static int64_t g_bits(float v){ int32_t u; memcpy(&u,&v,4); return (int64_t)u; }
#define fg384k_get  g_get
#define fg384k_put  g_put
#define fg384k_ld_i64 g_ld
#define fg384k_stw  g_stw
#define fg384k_ldf  g_ldf
#define fg384k_stf  g_stf
#define fg384k_bits g_bits
#define fg384k_fail g_fail
#define gg_init_globals fg384s_init_globals
#define main fg384s_generated_main
#include "host-qualified.c"
#undef main

// Normalise `n` logits into `out`: p[i] = exp(l[i] - max) / sum
int64_t fg384s_softmax(const float * logits, int64_t n, int32_t sorted, float * out) {
    const unsigned long long c = atomic_fetch_add(&g_calls, 1) + 1;
    if (c == 1) fprintf(stderr, "FG384S_SOFTMAX first call\n");
    if ((c % 100) == 0) fprintf(stderr, "FG384S_SOFTMAX calls=%llu\n", c);

    int64_t *pkt=(int64_t*)calloc(8*16,sizeof(int64_t));
    int64_t *outr=(int64_t*)calloc(4*4,sizeof(int64_t));
    int64_t *cnt=(int64_t*)calloc(4,sizeof(int64_t));
    if(!pkt||!outr||!cnt) abort();
    pkt[0*16+0]=n; pkt[0*16+1]=sorted;
    pkt[0*16+2]=(int64_t)(uintptr_t)logits; pkt[0*16+3]=(int64_t)(uintptr_t)out;
    pkt[1*16+0]=(int64_t)(uintptr_t)outr; pkt[2*16+0]=(int64_t)(uintptr_t)cnt;
    (void) fg384ds_gg_function_1((int64_t)(uintptr_t)pkt, 1);
    free(pkt); free(outr); free(cnt);
    return 0;
}
int fg384s_enabled(void){ const char*f=getenv("GGML_GOOSE_SAMPLER_SOFTMAX"); return !(f&&strcmp(f,"0")==0); }
