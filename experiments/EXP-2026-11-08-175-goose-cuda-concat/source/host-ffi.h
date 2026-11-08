#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int64_t fg175cn_arg(int64_t,int64_t);
int64_t fg175cn_plan(int64_t);
void fg175cn_put(int64_t,int64_t,int64_t,uint64_t);
int64_t fg175cn_ti(int64_t,int64_t,int64_t);
int64_t fg175cn_tc(int64_t);
int64_t fg175cn_oc(int64_t);
int64_t fg175cn_tblk(int64_t);
int64_t fg175cn_tsz(int64_t);
int64_t fg175cn_cont3(int64_t);
int64_t fg175cn_cont(int64_t);
int64_t fg175cn_nb3(int64_t);
int64_t fg175cn_launch(int64_t);
void fg175cn_fail(int64_t);
uint8_t fg175cnh_run(int64_t);
#ifdef __cplusplus
}
#endif
