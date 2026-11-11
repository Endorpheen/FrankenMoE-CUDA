// Bridge for the sampler selection (EXP386). Uniform draw and float access here; the decision is Goose.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <math.h>
static _Atomic unsigned long long g_calls;
static void g_fail(int64_t c){ fprintf(stderr, "FG386S validation code %lld\n", (long long)c); abort(); }
static void   g_put(int64_t p,int64_t k,int64_t i,uint64_t v){ ((int64_t*)(uintptr_t)p)[k*16+i]=(int64_t)v; }
static uint64_t g_get(int64_t p,int64_t k,int64_t i){ return (uint64_t)((const int64_t*)(uintptr_t)p)[k*16+i]; }
static int64_t g_ld(uint64_t b,int64_t o){ return *(const int64_t*)(uintptr_t)(b+(uint64_t)o); }
static void   g_stw(uint64_t b,int64_t o,int64_t v){ *(int64_t*)(uintptr_t)(b+(uint64_t)o)=v; }
static void   g_stf(uint64_t b,int64_t o,float v){ *(float*)(uintptr_t)(b+(uint64_t)o)=v; }
static float  g_ldf(uint64_t b,int64_t o){
    const float v = *(const float*)(uintptr_t)(b+(uint64_t)o);
    if (getenv("FG386_DEBUG")) fprintf(stderr, "DEBUG ldf base=%llx off=%lld -> %.9g\n",
                                       (unsigned long long) b, (long long) o, (double) v);
    return v;
}
static double g_ldf64(uint64_t b,int64_t o){ return *(const double*)(uintptr_t)(b+(uint64_t)o); }
static int64_t g_bits64(double v){ int64_t u; memcpy(&u,&v,8); return u; }
#define fg386k_get  g_get
#define fg386k_put  g_put
#define fg386k_ld_i64 g_ld
#define fg386k_stw  g_stw
#define fg386k_ldf  g_ldf
#define fg386k_stf  g_stf
#define fg386k_ldf64 g_ldf64
#define fg386k_bits64 g_bits64
#define fg386k_fail g_fail
#define gg_init_globals fg386s_init_globals
#define main fg386s_generated_main
#include "host-qualified.c"
#undef main

// Selection plus normalisation: max, exp, running sum against the target, then p /= sum_cum.
int64_t fg386s_pick(const float * logits, int64_t n, int32_t sorted, float * out, double rnd) {
    const unsigned long long c = atomic_fetch_add(&g_calls, 1) + 1;
    if (c == 1) fprintf(stderr, "FG386S_DIST_PICK first call\n");
    if ((c % 100) == 0) fprintf(stderr, "FG386S_DIST_PICK calls=%llu\n", c);

    double rbuf = rnd;
    int64_t *pkt=(int64_t*)calloc(8*16,sizeof(int64_t));
    int64_t *outr=(int64_t*)calloc(4*4,sizeof(int64_t));
    int64_t *cnt=(int64_t*)calloc(4,sizeof(int64_t));
    if(!pkt||!outr||!cnt) abort();
    pkt[0*16+0]=n; pkt[0*16+1]=sorted;
    pkt[0*16+2]=(int64_t)(uintptr_t)logits; pkt[0*16+3]=(int64_t)(uintptr_t)out;
    pkt[0*16+4]=(int64_t)(uintptr_t)&rbuf;
    pkt[1*16+0]=(int64_t)(uintptr_t)outr; pkt[2*16+0]=(int64_t)(uintptr_t)cnt;
    (void) fg386ds_gg_function_1((int64_t)(uintptr_t)pkt, 1);
    const int64_t ret = (int64_t) pkt[2*16+1];
    free(pkt); free(outr); free(cnt);
    return ret;
}
int fg386s_enabled(void){ const char*f=getenv("GGML_GOOSE_SAMPLER_DIST"); return !(f&&strcmp(f,"0")==0); }
