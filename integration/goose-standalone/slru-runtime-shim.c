// Host shim for the Goose runtime symbols the emitted SLRU policy references.
//
// The generated C declares gs_panic / gs_idxfail / gs_divfail / gs_rt_start but does not define them:
// they belong to the runtime, which the program links separately. The MoE cache lives inside
// libllama, where no Goose runtime exists, so the four are defined here as the same hard stops the
// native code would take. This is the pattern the existing EXP-263 bridge already uses (its g_fail
// is a trap), not a new one.
//
// gs_rt_start is a no-op because the module uses no data stack: the emitted code allocates nothing
// (checked - no gg_get_stack, gg_allocate, gg_append or gg_ref appears in the generated C). The
// policy touches only the caller's arrays.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define GS_RUNTIME_VERSION 1e714e6146bc
#define GS_API
#define GS_NORETURN __attribute__((noreturn))
#define GS_CAT_(a, b) a##b
#define GS_CAT(a, b) GS_CAT_(a, b)

GS_NORETURN void gs_panic(const char *msg) {
    fprintf(stderr, "goose runtime: %s\n", msg);
    abort();
}

GS_NORETURN void gs_idxfail(int64_t i, int64_t n, const char *file, int line) {
    fprintf(stderr, "goose runtime: index %lld out of bounds (length %lld) (%s:%d)\n",
            (long long) i, (long long) n, file, line);
    abort();
}

GS_NORETURN void gs_divfail(const char *file, int line) {
    fprintf(stderr, "goose runtime: division by zero (%s:%d)\n", file, line);
    abort();
}

void GS_CAT(gs_rt_start_, GS_RUNTIME_VERSION)(int argc, char **argv) {
    (void) argc;
    (void) argv;
}
