/* abi.c - strong C ABI: canonical IO records driven by the emitted Goose entry. */
#include "canonical-io.h"

int64_t fgcio_create(int64_t type, int64_t ptr, int64_t buf_size, int64_t skip) {
    return fg153_goose_canonical_entry(0, type, ptr, buf_size, skip, 0);
}
int64_t fgcio_write(int64_t h, int64_t src, int64_t size) {
    return fg153_goose_canonical_entry(1, h, src, size, 0, 0);
}
int64_t fgcio_read(int64_t h, int64_t dst, int64_t size) {
    return fg153_goose_canonical_entry(2, h, dst, size, 0, 0);
}
int64_t fgcio_write_tensor(int64_t h, int64_t t, int64_t size, int64_t off) {
    return fg153_goose_canonical_entry(3, h, t, size, off, 0);
}
int64_t fgcio_read_tensor(int64_t h, int64_t t, int64_t size, int64_t off) {
    return fg153_goose_canonical_entry(4, h, t, size, off, 0);
}
int64_t fgcio_n_bytes(int64_t h) { return fg153_goose_canonical_entry(5, h, 0, 0, 0, 0); }
int64_t fgcio_discard(int64_t h) { return fg153_goose_canonical_entry(6, h, 0, 0, 0, 0); }
int64_t fgcio_flush(int64_t h) { return fg153_goose_canonical_entry(7, h, 0, 0, 0, 0); }
int64_t fgcio_dispose(int64_t h) { return fg153_goose_canonical_entry(8, h, 0, 0, 0, 0); }
