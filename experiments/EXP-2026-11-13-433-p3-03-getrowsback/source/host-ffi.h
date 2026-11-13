#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint64_t fg433h_get(int64_t,int64_t,int64_t);
int64_t fg433h_plan(int64_t);
void fg433h_put(int64_t,int64_t,int64_t,uint64_t);
int64_t fg433h_launch(int64_t);
void fg433h_fail(int64_t);
uint8_t fg433h_run(int64_t);
#ifdef __cplusplus
}
#endif
