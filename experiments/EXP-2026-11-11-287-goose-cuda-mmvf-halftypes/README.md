# EXP287: CUDA `mul_mat_vec_f` with half and bfloat16 sources in Goose

`mul_mat_vec_f` (`mmvf.cu`), the half and bfloat16 source branches. EXP184
transferred the float branch with `ncols_dst = 1` and no fusion; this experiment
adds the two remaining source types to the same body.

## Verdict

**OFF_PATH** — bit-exact against the native kernel for every value, but the
fixed 32K/128 Apex profile never reaches these instantiations through the Goose
path, so nothing is linked into the shipped DSO. No performance or memory
claim is made.

## Scope transferred

Everything algorithmic is Goose: the channel/sample/row address arithmetic
(including the ids path), the pair-strided dot product, the warp reduction, the
cross-warp pass through shared staging, and the store.

Externs are typed primitives only: `ld_f32`, `ld_half`, `ld_bf16`, `st_f32`,
`mad` (fused multiply-add, `ggml_cuda_mad` order), `shfl_xor`, shared
load/store, `__syncthreads` and the trap hook.

The three source types differ in three places, all explicit:

| dtype | element size | pair stride | element loads |
|---|---:|---:|---|
| 0 float | 4 | 8 | `ld_f32` ×2 |
| 1 half | 2 | 4 | `ld_half` ×2 |
| 2 bfloat16 | 2 | 4 | `ld_bf16` ×2 |

`__half22float2` and the `nv_bfloat162` component reads are the same decoding as
the explicit per-element loads, in the same order, so the FMA sequence is
unchanged. The destination is always float, as in the native kernel for this
instantiation.

The native kernel advances `y` in floats and then indexes it as `float2`; the
module keeps the same split (`ybase = yb*4`, then `col2*8`). Mixing those units
is exactly the bug the first draft had, and the mutation table below pins it.

Host: `block_size_best` is the native search — start at the warp size with
`niter_best = ceil(ncols/(2*warp))`, then scan `bs = 2*warp .. max_block` in
warp steps, first minimum wins. Shared memory is `warp*4` bytes (no fusion),
grid is `(nrows, nchannels_dst, nsamples_or_ntokens)`.

## Gate

The oracle is a verbatim copy of the kernel from the read-only source tree
(`reference/mmvf-kernel.cuh`, source path and `sha256 64d6aae3…c9979d`
recorded; only the kernel name changed, because the kernel is defined in the
`.cu` and not in a header). Both arms receive the same arguments and the same
data, and the destination is compared bit for bit.

| check | result |
|---|---|
| shapes (8): `ncols` 4…8192, `nrows` 1…257, channels 1…4, samples 1…2 | 3 696 values per dtype pair |
| half arm | 1 848 values, all bit-identical |
| bfloat16 arm | 1 848 values, all bit-identical |
| failures / launch failures | 0 / 0 |
| host plan | 31 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| y pair base mixes elements and pairs (the first-draft bug) | 3 002 |
| half/bf16 element size becomes 4 bytes | 92 |
| half/bf16 pair stride becomes 8 bytes | 3 186 |
| half second lane reads the first element | 1 848 |
| warp ladder skips the 8-offset stage | 3 696 |
| cross-warp slot uses the thread id | 3 696 |
| host grid carries one extra channel | 5 |
| host shared memory is halved | 5 |

All eight mutations are caught by the shipped gate.

## Files

- `source/device.goose` — the port.
- `source/host.goose` — the block-size/shared/grid plan.
- `reference/mmvf-kernel.cuh` — verbatim native kernel copy with provenance.
- `tests/gpu-fixture.cu`, `tests/host-fixture.c` — the two gates.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
