/* canonical-io.h - APX030 canonical IO schema and C ABI (worker-3).
 * Records/vecs/maps are the canonical storage contract (worker8). Handles are
 * int64_t; no C++ containers or virtual IO classes on the active path.
 * Raw memcpy/file/tensor/ggml effects are external primitives.
 */
#ifndef FG153_CANONICAL_IO_H
#define FG153_CANONICAL_IO_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* worker8 canonical storage contract (declared here; worker8 owns the impl) */
int64_t fgc_record_new(int64_t kind);
int64_t fgc_record_get(int64_t h, int64_t key);
int64_t fgc_record_set(int64_t h, int64_t key, int64_t value);
int64_t fgc_i64_record_free(int64_t h);
int64_t fgc_vec_new(int64_t item_bytes);
int64_t fgc_vec_len(int64_t h);
int64_t fgc_i64_vec_data(int64_t h);
int64_t fgc_vec_resize(int64_t h, int64_t count);
int64_t fgc_i64_vec_free(int64_t h);
int64_t fgc_map_new(void);
int64_t fgc_map_put(int64_t h, int64_t key, int64_t value);
int64_t fgc_i64_map_get(int64_t h, int64_t key, int64_t out_ptr);
int64_t fgc_map_erase(int64_t h, int64_t key);
int64_t fgc_i64_map_free(int64_t h);

/* IO record schema (kind 3101), fields:
 *  0 type (0 dummy,1 write_host,2 read_host,3 write_file,4 read_file,
 *          5 write_device,6 read_device)
 *  1 ptr (raw byte pointer / FILE*)
 *  2 buf_size
 *  3 size (written/read bytes)
 *  4 skip_tensors
 *  5 file handle (FILE* as i64) for file types
 *  6 list vec handle (entries: kind 3102 record handles)
 *  7 mbufs map handle (device buffer cache; i64 buft -> group record 3103)
 *  8 reserved
 * Entry record (kind 3102): 0 tensor,1 ptr,2 size,3 offset.
 * Group record (kind 3103, device): 0 buft,1 n_tensors,2 total_size,
 *   3 ctx,4 buf,5 org vec,6 cpy vec.
 * Status: 0 success, -1 bounds/truncated, -2 io error, -3 import pending.
 */

int64_t fgcio_create(int64_t type, int64_t ptr, int64_t buf_size, int64_t skip_tensors);
int64_t fgcio_write(int64_t h, int64_t src, int64_t size);       /* 0/-1/-2 */
int64_t fgcio_read(int64_t h, int64_t dst, int64_t size);        /* 0/-1/-2 */
int64_t fgcio_write_tensor(int64_t h, int64_t tensor, int64_t size, int64_t offset);
int64_t fgcio_read_tensor(int64_t h, int64_t tensor, int64_t size, int64_t offset);
int64_t fgcio_n_bytes(int64_t h);
int64_t fgcio_discard(int64_t h);
int64_t fgcio_flush(int64_t h);      /* host/file flush; device = -3 (pending import) */
int64_t fgcio_dispose(int64_t h);    /* flush + free owned records/vecs/maps */

/* Goose entry emitted by the frozen stage3 (strong link below). */
int64_t fg153_goose_canonical_entry(int64_t mode, int64_t a, int64_t b,
                                    int64_t c, int64_t d, int64_t e);
/* raw primitives used by the Goose module (external effects only) */
int64_t fgcio_raw(int64_t op, int64_t a, int64_t b, int64_t c, int64_t d);

#ifdef __cplusplus
}
#endif
#endif
