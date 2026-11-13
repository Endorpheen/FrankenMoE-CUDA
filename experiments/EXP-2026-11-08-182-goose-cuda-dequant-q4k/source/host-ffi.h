#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint64_t fg182h_get(int64_t,int64_t,int64_t);
int64_t fg182h_plan(int64_t);
void fg182h_put(int64_t,int64_t,int64_t,uint64_t);
int64_t fg182h_launch(int64_t);
void fg182h_fail(int64_t);
uint8_t fg182dh_run(int64_t);
#ifdef __cplusplus
}
#endif
