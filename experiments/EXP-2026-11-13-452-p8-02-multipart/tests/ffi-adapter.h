#pragma once
#include "multipart-gen.h"

static int64_t fg452_decode(uint64_t a,int64_t b,uint64_t c,int64_t d){ return decode_component(a,b,c,d); }
static int64_t fg452_actions(int64_t n,const char *keys,int64_t klen,
                             const int64_t *koffs,const int64_t *klens,int64_t *out){
    sl_u8 k; k.data=(uint8_t*)(uintptr_t)keys; k.len=klen;
    sl_i64 so; so.data=(int64_t*)(uintptr_t)koffs; so.len=n;
    sl_i64 sl; sl.data=(int64_t*)(uintptr_t)klens; sl.len=n;
    sl_i64 sr; sr.data=(int64_t*)(uintptr_t)out;   sr.len=n;
    return form_actions(n,k,so,sl,sr);
}
static int64_t fg452_source(int64_t a,int64_t b){ return param_source(a,b); }

#ifdef __cplusplus
extern "C" {
#endif
uint32_t fg452_ld_u8(uint64_t p, uint64_t o) { return ((const uint8_t *) (uintptr_t) p)[o]; }
void     fg452_st_u8(uint64_t p, uint64_t o, uint32_t v) { ((uint8_t *) (uintptr_t) p)[o] = (uint8_t) v; }
void     fg452_st_i64(uint64_t p, uint64_t o, int64_t v) { *(int64_t *) ((char *) (uintptr_t) p + o) = v; }
#ifdef __cplusplus
}
#endif
