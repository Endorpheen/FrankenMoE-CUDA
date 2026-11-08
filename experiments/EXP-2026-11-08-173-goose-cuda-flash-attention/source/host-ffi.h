#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void fg175h_lock(int64_t,int64_t);
void fg175h_unlock(int64_t,int64_t);
int64_t fg175h_arg(int64_t,int64_t);
int64_t fg175h_plan(int64_t);
void fg175h_put(int64_t,int64_t,int64_t,uint64_t);
uint64_t fg175h_get(int64_t,int64_t,int64_t);
void fg175h_putf(int64_t,int64_t,float);
int64_t fg175h_ti(int64_t,int64_t,int64_t);
float fg175h_tpf(int64_t,int64_t);
int64_t fg175h_tpi(int64_t,int64_t);
int64_t fg175h_tc(int64_t);
int64_t fg175h_oc(int64_t);
int64_t fg175h_ndev(void);
int64_t fg175h_dev(int64_t,int64_t);
int64_t fg175h_carch(int64_t);
int64_t fg175h_blck(int64_t);
int64_t fg175h_tsize(int64_t);
int64_t fg175h_nel(int64_t);
int64_t fg175h_nrows(int64_t);
void fg175h_setdev(int64_t);
int64_t fg175h_occ(int64_t);
int64_t fg175h_setattr(int64_t);
int64_t fg175h_memo(int64_t,int64_t);
void fg175h_memo_set(int64_t,int64_t,int64_t);
int64_t fg175h_pool(int64_t,int64_t);
int64_t fg175h_conv(int64_t,int64_t);
int64_t fg175h_launch(int64_t);
int64_t fg175h_lfix(int64_t,int64_t);
float fg175h_powf(float,float);
float fg175h_u2f(uint64_t);
void fg175h_fail(int64_t);
void fg175h_host_entry(int64_t);
#ifdef __cplusplus
}
#endif
