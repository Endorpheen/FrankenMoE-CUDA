# EXP276: CUDA timestep embedding in Goose

`timestep_embedding_f32` and the host contract from `ggml-cuda/tsembd.cu`: the
odd-dimension centre zero, `freq = exp(-log(max_period)*j/half)`,
`arg = timestep*freq`, and the cos/sin pair, plus the grid
`(ceil(((dim+1)/2)/256), ne00, 1)`.

## Verdict

**OFF_PATH** — bit-exact against the native kernel, but the fixed 32K/128 Apex
profile does not execute TIMESTEP_EMBEDDING. No performance claim, shipped DSO
unchanged.

## Scope transferred

- device: all control flow and arithmetic. Externs are a float load/store and
  `expf`/`logf`/`cosf`/`sinf`, compiled in the same translation unit with the
  same fast-math flags as the native kernel, so the math calls are the same
  operations rather than approximations.
- host: `num_blocks`, block 256, grid-y = `ne00`, and validation of non-null
  pointers, `ne00 > 0`, `dim > 0`, `max_period > 0`, `nb1 >= dim*4`.

## Oracle

The kernel lives inside `tsembd.cu`, not a header. `reference/tsembd-kernel.cuh`
holds a **verbatim copy** of its body with the source sha256
(`54109ff1…abe82`) recorded; only the kernel name changed.

## Gate

| check | result |
|---|---|
| device: 6 dims (8, 9, 64, 100, 256, 257) × 3 timestep counts × 2 max_periods | 15 268 values bit-identical |
| device failures / launch failures | 0 / 0 |
| host: 4 valid plans (including dim 513, where the `+1` in `half_ceil` matters) + 4 rejections | 28 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| device half bound skips one column | 264 |
| odd-dim zero written at `half` instead of `2*half` | 44 |
| freq divides by `half` instead of multiplying | 2 858 |
| first store uses `sin` | 7 562 |
| host `half_ceil` drops the `+1` | 1 |

The host case `dim = 513` was added before the mutation run because
`ceil(513/2)` and `floor(513/2)` give different block counts there and nowhere
else in the original set — the mutation would otherwise have passed on a fixture
hole.

## Not integrated

Same rule as EXP245–275: emitted and gated, deliberately outside the candidate
link.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py
