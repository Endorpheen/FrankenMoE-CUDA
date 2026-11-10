# EXP236: `vec_dot_q6_K_q8_1_impl_mmq` in Goose — bit-exact and at parity

Q6_K is 9.7% of Apex kernel time by the census. This applies the recipe EXP235 proved: declare `dp4a` as an
extern primitive rather than writing the byte products out longhand.

## The two traps specific to Q6_K

1. **The scale is SIGNED.** `sc_reg` is `int8_t *` over a packed 32-bit word, so each byte needs
   sign extension. Reading it unsigned flips the sign of every scale term — 16812 of 30000 gate cases fail.
2. **Two accumulators, different scale bytes.** `sumi_d.x` and `sumi_d.y` are fed from different halves of
   the tile and combined with `sc_reg[i0/2+0]` and `sc_reg[i0/2+1]` respectively. Swapping them looks
   plausible and fails 19828 of 30000 cases.

## Host gate

    30000 checks, 0 failures

30000 random cases against a transcription of the native body, absolute tolerance `1e-3*(1+|ref|)`.
Mutations: unsigned scale 16812 failures, swapped accumulators 19828.

## GPU differential

    FG236K_KERNEL_MATCH nsteps=2 n=65536 bad=0 worst_rel=0.000e+00

**Bit-exact on the real device function, 65536 elements, zero difference.**

The run also prints `nsteps=1 ... bad=65291`, and that line is a non-configuration, not a defect:
`VDR_Q6_K_Q8_1_MMQ` is 8 and the native loop steps by 4, so the only real case is 2 steps. My harness
parameterised the step count and printed both; the 1-step column compares my module against a native body
running 2 steps, so of course they differ. Recorded rather than quietly dropped, because a reader seeing
one green row and one red row deserves to know which one describes reality.

## Timing

    native median 10527.5 ns   (min 10460.5, max 10806.0)
    goose  median 10588.5 ns   (min 10478.0, max 10637.0)
    delta  +0.58%

Parity, and this time **without** the structural problems that made the Q4_K *tile* loop 2.1× slower: the
inner product has no accumulator array to keep in a register and no loop bounds that need to be constexpr
beyond the two unrolled steps the emission handles.

`IDP.4A` is used because `dp4a` is declared, exactly as in EXP235. Without that declaration this kernel
would show the same 19% gap.

## Status

Bit-exact, at parity, **not wired**. Wiring means substituting the call site in `mmq-vec-dot.cuh`, which
also carries the tile loop that is still 2.1× slower for Q4_K — so Q6_K cannot be promoted alone without
re-measuring the combination.
