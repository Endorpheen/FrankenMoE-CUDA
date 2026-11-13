#define _GNU_SOURCE
#include <dlfcn.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

typedef void * ggml_backend_t_;
typedef void * ggml_backend_buffer_type_t_;
typedef void * ggml_backend_sched_t_;

static ggml_backend_sched_t_ (*real_new)(ggml_backend_t_ *, ggml_backend_buffer_type_t_ *, int, size_t, bool, bool);

ggml_backend_sched_t_ ggml_backend_sched_new(ggml_backend_t_ * backends, ggml_backend_buffer_type_t_ * bufts,
                                             int n_backends, size_t graph_size, bool parallel, bool op_offload) {
    if (!real_new) real_new = dlsym(RTLD_NEXT, "ggml_backend_sched_new");
    static int once = 0;
    if (!once) { once = 1; fprintf(stderr, "FG_PRELOAD op_offload forced false (asked=%d)\n", (int) op_offload); }
    return real_new(backends, bufts, n_backends, graph_size, parallel, false);
}
