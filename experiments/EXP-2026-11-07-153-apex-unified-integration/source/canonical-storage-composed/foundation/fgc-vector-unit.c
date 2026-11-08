/* worker-8 APX030: strong exports for generated/fgc-vector.c. */

#include "canonical-storage-primitives.h"
#include "canonical-storage.h"

#include <stdint.h>

#define main fgc_vector_unused_main
#include "generated/fgc-vector.c"
#undef main

int64_t fgc_vec_entry(int64_t op, int64_t handle, int64_t a, int64_t b) {
    return gg_function_1(op, handle, a, b, 0);
}

fgc_handle fgc_vec_new(int64_t item_bytes) { return fgc_vec_entry(0, item_bytes, 0, 0); }
int64_t fgc_vec_len(fgc_handle h) { return fgc_vec_entry(1, h, 0, 0); }
void * fgc_vec_data(fgc_handle h) { return (void *)(intptr_t) fgc_vec_entry(2, h, 0, 0); }
int64_t fgc_vec_item_bytes(fgc_handle h) { return fgc_vec_entry(11, h, 0, 0); }
int64_t fgc_vec_reserve(fgc_handle h, int64_t count) { return fgc_vec_entry(3, h, count, 0); }
int64_t fgc_vec_resize(fgc_handle h, int64_t count) { return fgc_vec_entry(4, h, count, 0); }
int64_t fgc_vec_clear(fgc_handle h) { return fgc_vec_entry(6, h, 0, 0); }
void fgc_vec_free(fgc_handle h) { (void) fgc_vec_entry(5, h, 0, 0); }

fgc_handle fgc_bytes_new(void) { return fgc_vec_entry(0, 1, 0, 0); }
int64_t fgc_bytes_len(fgc_handle h) { return fgc_vec_entry(1, h, 0, 0); }
void * fgc_bytes_data(fgc_handle h) { return (void *)(intptr_t) fgc_vec_entry(2, h, 0, 0); }
int64_t fgc_bytes_reserve(fgc_handle h, int64_t count) { return fgc_vec_entry(3, h, count, 0); }
int64_t fgc_bytes_resize(fgc_handle h, int64_t count) { return fgc_vec_entry(4, h, count, 0); }
int64_t fgc_bytes_append(fgc_handle h, const void * src, int64_t n) {
    return fgc_vec_entry(7, h, (int64_t)(intptr_t) src, n);
}
void fgc_bytes_free(fgc_handle h) { (void) fgc_vec_entry(5, h, 0, 0); }

int64_t fgc_str_set(fgc_handle h, const char * src, int64_t n) {
    return fgc_vec_entry(8, h, (int64_t)(intptr_t) src, n);
}
const char * fgc_str_data(fgc_handle h) {
    return (const char *)(intptr_t) fgc_vec_entry(9, h, 0, 0);
}
int64_t fgc_str_len(fgc_handle h) { return fgc_vec_entry(10, h, 0, 0); }
void fgc_str_free(fgc_handle h) { (void) fgc_vec_entry(5, h, 0, 0); }
