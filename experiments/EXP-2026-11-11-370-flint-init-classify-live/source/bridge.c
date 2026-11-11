// Bridge for the batch entry classification in llama_batch_allocr::init (EXP352).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
static void fg352s_tick(void){
    const unsigned long long n = atomic_fetch_add(&g_calls, 1) + 1;
    if (n == 1) fprintf(stderr, "FG352S_BATCH_CLASSIFY first call\n");
    if ((n % 100) == 0) fprintf(stderr, "FG352S_BATCH_CLASSIFY calls=%llu\n", n);
}
static void g_fail(int64_t c){ fprintf(stderr, "FG352S validation code %lld\n", (long long)c); abort(); }
static void   g_put(int64_t p,int64_t k,int64_t i,uint64_t v){ ((int64_t*)(uintptr_t)p)[k*16+i]=(int64_t)v; }
static uint64_t g_get(int64_t p,int64_t k,int64_t i){ return (uint64_t)((const int64_t*)(uintptr_t)p)[k*16+i]; }
static void   g_stw(uint64_t b,int64_t o,int64_t v){ *(int64_t*)(uintptr_t)(b+(uint64_t)o)=v; }
static int64_t g_ld(uint64_t b,int64_t o){ return *(const int64_t*)(uintptr_t)(b+(uint64_t)o); }
#define fg352k_get  g_get
#define fg352k_put  g_put
#define fg352k_ld_i64 g_ld
#define fg352k_stw  g_stw
#define fg352k_fail g_fail
#define gg_init_globals fg352s_init_globals
#define main fg352s_generated_main
#include "host-qualified.c"
#undef main

// returns 0 (ok) or a rejection code 1..6; flags[0..3] = mixed, has_token, has_embd, has_state
int64_t fg352s_classify(int64_t n, const int32_t * is_tok, const int32_t * has_embd, const int32_t * has_state,
                        const int32_t * id, int64_t n_vocab, const int32_t * nseqid, const int32_t * seqid,
                        int64_t n_seq_max, int64_t allow_mixed, int32_t * flags) {
    fg352s_tick();
    const int64_t m = n > 0 ? n : 1;
    int64_t *t=(int64_t*)malloc(sizeof(int64_t)*m), *e=(int64_t*)malloc(sizeof(int64_t)*m);
    int64_t *s=(int64_t*)malloc(sizeof(int64_t)*m), *d=(int64_t*)malloc(sizeof(int64_t)*m);
    int64_t *ns=(int64_t*)malloc(sizeof(int64_t)*m), *sq=(int64_t*)malloc(sizeof(int64_t)*m*4);
    if(!t||!e||!s||!d||!ns||!sq) abort();
    for(int64_t i=0;i<n;i++){
        t[i]=is_tok?is_tok[i]:0; e[i]=has_embd?has_embd[i]:0; s[i]=has_state?has_state[i]:0;
        d[i]=id?id[i]:0; ns[i]=nseqid?nseqid[i]:0;
        for(int64_t j=0;j<4;j++) sq[i*4+j]=seqid?seqid[i*4+j]:0;
    }
    const int64_t cap = 4*n + 16;
    int64_t *jr=(int64_t*)malloc(sizeof(int64_t)*3*(size_t)cap);
    int64_t *cnt=(int64_t*)calloc(4,sizeof(int64_t));
    int64_t *pkt=(int64_t*)calloc(8*16,sizeof(int64_t));
    if(!jr||!cnt||!pkt) abort();
    pkt[0*16+0]=n;pkt[0*16+1]=(int64_t)(uintptr_t)t;pkt[0*16+2]=(int64_t)(uintptr_t)e;
    pkt[0*16+3]=(int64_t)(uintptr_t)s;pkt[0*16+4]=(int64_t)(uintptr_t)d;pkt[0*16+5]=n_vocab;
    pkt[0*16+6]=(int64_t)(uintptr_t)ns;pkt[0*16+7]=(int64_t)(uintptr_t)sq;pkt[0*16+8]=n_seq_max;
    pkt[0*16+9]=allow_mixed;
    pkt[1*16+0]=(int64_t)(uintptr_t)jr;pkt[1*16+1]=(int64_t)(uintptr_t)cnt;
    (void)fg352ds_gg_function_1((int64_t)(uintptr_t)pkt,1);
    for (int i = 0; i < 4; ++i) flags[i] = 0;
    for (int64_t i = 0; i < cnt[0]; ++i) {
        if (jr[i*3] == 1) flags[jr[i*3+1]] = (int32_t) jr[i*3+2];
    }
    const int64_t code = (int64_t) pkt[2*16+1];
    free(pkt);free(cnt);free(jr);free(sq);free(ns);free(d);free(s);free(e);free(t);
    return code < 0 ? -code : 0;
}
int fg352s_enabled(void){ const char*f=getenv("GGML_GOOSE_BATCH_CLASSIFY"); return !(f&&strcmp(f,"0")==0); }
