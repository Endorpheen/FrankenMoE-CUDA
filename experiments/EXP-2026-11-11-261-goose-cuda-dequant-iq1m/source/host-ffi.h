#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint64_t fg261h_get(int64_t,int64_t,int64_t);
int64_t fg261h_plan(int64_t);
void fg261h_put(int64_t,int64_t,int64_t,uint64_t);
int64_t fg261h_launch(int64_t);
void fg261h_fail(int64_t);
uint8_t fg261dh_run(int64_t);
#ifdef __cplusplus
}
#endif
