/* Callable-unit wrapper for the emitted Goose entry (gg_function_1).
 * It intentionally wraps nothing numerically: it only converts raw pointers
 * to Goose slices and forwards the mode word. */
#include "apex_matrix.h"
#define main fgm_generated_unused_main
#include "apex_matrix_generated.c"
#undef main

int64_t fgm_matrix_entry(int64_t mode, const void * w, int64_t wbytes,
                         const void * x, int64_t xbytes, float * y, int64_t ylen,
                         int64_t p0, int64_t p1, int64_t p2, int64_t p3) {
    return gg_function_1(mode, gg_view((void*)w, wbytes), gg_view((void*)x, xbytes),
                         gg_view(y, ylen), p0, p1, p2, p3, 0);
}
