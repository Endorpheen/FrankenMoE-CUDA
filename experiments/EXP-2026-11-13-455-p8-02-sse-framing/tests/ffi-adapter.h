#pragma once
#include "sse-gen.h"

static int64_t fg455_frame(int64_t flavor,int64_t has_event,int64_t *out){
    return frame(flavor,has_event,(uint64_t)(uintptr_t)out);
}
static int64_t fg455_size(const int64_t *steps,int64_t n,int64_t payload,int64_t event,uint64_t out){
    sl_i64 s; s.data=(int64_t*)(uintptr_t)steps; s.len=n;
    return frame_size(s,payload,event,out);
}

#ifdef __cplusplus
extern "C" {
#endif
void fg455_st_i64(uint64_t p, uint64_t o, int64_t v) { *(int64_t *) ((char *) (uintptr_t) p + o) = v; }
#ifdef __cplusplus
}
#endif
