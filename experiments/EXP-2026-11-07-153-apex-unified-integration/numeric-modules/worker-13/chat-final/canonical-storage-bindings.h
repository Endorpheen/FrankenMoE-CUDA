#pragma once
/* Exact worker-8 canonical storage ABI plus the distinctly named typed bridge
 * used by the APX028 chat Goose i64 ABI.
 *
 * The bridge names never alias a canonical symbol, so the real canonical
 * signatures (void frees, void * vector data, int64_t * map out pointer) stay
 * intact.  The chat module only needs records, byte/string vectors and i64
 * vectors; maps/sets are provided by the same ready foundation and linked for
 * record/message schema checks.
 */
#include "canonical-storage.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int64_t cf_fgc_record_new(int64_t kind);
int64_t cf_fgc_record_get(int64_t handle, int64_t key);
int64_t cf_fgc_record_set(int64_t handle, int64_t key, int64_t value);
int64_t cf_fgc_record_free(int64_t handle);

int64_t cf_fgc_vec_new(int64_t item_bytes);
int64_t cf_fgc_vec_len(int64_t handle);
int64_t cf_fgc_vec_data(int64_t handle);
int64_t cf_fgc_vec_resize(int64_t handle, int64_t count);
int64_t cf_fgc_vec_free(int64_t handle);

int64_t cf_fgc_map_new(void);
int64_t cf_fgc_map_put(int64_t handle, int64_t key, int64_t value);
int64_t cf_fgc_map_get(int64_t handle, int64_t key, int64_t out_ptr);
int64_t cf_fgc_map_erase(int64_t handle, int64_t key);
int64_t cf_fgc_map_free(int64_t handle);

int64_t cf_fgc_set_new(void);
int64_t cf_fgc_set_insert(int64_t handle, int64_t key);
int64_t cf_fgc_set_erase(int64_t handle, int64_t key);
int64_t cf_fgc_set_lower_bound(int64_t handle, int64_t key, int64_t out_ptr);
int64_t cf_fgc_set_first(int64_t handle, int64_t out_ptr);
int64_t cf_fgc_set_next(int64_t handle, int64_t key, int64_t out_ptr);
int64_t cf_fgc_set_free(int64_t handle);

int64_t cf_fgc_bytes_new(void);
int64_t cf_fgc_bytes_append(int64_t handle, int64_t src, int64_t n);
int64_t cf_fgc_bytes_free(int64_t handle);
int64_t cf_fgc_str_set(int64_t handle, int64_t src, int64_t n);
int64_t cf_fgc_str_data(int64_t handle);
int64_t cf_fgc_str_len(int64_t handle);
int64_t cf_fgc_str_free(int64_t handle);

#ifdef __cplusplus
}
#endif
