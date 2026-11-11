// Bridge for the Q4_K x Q8_K dot (EXP410). Byte access, fp16 decode and float bits stay here; the loop is Goose.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <math.h>
static _Atomic unsigned long long g_calls;
static void   g_stb(uint64_t b,int64_t o,int64_t v){ *(uint8_t*)(uintptr_t)(b+(uint64_t)o)=(uint8_t)v; }
static void   g_stw(uint64_t b,int64_t o,int64_t v){ *(int64_t*)(uintptr_t)(b+(uint64_t)o)=v; }
static int64_t g_ld(uint64_t b,int64_t o){ return *(const int64_t*)(uintptr_t)(b+(uint64_t)o); }
static void   g_put(int64_t p,int64_t k,int64_t i,uint64_t v){ ((int64_t*)(uintptr_t)p)[k*16+i]=(int64_t)v; }
static uint64_t g_get(int64_t p,int64_t k,int64_t i){ return (uint64_t)((const int64_t*)(uintptr_t)p)[k*16+i]; }
static int64_t g_ldu8(uint64_t b,int64_t o){ return *(const uint8_t*)(uintptr_t)(b+(uint64_t)o); }
static int64_t g_ldi8(uint64_t b,int64_t o){ return *(const int8_t*)(uintptr_t)(b+(uint64_t)o); }
static int64_t g_ldu32(uint64_t b,int64_t o){ uint32_t v; memcpy(&v,(const void*)(uintptr_t)(b+(uint64_t)o),4); return v; }
static int64_t g_ldi16(uint64_t b,int64_t o){ int16_t v; memcpy(&v,(const void*)(uintptr_t)(b+(uint64_t)o),2); return v; }
static float   g_ldf(uint64_t b,int64_t o){ float v; memcpy(&v,(const void*)(uintptr_t)(b+(uint64_t)o),4); return v; }
static float   g_f16f32(int64_t h){ uint16_t u=(uint16_t)h; uint32_t s=(u>>15)&1, e=(u>>10)&0x1f, m=u&0x3ff; uint32_t o;
    if(e==0){ if(m==0) o=s<<31; else { int sh=0; while(!(m&0x400)){ m<<=1; ++sh; } m&=0x3ff; o=(s<<31)|((uint32_t)(127-15-sh)<<23)|(m<<13); } }
    else if(e==31){ o=(s<<31)|0x7f800000|(m<<13); }
    else { o=(s<<31)|((e+112)<<23)|(m<<13); }
    float f; memcpy(&f,&o,4); return f; }
static int64_t g_f32bits(float v){ int32_t u; memcpy(&u,&v,4); return u; }
static void g_fail(int64_t c){ fprintf(stderr, "FG410S validation code %lld\n", (long long)c); abort(); }
#define fg410k_get  g_get
#define fg410k_put  g_put
#define fg410k_stw  g_stw
#define fg410k_stb  g_stb
#define fg410k_ld_i64 g_ld
#define fg410k_ldu8 g_ldu8
#define fg410k_ldi8 g_ldi8
#define fg410k_ldi16 g_ldi16
#define fg410k_ldu32 g_ldu32
#define fg410k_ldf  g_ldf
#define fg410k_f16f32 g_f16f32
#define fg410k_f32bits g_f32bits
#define fg410k_fail g_fail
#define gg_init_globals fg410s_init_globals
#define main fg410s_generated_main
#include "host-qualified.c"
#undef main

float fg410s_dot(int64_t nb, const void * q4, const void * q8) {
    const unsigned long long c = atomic_fetch_add(&g_calls, 1) + 1;
    if (c == 1) fprintf(stderr, "FG410S_Q4K_DOT first call calls=1\n");
    if ((c % 1000) == 0) fprintf(stderr, "FG410S_Q4K_DOT calls=%llu\n", c);
    int64_t *pkt=(int64_t*)calloc(8*16,sizeof(int64_t));
    int64_t *sc=(int64_t*)calloc(1100,sizeof(int64_t));
    if(!pkt||!sc) abort();
    pkt[0*16+0]=nb; pkt[0*16+1]=(int64_t)(uintptr_t)q4; pkt[0*16+2]=(int64_t)(uintptr_t)q8;
    pkt[1*16+0]=(int64_t)(uintptr_t)sc; pkt[2*16+0]=0;
    (void) fg410ds_gg_function_1((int64_t)(uintptr_t)pkt, 1);
    if (getenv("FG410_DEBUG")) {
        float ddbg, s0dbg; int64_t sib;
        { int32_t u; memcpy(&u, (char*)sc + 1300, 4); memcpy(&ddbg, &u, 4);
          memcpy(&u, (char*)sc + 1316, 4); memcpy(&s0dbg, &u, 4);
          memcpy(&sib, (char*)sc + 1308, 8); }
        fprintf(stderr, "DBG nb=%lld raw=%lld d=%.9g sumi=%lld sums0=%.9g\n",
                (long long) nb, (long long) pkt[2*16+0], ddbg, (long long) sib, s0dbg);
    }
    float r; { int32_t u = (int32_t) pkt[2*16+0]; memcpy(&r, &u, 4); }
    free(pkt); free(sc);
    return r;
}
int fg410s_enabled(void){ const char* f=getenv("GGML_GOOSE_CPU_Q4K_DOT"); return !(f&&strcmp(f,"0")==0); }
