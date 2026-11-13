#pragma once
#include "io-gen.h"

static int64_t fg458_fits(int64_t s,int64_t r){ return fits(s,r); }
static int64_t fg458_arr(int64_t n,int64_t e,int64_t r,int64_t *out){ return array_check(n,e,r,(uint64_t)(uintptr_t)out); }
static int64_t fg458_str(int64_t s,int64_t r,int64_t *out){ return string_check(s,r,(uint64_t)(uintptr_t)out); }
static int64_t fg458_seek(int64_t a,int64_t o,int64_t r,int64_t *out){ return seek(a,o,r,(uint64_t)(uintptr_t)out); }
static int64_t fg458_raw(int64_t s,int64_t c,int64_t o,int64_t r,int64_t end,int64_t *out){
    return read_raw(s,c,o,r,end,(uint64_t)(uintptr_t)out);
}
static int64_t fg458_fremain(int64_t end,int64_t cur,int64_t *out){ return file_remain(end,cur,(uint64_t)(uintptr_t)out); }

#ifdef __cplusplus
extern "C" {
#endif
void fg458_st_i64(uint64_t p, uint64_t o, int64_t v) { *(int64_t *) ((char *) (uintptr_t) p + o) = v; }
#ifdef __cplusplus
}
#endif
