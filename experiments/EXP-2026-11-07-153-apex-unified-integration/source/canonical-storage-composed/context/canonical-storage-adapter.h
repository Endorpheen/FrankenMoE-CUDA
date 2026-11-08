/* Worker5 module seam over the real worker8 canonical storage.
 *
 * The public header always remains /inputs/canonical-storage-ready/canonical-storage.h
 * with its exact signatures (void frees, pointer-returning data, set out-params).
 * This adapter only converts pointer/void results to int64_t for Goose and
 * implements byte/word access through worker8's declared raw primitives. It owns
 * no container or storage policy. */
#ifndef FGCX_STORAGE_ADAPTER_H
#define FGCX_STORAGE_ADAPTER_H
#include <stdint.h>
#include "canonical-storage.h"
#include "canonical-storage-primitives.h"

#ifdef __cplusplus
extern "C" {
#endif

int64_t  fgcx_record_new(int64_t kind);
int64_t  fgcx_record_get(int64_t rec, int64_t key);
int64_t  fgcx_record_set(int64_t rec, int64_t key, int64_t value);
int64_t  fgcx_record_kind(int64_t rec);
void     fgcx_record_free(int64_t rec);

int64_t  fgcx_vec_new(int64_t item_bytes);
int64_t  fgcx_vec_len(int64_t vec);
int64_t  fgcx_vec_data(int64_t vec);
int64_t  fgcx_vec_reserve(int64_t vec, int64_t count);
int64_t  fgcx_vec_resize(int64_t vec, int64_t count);
void     fgcx_vec_free(int64_t vec);

int64_t  fgcx_load_i32(int64_t addr);
int64_t  fgcx_load_i8(int64_t addr);
void     fgcx_store_i32(int64_t addr, int64_t value);
void     fgcx_store_i8(int64_t addr, int64_t value);
void     fgcx_store_i64(int64_t addr, int64_t value);
void     fgcx_memcpy(int64_t dst, int64_t src, int64_t bytes);
void     fgcx_memzero(int64_t dst, int64_t bytes);
int64_t  fgcx_ptr_add(int64_t base, int64_t bytes);

#ifdef __cplusplus
}
#endif
#endif
