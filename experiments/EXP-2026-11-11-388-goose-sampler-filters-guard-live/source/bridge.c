// Bridge for the sampler selection (EXP388). Uniform draw and float access here; the decision is Goose.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <math.h>
static _Atomic unsigned long long g_calls;
static void g_fail(int64_t c){ fprintf(stderr, "FG388S validation code %lld\n", (long long)c); abort(); }
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
#define fg388k_get  g_get
#define fg388k_put  g_put
#define fg388k_ld_i64 g_ld
#define fg388k_stw  g_stw
#define fg388k_ldf  g_ldf
#define fg388k_stf  g_stf
#define fg388k_ldf64 g_ldf64
#define fg388k_bits64 g_bits64
#define fg388k_fail g_fail
#define gg_init_globals fg388s_init_globals
#define main fg388s_generated_main
#include "host-qualified.c"
#undef main

// op 0: dry multiplier/window; op 1: dry base; op 2: top-n-sigma; op 3: xtc probability; op 4: xtc threshold.
int64_t fg388s_decide(int64_t op, int64_t a, int64_t b) {
    const unsigned long long c = atomic_fetch_add(&g_calls, 1) + 1;
    if (c == 1) fprintf(stderr, "FG388S_SAMPLER_FILTERS first call\n");
    if ((c % 100) == 0) fprintf(stderr, "FG388S_SAMPLER_FILTERS calls=%llu\n", c);

    int64_t *pkt=(int64_t*)calloc(8*16,sizeof(int64_t));
    int64_t *outr=(int64_t*)calloc(4*4,sizeof(int64_t));
    int64_t *cnt=(int64_t*)calloc(4,sizeof(int64_t));
    if(!pkt||!outr||!cnt) abort();
    pkt[0*16+0]=op; pkt[0*16+1]=a; pkt[0*16+2]=b;
    pkt[1*16+0]=(int64_t)(uintptr_t)outr; pkt[2*16+0]=(int64_t)(uintptr_t)cnt;
    (void) fg388ds_gg_function_1((int64_t)(uintptr_t)pkt, 1);
    const int64_t ret = (int64_t) pkt[2*16+1];
    free(pkt); free(outr); free(cnt);
    return ret;
}
int fg388s_enabled(void){ const char*f=getenv("GGML_GOOSE_SAMPLER_FILTERS"); return !(f&&strcmp(f,"0")==0); }
