/* worker-8 APX030: strong exports for generated/fgc-record.c. */

#include "canonical-storage-primitives.h"
#include "canonical-storage.h"

#include <stdint.h>

#define main fgc_record_unused_main
#include "generated/fgc-record.c"
#undef main

int64_t fgc_record_entry(int64_t op, int64_t handle, int64_t a, int64_t b) {
    return gg_function_1(op, handle, a, b, 0);
}

fgc_handle fgc_record_new(int64_t kind) { return fgc_record_entry(0, kind, 0, 0); }
int64_t fgc_record_get(fgc_handle h, int64_t key) { return fgc_record_entry(1, h, key, 0); }
int64_t fgc_record_set(fgc_handle h, int64_t key, int64_t value) {
    return fgc_record_entry(2, h, key, value);
}
int64_t fgc_record_kind(fgc_handle h) { return fgc_record_entry(3, h, 0, 0); }
void fgc_record_free(fgc_handle h) { (void) fgc_record_entry(4, h, 0, 0); }
int64_t fgc_record_clear(fgc_handle h) { return fgc_record_entry(5, h, 0, 0); }
int64_t fgc_record_count(fgc_handle h) { return fgc_record_entry(6, h, 0, 0); }
