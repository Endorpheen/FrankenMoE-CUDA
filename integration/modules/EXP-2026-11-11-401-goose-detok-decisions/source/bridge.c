// Bridge for the KV slot walk (EXP401). Flat arrays in, indices out; the predicate is fg199k_can_use.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
static void g_fail(int64_t c){ fprintf(stderr, "FG401S validation code %lld\n", (long long)c); abort(); }
static void   g_put(int64_t p,int64_t k,int64_t i,uint64_t v){ ((int64_t*)(uintptr_t)p)[k*16+i]=(int64_t)v; }
static uint64_t g_get(int64_t p,int64_t k,int64_t i){ return (uint64_t)((const int64_t*)(uintptr_t)p)[k*16+i]; }
static int64_t g_ld(uint64_t b,int64_t o){ return *(const int64_t*)(uintptr_t)(b+(uint64_t)o); }
static void   g_stw(uint64_t b,int64_t o,int64_t v){ *(int64_t*)(uintptr_t)(b+(uint64_t)o)=v; }
#define fg401k_get  g_get
#define fg401k_put  g_put
#define fg401k_ld_i64 g_ld
#define fg401k_stw  g_stw
#define fg401k_fail g_fail
extern int64_t fg199k_can_use(int64_t occupied, int64_t seq_count, int64_t pos_cell,
                              int64_t seq_pos_max_1, int64_t n_swa, int64_t swa_type);
#define fg401k_can fg199k_can_use
#define gg_init_globals fg401s_init_globals
#define main fg401s_generated_main
#include "host-qualified.c"
#undef main

// op 0: skip unknown/control (a=special, b=attr); op 1: lstrip applies (a=count, b=size);
// op 2: detokenise loop bound.
int64_t fg401s_decide(int64_t op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t * out2) {
    const unsigned long long n = atomic_fetch_add(&g_calls, 1) + 1;
    if (n == 1) fprintf(stderr, "FG401S_DETOK_DECISIONS first call calls=1\n");
    if ((n % 1000) == 0) fprintf(stderr, "FG401S_DETOK_DECISIONS calls=%llu\n", n);

    int64_t *pkt=(int64_t*)calloc(8*16,sizeof(int64_t));
    int64_t *outr=(int64_t*)calloc(4*4,sizeof(int64_t));
    int64_t *cnt=(int64_t*)calloc(4,sizeof(int64_t));
    if(!pkt||!outr||!cnt) abort();
    pkt[0*16+0]=op; pkt[0*16+1]=a; pkt[0*16+2]=b;
    pkt[1*16+0]=(int64_t)(uintptr_t)outr; pkt[2*16+0]=(int64_t)(uintptr_t)cnt;
    (void) fg401ds_gg_function_1((int64_t)(uintptr_t)pkt, 1);
    const int64_t ret = (int64_t) pkt[2*16+1];
    if (out2) *out2 = (int64_t) pkt[2*16+2];
    free(pkt); free(outr); free(cnt);
    return ret;
}
int fg401s_enabled(void){ const char*f=getenv("GGML_GOOSE_DETOK_DECISIONS"); return !(f&&strcmp(f,"0")==0); }
