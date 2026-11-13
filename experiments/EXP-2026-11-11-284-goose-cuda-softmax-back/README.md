# EXP284: CUDA softmax backward in Goose

`soft_max_back_f32` from `ggml-cuda/softmax.cu`: one row per 32-thread block, no
shared memory.

    dgf_dot  = sum over col of dstf[col]*grad[col]   (WARP_SIZE stride)
    dst[col] = scale * (grad[col] - dgf_dot) * dstf[col]

## Verdict

**OFF_PATH** — bit-exact against the native kernel, but the fixed 32K/128 Apex
profile does not execute SOFT_MAX_BACK. No performance claim, shipped DSO
unchanged.

## Scope transferred

The dot product, the 16/8/4/2/1 shuffle-sum ladder, the WARP_SIZE-strided write
pass and the final expression, all in Goose. Externs are 32-bit float loads, a
store and a shuffle. Host: block `(32,1,1)`, grid `(nrows,1,1)`, pointer and
extent checks.

## Oracle

Lines 256–276 of `softmax.cu` copied verbatim into
`reference/softmax-back.cuh` with the source sha256
(`ac288d3d…84976`) recorded; only the kernel name changed.

## Gate

| check | result |
|---|---|
| device: 6 column counts (1,31,32,33,100,1000) × 2 row counts × 3 scales (1.0, 0.5, −2.0) | 14 364 values bit-identical |
| device failures / launch failures | 0 / 0 |
| host: 3 plans + 2 rejections | 16 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| dot product drops the gradient factor | 14 363 |
| warp ladder starts at 8 | 14 346 |
| final expression drops the `dstf` factor | 14 355 |
| dot loop strides by 16 instead of 32 | 14 344 |
| host block 32 -> 64 | 3 |

## Own defect found before the gate

The first reference copy stopped at the last statement of the kernel, leaving the
closing braces out. The next `#include` then landed inside the kernel body and the
compiler produced 29 cascading "identifier undefined" errors that pointed at the
generated module rather than at the truncated oracle. The range was extended to
line 276. When a copied oracle produces errors *inside* the module it is meant to
check, check the oracle's braces first.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py
