#ifndef FG160_SERVICE_API_H
#define FG160_SERVICE_API_H
#include <stdint.h>
#include <string.h>
#ifndef FG160_CALLER
#define FG160_CALLER 0
#endif
#ifdef __cplusplus
extern "C" {
#endif
extern int fg160_fusion_enabled;
extern int fg160_graph_enabled;
__attribute__((visibility("default"))) uint64_t fg160_query(int64_t mode,const uint64_t *args,int64_t caller);
#ifdef __cplusplus
}
#endif
static inline uint64_t fg160_packet(int64_t mode,uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f,uint64_t g){const uint64_t args[]={a,b,c,d,e,f,g};return fg160_query(mode,args,FG160_CALLER);}
static inline uint64_t fg160_float_bits(float f){uint32_t u;memcpy(&u,&f,4);return u;}
static inline float fg160_float_value(uint64_t v){uint32_t u=(uint32_t)v;float f;memcpy(&f,&u,4);return f;}
#define FG160_A(x) ((uint64_t)(uintptr_t)(x))
#endif
