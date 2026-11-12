#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int64_t fglog_run(uint64_t out, int64_t cap, uint64_t evout, int64_t level, int64_t mode,
                  int64_t last, uint64_t text, int64_t len, uint64_t ts, int64_t tslen);
#ifdef __cplusplus
}
#endif
