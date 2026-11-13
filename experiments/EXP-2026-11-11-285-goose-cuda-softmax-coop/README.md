# EXP285: CUDA cooperative softmax in Goose — softmax family complete

`soft_max_f32_parallelize_cols` and its single-row body from
`ggml-cuda/softmax.cu`. This is the branch the native takes only when the
shared-memory softmax does not fit (`nbytes_shared > smpbo`), cooperative launch
is supported, `ncols/rows > 8192`, and there is no mask, no sinks, `scale == 1`
and `max_bias == 0`.

## Verdict

**OFF_PATH** — bit-exact against the native cooperative kernel, but the fixed
32K/128 Apex profile does not execute softmax at all (Flash Attention has its own
internal softmax). No performance claim, shipped DSO unchanged.

## Scope transferred

All algorithm content is Goose: the 4-values-per-thread grid-strided passes, the
block max/sum reduces, the CTA-level exchange through global memory
(`tmp_maxs`/`tmp_sums`) and the grid barrier between the two aggregations.
Primitives: dynamic shared base and load/store, `__syncthreads`, shuffles, and
`cooperative_groups::this_grid().sync()`. The final divide pass is included.

Host: this module plans **only** the cooperative branch and rejects every other
configuration with a distinct code instead of planning a different algorithm —
25 (shared would fit), 26 (no cooperative support), 27 (`ncols/rows <= 8192`),
28 (`scale != 1`), 29 (`max_bias != 0`). Plan: block 256, grid `nsm`, shared
256 bytes.

With this, every softmax kernel in `softmax.cu` has a Goose implementation:
forward mask-free (EXP280), mask F32/F16 (EXP281), sinks (EXP282), backward
(EXP284) and cooperative (EXP285).

## Gate

| check | result |
|---|---|
| device: 3 cases (20000×1, 40000×2, 100000×1) with cooperative launch on both arms | 200 000 values bit-identical |
| device failures / launch failures | 0 / 0 |
| host: 1 plan + 5 branch rejections | 16 checks / 0 failures |

## Mutations — and a value-placement lesson

| mutation | failures |
|---|---:|
| grid stride drops `blockDim` | 200 000 |
| global max load misses the last CTA | 142 141 |
| exp pass subtracts the CTA max instead of the global max | 200 000 |
| divide pass skips the division | 64 672 |
| host block 256 -> 128 | 1 |

The "last CTA" mutation first passed: whether skipping one CTA matters depends on
where the maximum lives, and with random rows it usually lives elsewhere. The
fixture now places each row's maximum inside the last CTA. That alone then hid
the divide mutation, because a spike of 50 saturated every exponential to zero
and `0/ts == 0`. A spike of 2.0 is unique (rows are ±1) and keeps the
exponentials non-zero, which makes both mutations observable at once.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py
