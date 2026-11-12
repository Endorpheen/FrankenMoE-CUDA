#pragma once
#include "stream-gen.h"

static int64_t fg453_session_append(int64_t a,int64_t b,int64_t c,int64_t d,uint64_t e){
    return append(a,b,c,d,e);
}
static int64_t fg453_session_read(int64_t a,int64_t b,int64_t c,int64_t d,int64_t e,uint64_t f){
    return read_decision(a,b,c,d,e,f);
}

#ifdef __cplusplus
extern "C" {
#endif
void fg453_st_i64(uint64_t p, uint64_t o, int64_t v) { *(int64_t *) ((char *) (uintptr_t) p + o) = v; }
#ifdef __cplusplus
}
#endif
