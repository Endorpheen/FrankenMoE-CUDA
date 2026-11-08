#include "bridge.h"

#define main fg153_generated_main
#include "batch-allocr.c"
#undef main

// Emitter id 1 is the entry: it is the only generated function with the
// seven-argument mode+a..f signature that calls fg153_ba_witness (see
// batch-allocr.c:8181 in the emitted file).
int64_t fg153_goose_entry(int64_t mode, int64_t a, int64_t b, int64_t c,
                          int64_t d, int64_t e, int64_t f) {
    return gg_function_1(mode, a, b, c, d, e, f, 0);
}
