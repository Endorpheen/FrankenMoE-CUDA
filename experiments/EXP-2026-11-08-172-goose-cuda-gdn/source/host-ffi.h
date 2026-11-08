#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int64_t fg174s_arg(int64_t,int64_t);
int64_t fg174s_plan_ptr(int64_t);
uint64_t fg174s_hget(int64_t,int64_t,int64_t);
void fg174s_hput(int64_t,int64_t,int64_t,uint64_t);
void fg174s_hfloat(int64_t,int64_t,float);
int64_t fg174s_ti(int64_t,int64_t,int64_t);
uint8_t fg174s_contiguous(int64_t);
int64_t fg174s_f32_type(void);
int64_t fg174s_warp_size(void);
int64_t fg174s_nsm(void);
float fg174s_sqrt(float);
void fg174s_launch(int64_t);
void fg174s_fail(int64_t);
uint8_t fg174m_launch(int64_t);
#ifdef __cplusplus
}
#endif
