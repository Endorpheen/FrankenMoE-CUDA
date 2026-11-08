#pragma once
#include "../buffers/bridge.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int fg153_scheduler_enabled(void);
int64_t fg153_scheduler(int64_t,int64_t,int64_t,int64_t);
int64_t fg153_sg(int64_t,int64_t,int64_t);
void fg153_sp(int64_t,int64_t,int64_t,int64_t);
int64_t fg153_ss(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
void fg153_sw(int64_t);
uint64_t fg153_scheduler_count(int);
int fg153_scheduler_validate_layout(void);
#ifdef __cplusplus
}
#endif
