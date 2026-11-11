#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint64_t fg274h_get(int64_t,int64_t,int64_t);
int64_t fg274h_plan(int64_t);
void fg274h_put(int64_t,int64_t,int64_t,uint64_t);
int64_t fg274h_launch(int64_t);
float fg274h_u32f(uint32_t);
float fg274h_ceilf(float);
void fg274h_fail(int64_t);
uint8_t fg274dh_run(int64_t);
#ifdef __cplusplus
}
#endif
