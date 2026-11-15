# EXP-2026-11-15-668 — P1-12: why a multi-block row cannot be routed bit-exactly

Base: `b5048fed`. Measurement only; no production change. No model, no GPU.

## 1. The question

EXP667 routed the four single-block kernels behind a runtime switch and refused
`nb > 1`, leaving multi-block rows on the native loop. That refusal was justified
by correctness (a one-block kernel would drop the tail), but it left a second
question open: could the caller simply call the single-block kernel once per block
and sum the results, keeping the row on the Goose path?

If the answer were yes, the whole CPU MoE dot path would be Goose-owned today.
This experiment answers it with a measurement rather than an assumption.

## 2. Measurement

`q4_K x q8_K`, 128 iterations per arm, synthetic blocks with a deterministic
generator, comparing

- native: `ggml_vec_dot_q4_K_q8_K_generic(n = nb * 256)`, one call, or
- candidate: `nb` calls to `fg_cpu_simd_q4_k_q8_k` plus a scalar sum.

| nb | bitwise differs | max ULP | max relative |
|---:|---:|---:|---:|
| 1 | 0 / 128 | 0 | 0 |
| 2 | 59 / 128 | 2 | 1.84e-07 |
| 4 | 62 / 128 | 2 | 1.84e-07 |
| 8 | 56 / 128 | 3 | 2.67e-07 |

At `nb == 1` the match is exact, which is exactly the case EXP667 routes. Every
`nb > 1` diverges, and the divergence grows with `nb`.

## 3. Why it diverges — structural, not a kernel defect

The native row kernel keeps **one vector accumulator across all blocks** and folds
it once at the end (`arch/x86/quants.c`, `ggml_vec_dot_q4_K_q8_K`):

```
__m256 acc = _mm256_setzero_ps();
__m128 acc_m = _mm_setzero_ps();
for (int i = 0; i < nb; ++i) {
    ...
    acc = _mm256_fmadd_ps(vd, _mm256_cvtepi32_ps(sumi), acc);
}
...
*s = hsum_float_8(acc) + _mm_cvtss_f32(acc_m);
```

So the native order is `hsum(Σ_blocks lanes)`.

A single-block C entry point returns one `float`, which has already performed
`hsum(lanes)` for that block. Summing those scalars gives
`Σ_blocks hsum(lanes)` — a different association of the same terms. No ordering
of the caller's additions can reconstruct the native value, because the eight
lanes are no longer separately visible. The 1–3 ULP spread is the arithmetic
consequence, not an implementation error.

This is why the correct integration for `nb == 1` is a scalar comparison and the
correct integration for `nb > 1` needs an accumulator that survives the block
loop.

## 4. Decision

The multi-block row stays native. It is recorded as the stated P1-12 boundary,
not as a defect and not as a hidden regression:

- the switch is active for `nb == 1` only, and the gate asserts that a
  multi-block row is refused (EXP667);
- the row loop is the native `ggml_vec_dot_*`, which keeps its own order;
- covering `nb > 1` requires a row-level entry point in the Goose SIMD surface
  that keeps the accumulator live across blocks.

A request for exactly that entry point was sent to the compiler owner
(`runtime-row-contract-p1-12-20261115T1315Z` in the shared mailbox) together with
these numbers. If it is judged out of scope, P1-12 closes with the single-block
scope documented and the multi-block remainder reclassified as substrate — that
decision belongs to Igor, not to this experiment.

## 5. What this measurement protects

Shipping a scalar-sum fast path would have produced a plausible-looking but
silently different result for every multi-block row, at up to 3 ULP, on the
decode path where no differential oracle exists yet. The refusal costs the
speedup; the alternative costs correctness evidence. The numbers above are the
reason the switch is narrow rather than broad.

## 6. Reproduce

The harness is `tests_multiblock.c` in this card. It needs the accepted
`blocks.o` and the native objects; `integration/p1-12/tests/run.sh` builds the
latter.
