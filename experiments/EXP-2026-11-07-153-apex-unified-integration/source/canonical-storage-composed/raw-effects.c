#include <stdint.h>
#include <stdlib.h>
#include <string.h>
int64_t fgc_raw_calloc(int64_t nbytes) { return nbytes > 0 ? (int64_t) (intptr_t) calloc(1, (size_t) nbytes) : 0; }
void fgc_raw_free(int64_t p) { free((void *) (intptr_t) p); }
void fgc_raw_memcpy(int64_t dst, int64_t src, int64_t n) { if (dst && src && n > 0) memcpy((void *) (intptr_t) dst, (void *) (intptr_t) src, (size_t) n); }
void fgc_raw_memset(int64_t dst, int64_t v, int64_t n) { if (dst && n > 0) memset((void *) (intptr_t) dst, (int) v, (size_t) n); }
int64_t fgc_raw_ptr_add(int64_t base, int64_t off) { return base ? (int64_t) (intptr_t) ((unsigned char *) (intptr_t) base + off) : 0; }
int64_t fgc_raw_load_i64(int64_t a) { return a ? *(int64_t *) (intptr_t) a : 0; }
int64_t fgc_raw_load_i32(int64_t a) { return a ? *(int32_t *) (intptr_t) a : 0; }
int64_t fgc_raw_load_i8(int64_t a) { return a ? *(int8_t *) (intptr_t) a : 0; }
void fgc_raw_store_i64(int64_t a, int64_t v) { if (a) *(int64_t *) (intptr_t) a = v; }
void fgc_raw_store_i32(int64_t a, int64_t v) { if (a) *(int32_t *) (intptr_t) a = (int32_t) v; }
void fgc_raw_store_i8(int64_t a, int64_t v) { if (a) *(int8_t *) (intptr_t) a = (int8_t) v; }
void fgc_raw_store_f32(int64_t a, int64_t bits) { if (a) *(int32_t *) (intptr_t) a = (int32_t) bits; }
