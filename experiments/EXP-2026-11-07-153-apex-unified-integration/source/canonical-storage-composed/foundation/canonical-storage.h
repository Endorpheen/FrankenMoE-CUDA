#pragma once
/* APX030 canonical shared storage (worker-8).
 *
 * Dynamic POD vector, record fields, i64 -> i64 map, ordered i64 set and
 * byte/string storage.  Implementation is Goose (see the fgc-*.goose modules);
 * the only native effects are raw malloc/free/memcpy/memzero/pointer-add,
 * indexed i64/u8 access and one CAS free guard.  No C++ std container and no
 * hidden growth/probing/balancing policy lives behind these symbols.
 *
 * Handle rules:
 *   - every handle is an opaque int64_t owned by exactly one caller;
 *   - a handle is invalid after its free function returns;
 *   - containers never own stored i64 values or other handles: a schema owner
 *     implements retain/transfer/destruction (no global GC);
 *   - 0 is the null handle and is accepted by the void free functions.
 *
 * Status:
 *   FGC_OK (0) success; negative values are errors.  get-style calls return
 *   0/1 or the stored value as documented.  A record get on an absent key
 *   returns 0; operations on a handle of a different container kind are
 *   rejected (status functions return a negative code, get-style return 0).
 *
 * Signed keys: records, maps and sets accept every int64_t key, including 0,
 * -1, INT64_MIN and INT64_MAX.  Map/vector tombstones and empty states are
 * explicit, never inferred from a key value.
 *
 * Each module publishes its own schema; this header publishes only the
 * storage ABI and the generated entry functions for mechanical binding. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int64_t fgc_handle;

#define FGC_OK           INT64_C(0)
#define FGC_ERR_INVAL    INT64_C(-1)
#define FGC_ERR_NOMEM    INT64_C(-2)
#define FGC_ERR_OVERFLOW INT64_C(-3)
#define FGC_ERR_KIND     INT64_C(-4)

/* ---- strong generated entries (mechanically bound to gg_function_1) ------- */
int64_t fgc_record_entry(int64_t op, int64_t handle, int64_t a, int64_t b);
int64_t fgc_vec_entry   (int64_t op, int64_t handle, int64_t a, int64_t b);
int64_t fgc_map_entry   (int64_t op, int64_t handle, int64_t a, int64_t b);
int64_t fgc_set_entry   (int64_t op, int64_t handle, int64_t a, int64_t b);

/* ---- record -------------------------------------------------------------- */
fgc_handle fgc_record_new(int64_t kind);
int64_t    fgc_record_get(fgc_handle handle, int64_t key);          /* value or 0 */
int64_t    fgc_record_set(fgc_handle handle, int64_t key, int64_t value);
int64_t    fgc_record_kind(fgc_handle handle);                      /* kind or -1 */
int64_t    fgc_record_count(fgc_handle handle);
int64_t    fgc_record_clear(fgc_handle handle);
void       fgc_record_free(fgc_handle handle);

/* ---- dynamic POD vector -------------------------------------------------- */
fgc_handle fgc_vec_new(int64_t item_bytes);
int64_t    fgc_vec_len(fgc_handle handle);
void *     fgc_vec_data(fgc_handle handle);
int64_t    fgc_vec_item_bytes(fgc_handle handle);
int64_t    fgc_vec_reserve(fgc_handle handle, int64_t count);
int64_t    fgc_vec_resize(fgc_handle handle, int64_t count);
int64_t    fgc_vec_clear(fgc_handle handle);
void       fgc_vec_free(fgc_handle handle);

/* ---- byte / string storage (vector with item_bytes == 1) ----------------- */
fgc_handle  fgc_bytes_new(void);
int64_t     fgc_bytes_len(fgc_handle handle);
void *      fgc_bytes_data(fgc_handle handle);
int64_t     fgc_bytes_reserve(fgc_handle handle, int64_t count);
int64_t     fgc_bytes_resize(fgc_handle handle, int64_t count);
int64_t     fgc_bytes_append(fgc_handle handle, const void * src, int64_t n);
void        fgc_bytes_free(fgc_handle handle);
int64_t     fgc_str_set(fgc_handle handle, const char * src, int64_t n); /* n + NUL */
const char *fgc_str_data(fgc_handle handle);
int64_t     fgc_str_len(fgc_handle handle);                              /* without NUL */
void        fgc_str_free(fgc_handle handle);

/* ---- i64 -> i64 map ------------------------------------------------------ */
fgc_handle fgc_map_new(void);
int64_t    fgc_map_put(fgc_handle handle, int64_t key, int64_t value);
int64_t    fgc_map_get(fgc_handle handle, int64_t key, int64_t * out_value); /* 1/0 */
int64_t    fgc_map_erase(fgc_handle handle, int64_t key);
int64_t    fgc_map_size(fgc_handle handle);
int64_t    fgc_map_clear(fgc_handle handle);
void       fgc_map_free(fgc_handle handle);

/* ---- ordered i64 set (balanced AVL) -------------------------------------- */
fgc_handle fgc_set_new(void);
int64_t    fgc_set_insert(fgc_handle handle, int64_t key);
int64_t    fgc_set_erase(fgc_handle handle, int64_t key);
int64_t    fgc_set_contains(fgc_handle handle, int64_t key);                 /* 1/0 */
int64_t    fgc_set_lower_bound(fgc_handle handle, int64_t key, int64_t * out_key); /* 1/0 */
int64_t    fgc_set_first(fgc_handle handle, int64_t * out_key);              /* 1/0 */
int64_t    fgc_set_next(fgc_handle handle, int64_t key, int64_t * out_key);  /* 1/0, > key */
int64_t    fgc_set_size(fgc_handle handle);
int64_t    fgc_set_height(fgc_handle handle); /* instrumentation: AVL root height */
int64_t    fgc_set_clear(fgc_handle handle);
void       fgc_set_free(fgc_handle handle);

#ifdef __cplusplus
}
#endif
