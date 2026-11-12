#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int64_t fg150_cache(int64_t,int64_t);
int64_t fg150_cache_get(int64_t,int64_t,int64_t);
void fg150_cache_put(int64_t,int64_t,int64_t,int64_t);
void fg150_cache_action(int64_t,int64_t,int64_t,int64_t);
#ifdef __cplusplus
}
#endif
