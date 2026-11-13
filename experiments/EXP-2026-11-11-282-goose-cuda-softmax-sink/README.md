# EXP282: CUDA softmax with sinks in Goose (mask F32/F16)

`soft_max_f32` with sinks for both mask types, extending EXP281. Sinks enter in
two places in native:

    float max_val = sinks ? sinks[i02] : -INFINITY;
    ...
    if (sinks) tmp += expf(sinks[i02] - max_val);

with `i02 = blockIdx.y`.

## Verdict

**OFF_PATH** — bit-exact against both native mask+ sinks instantiations; the fixed
32K/128 Apex profile runs Flash Attention with its own softmax. No performance
claim, shipped DSO unchanged.

## Scope transferred

The EXP281 body plus the two sink sites above. Sinks are loaded as
`ld_f32(sinks, i02*4)`. All control flow and arithmetic is Goose; shared memory
and the reduce ladders are the EXP279/280 primitives.

Still not ported: the cooperative `parallelize_cols` kernel (rejected by the host
plan with code 24) and `soft_max_back`.

## Gate

| check | result |
|---|---|
| device: 5 shapes × 2 mask types (sinks present in every case) | 15 584 values bit-identical |
| device failures / launch failures | 0 / 0 |
| host: EXP280 plan gate (mask/sink independent) | 45 checks / 0 failures |

## Mutations — and two value-domain lessons

| mutation | failures |
|---|---:|
| sink term dropped from the denominator | 5 672 |
| sink exponent adds the max instead of subtracting | 15 584 |
| F16 mask offset divides by 4 instead of 2 | 4 868 |
| mask term drops the alibi slope | 3 072 |
| F32 entry passes `bx` as the sink index | 1 024 |

The first run missed two mutations, and both were value-domain problems:

1. **Large sinks saturate everything.** With `sinks = 1000 + 50*i` and row values
   around ±125, `expf(v - max)` underflows to 0 and every sink mutation produces
   the same all-zero output. The fixture now uses row values in ±1 and
   `sinks = 5 + i`, where the column exponentials are normal.
2. **The max-init mutation is algebraically neutral.** `max_val = max(sink, row)`
   versus `max_val = row` cancels in the softmax ratio — sink shift invariance —
   so it can never fail on values, only on saturation. It was replaced with
   "drop the sink term from the denominator", which is observable.

This is a sharper version of the EXP279 note: it is not enough for the generator
to span the control flow; its values must also stay in the regime where the
mutation is observable.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py
