#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int fg153_hybrid_enabled(void);
int64_t fg153_hybrid(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
int64_t fg153_hm_get(int64_t,int64_t,int64_t);
void fg153_hm_put(int64_t,int64_t,int64_t,int64_t);
int64_t fg153_hm_call(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
void fg153_hm_witness(int64_t);
#ifdef __cplusplus
}
#endif
