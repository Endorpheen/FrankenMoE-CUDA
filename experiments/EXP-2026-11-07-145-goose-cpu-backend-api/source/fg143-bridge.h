#pragma once
#include <stdint.h>
struct fg143_tensor {int64_t ne[4],nb[4],kind,bytes;};
struct fg143_node {struct fg143_tensor x,y,z,d;int64_t op,ith,nth,ip[16];float fp[8];};
#ifdef __cplusplus
extern "C" {
#endif
void fg143_workspace(const struct fg143_node*,int64_t*);
uint8_t fg143_compute(const struct fg143_node*,const void*,const void*,const void*,void*,float*,int64_t,int64_t*,int64_t);
#ifdef __cplusplus
}
#endif
