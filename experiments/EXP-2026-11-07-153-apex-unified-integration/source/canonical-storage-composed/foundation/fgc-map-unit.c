/* worker-8 APX030: strong exports for generated/fgc-map.c. */

#include "canonical-storage-primitives.h"
#include "canonical-storage.h"

#include <stdint.h>

#define main fgc_map_unused_main
#include "generated/fgc-map.c"
#undef main

int64_t fgc_map_entry(int64_t op, int64_t handle, int64_t a, int64_t b) {
    return gg_function_1(op, handle, a, b, 0);
}

fgc_handle fgc_map_new(void) { return fgc_map_entry(0, 0, 0, 0); }
int64_t fgc_map_put(fgc_handle h, int64_t key, int64_t value) {
    return fgc_map_entry(1, h, key, value);
}
int64_t fgc_map_get(fgc_handle h, int64_t key, int64_t * out_value) {
    return fgc_map_entry(2, h, key, (int64_t)(intptr_t) out_value);
}
int64_t fgc_map_erase(fgc_handle h, int64_t key) { return fgc_map_entry(3, h, key, 0); }
int64_t fgc_map_size(fgc_handle h) { return fgc_map_entry(5, h, 0, 0); }
int64_t fgc_map_clear(fgc_handle h) { return fgc_map_entry(6, h, 0, 0); }
void fgc_map_free(fgc_handle h) { (void) fgc_map_entry(4, h, 0, 0); }
