#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int64_t fg171r_arg(int64_t,int64_t);
int64_t fg171r_plan_ptr(int64_t);
uint64_t fg171r_hget(int64_t,int64_t,int64_t);
void fg171r_hput(int64_t,int64_t,int64_t,uint64_t);
void fg171r_hfloat(int64_t,int64_t,float);
int64_t fg171r_ti(int64_t,int64_t,int64_t);
float fg171r_tf(int64_t,int64_t);
uint8_t fg171r_contiguous(int64_t);
int64_t fg171r_f32_type(void);
int64_t fg171r_f16_type(void);
int64_t fg171r_i32_type(void);
float fg171r_hfloor(float);
float fg171r_hceil(float);
float fg171r_hlog(float);
float fg171r_hpow(float,float);
void fg171r_launch(int64_t);
void fg171r_fail(int64_t);
#ifdef __cplusplus
}
#endif
