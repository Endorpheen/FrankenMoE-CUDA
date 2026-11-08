#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int64_t fg175s_arg(int64_t,int64_t);
int64_t fg175s_plan(int64_t);
void fg175s_put(int64_t,int64_t,int64_t,uint64_t);
uint64_t fg175s_get(int64_t,int64_t,int64_t);
int64_t fg175s_ti(int64_t,int64_t,int64_t);
int64_t fg175s_tc(int64_t);
int64_t fg175s_oc(int64_t);
int64_t fg175s_launch(int64_t);
void fg175s_fail(int64_t);
uint8_t fg176rh_run(int64_t);
#ifdef __cplusplus
}
#endif
