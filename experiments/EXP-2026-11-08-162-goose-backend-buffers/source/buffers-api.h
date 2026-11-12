#ifndef FG162_BUFFERS_API_H
#define FG162_BUFFERS_API_H
#include <stdint.h>
static bool fg162_enabled=true;
struct fg162_packet{uint64_t v[10];struct ggml_backend_buffer prototype;struct ggml_backend_buffer_i empty_iface;struct ggml_tallocr tallocr;};
static uint64_t fg162_run(struct fg162_packet*);
static inline uint64_t fg162_call(int64_t mode,uint64_t a=0,uint64_t b=0,uint64_t c=0,uint64_t d=0){fg162_packet p{};p.v[0]=(uint64_t)mode;p.v[1]=a;p.v[2]=b;p.v[3]=c;p.v[4]=d;return fg162_run(&p);}
#define FG162_A(x) ((uint64_t)(uintptr_t)(x))
#endif
