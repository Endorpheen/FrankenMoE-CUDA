#ifndef FG169T_HOST_FFI_H
#define FG169T_HOST_FFI_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int64_t fg169t_arg(int64_t,int64_t);
int64_t fg169t_plan_ptr(int64_t);
uint64_t fg169t_hget(int64_t,int64_t,int64_t);
void fg169t_hput(int64_t,int64_t,int64_t,uint64_t);
void fg169t_hfloat(int64_t,int64_t,float);
int64_t fg169t_ti(int64_t,int64_t,int64_t);
float fg169t_tf(int64_t,int64_t);
uint8_t fg169t_contiguous(int64_t);
int64_t fg169t_cfg(int64_t,int64_t);
int64_t fg169t_constant(int64_t);
void fg169t_launch(int64_t);
void fg169t_fail(int64_t);
#ifdef __cplusplus
}
#endif
#endif
