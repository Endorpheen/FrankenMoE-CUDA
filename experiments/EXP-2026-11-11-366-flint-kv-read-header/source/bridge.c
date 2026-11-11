// Bridge for the KV state-read header decisions (EXP366).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
static void fg366s_tick(void){
    const unsigned long long n = atomic_fetch_add(&g_calls, 1) + 1;
    if (n == 1) fprintf(stderr, "FG366S_KV_READ_HEADER first call\n");
    if ((n % 100) == 0) fprintf(stderr, "FG366S_KV_READ_HEADER calls=%llu\n", n);
}
static void g_fail(int64_t c){ fprintf(stderr, "FG366S validation code %lld\n", (long long)c); abort(); }
static void   g_put(int64_t p,int64_t k,int64_t i,uint64_t v){ ((int64_t*)(uintptr_t)p)[k*16+i]=(int64_t)v; }
static uint64_t g_get(int64_t p,int64_t k,int64_t i){ return (uint64_t)((const int64_t*)(uintptr_t)p)[k*16+i]; }
static void   g_stw(uint64_t b,int64_t o,int64_t v){ *(int64_t*)(uintptr_t)(b+(uint64_t)o)=v; }
static int64_t g_ld(uint64_t b,int64_t o){ return *(const int64_t*)(uintptr_t)(b+(uint64_t)o); }
#define fg366k_get  g_get
#define fg366k_put  g_put
#define fg366k_ld_i64 g_ld
#define fg366k_stw  g_stw
#define fg366k_fail g_fail
#define gg_init_globals fg366s_init_globals
#define main fg366s_generated_main
#include "host-qualified.c"
#undef main

int64_t fg366s_read_header(int64_t nl_ref,int64_t nl,int64_t cc,int64_t size,int64_t tr_ref,int64_t tr,
                           int64_t rk_ref,int64_t rk,int64_t rv_ref,int64_t rv,
                           const int32_t* kt,const int32_t* kr,const int32_t* vt,const int32_t* vr,
                           uint64_t* k_mask,uint64_t* v_mask){
    fg366s_tick();
    const int64_t n = nl > 0 ? nl : 1;
    int64_t *a=(int64_t*)malloc(sizeof(int64_t)*n), *b=(int64_t*)malloc(sizeof(int64_t)*n);
    int64_t *c=(int64_t*)malloc(sizeof(int64_t)*n), *d=(int64_t*)malloc(sizeof(int64_t)*n);
    int64_t *jr=(int64_t*)malloc(sizeof(int64_t)*3*(size_t)(nl+8));
    int64_t *cnt=(int64_t*)calloc(4,sizeof(int64_t));
    int64_t *pkt=(int64_t*)calloc(8*16,sizeof(int64_t));
    if(!a||!b||!c||!d||!jr||!cnt||!pkt) abort();
    for(int64_t i=0;i<nl;i++){a[i]=kt?kt[i]:0;b[i]=kr?kr[i]:0;c[i]=vt?vt[i]:0;d[i]=vr?vr[i]:0;}
    pkt[0*16+0]=nl_ref;pkt[0*16+1]=nl;pkt[0*16+2]=cc;pkt[0*16+3]=size;
    pkt[0*16+4]=tr_ref;pkt[0*16+5]=tr;pkt[0*16+6]=rk_ref;pkt[0*16+7]=rk;
    pkt[0*16+8]=rv_ref;pkt[0*16+9]=rv;
    pkt[0*16+10]=(int64_t)(uintptr_t)a;pkt[0*16+11]=(int64_t)(uintptr_t)b;
    pkt[0*16+12]=(int64_t)(uintptr_t)c;pkt[0*16+13]=(int64_t)(uintptr_t)d;
    pkt[1*16+0]=(int64_t)(uintptr_t)jr;pkt[1*16+1]=(int64_t)(uintptr_t)cnt;
    (void)fg366ds_gg_function_1((int64_t)(uintptr_t)pkt,1);
    *k_mask=0;*v_mask=0;
    for(int64_t i=0;i<cnt[0];i++){
        if(jr[i*3]==0){ if(jr[i*3+1]==0)*k_mask=(uint64_t)jr[i*3+2]; else *v_mask=(uint64_t)jr[i*3+2]; }
    }
    const int64_t code=(int64_t)pkt[2*16+1];
    free(pkt);free(cnt);free(jr);free(d);free(c);free(b);free(a);
    return code<0?-code:0;
}
int fg366s_enabled(void){ const char*f=getenv("GGML_GOOSE_KV_READ_HEADER"); return !(f&&strcmp(f,"0")==0); }
