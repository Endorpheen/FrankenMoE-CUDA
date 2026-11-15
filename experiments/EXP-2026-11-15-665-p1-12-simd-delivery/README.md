# EXP-2026-11-15-665 — P1-12: the SIMD delivery is accepted on a second, native oracle

Base: `ed9e0979`. No model, no server, no GPU, no benchmark. This experiment
does not change production code; it accepts a delivery from the compiler owner
and states exactly what that acceptance covers.

## 1. Why this needed a second oracle

The compiler owner delivered a Goose SIMD surface with four quantized block
kernels and proved them against `ggml_vec_dot_*_generic`. That proof is
necessary and was reproduced unchanged here, but it is one oracle run by the
author of the code under test. P1-12 closes a *runtime* requirement, so the
runtime side has to establish the same fact independently, against the native
code this repository actually ships.

The delivery:

| item | value |
|---|---|
| commit | `7738b34a38c7be0946f743b23bb373495bfec3f3`, branch `codex/p1-12-simd-20261115` |
| package | `/home/end0/AI/Fried-Goose/build/qualified/cpu-simd-8b7a56bb835bc87c` |
| `compiler` SHA-256 | `8b7a56bb835bc87c8c7704341cb5f92a353de5b610418968307ebe171434b88e` |
| `cpu/production/blocks.o` SHA-256 | `67f7f760dd9fcfb6b9f828777ea06750f3b9a032b3b9ffe4ebd8307b055eab4e` |
| ABI | `fg_cpu_simd_bytes { const uint8_t * data; int64_t len; }` |
| entry points | `fg_cpu_simd_{q4_k_q8_k,q6_k_q8_k,q8_0_q8_0,q4_0_q8_0,features}()` |
| contract | one block per call; return value is the integer accumulator, `*value` the fp32 result |

Both hashes were checked against the published values before anything was used.

## 2. What was measured

Two oracles, both on this host (gcc-13 13.4.0, AVX2+FMA reported as `0x7`):

1. **The author's verifier, run unchanged**: 16 384 blocks per arm,
   `integer=exact`, `float=bit-exact` on all five arms (avx2, scalar, disabled,
   gcc-avx2, gcc13-avx2); `native_calls` 1 568 768 on the three native arms and
   0 on scalar and disabled.
2. **A runtime-side oracle against the unmodified native sources**:
   `ggml-quants.c` (`quantize_row_*_ref`), `ggml-cpu/quants.c`
   (`ggml_vec_dot_*_generic`) and `ggml-cpu/arch/x86/quants.c` (the AVX2 kernels
   the shipped CPU path runs). Nothing is transcribed and no kernel is wrapped:
   both arms are the native code.

| run | cases | failures | AVX2 vs scalar spread |
|---|---:|---:|---:|
| 256 iterations | 1 024 | 0 | 0 |
| 512 iterations | 2 048 | 0 | 0 |
| 1 024 iterations | 4 096 | 0 | 0 |
| 4 096 iterations | 16 384 | 0 | 0 |

Integer minima and fp32 values are bit-identical, signed zero included. The
last column matters: the Goose accumulation order reproduces the native order on
this corpus rather than merely landing inside a tolerance.

## 3. Two findings for the record

1. `blocks.h` and the object agree: the return value is the integer
   accumulator, not a status code. A harness that treats it as a status reports
   nonsense; this is a contract-reading mistake, not a delivery defect.
2. A zero-filled `ggml_table_f32_f16` makes the *native* AVX2 arm return 0.0 for
   every block. The table is filled from the real fp16 conversion in the stub,
   because the native reference must not be weakened. Anyone else linking these
   native objects needs the same care.

## 4. Scope of the acceptance

Accepted: the four single-block entry points on AVX2+FMA with the scalar
fallback, at the measured parity above.

Not accepted, and not claimed by the delivery: NEON/SVE/AVX512/VNNI, arbitrary
multi-threaded self-hosted application support, and any performance number. The
single-block example is not a multi-block contract; full-row and MoE
accumulation loops keep the native order.

The full frozen runtime `main.goose` still refuses `export fn`, so this delivery
does not replace the composed unit. It provides a separate Goose object with a C
ABI, which is what lets the CPU MoE path be switched without waiting for that
compiler limitation to be lifted.

## 5. Reproduce

```
bash integration/p1-12/tests/run.sh 1024
```

The script refuses to run when `blocks.o` has a different SHA-256 than the
published one. `tests/native_stubs.c` supplies only the two ggml support symbols
the linked objects need (`ggml_abort`, `ggml_table_f32_f16`) and the type-trait
helpers; no numeric kernel is replaced.

## 6. Next

The four entry points go behind a runtime switch in the CPU MoE path with the
native kernels as the OFF arm (rule 10). That integration is the remaining P1-12
work and is a separate experiment.
