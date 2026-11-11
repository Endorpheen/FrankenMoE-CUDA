#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint64_t fg285h_get(int64_t,int64_t,int64_t);
int64_t fg285h_plan(int64_t);
void fg285h_put(int64_t,int64_t,int64_t,uint64_t);
int64_t fg285h_launch(int64_t);
float fg285h_u32f(uint32_t);
void fg285h_fail(int64_t);
uint8_t fg285dh_run(int64_t);
#ifdef __cplusplus
}
#endif
