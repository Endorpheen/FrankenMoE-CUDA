# EXP P7-02 S3 — GGUF tensor descriptors and data section

Goose port of the tensor half of `ggml/src/gguf.cpp`: `gguf_add_tensor`,
`gguf_set_tensor_type`, `gguf_set_tensor_data`, the cumulative padded offsets,
`gguf_write_tensor_meta`, `gguf_write_tensor_data` and the data section of
`gguf_write_out`. The file also carries the accepted S1 context and the S2
metadata writer, so one standalone unit writes a complete GGUF file.

Base commit: `6954ee4a` (`EXP-2026-11-14-596`, the accepted S2). Native tree:
`upstream/llama.cpp` (GGUF version 3, default alignment 32). The experiment id is
assigned by the lead at integration; this tree is not committed.

## What this slice covers

`source/gguf-tensors.goose` is S1 + S2 + the tensor block:

```
s3_init_tensors(tst, alignment)                       -> 0 or -1
s3_add_tensor(tst, tpool, tne, trec,
              name, ndims, ne, type, data)            -> index or -1
s3_set_tensor_type(tst, tpool, tne, trec, name, type) -> 0 or -1
s3_set_tensor_data(tst, tpool, tne, trec, name, data) -> 0 or -1
s3_find(tst, tpool, trec, name)                       -> index or -1
s3_tensor_size(tne, trec, i)                          -> nbytes
s3_write(st, pool, kv..., tst, tpool, tne, trec,
         out, version, only_meta)                     -> bytes or -1
```

Wire layout produced by `s3_write`:

```
magic "GGUF" | u32 version | u64 n_tensors | u64 n_kv
KV*
descriptor*: u64 name length + name, u32 n_dims, i64 ne[0..n_dims),
             i32 ggml_type, u64 offset
zero pad to alignment
(only_meta == 0) data*: nbytes of tensor i, zero pad to alignment
```

The descriptor offset of tensor *i* is `offset[i-1] + PAD(nbytes[i-1],
alignment)`, with `offset[0] = 0`; that is exactly what `gguf_add_tensor` and
`gguf_set_tensor_type` compute natively. `n_dims` is `ggml_n_dims` over `ne`
(the highest dimension > 1, at least 1), not the declared count. `nbytes` is
`type_size * ne[0] / block_size * ne[1] * ne[2] * ne[3]` for a contiguous
tensor, from the native type size / block size table.

Semantics kept from native:

- a duplicate tensor name is refused (native `GGML_ABORT`s, the port returns -1);
- `set_tensor_type` recomputes every later offset and refuses a row that does
  not divide by the new block size;
- `set_tensor_data` only records the image; `s3_write` emits `nbytes` of the
  current type and returns -1 when the stored image is shorter (native would
  read past the pointer);
- `only_meta` stops after the descriptor pad, so the metadata bytes are a prefix
  of the full file and no data pointer is needed;
- each tensor's data is followed by zero padding to the alignment, so the total
  size is a multiple of the alignment.

## Layout

```
source/gguf-tensors.goose  port (S1 + S2 + S3)
tests/fixture.cpp          native + port in one binary, byte compare + reload
tests/build.sh             gate build and run (-O1)
tests/mutate.py            12 mutations, two halves of 6, port built at -O0
tests/regress-s2.sh        the accepted S2 gate against this source
tests/gate.txt             recorded gate output
tests/mutations.txt        recorded mutation results
```

## Gate

```
bash tests/build.sh
```

`build.sh` generates `build/gguf_tensors.c/.h` with `goose --standalone`,
compiles it with `gcc-13 -O1`, builds `tests/fixture.cpp` against the native
`libggml-base` and runs the comparison. Generated C and objects stay in
`build/`; the source tree stays clean.

Recorded result:

```
FG_G3 cases=41 checks=307 failures=0
```

The 41 cases are the 31 S2 metadata cases (regression through `s2_write`), one
zero-tensor case through `s3_write`, and nine tensor cases. Every tensor case
compares, against a fresh native context driven through the public API:

- the canonical KV dump;
- the descriptor dump: name, type, `n_dims`, `ne[4]`, offset, `nbytes`;
- the metadata bytes (`gguf_get_meta_size` / `gguf_get_meta_data`);
- the full bytes (`gguf_write_to_file`);
- the `only_meta` output as a prefix of the full output.

Each tensor case also writes the port's full bytes to a temporary file and
reloads them through `gguf_init_from_file` with `no_alloc=false`: the alignment,
the descriptor dump and the data read back through `ggml_get_tensor` are checked.

