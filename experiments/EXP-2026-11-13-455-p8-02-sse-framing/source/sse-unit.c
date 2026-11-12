#include <stdint.h>
#include "sse-qualified.c"
int64_t fg455_frame(int64_t flavor,int64_t has_event,int64_t *out){ return fg455_function_1(flavor,has_event,(uint64_t)(uintptr_t)out,0);}
int64_t fg455_size(const int64_t *steps,int64_t n,int64_t payload,int64_t event,uint64_t out){ fg455_slice s; s.data=(void*)steps; s.len=n; return fg455_function_4(s,payload,event,out,0);}
