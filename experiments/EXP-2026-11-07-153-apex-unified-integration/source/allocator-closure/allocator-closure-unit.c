/* worker-8: wrapper TU for generated/allocator-closure.c.
 * The emitter makes entry() gg_function_1; the wrapper gives it the stable
 * C ABI used by the hook and by the differential fixture. */

#include "allocator-closure-bridge.h"

#include <stdint.h>

#define main fg153_alloc_closure_unused_main
#include "generated/allocator-closure.c"
#undef main

int64_t fg153_alloc_closure(int64_t mode, int64_t a, int64_t b, int64_t c) {
    return gg_function_1(mode, a, b, c, 0, 0, 0, 0);
}
