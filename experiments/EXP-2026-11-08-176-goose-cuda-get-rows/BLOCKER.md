# EXP176 GET_ROWS: state and blocker

Status: NOT QUALIFIED. No correctness or performance claim. The candidate builds
and links, but the differential gate does not pass and the gate itself is still
not fully trustworthy, so no result can be attributed with confidence.

## Implemented

| Native (`ggml-cuda/getrows.cu`) | Goose |
|---|---|
| `k_get_rows_float_vec<dst_t>` (same type, 16-byte `int4`) | `fg176g_vec` |
| `k_get_rows_float<src0_t,dst_t>` (element path) | `fg176g_scalar` |
| `ggml_cuda_op_get_rows` asserts, `ne13==1`, `nb[0]` type size | `run()` in `host.goose` |
| `init_fastdiv_values(ne12)` → `<mp,L,divisor>`, `fastdiv = (umulhi(n,mp)+n)>>L` | `fastdiv()` |
| vector predicate: VEC, enough_blocks, `ne00%VEC`, six `%16` strides, pointer alignment | `use_vec` |
| grid/block sizing; `s1..s3`, `s10..s12`, `ne00v` | `run()` |

Counter: `FG176G_CUDA_GET_ROWS host=…,cuda_status=…,paths=…,guards=…`.
Rollback: `GGML_GOOSE_CUDA_GET_ROWS=0` selects the private-renamed native
`ggml_cuda_op_get_rows` in the same DSO. Base lineage EXP175, so CONCAT, SET_ROWS,
FA, RoPE, GDN, SSM, NORM, activations, broadcast and top-k are unchanged; the
only new exports are the two diagnostic readers, as in every earlier stage.

## Defects in my own work, found and fixed

1. `ti` key collision — the tensor-type query at `k==2` was overwritten by a
   packet alignment query, breaking `tsz_of` so the host rejected every case.
   Alignment moved to `k==9`.
2. Route inversion — host marks the vector route `1`, adapter sent `1` to the
   scalar template. Now `1` → `fg176g_kernel<0>` (vector), `0` → `<1>` (scalar).
3. 16-byte copy — the vector path copied one `int32`; it now copies all four
   words of the native `int4`, written explicitly instead of looped.
4. Kernel stack — compute-sanitizer reported a stack overflow in the scalar
   kernel at threads ≥32 of block (1,0,0). The `es` guard was hoisted out of the
   element loop and the `w` loop unrolled; `ptxas -v` now reports 0 stack and 0
   spills. The sanitizer run predates the fix and must be repeated.
5. **Element scale** — `drow` was multiplied by 16 instead of the element size.
   Native casts the element row pointer to `int4*`, so the byte base is
   `drow * sizeof(dst_t)` and only the intra-row vector index scales by 16. This
   was the defect that made every multi-plane case write over plane 0.
6. `s10..s12` — these are `nb10..nb12 = src1->nb[0..2]` in native, not
   `nb[1..3]`. Using `nb[1..3]` shifted every row index by one dimension.

## The remaining blocker

Fixture invariants were wrong at first (violated `dst->ne[2] == src0->ne[2] ==
src1->ne[1]` and `dst->ne[1] == src1->ne[0]`, which
`ggml_compute_forward_get_rows_f32` asserts). After fixing them, three of five
cases match byte-for-byte between the Goose route and the native op
(`scalar-few-blocks`, `scalar-odd-row`, `vec-basic`).

The two multi-plane cases still disagree, and neither arm fully covers the
output, so the fixture is not yet a valid oracle:

| case | Goose | Native |
|---|---|---|
| `vec-planes` | planes 0,1 complete; 2,3 partial (31776 NaN of 65536) | planes 0,1,3 complete; 2 partial (15296 NaN) |
| `vec-multiplane` | fails | fails |

Native leaving part of plane 2 unwritten shows the fixture's plane geometry does
not match what the kernel addresses, so a Goose difference cannot yet be
separated from a fixture difference. The specific open question is how the
kernel's `(i10, i11, i12)` triple maps onto `dst->ne = {ne00, ne10, ne11, ne12}`
when `src1` carries two non-unit outer dimensions; the native kernel's
`drow = i10*s1 + i11*s2 + i12*s3` with `s2 = nb2/sizeof(dst_t)` and
`s3 = nb3/sizeof(dst_t)` needs to be matched against a fixture that constructs
those strides exactly as `ggml` would for a real `GET_ROWS` node, rather than as
a dense array I laid out by hand.

## Next steps

1. Re-run `compute-sanitizer --tool memcheck` against the current candidate so the
   stack-overflow finding is confirmed or withdrawn.
2. Build the fixture from a real `ggml_get_rows` graph so strides and extents come
   from `ggml` rather than from hand-written `nb[]`, and validate the reference
   against the CPU `GET_ROWS` implementation on the same tensors. Until the native
   arm passes every case, no Goose deviation is interpretable.
3. Only then run the fixed-profile Apex gate.

No Apex measurement was taken for EXP176, no commit was made, and production, the
active runtime and the compiler are untouched.
