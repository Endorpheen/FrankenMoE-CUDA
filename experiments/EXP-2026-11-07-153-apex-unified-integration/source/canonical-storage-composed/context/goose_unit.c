#include "canonical-storage-imports.h"

#define main fgc_generated_main
#include "canonical-context.c"
#undef main

/* Strong binding to the observed generated entry (gg_function_1: the only
 * seven-argument mode+a..f function in the emitted C). */
int64_t fgc_goose_entry(int64_t mode, int64_t a, int64_t b, int64_t c,
                        int64_t d, int64_t e, int64_t f) {
    return gg_function_1(mode, a, b, c, d, e, f, 0);
}
