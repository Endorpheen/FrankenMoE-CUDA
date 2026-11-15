// Data loading for the standalone runtime: a GGUF's tensor payload, with the
// layout arithmetic owned by goose/model/gguf-offsets.goose.
//
// Division of labour, unchanged from the earlier layers:
//   substrate  ggml-base reads the file and materialises the tensors
//   Goose      gguf-offsets.goose owns nbytes, layout, align_up and align_ok
//   host       this file: the bookkeeping and the agreement check
//
// Why the check exists: ggml lays the payload out itself. If its placement and
// the Goose layout disagree, one of the two is wrong, and that difference would
// otherwise surface as a wrong matmul much later. `mismatches` makes it loud.
#ifndef FG_GOOSE_TENSOR_DATA_H
#define FG_GOOSE_TENSOR_DATA_H

#include <stdint.h>
#include <stddef.h>

#include "gguf.h"
#include "ggml.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void **  data;        // per-tensor view, index matches the GGUF tensor id
    size_t * size;        // per-tensor byte size
    int64_t  n_tensors;
    int64_t  layout_rc;   // the Goose layout's own return code (0 = accepted)
    size_t   total;       // padded data size the Goose layout computed
    size_t   copied;      // bytes the reader placed
    int64_t  mismatches;  // tensors whose ggml offset differs from the Goose one
} fg_tensor_arena;

// Records every tensor payload materialised in `gctx` and compares its placement
// with the Goose layout. Returns 0 on success, -1 when the Goose layout refuses
// the descriptors, -2 on a host failure.
int fg_goose_tensor_load(const struct gguf_context * ctx,
                         struct ggml_context * gctx,
                         fg_tensor_arena * out);

void fg_goose_tensor_free(fg_tensor_arena * a);

#ifdef __cplusplus
}
#endif

#endif // FG_GOOSE_TENSOR_DATA_H
