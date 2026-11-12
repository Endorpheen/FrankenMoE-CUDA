// Bridge for the aligned-read arithmetic (EXP377; native line llama-mmap.cpp).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
static void g_fail(int64_t c){ fprintf(stderr, "FG377S validation code %lld\n", (long long)c); abort(); }
static void   g_put(int64_t p,int64_t k,int64_t i,uint64_t v){ ((int64_t*)(uintptr_t)p)[k*16+i]=(int64_t)v; }
static uint64_t g_get(int64_t p,int64_t k,int64_t i){ return (uint64_t)((const int64_t*)(uintptr_t)p)[k*16+i]; }
static void   g_stw(uint64_t b,int64_t o,int64_t v){ *(int64_t*)(uintptr_t)(b+(uint64_t)o)=v; }
static int64_t g_ld(uint64_t b,int64_t o){ return *(const int64_t*)(uintptr_t)(b+(uint64_t)o); }
#define fg377k_get  g_get
#define fg377k_put  g_put
#define fg377k_ld_i64 g_ld
#define fg377k_stw  g_stw
#define fg377k_fail g_fail
#define gg_init_globals fg377s_init_globals
#define main fg377s_generated_main
#include "host-qualified.c"
#undef main

// offset, size, alignment, cap -> { aligned_offset, bytes_to_read, skip, buffer_size }
int64_t fg377s_plan(int64_t offset, int64_t size, int64_t alignment, int64_t cap, int64_t * out) {
    const unsigned long long n = atomic_fetch_add(&g_calls, 1) + 1;
    if (n == 1) fprintf(stderr, "FG377S_FILE_ALIGN first call calls=1\n");
    if ((n % 1000) == 0) fprintf(stderr, "FG377S_FILE_ALIGN calls=%llu\n", n);

    int64_t *pkt=(int64_t*)calloc(8*16,sizeof(int64_t));
    if(!pkt) abort();
    pkt[0*16+0]=offset; pkt[0*16+1]=size; pkt[0*16+2]=alignment; pkt[0*16+3]=cap;
    pkt[1*16+0]=(int64_t)(uintptr_t)out;
    (void) fg377ds_gg_function_1((int64_t)(uintptr_t)pkt, 1);
    free(pkt);
    return 0;
}
int fg377s_enabled(void){ const char*f=getenv("GGML_GOOSE_FILE_ALIGN"); return !(f&&strcmp(f,"0")==0); }
