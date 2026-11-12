#pragma once
#include "manager-gen.h"

static int64_t fg454_retire(int64_t op,int64_t found,uint64_t out){ return retire_plan(op,found,out); }
static int64_t fg454_stop(uint64_t out){ return stop_all_plan(out); }
static int64_t fg454_cutoff(int64_t now,int64_t ttl,uint64_t out){ return gc_cutoff(now,ttl,out); }
static int64_t fg454_gc(int64_t completed,int64_t now,int64_t ttl,uint64_t out){
    return gc_decision(completed,now,ttl,out);
}

#ifdef __cplusplus
extern "C" {
#endif
void fg454_st_i64(uint64_t p, uint64_t o, int64_t v) { *(int64_t *) ((char *) (uintptr_t) p + o) = v; }
#ifdef __cplusplus
}
#endif
