# EXP280: CUDA softmax in Goose — mask-free, sink-free path

`soft_max_f32` from `ggml-cuda/softmax.cu` for the path with no mask and no sinks,
alibi included. This reuses the shared-memory pattern that EXP279 established for
cumsum.

## Verdict

**OFF_PATH** — bit-exact against the native dynamic instantiation, but the fixed
32K/128 Apex profile executes Flash Attention, which has its own internal
softmax; standalone SOFT_MAX is not on this profile. No performance claim, shipped
DSO unchanged.

## Scope transferred — and what is not

Ported: the row/grid mapping, the alibi slope (`max_bias`, `n_head_log2`, `m0`,
`m1`), the value pass `x*scale + 0.0f` (the mask term is a real `+0.0f`, which
matters for signed zero), the block max reduce, the `expf(v - max)` pass, the
shared-memory reuse, the block sum reduce, `1/sum`, and the final normalize pass.
Shared memory is explicit primitives; the reduce ladders are shuffles; statement
order is preserved.

Not ported, and rejected in the host module rather than silently substituted:

- mask inputs (f16/f32) and sinks,
- the cooperative `soft_max_f32_parallelize_cols` kernel (taken by the native
  only when shared memory is insufficient and `ncols/rows > 8192`, no mask/sinks,
  `scale == 1`, `max_bias == 0`) — host code 24,
- `soft_max_back_f32`.

## Gate

| check | result |
|---|---|
| device: 8 cases (ncols 1/32/33/100/128/1024/4096 + a 1000-col low-shared path; 4096×2×3 with `max_bias=8` exercises alibi) | 27 894 values bit-identical |
| device failures / launch failures | 0 / 0 |
| host: 6 plans (1…5000) + low path + cooperative rejection + 2 input rejections | 45 checks / 0 failures |

The oracle is a verbatim copy of the struct and kernel with the source sha256
(`ac288d3d…84976`) recorded; only the kernel name changed. The fixture calls the
native dynamic instantiation `<use_shared,0,0,float>`, which is the form the Goose
module implements.

## Mutations — and a self-masking clamp

| mutation | failures |
|---|---:|
| block max tracks the last value | 2 730 |
| warp max ladder adds instead of max | 27 894 |
| exp pass forgets to subtract the row max | 9 463 |
| inverse sum uses 2.0 | 9 541 |
| host doubling cap 1024 -> 2048 | 1 |

The host mutation first passed with **0 failures**: my translated doubling loop
had an extra defensive clamp `if nth > 1024 { nth = 1024; }` that re-capped the
value the mutation had raised. The clamp was redundant — the native loop condition
already caps at 1024 — so the Go code was rewritten to the native form
`while nth < ncols && nth < 1024`, and the mutation was then caught. A guard that
is not in the reference can hide a defect from a mutation test; recorded because
it is a module-side version of the same lesson.

Inputs are finite values (the EXP279 NaN-masking lesson).

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py
