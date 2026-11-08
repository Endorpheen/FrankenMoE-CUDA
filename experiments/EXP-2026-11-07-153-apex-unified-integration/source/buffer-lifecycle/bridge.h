#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int fg153_buffer_lifecycle_enabled(void);
int64_t fg153_buffer_lifecycle(int64_t,int64_t,int64_t);
int64_t fg153_bl_get(int64_t,int64_t,int64_t);
void fg153_bl_put(int64_t,int64_t,int64_t,int64_t);
int64_t fg153_bl_call(int64_t,int64_t,int64_t,int64_t);
void fg153_bl_witness(int64_t);
#ifdef __cplusplus
}
#endif
