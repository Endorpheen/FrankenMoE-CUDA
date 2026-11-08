#pragma once
/* APX030 canonical shared storage (worker-8), exact published ABI. */
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

int64_t fgc_record_entry(int64_t op, int64_t handle, int64_t a, int64_t b);
int64_t fgc_vec_entry   (int64_t op, int64_t handle, int64_t a, int64_t b);
int64_t fgc_map_entry   (int64_t op, int64_t handle, int64_t a, int64_t b);
int64_t fgc_set_entry   (int64_t op, int64_t handle, int64_t a, int64_t b);

fgc_handle fgc_record_new(int64_t kind);
int64_t    fgc_record_get(fgc_handle handle, int64_t key);
int64_t    fgc_record_set(fgc_handle handle, int64_t key, int64_t value);
int64_t    fgc_record_kind(fgc_handle handle);
int64_t    fgc_record_count(fgc_handle handle);
int64_t    fgc_record_clear(fgc_handle handle);
void       fgc_record_free(fgc_handle handle);

fgc_handle fgc_vec_new(int64_t item_bytes);
int64_t    fgc_vec_len(fgc_handle handle);
void *     fgc_vec_data(fgc_handle handle);
int64_t    fgc_vec_item_bytes(fgc_handle handle);
int64_t    fgc_vec_reserve(fgc_handle handle, int64_t count);
int64_t    fgc_vec_resize(fgc_handle handle, int64_t count);
int64_t    fgc_vec_clear(fgc_handle handle);
void       fgc_vec_free(fgc_handle handle);

fgc_handle  fgc_bytes_new(void);
int64_t     fgc_bytes_len(fgc_handle handle);
void *      fgc_bytes_data(fgc_handle handle);
int64_t     fgc_bytes_reserve(fgc_handle handle, int64_t count);
int64_t     fgc_bytes_resize(fgc_handle handle, int64_t count);
int64_t     fgc_bytes_append(fgc_handle handle, const void * src, int64_t n);
void        fgc_bytes_free(fgc_handle handle);
int64_t     fgc_str_set(fgc_handle handle, const char * src, int64_t n);
const char *fgc_str_data(fgc_handle handle);
int64_t     fgc_str_len(fgc_handle handle);
void        fgc_str_free(fgc_handle handle);

fgc_handle fgc_map_new(void);
int64_t    fgc_map_put(fgc_handle handle, int64_t key, int64_t value);
int64_t    fgc_map_get(fgc_handle handle, int64_t key, int64_t * out_value);
int64_t    fgc_map_erase(fgc_handle handle, int64_t key);
int64_t    fgc_map_size(fgc_handle handle);
int64_t    fgc_map_clear(fgc_handle handle);
void       fgc_map_free(fgc_handle handle);

fgc_handle fgc_set_new(void);
int64_t    fgc_set_insert(fgc_handle handle, int64_t key);
int64_t    fgc_set_erase(fgc_handle handle, int64_t key);
int64_t    fgc_set_contains(fgc_handle handle, int64_t key);
int64_t    fgc_set_lower_bound(fgc_handle handle, int64_t key, int64_t * out_key);
int64_t    fgc_set_first(fgc_handle handle, int64_t * out_key);
int64_t    fgc_set_next(fgc_handle handle, int64_t key, int64_t * out_key);
int64_t    fgc_set_size(fgc_handle handle);
int64_t    fgc_set_height(fgc_handle handle);
int64_t    fgc_set_clear(fgc_handle handle);
void       fgc_set_free(fgc_handle handle);

#ifdef __cplusplus
}
#endif
