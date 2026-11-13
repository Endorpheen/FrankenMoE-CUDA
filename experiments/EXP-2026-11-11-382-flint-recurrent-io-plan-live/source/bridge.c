// Bridge for the recurrent I/O plan decisions (EXP382; native line llama-sampler.cpp).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
static void g_fail(int64_t c){ fprintf(stderr, "FG382S validation code %lld\n", (long long)c); abort(); }
static void   g_put(int64_t p,int64_t k,int64_t i,uint64_t v){ ((int64_t*)(uintptr_t)p)[k*16+i]=(int64_t)v; }
static uint64_t g_get(int64_t p,int64_t k,int64_t i){ return (uint64_t)((const int64_t*)(uintptr_t)p)[k*16+i]; }
static void   g_stw(uint64_t b,int64_t o,int64_t v){ *(int64_t*)(uintptr_t)(b+(uint64_t)o)=v; }
static int64_t g_ld(uint64_t b,int64_t o){ return *(const int64_t*)(uintptr_t)(b+(uint64_t)o); }
#define fg382k_get  g_get
#define fg382k_put  g_put
#define fg382k_ld_i64 g_ld
#define fg382k_stw  g_stw
#define fg382k_fail g_fail
#define gg_init_globals fg382s_init_globals
#define main fg382s_generated_main
#include "host-qualified.c"
#undef main

// op 0: skip a layer without an R tensor (a=has_r); op 1: the layer belongs to the S pass (a=has_s).
int64_t fg382s_decide(int64_t op, int64_t a, int64_t b) {
    const unsigned long long n = atomic_fetch_add(&g_calls, 1) + 1;
    if (n == 1) fprintf(stderr, "FG382S_RECURRENT_IO_PLAN first call calls=1\n");
    if ((n % 1000) == 0) fprintf(stderr, "FG382S_RECURRENT_IO_PLAN calls=%llu\n", n);

    int64_t *pkt=(int64_t*)calloc(8*16,sizeof(int64_t));
    int64_t *out=(int64_t*)calloc(4,sizeof(int64_t));
    int64_t *cnt=(int64_t*)calloc(4,sizeof(int64_t));
    if(!pkt||!out||!cnt) abort();
    pkt[0*16+0]=op; pkt[0*16+1]=a; pkt[0*16+2]=b;
    pkt[1*16+0]=(int64_t)(uintptr_t)out; pkt[2*16+0]=(int64_t)(uintptr_t)cnt;
    (void) fg382ds_gg_function_1((int64_t)(uintptr_t)pkt, 1);
    const int64_t ret = (int64_t) pkt[2*16+1];
    free(pkt); free(out); free(cnt);
    return ret;
}
int fg382s_enabled(void){ const char*f=getenv("GGML_GOOSE_RECURRENT_IO_PLAN"); return !(f&&strcmp(f,"0")==0); }
