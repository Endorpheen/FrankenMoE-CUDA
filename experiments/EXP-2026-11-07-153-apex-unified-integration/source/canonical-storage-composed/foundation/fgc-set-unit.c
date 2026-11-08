/* worker-8 APX030: strong exports for generated/fgc-set.c. */

#include "canonical-storage-primitives.h"
#include "canonical-storage.h"

#include <stdint.h>

#define main fgc_set_unused_main
#include "generated/fgc-set.c"
#undef main

int64_t fgc_set_entry(int64_t op, int64_t handle, int64_t a, int64_t b) {
    return gg_function_1(op, handle, a, b, 0);
}

fgc_handle fgc_set_new(void) { return fgc_set_entry(0, 0, 0, 0); }
int64_t fgc_set_insert(fgc_handle h, int64_t key) { return fgc_set_entry(1, h, key, 0); }
int64_t fgc_set_erase(fgc_handle h, int64_t key) { return fgc_set_entry(2, h, key, 0); }
int64_t fgc_set_contains(fgc_handle h, int64_t key) { return fgc_set_entry(3, h, key, 0); }
int64_t fgc_set_lower_bound(fgc_handle h, int64_t key, int64_t * out_key) {
    return fgc_set_entry(4, h, key, (int64_t)(intptr_t) out_key);
}
int64_t fgc_set_first(fgc_handle h, int64_t * out_key) {
    return fgc_set_entry(5, h, (int64_t)(intptr_t) out_key, 0);
}
int64_t fgc_set_next(fgc_handle h, int64_t key, int64_t * out_key) {
    return fgc_set_entry(6, h, key, (int64_t)(intptr_t) out_key);
}
int64_t fgc_set_size(fgc_handle h) { return fgc_set_entry(8, h, 0, 0); }
int64_t fgc_set_height(fgc_handle h) { return fgc_set_entry(9, h, 0, 0); }
int64_t fgc_set_clear(fgc_handle h) { return fgc_set_entry(10, h, 0, 0); }
void fgc_set_free(fgc_handle h) { (void) fgc_set_entry(7, h, 0, 0); }
