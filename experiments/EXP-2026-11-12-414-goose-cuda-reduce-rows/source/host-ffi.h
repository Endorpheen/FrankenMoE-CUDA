#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint64_t fg414h_get(int64_t,int64_t,int64_t);
int64_t fg414h_plan(int64_t);
void fg414h_put(int64_t,int64_t,int64_t,uint64_t);
int64_t fg414h_launch(int64_t);
void fg414h_fail(int64_t);
uint8_t fg414h_run(int64_t);
#ifdef __cplusplus
}
#endif
