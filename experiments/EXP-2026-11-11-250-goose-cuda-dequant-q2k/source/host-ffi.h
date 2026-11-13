#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint64_t fg250h_get(int64_t,int64_t,int64_t);
int64_t fg250h_plan(int64_t);
void fg250h_put(int64_t,int64_t,int64_t,uint64_t);
int64_t fg250h_launch(int64_t);
void fg250h_fail(int64_t);
uint8_t fg250dh_run(int64_t);
#ifdef __cplusplus
}
#endif
