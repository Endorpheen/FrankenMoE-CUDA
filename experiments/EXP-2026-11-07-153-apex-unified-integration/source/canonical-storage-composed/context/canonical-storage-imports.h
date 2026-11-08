/* Exact cross-module imports for the canonical storage foundation.
 * Source of truth: inputs/canonical-storage-contract.md lines 8-11 (worker8).
 * These are declarations only; production binds worker8's real exports.
 * Local checks link a real reference implementation in local-fgc-runtime.c,
 * which is NOT part of the canonical production path.
 */
#ifndef FGC_STORAGE_IMPORTS_H
#define FGC_STORAGE_IMPORTS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

int64_t fgc_record_new(int64_t kind);
int64_t fgc_record_get(int64_t rec, int64_t key);
int64_t fgc_record_set(int64_t rec, int64_t key, int64_t value);
int64_t fgc_record_kind(int64_t rec);
int64_t fgc_i64_record_free(int64_t rec);

int64_t fgc_vec_new(int64_t item_bytes);
int64_t fgc_vec_len(int64_t vec);
int64_t fgc_i64_vec_data(int64_t vec);
int64_t fgc_vec_reserve(int64_t vec, int64_t count);
int64_t fgc_vec_resize(int64_t vec, int64_t count);
int64_t fgc_i64_vec_free(int64_t vec);

int64_t fgc_map_new(void);
int64_t fgc_map_put(int64_t map, int64_t key, int64_t value);
int64_t fgc_i64_map_get(int64_t map, int64_t key, int64_t out_ptr);
int64_t fgc_map_erase(int64_t map, int64_t key);
int64_t fgc_i64_map_free(int64_t map);

/* Raw memory effects used by canonical modules. These are external primitives
 * by contract, not owned containers: allocation/copy/pointer arithmetic only. */
int64_t fgc_raw_calloc(int64_t nbytes);
void    fgc_raw_free(int64_t ptr);
void    fgc_raw_memcpy(int64_t dst, int64_t src, int64_t nbytes);
void    fgc_raw_memset(int64_t dst, int64_t value, int64_t nbytes);
int64_t fgc_raw_ptr_add(int64_t base, int64_t byte_off);
int64_t fgc_raw_load_i64(int64_t addr);
int64_t fgc_raw_load_i32(int64_t addr);
int64_t fgc_raw_load_i8(int64_t addr);
void    fgc_raw_store_i64(int64_t addr, int64_t value);
void    fgc_raw_store_i32(int64_t addr, int64_t value);
void    fgc_raw_store_i8(int64_t addr, int64_t value);
void    fgc_raw_store_f32(int64_t addr, int64_t float_bits);

#ifdef __cplusplus
}
#endif
#endif
