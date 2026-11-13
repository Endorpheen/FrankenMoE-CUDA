#ifndef FG163_STREAM_API_H
#define FG163_STREAM_API_H
#include <stdint.h>
static bool fg163_enabled=true;
static uint64_t fg163_run(const uint64_t*);
static inline uint64_t fg163_call(int64_t mode,uint64_t a=0,uint64_t b=0,uint64_t c=0,uint64_t d=0,uint64_t e=0,uint64_t f=0,uint64_t g=0,uint64_t h=0){const uint64_t args[]={(uint64_t)mode,a,b,c,d,e,f,g,h};return fg163_run(args);}
#define FG163_A(x) ((uint64_t)(uintptr_t)(x))
#endif
