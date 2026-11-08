# EXP176 GET_ROWS: resolved and qualified

Status: local gate PASS. CUDA `GET_ROWS` float fast paths are Goose and match the
native operator byte-for-byte. The one fixed-profile Apex gate is recorded in
`apex-summary.json`.

## What was transferred

| Native (`ggml-cuda/getrows.cu`) | Goose |
|---|---|
| `k_get_rows_float_vec<dst_t>` (same type, 16-byte `int4` copies) | `fg176g_vec` |
| `k_get_rows_float<src0_t,dst_t>` (element path) | `fg176g_scalar` |
| `ggml_cuda_op_get_rows` asserts, `ne13==1`, `nb[0]` type size | `run()` in `host.goose` |
| `init_fastdiv_values(ne12)` → `<mp,L,divisor>`, `fastdiv(n) = (umulhi(n,mp)+n)>>L` | `fastdiv()` |
| vector predicate: VEC, enough_blocks, `ne00%VEC`, six `%16` strides, pointer alignment | `use_vec` |
| grid/block sizing; `s1..s3`, `s10..s12`, `ne00v` | `run()` |

Witness: `FG176G_CUDA_GET_ROWS host=…,cuda_status=…,paths=…,guards=…`.
Rollback: `GGML_GOOSE_CUDA_GET_ROWS=0` restores the private-renamed native
`ggml_cuda_op_get_rows` in the same DSO. Base lineage EXP175, so CONCAT,
SET_ROWS, FA, RoPE, GDN, SSM, NORM, activations, broadcast and top-k are
unchanged. The export surface differs from EXP175 by exactly the two diagnostic
readers.

## Defects in my own work, found and fixed

1. `ti` key collision — the tensor-type query at `k==2` was overwritten by a
   packet alignment query, breaking `tsz_of` so the host rejected every case.
   Alignment moved to `k==9`.
2. Route inversion — the host marks the vector route `1`, the adapter sent `1` to
   the scalar template. Now `1` → `fg176g_kernel<0>` (vector), `0` → `<1>`.
3. 16-byte copy — the vector path copied one `int32`; it now copies all four
   words of the native `int4`.
4. Kernel stack — compute-sanitizer reported a stack overflow in the scalar
   kernel. The `es` guard was hoisted out of the element loop and the inner loop
   unrolled. The current build reports `ERROR SUMMARY: 0 errors`.
5. Element scale — `drow` was multiplied by 16 instead of the element size.
   Native casts the element row pointer to `int4*`, so the byte base is
   `drow * sizeof(dst_t)` and only the intra-row vector index scales by 16.
6. `s10..s12` — these are `nb10..nb12 = src1->nb[0..2]` in native, not
   `nb[1..3]`. Using `nb[1..3]` shifted every row index by one dimension.

## The fixture defect that made the gate unreadable

My first fixture violated the `ggml_get_rows` node contract, which
`ggml_get_rows` itself asserts:

```
GGML_ASSERT(a->ne[2] == b->ne[1]);
GGML_ASSERT(a->ne[3] == b->ne[2]);
GGML_ASSERT(b->ne[3] == 1);
result = new_tensor_4d(type, a->ne[0], b->ne[0], b->ne[1], b->ne[2]);
```

so `dst->ne = {src0->ne[0], src1->ne[0], src1->ne[1], src1->ne[2]}` and in
particular `src0->ne[3] == src1->ne[2]`. My multi-plane cases set
`src0->ne[3] = 1` while `src1->ne[2]` was 2 or 3. That is why the *unchanged
native* arm failed those cases identically: the fixture, not the candidate, was
wrong. With the assert set satisfied, both arms pass every case.

## Local gate

Nine cases, each run in both arms of the same candidate DSO:

| check | result |
|---|---|
| `gpu-fixture` Goose | 27 checks / 0 failures / 9 cases |
| `gpu-fixture` native rollback | 27 / 0 / 9 |
| byte-for-byte Goose vs native destinations | 9 of 9 identical |
| compute-sanitizer memcheck | `ERROR SUMMARY: 0 errors` |
| compute-sanitizer racecheck | `0 hazards`, 0 errors, 0 warnings |
| export surface vs EXP175 | exactly the two diagnostics added |

Cases cover the vector route (basic, multi-plane, single index, duplicated
indices, a wide row spanning several y-blocks, deep outer dims) and the scalar
route (too few blocks, odd `ne00`).

## Reproduce

`python3 emit_sources.py`, `python3 emit_bridge.py` (if present),
`python3 build_runtime.py`, `python3 test_local.py`. `FG176G_CASE=<name>` isolates
one case, `FG176G_DUMP=<dir>` writes the raw destinations for an external
byte comparison, `GGML_GOOSE_CUDA_GET_ROWS=0` selects the native rollback.

## Remaining GET_ROWS scope

Not transferred and still native: the quantized dequantizing paths
(`get_rows_cuda_q` for Q4_0/Q4_1/Q5_0/Q5_1/Q8_0 etc. and `get_rows_cuda_kq` for
the K-quants), `GET_ROWS_BACK`, and the non-float destination types. Those depend
on the dequantization module (P3-07) and remain open under P3-03.
