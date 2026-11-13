#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint64_t fg413h_get(int64_t,int64_t,int64_t);
int64_t fg413h_plan(int64_t);
void fg413h_put(int64_t,int64_t,int64_t,uint64_t);
int64_t fg413h_launch(int64_t);
void fg413h_fail(int64_t);
uint8_t fg413h_run(int64_t);
#ifdef __cplusplus
}
#endif