Alignment 64 and 1 cannot be set on an in-memory native context
(`gguf_init_empty` fixes 32), so the fixture writes an alignment-only file with
the port, reloads it natively to obtain a context with that alignment, and then
calls `gguf_add_tensor` on it. That makes the non-default cases byte-comparable
through public calls only.

## Cases

| Case | Tensors | align | meta / full bytes |
|------|---------|------:|------------------:|
| `t_none` | none | 32 | equal (S2 bytes) |
| `t_one` | f32 [2,2] | 32 | 96 / 128 |
| `t_four` | f32 [2,2], f16 [4,2], q8_0 [32], i32 [3] | 32 | 192 / 352 |
| `t_four_kv` | the four plus 5 KVs | 32 | 384 / 544 |
| `t_retype_mid` | f32, f32→f16, f32 | 32 | 128 / 224 |
| `t_retype_first` | f32→f16 [4,2], i32 [3] | 32 | 128 / 192 |
| `t_shapes` | f32 [1], i32 [3], f32 [2,3], f16 [2,2,2], i8 [2,2,2,2], f32 [4] declared 2D | 32 | 288 / 480 |
| `t_align64` | the four | 64 | 320 / 576 |
| `t_align1` | f32 [3], i32 [5] | 1 | 123 / 155 |
| `t_meta_no_data` | f32 [4], q8_0 [32], no data images | 32 | 128 / refused |

`t_align64` covers cumulative offsets 0/64/128/192 and 64-byte data padding;
`t_align1` has no padding at all (offsets 0 and 12).

Port-only refusals (`negative`): duplicate name, unknown tensor in
`set_tensor_type` / `set_tensor_data`, bad `ndims`, empty name, removed type
(block size 0), non-power-of-two alignment, a short data image after a retype,
and a missing data image on a full write. Native aborts in these situations
(`GGML_ABORT` / assertions), so they cannot run in the same process and are
checked on the port side only.

## Mutations

```
python3 tests/mutate.py --half 1   # 6/6 caught
python3 tests/mutate.py --half 2   # 6/6 caught
```

Each mutation rewrites the port, regenerates C, rebuilds the fixture (port at
`-O0`) and must make the gate fail. Mutant sources and binaries live in
`tests/mut/Mxx/`.

Half 1: offset accumulation without padding, `n_dims` hard-coded to 4, all four
`ne` written, tensor name without a length prefix, missing data padding, tensor
type written as 8 bytes.

Half 2: the 32-element block group reports 16, f16 type size 4, data copy starts
one byte late, data blocks written last-to-first, descriptors written
last-to-first, no pad between descriptors and data.

## Recorded limits

- **Contiguous tensors only.** Native `gguf_write_tensor_data` asserts
  `ggml_is_contiguous`; the port computes the contiguous `nbytes` and has no
  stride model. A non-contiguous tensor is out of scope.
- **Raw data pointer only.** Native uses `ggml_backend_tensor_get` when
  `info.t.buffer` is set (GPU / backend tensors). The port has no backend and
  expects materialized bytes; a model whose tensors live in a backend buffer
  must copy them out first.
- **Declared `ndims` is validated, not written.** The wire value follows
  `ggml_n_dims` over `ne`, like native. Passing `ndims=2` with `ne=[4,1,1,1]`
  writes `n_dims=1`, which is what `gguf_add_tensor` would do.
- **The alignment is tensor state.** `s3_init_tensors` sets it once and both
  offsets and padding use it. The port does not inject the
  `general.alignment` KV: the caller must keep that KV equal to the same value,
  otherwise the produced file is internally inconsistent.
- **Type table is a copy of this tree.** It carries all 43 `ggml_type` values
  (block size 0 for removed types, refused). A newer upstream type needs a table
  update; the gate exercises f32, f16, q8_0, i32 and i8 only.
- **`nbytes` arithmetic is i64 without overflow checks.** Native has
  `SIZE_MAX` guards in the reader; the port assumes tensor sizes that fit. The
  largest gate tensor is 34 bytes.
- **Reloaded native contexts cannot be fully re-serialized.** `gguf_init_from_file`
  leaves `ggml_tensor::buffer` uninitialized, so `gguf_write_to_file` on a
  reloaded context may take the backend-copy branch and crash. This is native
  behaviour, not a port property. The fixture therefore compares metadata,
  descriptors, data read-back and padding on the reload path instead of a second
  full native write; the full bytes are already compared directly against a
  fresh aligned native context.
- **Single-threaded, no weights, no GPU.** All tensors are synthetic and small;
  the fixture buffer is 1 MiB.
