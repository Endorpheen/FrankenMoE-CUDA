#pragma once
#include "offsets-gen.h"

static int64_t fg457_up(int64_t x,int64_t n){ return align_up(x,n); }
static int64_t fg457_ok(int64_t a){ return align_ok(a); }
static int64_t fg457_nbytes(int64_t t,int64_t b,int64_t ne,int64_t *out){
    return nbytes(t,b,ne,(uint64_t)(uintptr_t)out);
}
static int64_t fg457_layout(const int64_t *rel,const int64_t *nbv,int64_t n,int64_t align,
                            int64_t data_start,int64_t *out){
    sl_i64 r; r.data=(int64_t*)(uintptr_t)rel; r.len=n;
    sl_i64 b; b.data=(int64_t*)(uintptr_t)nbv; b.len=n;
    sl_i64 o; o.data=(int64_t*)(uintptr_t)out; o.len=n+1;
    return layout(r,b,align,data_start,o);
}

#ifdef __cplusplus
extern "C" {
#endif
void fg457_st_i64(uint64_t p, uint64_t o, int64_t v) { *(int64_t *) ((char *) (uintptr_t) p + o) = v; }
#ifdef __cplusplus
}
#endif
