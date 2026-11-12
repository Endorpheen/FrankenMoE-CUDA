#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint64_t fg341k_get(int64_t,int64_t,int64_t);
void fg341k_put(int64_t,int64_t,int64_t,uint64_t);
int64_t fg341k_ld_i64(uint64_t,int64_t);
void fg341k_stw(uint64_t,int64_t,int64_t);
void fg341k_fail(int64_t);
uint8_t fg341dh_run(int64_t);
#ifdef __cplusplus
}
#endif
