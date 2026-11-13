# EXP303: P6-02 closure -- attachment decided by the module, the rest native and measured

Criterion: "All model tensor ownership, backend attachment, metadata queries and unload/error cleanup implemented with ABI
parity."

The integration rule for this migration is that modules return decisions while native code keeps containers, ownership and
logging. This item is closed by naming which of the four areas follows which rule, and by measuring the parity of the parts
that stay native.

## Backend attachment -- ported and live

The buffer-type choice is a module decision already: `llama-model-loader.cpp:1122-1138` asks `fg404s_decide` for both the
"no operation, take the first buffer" case and the per-device probe loop, with `weight_buft_supported` as the native
predicate. The accepted run prints `FG404S_WEIGHT_PLACEMENT`, so the decision path is live on real weights. The model side
then allocates from the chosen buffer type -- native ownership by design.

## Tensor ownership -- native, exercised

`llama-model.cpp` allocates through `ggml_backend_alloc_ctx_tensors_from_buft`, the dummy-buffer path for `no_alloc`, the
host-pointer mapping path, and the mlock registration. All of these are ownership, which stays native by the same rule. The
accepted run allocates the full 40-block MoE model and shuts down cleanly.

## Metadata queries -- native, parity measured

A direct C-API probe (`meta-abi.c`) loads the real model on both lines and walks all metadata:

| check | native | ours |
|---|---|---|
| entry count | 47 | 47 |
| FNV hash over every key/value in index order | `d4e54caca421a3c8` | `d4e54caca421a3c8` |
| missing key: return and `buf[0]` | -1, cleared | -1, cleared |
| truncation to 4 bytes: return and buffer | 9, `qwe` | 9, `qwe` |
| `buf_size = 0, buf = NULL` | 9 | 9 |

So index order, the would-be length from `snprintf`, the miss semantics and the null-buffer case are identical rather than
merely plausible.

## Unload and error cleanup -- native, parity measured

- Normal path: the probe loads and frees the model, exit code 0 on both lines; the server runs too and exits 0 on SIGINT.
- Failure path: the three invalid-GGUF fixtures from EXP302 produce identical messages and exit 1 on both lines.
- Ordering: the expert-pin owner is released before buffers and mmaps, as the comment in `llama-model.cpp` states; buffers
  are held by `ggml_backend_buffer_ptr`, so a partial failure frees what was built.

## Boundary

Three of the four areas stay native because they are ownership, cleanup and pure string formatting -- the exact categories
the migration rule keeps native. They are recorded here with measured parity instead of being silently omitted, and the one
area with a real decision (attachment) is ported and live.
