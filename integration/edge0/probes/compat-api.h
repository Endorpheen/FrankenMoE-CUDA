#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void * e0gpu_create(const void * du,size_t bytes);
void e0gpu_destroy(void * state);
int e0gpu_dot(void * state,int type,uint64_t key,int matrix,const void * record,
              const void * activation,size_t activation_bytes,float * out,int n,int rows);
void e0gpu_stats(void * state);
#ifdef __cplusplus
}
#endif
