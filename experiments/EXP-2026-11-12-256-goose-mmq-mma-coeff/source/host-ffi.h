#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint64_t fg256s_get(int64_t,int64_t,int64_t);
void fg256s_put(int64_t,int64_t,int64_t,uint64_t);
void fg256s_fail(int64_t);
int64_t fg256s_f2b(float);
float fg256s_b2f(int64_t);
float fg256s_fma(float,float,float);
float fg256s_mul(float,float);
uint8_t fg186qh_run(int64_t);
#ifdef __cplusplus
}
#endif
