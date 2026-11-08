#pragma once
/* worker-8: POD mirror of the two allocator records needed by the remaining
 * reserve_n_size closure.  Source of truth:
 *   inputs/active-ggml-alloc.c  (struct ggml_gallocr, struct ggml_dyn_tallocr)
 *
 * Only the prefix of struct ggml_gallocr up to buffer_growth_limits is named.
 * The following fields (hash_set, hash_values, node_allocs, ...) are opaque to
 * this closure and therefore deliberately absent.  All offsets are checked by
 * the static assertions below; the executable differential in
 * allocator-closure-fixture.c additionally proves the mirror against the
 * qualified runtime library on real CPU graphs.
 *
 * No policy, no loops, no allocation: this header is pure layout. */

#include <stddef.h>
#include <stdint.h>

#define W8_VBUFFER_MAX_CHUNKS 16

struct w8_dyn_tallocr_layout {
    size_t alignment;
    size_t max_chunk_size;
    void * chunks[W8_VBUFFER_MAX_CHUNKS];
    int n_chunks;
};

struct w8_gallocr_layout {
    void * bufts;                       /* ggml_backend_buffer_type_t * */
    void ** buffers;                    /* struct vbuffer **            */
    struct w8_dyn_tallocr_layout ** buf_tallocs;
    int n_buffers;
    size_t * buffer_growth_limits;
};

_Static_assert(sizeof(void *) == 8, "worker-8 allocator closure requires 64-bit ABI");
_Static_assert(sizeof(size_t) == 8, "worker-8 allocator closure requires 64-bit size_t");

_Static_assert(offsetof(struct w8_dyn_tallocr_layout, alignment) == 0,
               "dyn tallocr alignment offset");
_Static_assert(offsetof(struct w8_dyn_tallocr_layout, max_chunk_size) == 8,
               "dyn tallocr max_chunk_size offset");
_Static_assert(offsetof(struct w8_dyn_tallocr_layout, chunks) == 16,
               "dyn tallocr chunks offset");
_Static_assert(offsetof(struct w8_dyn_tallocr_layout, n_chunks) == 16 + W8_VBUFFER_MAX_CHUNKS * 8,
               "dyn tallocr n_chunks offset");

_Static_assert(offsetof(struct w8_gallocr_layout, bufts) == 0,
               "galloc bufts offset");
_Static_assert(offsetof(struct w8_gallocr_layout, buffers) == 8,
               "galloc buffers offset");
_Static_assert(offsetof(struct w8_gallocr_layout, buf_tallocs) == 16,
               "galloc buf_tallocs offset");
_Static_assert(offsetof(struct w8_gallocr_layout, n_buffers) == 24,
               "galloc n_buffers offset");
_Static_assert(offsetof(struct w8_gallocr_layout, buffer_growth_limits) == 32,
               "galloc buffer_growth_limits offset");
