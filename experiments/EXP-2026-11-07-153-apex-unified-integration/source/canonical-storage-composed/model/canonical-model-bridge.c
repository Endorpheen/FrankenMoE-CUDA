// worker-4/canonical-model: typed i64 accessors over worker8 canonical storage.
// No ownership or algorithm policy lives here.
#include "canonical-storage-required.h"

#include <string.h>

int64_t fgc_vec_get_i64(int64_t handle, int64_t index) {
    const int64_t * p = (const int64_t *) (intptr_t) fgc_i64_vec_data(handle);
    return p ? p[index] : 0;
}

int64_t fgc_vec_set_i64(int64_t handle, int64_t index, int64_t value) {
    int64_t * p = (int64_t *) (intptr_t) fgc_i64_vec_data(handle);
    if (!p) return -1;
    p[index] = value;
    return 0;
}

int64_t fgc_map_get_i64(int64_t handle, int64_t key) {
    int64_t out = 0;
    return fgc_i64_map_get(handle, key, (int64_t) (intptr_t) &out) ? out : 0;
}

int64_t fgc_map_has(int64_t handle, int64_t key) {
    int64_t out = 0;
    return fgc_i64_map_get(handle, key, (int64_t) (intptr_t) &out) != 0;
}

int64_t fgc_bits_f32(float value) {
    uint32_t u = 0;
    memcpy(&u, &value, sizeof u);
    return (int64_t) u;
}

float fgc_from_bits_i64(int64_t bits) {
    uint32_t u = (uint32_t) bits;
    float f = 0.0f;
    memcpy(&f, &u, sizeof f);
    return f;
}
