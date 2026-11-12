#ifndef FG164_SCHEDULER_API_H
#define FG164_SCHEDULER_API_H
#include <stdint.h>
static bool fg164_enabled=true;
struct fg164_packet{uint64_t args[9];struct ggml_cgraph view;};
static uint64_t fg164_run(fg164_packet*);
static inline uint64_t fg164_call(int64_t mode,uint64_t a=0,uint64_t b=0,uint64_t c=0,uint64_t d=0,uint64_t e=0,uint64_t f=0){fg164_packet p{};p.args[0]=(uint64_t)mode;p.args[1]=a;p.args[2]=b;p.args[3]=c;p.args[4]=d;p.args[5]=e;p.args[6]=f;return fg164_run(&p);}
#define FG164_A(x) ((uint64_t)(uintptr_t)(x))
static void fg164_dep_notify(void*data,ggml_tensor*t,ggml_tensor*until){fg164_call(50,FG164_A(data),FG164_A(t),FG164_A(until));}
#endif
