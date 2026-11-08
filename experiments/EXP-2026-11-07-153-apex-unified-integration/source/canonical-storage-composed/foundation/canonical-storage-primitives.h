#pragma once
/* worker-8 APX030 canonical storage: raw effect primitives.
 *
 * These are the only operations the Goose modules may use.  They contain no
 * container, growth, probing, balancing or ownership policy: allocation and
 * release are raw libc, copies are raw memcpy/memzero, field access is an
 * index into caller-owned native memory, and CAS is a single atomic
 * compare-exchange.  Every policy decision lives in the .goose modules.
 *
 * The fgc_test_* hooks are fixture-only and are not part of the canonical
 * cross-module ABI. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Raw effects.  All sizes/offsets are bytes and must be non-negative. */
int64_t  fgc_alloc(int64_t bytes);                       /* malloc, 0 on failure */
void     fgc_free_ptr(int64_t ptr, int64_t bytes);       /* free, ptr==0 is a no-op */
int64_t  fgc_memcpy(int64_t dst, int64_t src, int64_t bytes);
int64_t  fgc_memzero(int64_t dst, int64_t bytes);
int64_t  fgc_add_ptr(int64_t ptr, int64_t bytes);        /* ptr + bytes, no deref */

/* Raw typed field access into caller-owned native memory, index in elements. */
int64_t  fgc_load_i64(int64_t base, int64_t index);
void     fgc_store_i64(int64_t base, int64_t index, int64_t value);
int64_t  fgc_load_u8(int64_t base, int64_t index);
void     fgc_store_u8(int64_t base, int64_t index, int64_t value);

/* Raw atomic compare-exchange on an i64 slot; returns the previous value. */
int64_t  fgc_cas_i64(int64_t base, int64_t index, int64_t expected, int64_t desired);

void     fgc_abort(int64_t code) __attribute__((noreturn));
void     fgc_witness(int64_t mode);

/* Fixture-only allocation-failure and live-counter instrumentation. */
void     fgc_test_alloc_reset(void);
void     fgc_test_fail_alloc_at(int64_t call_index); /* 0 disables injection */
int64_t  fgc_live_allocs(void);
int64_t  fgc_live_bytes(void);

#ifdef __cplusplus
}
#endif
