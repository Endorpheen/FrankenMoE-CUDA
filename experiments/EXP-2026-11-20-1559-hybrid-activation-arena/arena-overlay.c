#define _GNU_SOURCE

#include "ggml-alloc.h"
#include "ggml-backend.h"

#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MIB = 1024 * 1024, BASELINE_BYTES = 64 * MIB, WIDE_BYTES = 384 * MIB };

static bool (*reserve_native)(ggml_gallocr_t, struct ggml_cgraph *, const int *, const int *);
static ggml_backend_buffer_t (*allocate_native)(ggml_backend_buffer_type_t, size_t);
static const char * (*buffer_type_name)(ggml_backend_buffer_type_t);
static size_t (*buffer_size)(ggml_backend_buffer_t);
static bool widen;
static pthread_once_t initialize = PTHREAD_ONCE_INIT;
static _Thread_local unsigned reserve_depth;
static _Thread_local unsigned cuda_allocations;

static void resolve(void) {
    reserve_native = dlsym(RTLD_NEXT, "ggml_gallocr_reserve_n");
    allocate_native = dlsym(RTLD_NEXT, "ggml_backend_buft_alloc_buffer");
    buffer_type_name = dlsym(RTLD_NEXT, "ggml_backend_buft_name");
    buffer_size = dlsym(RTLD_NEXT, "ggml_backend_buffer_get_size");
    if (!reserve_native || !allocate_native || !buffer_type_name || !buffer_size) {
        fputs("Hybrid arena experiment: missing allocation ABI\n", stderr);
        abort();
    }
    const char * option = getenv("FG_EXP1476_WIDE_ARENA");
    widen = option && strcmp(option, "1") == 0;
}

bool ggml_gallocr_reserve_n(ggml_gallocr_t allocator, struct ggml_cgraph * graph,
                           const int * node_buffers, const int * leaf_buffers) {
    pthread_once(&initialize, resolve);
    if (reserve_depth == 0) cuda_allocations = 0;
    ++reserve_depth;
    const bool result = reserve_native(allocator, graph, node_buffers, leaf_buffers);
    --reserve_depth;
    return result;
}

ggml_backend_buffer_t ggml_backend_buft_alloc_buffer(ggml_backend_buffer_type_t type, size_t bytes) {
    pthread_once(&initialize, resolve);
    if (reserve_depth == 0) return allocate_native(type, bytes);
    const char * name = buffer_type_name(type);
    if (!name || strcmp(name, "CUDA0") != 0) return allocate_native(type, bytes);

    // Extend only a CUDA compute-buffer request that crosses the old arena limit.
    ++cuda_allocations;
    size_t target = bytes;
    if (widen && cuda_allocations == 1 && bytes > BASELINE_BYTES && bytes < WIDE_BYTES) {
        target = WIDE_BYTES;
    }
    ggml_backend_buffer_t buffer = allocate_native(type, target);
    if (!buffer && target != bytes) {
        target = bytes;
        buffer = allocate_native(type, bytes);
    }
    fprintf(stderr, "HYBRID_ARENA requested=%zu target=%zu allocated=%zu\n",
            bytes, target, buffer ? buffer_size(buffer) : 0);
    return buffer;
}
