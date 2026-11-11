# EXP411: the dispatched AVX2 q4_K x q8_K dot, ported to Goose

## Hypothesis

The byte-exact replacement for the kernel the server actually calls is the AVX2 association, not the portable
one. EXP410 reproduced the portable algorithm; this item reproduces `arch/x86/quants.c`'s AVX2 variant.

## What differs from the portable algorithm (all four matter for the float result)

1. `dmin = -y.d * f16(x.dmin)` — the sign is folded into the constant and accumulated.
2. The min term is four products, `mins[i] * (bsums[2i] + bsums[2i+1])`, i = 0..3.
3. The scale term is a fused multiply-add: `acc = fma(d, sumi_i32, acc)`.
4. Two accumulators reduced separately: `*s = hsum_float_8(acc) + acc_m_scalar`, with
   `hsum = (r0+r2)+(r1+r3)`, `r[i] = sums[i+4]+sums[i]`, and `acc_m = (m0+m2)+(m1+m3)`.

## Gate

Reference: the **library's own** `ggml_vec_dot_q4_K_q8_K` (the dispatched symbol), after `ggml_cpu_init()`.

    FG410S_ADAPTOR cases=160 checks=161 failures=0

160 of 160 bit-exact, nb = 1, 2, 3, 7.

## Mutation status: needs a clean re-run

The gate is 160/160 against the library's dispatched symbol. The mutation loop, however, did not produce a
trustworthy verdict: a sign mutation of the min term changed the result in one probe while the probe's own
reference changed with it, which means the probe was not measuring a single fixed reference.

Recorded honestly rather than counted: mutations must be re-run against `tests/adaptor.cpp`, where the
reference symbol is fixed in the same binary as the module, and each mutation must be shown to change the
output before it can be called caught or missed.
