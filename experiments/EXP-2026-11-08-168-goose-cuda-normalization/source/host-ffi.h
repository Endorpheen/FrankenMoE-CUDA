#ifndef FG168_HOST_FFI_H
#define FG168_HOST_FFI_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int64_t fg168_arg(int64_t,int64_t);
int64_t fg168_plan_ptr(int64_t);
uint64_t fg168_hget(int64_t,int64_t,int64_t);
void fg168_hput(int64_t,int64_t,int64_t,uint64_t);
void fg168_hfloat(int64_t,int64_t,float);
int64_t fg168_ti(int64_t,int64_t,int64_t);
float fg168_tf(int64_t,int64_t);
uint8_t fg168_contiguous(int64_t);
int64_t fg168_f32_type(void);
void fg168_launch(int64_t);
void fg168_fail(int64_t);
#ifdef __cplusplus
}
#endif
#endif
