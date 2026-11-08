#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int fg153_hybrid_inputs_enabled(void);
int64_t fg153_hybrid_inputs(int64_t,int64_t);
int64_t fg153_hi_get(int64_t,int64_t,int64_t);
void fg153_hi_put(int64_t,int64_t,int64_t);
int64_t fg153_hi_ne(int64_t,int64_t);
int64_t fg153_hi_call(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
void fg153_hi_witness(int64_t);
#ifdef __cplusplus
}
#endif
