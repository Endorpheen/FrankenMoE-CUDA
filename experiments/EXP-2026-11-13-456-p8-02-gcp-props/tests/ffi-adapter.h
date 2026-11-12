#pragma once
#include "gcp-gen.h"
#ifdef __cplusplus
extern "C" {
#endif
void goose_init_props(int argc, char **argv);
int64_t props_keys(int64_t,int64_t *);
int64_t props_mode(int64_t,int64_t *);
int64_t post_props_decision(int64_t,int64_t *);
#ifdef __cplusplus
}
#endif

static int64_t fg456_mode(const char *p,int64_t n,int64_t *out){
    return mode_enabled((uint64_t)(uintptr_t)p,n,(uint64_t)(uintptr_t)out);
}
static int64_t fg456_route(int64_t hv,const char *v,int64_t vn,const char *d,int64_t dn,
                           char *out,int64_t cap){
    return route(hv,(uint64_t)(uintptr_t)v,vn,(uint64_t)(uintptr_t)d,dn,(uint64_t)(uintptr_t)out,cap);
}
static int64_t fg456_port(int64_t en,int64_t sp,int64_t ap,int64_t *out){
    return port_select(en,sp,ap,(uint64_t)(uintptr_t)out);
}
static int64_t fg456_alias(const char *p,int64_t n,char *out,int64_t cap){
    return gcp_alias((uint64_t)(uintptr_t)p,n,(uint64_t)(uintptr_t)out,cap);
}
static int64_t fg456_pshape(int64_t o,int64_t h,int64_t c,int64_t *out){
    return predict_shape(o,h,c,(uint64_t)(uintptr_t)out);
}
static int64_t fg456_ishape(int64_t o,int64_t h,int64_t *out){
    return instance_shape(o,h,(uint64_t)(uintptr_t)out);
}

static int64_t fg456_keys(int64_t has,int64_t *out){ return props_keys(has,out); }
static int64_t fg456_pmode(int64_t sleeping,int64_t *out){ return props_mode(sleeping,out); }
static int64_t fg456_post(int64_t enabled,int64_t *out){ return post_props_decision(enabled,out); }

#ifdef __cplusplus
extern "C" {
#endif
uint32_t fg456_ld_u8(uint64_t p, uint64_t o) { return ((const uint8_t *) (uintptr_t) p)[o]; }
void     fg456_st_u8(uint64_t p, uint64_t o, uint32_t v) { ((uint8_t *) (uintptr_t) p)[o] = (uint8_t) v; }
void     fg456_st_i64(uint64_t p, uint64_t o, int64_t v) { *(int64_t *) ((char *) (uintptr_t) p + o) = v; }
void     fg457_st_i64(uint64_t p, uint64_t o, int64_t v) { *(int64_t *) ((char *) (uintptr_t) p + o) = v; }
#ifdef __cplusplus
}
#endif
