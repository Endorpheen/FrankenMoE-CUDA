/* Adapter implementation over worker8's real strong exports. */
#include "canonical-storage-adapter.h"

int64_t fgcx_record_new(int64_t kind) { return fgc_record_new(kind); }
int64_t fgcx_record_get(int64_t rec, int64_t key) { return fgc_record_get(rec, key); }
int64_t fgcx_record_set(int64_t rec, int64_t key, int64_t value) { return fgc_record_set(rec, key, value); }
int64_t fgcx_record_kind(int64_t rec) { return fgc_record_kind(rec); }
void    fgcx_record_free(int64_t rec) { fgc_i64_record_free(rec); }

int64_t fgcx_vec_new(int64_t item_bytes) { return fgc_vec_new(item_bytes); }
int64_t fgcx_vec_len(int64_t vec) { return fgc_vec_len(vec); }
int64_t fgcx_vec_data(int64_t vec) { return (int64_t) (intptr_t) fgc_i64_vec_data(vec); }
int64_t fgcx_vec_reserve(int64_t vec, int64_t count) { return fgc_vec_reserve(vec, count); }
int64_t fgcx_vec_resize(int64_t vec, int64_t count) { return fgc_vec_resize(vec, count); }
void    fgcx_vec_free(int64_t vec) { fgc_i64_vec_free(vec); }

int64_t fgcx_load_i32(int64_t addr) {
    if (!addr) return 0;
    uint32_t v = 0;
    v |= (uint32_t) (fgc_load_u8(addr, 0) & 0xff);
    v |= (uint32_t) (fgc_load_u8(addr, 1) & 0xff) << 8;
    v |= (uint32_t) (fgc_load_u8(addr, 2) & 0xff) << 16;
    v |= (uint32_t) (fgc_load_u8(addr, 3) & 0xff) << 24;
    return (int64_t) (int32_t) v;
}
int64_t fgcx_load_i8(int64_t addr) { return addr ? (int64_t) (int8_t) (fgc_load_u8(addr, 0) & 0xff) : 0; }
void fgcx_store_i32(int64_t addr, int64_t value) {
    if (!addr) return;
    uint32_t v = (uint32_t) value;
    fgc_store_u8(addr, 0, (int64_t) (v & 0xff));
    fgc_store_u8(addr, 1, (int64_t) ((v >> 8) & 0xff));
    fgc_store_u8(addr, 2, (int64_t) ((v >> 16) & 0xff));
    fgc_store_u8(addr, 3, (int64_t) ((v >> 24) & 0xff));
}
void fgcx_store_i8(int64_t addr, int64_t value) { if (addr) fgc_store_u8(addr, 0, value & 0xff); }
void fgcx_store_i64(int64_t addr, int64_t value) { if (addr) fgc_store_i64(addr, 0, value); }
void fgcx_memcpy(int64_t dst, int64_t src, int64_t bytes) { if (bytes > 0) (void) fgc_memcpy(dst, src, bytes); }
void fgcx_memzero(int64_t dst, int64_t bytes) { if (bytes > 0) (void) fgc_memzero(dst, bytes); }
int64_t fgcx_ptr_add(int64_t base, int64_t bytes) { return fgc_add_ptr(base, bytes); }
