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

## The first "160/160" was false, and how it was false

The gate builds `bridge.c`, which contained the drop-in hook exporting `ggml_vec_dot_q4_K_q8_K`. The gate calls
that same symbol as its reference, so it compared the module **with itself**. Every mutation passed, which is
what exposed it: a `high nibble not shifted` mutation cannot survive an honest comparison.

Fix: the hook moved to `source/hook.c`, separate from the bridge. Gate = `adaptor.cpp` + `bridge.o` only; the
drop-in library = `bridge.o` + `hook.o`.

With the honest gate the real numbers appeared, and two genuine AVX2 differences were found and fixed:

| defect | signal | fix |
|---|---|---|
| min term built from one product per lane | 160/160 -> 96/160 | `madd_epi16` sums pairs: `prod[i] = mins[2i]*(bs[4i]+bs[4i+1]) + mins[2i+1]*(bs[4i+2]+bs[4i+3])` |
| assumed one scale per 32-block | no change (equivalent) | AVX2 applies `scale_l` to the low half and `scale_h` to the high half, which is index `sc[j]` either way |

Current: **96 of 160 fail**, differences are ~3e-6 relative, e.g. `-611.969666` vs `-611.971619`. Larger than
one ulp, so a term still differs rather than only its association.

Lesson recorded: a reference must not be the same symbol the module defines.

## State after the honest gate

96 of 160 fail with ~3e-6 relative differences. Ruled out by exact comparison against the AVX2 source:

- scale assignment: AVX2 applies `scale[2j]` to the low 32 of each 64-element group and `scale[2j+1]` to the
  high 32, which for sub-block index `j` is exactly `sc[j]` — my formula `sc[grp*2 + half]` reduces to the same
  indexing, so the earlier "one scale per 32-block" reading was right by coincidence of index arithmetic;
- min term: `madd_epi16` pairs confirmed, `prod[i] = mins[2i]*(bs[4i]+bs[4i+1]) + mins[2i+1]*(bs[4i+2]+bs[4i+3])`;
- horizontal orders: `hsum = (r0+r2)+(r1+r3)` with `r[i]=sums[i+4]+sums[i]`, and `acc_m = (m0+m2)+(m1+m3)`;
- `d` and `dmin` sign folding are exact in float.

Next probe, and it is decisive rather than another guess: build an intrinsic `-mavx2 -mfma` transcription of
`arch/x86/quants.c` in the test, diff its `hsum`, `msum` and `sumi` against the module's on the same block, and
find the first member that differs. The source for the AVX2 path exists, so the remaining work is to compare
member by member instead of reasoning about it.

## Clean: 160/160 against the dispatched AVX2, and the defect that was left

Isolating the two terms decided it in one step:

| data | what it exercises | failures |
|---|---|---|
| `x.dmin = 0` | scale branch only | 104 |
| `x.d = 0` | min branch only | **0** |

So the min branch was already exact and the scale branch was wrong **by value**, not by association — which is
why every horizontal-order variant landed between 89 and 105 instead of reaching zero.

The defect: lane grouping. The portable kernel assigns lane `l` the elements `l, l+8, l+16, l+24`
(`li % 8`). AVX2 goes through `maddubs` then `madd_epi16`, so lane `i` owns the four consecutive elements
`4i..4i+3`, i.e. `li / 4`. With `li / 4`:

    FG410S_ADAPTOR cases=160 checks=161 failures=0

160 of 160 bit-exact against `ggml_vec_dot_q4_K_q8_K` for nb = 1, 2, 3, 7.
