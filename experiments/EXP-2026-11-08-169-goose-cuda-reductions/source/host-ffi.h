#ifndef FG169_HOST_FFI_H
#define FG169_HOST_FFI_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int64_t fg169_arg(int64_t,int64_t);
int64_t fg169_plan_ptr(int64_t);
uint64_t fg169_hget(int64_t,int64_t,int64_t);
void fg169_hput(int64_t,int64_t,int64_t,uint64_t);
int64_t fg169_ti(int64_t,int64_t,int64_t);
uint8_t fg169_pred(int64_t,int64_t);
int64_t fg169_constant(int64_t,int64_t);
void fg169_launch(int64_t);
uint64_t fg169_cub_query(int64_t);
uint64_t fg169_pool_alloc(int64_t,uint64_t);
void fg169_cub_run(int64_t,uint64_t);
void fg169_pool_free(int64_t,uint64_t);
void fg169_fail(int64_t);
#ifdef __cplusplus
}
#endif
#endif
