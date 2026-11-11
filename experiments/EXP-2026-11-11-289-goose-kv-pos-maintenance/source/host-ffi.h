#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void fg289k_fail(int64_t);
int64_t fg289k_ld_i64(uint64_t,int64_t);
void fg289k_st_i64(uint64_t,int64_t,int64_t);
uint64_t fg289h_get(int64_t,int64_t,int64_t);
void fg289h_put(int64_t,int64_t,int64_t,uint64_t);
uint8_t fg289dh_run(int64_t);
#ifdef __cplusplus
}
#endif
