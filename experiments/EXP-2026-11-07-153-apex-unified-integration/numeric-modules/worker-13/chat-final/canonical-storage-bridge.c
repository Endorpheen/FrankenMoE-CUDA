#include "canonical-storage-bindings.h"

#include <stdint.h>

int64_t cf_fgc_record_new(int64_t kind) { return fgc_record_new(kind); }
int64_t cf_fgc_record_get(int64_t h, int64_t k) { return fgc_record_get(h, k); }
int64_t cf_fgc_record_set(int64_t h, int64_t k, int64_t v) { return fgc_record_set(h, k, v); }
int64_t cf_fgc_record_free(int64_t h) { fgc_record_free(h); return 0; }

int64_t cf_fgc_vec_new(int64_t item_bytes) { return fgc_vec_new(item_bytes); }
int64_t cf_fgc_vec_len(int64_t h) { return fgc_vec_len(h); }
int64_t cf_fgc_vec_data(int64_t h) { return (int64_t) (intptr_t) fgc_vec_data(h); }
int64_t cf_fgc_vec_resize(int64_t h, int64_t n) { return fgc_vec_resize(h, n); }
int64_t cf_fgc_vec_free(int64_t h) { fgc_vec_free(h); return 0; }

int64_t cf_fgc_map_new(void) { return fgc_map_new(); }
int64_t cf_fgc_map_put(int64_t h, int64_t k, int64_t v) { return fgc_map_put(h, k, v); }
int64_t cf_fgc_map_get(int64_t h, int64_t k, int64_t out) {
    return fgc_map_get(h, k, (int64_t *) (intptr_t) out);
}
int64_t cf_fgc_map_erase(int64_t h, int64_t k) { return fgc_map_erase(h, k); }
int64_t cf_fgc_map_free(int64_t h) { fgc_map_free(h); return 0; }

int64_t cf_fgc_set_new(void) { return fgc_set_new(); }
int64_t cf_fgc_set_insert(int64_t h, int64_t k) { return fgc_set_insert(h, k); }
int64_t cf_fgc_set_erase(int64_t h, int64_t k) { return fgc_set_erase(h, k); }
int64_t cf_fgc_set_lower_bound(int64_t h, int64_t k, int64_t out) {
    return fgc_set_lower_bound(h, k, (int64_t *) (intptr_t) out);
}
int64_t cf_fgc_set_first(int64_t h, int64_t out) {
    return fgc_set_first(h, (int64_t *) (intptr_t) out);
}
int64_t cf_fgc_set_next(int64_t h, int64_t k, int64_t out) {
    return fgc_set_next(h, k, (int64_t *) (intptr_t) out);
}
int64_t cf_fgc_set_free(int64_t h) { fgc_set_free(h); return 0; }

int64_t cf_fgc_bytes_new(void) { return fgc_bytes_new(); }
int64_t cf_fgc_bytes_append(int64_t h, int64_t src, int64_t n) {
    return fgc_bytes_append(h, (const void *) (intptr_t) src, n);
}
int64_t cf_fgc_bytes_free(int64_t h) { fgc_bytes_free(h); return 0; }
int64_t cf_fgc_str_set(int64_t h, int64_t src, int64_t n) {
    return fgc_str_set(h, (const char *) (intptr_t) src, n);
}
int64_t cf_fgc_str_data(int64_t h) { return (int64_t) (intptr_t) fgc_str_data(h); }
int64_t cf_fgc_str_len(int64_t h) { return fgc_str_len(h); }
int64_t cf_fgc_str_free(int64_t h) { fgc_str_free(h); return 0; }
