// worker-2/kv-active-closure/q4a-memory-owner.h
// Exact typed contract for canonical memory owner 12. The owner may bind a
// range-prepare primitive; when unbound the active backend registry hook is
// used, matching EXP101 lazy VMM behaviour. No owner layout is assumed.
#pragma once
#include <cstddef>
#include <cstdint>

typedef void (*q4a_range_prepare_fn)(void * buffer, int64_t offset, int64_t size);

#ifdef __cplusplus
extern "C" {
#endif
void fg153_q4a_bind_range_prepare(q4a_range_prepare_fn fn);
int  q4a_range_prepare_bound(void);
void q4a_range_prepare_call(void * buffer, size_t offset, size_t size);
#ifdef __cplusplus
}
#endif
