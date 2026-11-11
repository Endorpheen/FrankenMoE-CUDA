// Bridge for the KV slot walk (EXP392). Flat arrays in, indices out; the predicate is fg199k_can_use.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
static void g_fail(int64_t c){ fprintf(stderr, "FG392S validation code %lld\n", (long long)c); abort(); }
static void   g_put(int64_t p,int64_t k,int64_t i,uint64_t v){ ((int64_t*)(uintptr_t)p)[k*16+i]=(int64_t)v; }
static uint64_t g_get(int64_t p,int64_t k,int64_t i){ return (uint64_t)((const int64_t*)(uintptr_t)p)[k*16+i]; }
static int64_t g_ld(uint64_t b,int64_t o){ return *(const int64_t*)(uintptr_t)(b+(uint64_t)o); }
static void   g_stw(uint64_t b,int64_t o,int64_t v){ *(int64_t*)(uintptr_t)(b+(uint64_t)o)=v; }
#define fg392k_get  g_get
#define fg392k_put  g_put
#define fg392k_ld_i64 g_ld
#define fg392k_stw  g_stw
#define fg392k_fail g_fail
extern int64_t fg199k_can_use(int64_t occupied, int64_t seq_count, int64_t pos_cell,
                              int64_t seq_pos_max_1, int64_t n_swa, int64_t swa_type);
#define fg392k_can fg199k_can_use
#define gg_init_globals fg392s_init_globals
#define main fg392s_generated_main
#include "host-qualified.c"
#undef main

// Walk the cache from `head` and fill `out` with n_tokens indices, or return -1.
int64_t fg392s_find_slots(int64_t size, int64_t head, int64_t n_tokens, int64_t cont,
                          int64_t n_swa, int64_t swa_type,
                          const int64_t * pos, const int64_t * seqcnt, const int64_t * maxp1,
                          int64_t * out, int64_t cap) {
    const unsigned long long c = atomic_fetch_add(&g_calls, 1) + 1;
    if (c == 1) fprintf(stderr, "FG392S_KV_FIND_SLOT first call\n");
    if ((c % 1000) == 0) fprintf(stderr, "FG392S_KV_FIND_SLOT calls=%llu\n", c);

    int64_t *pkt=(int64_t*)calloc(8*16,sizeof(int64_t));
    int64_t *cnt=(int64_t*)calloc(4,sizeof(int64_t));
    if(!pkt||!cnt) abort();
    pkt[0*16+0]=size; pkt[0*16+1]=head; pkt[0*16+2]=n_tokens; pkt[0*16+3]=cont;
    pkt[0*16+4]=n_swa; pkt[0*16+5]=swa_type;
    pkt[1*16+0]=(int64_t)(uintptr_t)pos; pkt[1*16+1]=(int64_t)(uintptr_t)seqcnt;
    pkt[1*16+2]=(int64_t)(uintptr_t)maxp1; pkt[1*16+3]=(int64_t)(uintptr_t)out; pkt[1*16+4]=cap;
    pkt[2*16+0]=(int64_t)(uintptr_t)cnt;
    (void) fg392ds_gg_function_1((int64_t)(uintptr_t)pkt, 1);
    const int64_t ret = (int64_t) pkt[2*16+1];
    free(pkt); free(cnt);
    return ret;
}
int fg392s_enabled(void){ const char*f=getenv("GGML_GOOSE_KV_FIND_SLOT"); return !(f&&strcmp(f,"0")==0); }
