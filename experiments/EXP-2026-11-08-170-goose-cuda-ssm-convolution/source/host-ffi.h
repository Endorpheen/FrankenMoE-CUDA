#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int64_t fg170s_arg(int64_t,int64_t);
int64_t fg170s_plan_ptr(int64_t);
uint64_t fg170s_hget(int64_t,int64_t,int64_t);
void fg170s_hput(int64_t,int64_t,int64_t,uint64_t);
int64_t fg170s_ti(int64_t,int64_t,int64_t);
uint8_t fg170s_contiguous(int64_t);
int64_t fg170s_f32_type(void);
void fg170s_launch(int64_t);
void fg170s_fail(int64_t);
#ifdef __cplusplus
}
#endif
