# EXP-2026-11-15-633 — signed-zero fix in the basic dequantizers (P7-04)

The canonical `quant-dequant.goose` lost the sign of zero in two places:

1. `dq_half` returned `0.0 - v`, which negates a non-zero value but turns
   `-0.0` into `+0.0`. The native `GGML_FP16_TO_FP32` keeps the sign bit.
2. The q4_0 and q5_0 arms added `m` (a zero min) although those types have no
   minimum: `-0.0 + 0.0` is `+0.0`, while native computes `x*d` only.

The old gate compared numerically and folded `-0` to `+0`, so the defect passed
all 300 cases. This experiment compares the native and Goose results bit-exactly
and keeps the three signed-zero probes out of the folded corpus.

## Result

- Fixed module: 300/300 cases, 0 failures, bit-exact.
- Old canonical module under the same gate: 12+ failures (`want=-0 got=0`).
- Mutations: 12/12 in two halves, plus 3/3 signed-zero-specific (half-negation,
  q4_0 zero-min, q5_0 always-min).

## Files

- `source/quant-dequant.goose` — the fixed module (now canonical).
- `tests/fixture.cpp` — bit-exact differential gate against the native
  `to_float` for q4_0/q4_1/q5_0/q5_1/q8_0.
- `tests/mutate.py`, `tests/build.sh` — mutation harness and build.

## Boundaries

The gate uses the native dequantizer from the reference llama.cpp build; fp16
conversion is native. The fix is arithmetic only and changes no ABI.
