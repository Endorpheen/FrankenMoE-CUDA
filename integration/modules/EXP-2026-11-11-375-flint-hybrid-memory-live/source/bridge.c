// Bridge for the hybrid memory decisions (EXP375).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
static void g_fail(int64_t c){ fprintf(stderr, "FG375S validation code %lld\n", (long long)c); abort(); }
static void   g_put(int64_t p,int64_t k,int64_t i,uint64_t v){ ((int64_t*)(uintptr_t)p)[k*16+i]=(int64_t)v; }
static uint64_t g_get(int64_t p,int64_t k,int64_t i){ return (uint64_t)((const int64_t*)(uintptr_t)p)[k*16+i]; }
static void   g_stw(uint64_t b,int64_t o,int64_t v){ *(int64_t*)(uintptr_t)(b+(uint64_t)o)=v; }
static int64_t g_ld(uint64_t b,int64_t o){ return *(const int64_t*)(uintptr_t)(b+(uint64_t)o); }
#define fg375k_get  g_get
#define fg375k_put  g_put
#define fg375k_ld_i64 g_ld
#define fg375k_stw  g_stw
#define fg375k_fail g_fail
#define gg_init_globals fg375s_init_globals
#define main fg375s_generated_main
#include "host-qualified.c"
#undef main

// op 0: pos_min combination, op 1: pos_max combination, op 2: clear-attn-on-failed-read.
int64_t fg375s_decide(int64_t op, int64_t a, int64_t b) {
    const unsigned long long n = atomic_fetch_add(&g_calls, 1) + 1;
    if (n == 1) fprintf(stderr, "FG375S_HYBRID_MEMORY first call calls=1\n");
    if ((n % 100) == 0) fprintf(stderr, "FG375S_HYBRID_MEMORY calls=%llu\n", n);

    int64_t *pkt=(int64_t*)calloc(8*16,sizeof(int64_t));
    int64_t *jr=(int64_t*)malloc(sizeof(int64_t)*3*8);
    int64_t *cnt=(int64_t*)calloc(4,sizeof(int64_t));
    if(!pkt||!jr||!cnt) abort();
    pkt[0*16+0]=op; pkt[0*16+1]=a; pkt[0*16+2]=b;
    pkt[1*16+0]=(int64_t)(uintptr_t)jr; pkt[2*16+0]=(int64_t)(uintptr_t)cnt;
    (void) fg375ds_gg_function_1((int64_t)(uintptr_t)pkt, 1);
    const int64_t ret = (int64_t) pkt[2*16+1];
    free(pkt); free(cnt); free(jr);
    return ret;
}
int fg375s_enabled(void){ const char*f=getenv("GGML_GOOSE_HYBRID_MEMORY"); return !(f&&strcmp(f,"0")==0); }
