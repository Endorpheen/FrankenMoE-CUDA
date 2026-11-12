// C primitives for the Goose FrankenMoE log formatter.
#include <stdint.h>

uint8_t fglog_ld_u8(uint64_t p, uint64_t o) {
    return ((const uint8_t *) (uintptr_t) p)[o];
}

void fglog_st_u8(uint64_t p, uint64_t o, uint8_t v) {
    ((uint8_t *) (uintptr_t) p)[o] = v;
}

void fglog_st_i64(uint64_t p, uint64_t o, int64_t v) {
    *(int64_t *) ((char *) (uintptr_t) p + o) = v;
}
