#pragma once
#include <stdint.h>
struct fg_el_shape {int64_t ne[4],stride[4],kind;};
struct fg_el_plan {struct fg_el_shape a,b,d;int64_t ith,nth,mode,op;float params[4];};
#ifdef __cplusplus
extern "C" {
#endif
void fg_element_compute(const struct fg_el_plan*,const void*,int64_t,const void*,int64_t,void*,int64_t);
#ifdef __cplusplus
}
#endif
