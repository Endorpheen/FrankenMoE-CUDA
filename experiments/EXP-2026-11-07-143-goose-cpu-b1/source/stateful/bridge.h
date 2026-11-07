#pragma once
#include "../bridge.h"
struct fg143_state_plan {struct fg143_tensor source[7],dst;int64_t ith,nth,op,snapshots;float fp[4];};
#ifdef __cplusplus
extern "C" {
#endif
uint8_t fg143_state_compute(const struct fg143_state_plan*,const void*const*,void*);
#ifdef __cplusplus
}
#endif
