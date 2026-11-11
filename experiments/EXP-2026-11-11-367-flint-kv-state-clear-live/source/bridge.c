// Bridge for the KV state clearing plan (EXP367).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
static void fg367s_tick(void){
    const unsigned long long n = atomic_fetch_add(&g_calls, 1) + 1;
    if (n == 1) fprintf(stderr, "FG367S_KV_STATE_CLEAR first call\n");
    if ((n % 100) == 0) fprintf(stderr, "FG367S_KV_STATE_CLEAR calls=%llu\n", n);
}
static void g_fail(int64_t c){ fprintf(stderr, "FG367S validation code %lld\n", (long long)c); abort(); }
static void   g_put(int64_t p,int64_t k,int64_t i,uint64_t v){ ((int64_t*)(uintptr_t)p)[k*16+i]=(int64_t)v; }
static uint64_t g_get(int64_t p,int64_t k,int64_t i){ return (uint64_t)((const int64_t*)(uintptr_t)p)[k*16+i]; }
static void   g_stw(uint64_t b,int64_t o,int64_t v){ *(int64_t*)(uintptr_t)(b+(uint64_t)o)=v; }
static int64_t g_ld(uint64_t b,int64_t o){ return *(const int64_t*)(uintptr_t)(b+(uint64_t)o); }
#define fg367k_get  g_get
#define fg367k_put  g_put
#define fg367k_ld_i64 g_ld
#define fg367k_stw  g_stw
#define fg367k_fail g_fail
#define gg_init_globals fg367s_init_globals
#define main fg367s_generated_main
#include "host-qualified.c"
#undef main

// returns the number of clears; fills (kind, layer, offset, size) quads
int64_t fg367s_clear(int64_t n_idx, const int32_t * idxs, int64_t head, int64_t cells_size, int64_t n_layer,
                     const int32_t * k_row, const int32_t * v_present, const int32_t * v_row,
                     int64_t v_trans, int64_t v_el, int64_t contig,
                     int32_t * out_kind, int32_t * out_il, int64_t * out_off, int64_t * out_size,
                     int64_t cap) {
    fg367s_tick();
    const int64_t nI = n_idx > 0 ? n_idx : 1;
    const int64_t nL = n_layer > 0 ? n_layer : 1;
    int64_t *ix=(int64_t*)malloc(sizeof(int64_t)*nI);
    int64_t *kr=(int64_t*)malloc(sizeof(int64_t)*nL);
    int64_t *vp=(int64_t*)malloc(sizeof(int64_t)*nL);
    int64_t *vr=(int64_t*)malloc(sizeof(int64_t)*nL);
    if(!ix||!kr||!vp||!vr) abort();
    for(int64_t i=0;i<n_idx;i++) ix[i]=idxs?idxs[i]:0;
    for(int64_t i=0;i<n_layer;i++){kr[i]=k_row?k_row[i]:0;vp[i]=v_present?v_present[i]:0;vr[i]=v_row?v_row[i]:0;}
    const int64_t capj = 4*(n_layer*4 + n_idx*n_layer*4 + 16);
    int64_t *jr=(int64_t*)malloc(sizeof(int64_t)*4*(size_t)capj);
    int64_t *cnt=(int64_t*)calloc(4,sizeof(int64_t));
    int64_t *pkt=(int64_t*)calloc(8*16,sizeof(int64_t));
    if(!jr||!cnt||!pkt) abort();
    pkt[0*16+0]=n_idx;pkt[0*16+1]=(int64_t)(uintptr_t)ix;pkt[0*16+2]=head;pkt[0*16+3]=cells_size;
    pkt[0*16+4]=n_layer;pkt[0*16+5]=(int64_t)(uintptr_t)kr;pkt[0*16+6]=(int64_t)(uintptr_t)vp;
    pkt[0*16+7]=(int64_t)(uintptr_t)vr;pkt[0*16+8]=v_trans;pkt[0*16+9]=v_el;pkt[0*16+10]=contig;
    pkt[1*16+0]=(int64_t)(uintptr_t)jr;pkt[1*16+1]=(int64_t)(uintptr_t)cnt;
    (void)fg367ds_gg_function_1((int64_t)(uintptr_t)pkt,1);
    const int64_t total = pkt[2*16+1];
    int64_t n = 0;
    for (int64_t i = 0; i < total && n < cap; ++i) {
        out_kind[n] = (int32_t) jr[i*4];
        out_il[n]   = (int32_t) jr[i*4+1];
        out_off[n]  = jr[i*4+2];
        out_size[n] = jr[i*4+3];
        ++n;
    }
    free(pkt);free(cnt);free(jr);free(vr);free(vp);free(kr);free(ix);
    return n;
}
int fg367s_enabled(void){ const char*f=getenv("GGML_GOOSE_KV_STATE_CLEAR"); return !(f&&strcmp(f,"0")==0); }
