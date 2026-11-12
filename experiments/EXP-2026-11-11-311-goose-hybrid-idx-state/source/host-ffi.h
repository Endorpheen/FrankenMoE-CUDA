#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint64_t fg311k_get(int64_t,int64_t,int64_t);
void fg311k_put(int64_t,int64_t,int64_t,uint64_t);
int64_t fg311k_ld_i64(uint64_t,int64_t);
void fg311k_st_i64(uint64_t,int64_t,int64_t);
void fg311k_fail(int64_t);
uint8_t fg311dh_run(int64_t);
#ifdef __cplusplus
}
#endif
