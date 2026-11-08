#pragma once
/* worker-8 typed contract for the remaining allocator ownership closure.
 *
 * Generated Goose (allocator-closure.goose) imports the fg153_ac_* primitives
 * below.  They are O(1) typed reads/writes only: no allocation, no loops, no
 * lifetime policy.  reserve_n_size itself lives in Goose; the only helper that
 * is reused from the already-qualified parent is fg153_memory(mode 4), the
 * dyn-tallocr chunk max_size accessor.
 *
 * fg153_alloc_closure is the generated entry wrapper (see
 * allocator-closure-unit.c): mode 0 computes per-buffer planned sizes,
 * modes 1..8 expose primitives/control helpers for differential testing. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int fg153_alloc_closure_enabled(void);
int64_t fg153_alloc_closure(int64_t mode, int64_t a, int64_t b, int64_t c);

/* Typed primitives implemented in allocator-closure-services.c. */
int64_t fg153_ac_nbuffers(int64_t galloc);
int64_t fg153_ac_talloc(int64_t galloc, int64_t index);
int64_t fg153_ac_nchunks(int64_t talloc);
int64_t fg153_ac_sizeget(int64_t sizes, int64_t index);
void    fg153_ac_sizeset(int64_t sizes, int64_t index, int64_t value);
void    fg153_ac_witness(int64_t mode);

/* Reused parent primitive (already qualified memory kernel). */
int64_t fg153_memory(int64_t mode, int64_t a, int64_t b, int64_t c, int64_t d);

#ifdef __cplusplus
}
#endif
