# EXP235: `vec_dot_q4_K_q8_1_impl_mmq` in Goose — the inner kernel of the 30% block

This is the arithmetic inside `mul_mat_q` for Q4_K, which the census puts at 30.3% of kernel time, the
largest single remaining native item. The outer tiling loop stays native for now; this is the 25-line
inner product it calls.

## What moved

    sumf_d = 0, sumf_m = 0
    for i in 0..2:                                    # QR4_K*VDR/QI8_1 = 2*8/8
        sumi_d = 0
        for j in 0..8:
            sumi_d = dp4a((v[j] >> 4*i) & 0x0F0F0F0F, u[i*8 + j], sumi_d)
        sumf_d += dsx[i] * (sc[i] * sumi_d)
        sumf_m += dsy[i] * m[i]
    return dm4x*sumf_d - dm4y*sumf_m

**`dp4a` is ported as real arithmetic**, not left as a primitive: four sign-extended byte products plus
an accumulator, in a loop. The sign extension is the part that matters — `b` holds signed q8 bytes while
`a` holds masked 4-bit nibbles, so `(int8_t)` on both sides is required and `(uint8_t)` on the right would
be silently wrong for half the input range.

Only the two `half2 -> float2` conversions stay primitives, because they are format decoding rather than
arithmetic and the bridge already owns the half type.

Accumulation uses i64 where native uses int32. Every product is at most 15*127 and there are 32 of them,
so the value cannot approach the int32 limit; the wider type cannot change a result.

## Gate

    30000 checks, 0 failures

30000 random cases with `n_i` in {0,1,2} (the native bound is 2), random packed `v` words, `u` built from
signed bytes in [-127,127], random scale/min bytes and half-range floats. Compared against a direct C++
transcription of the native body including its own `dp4a`, using an **absolute** tolerance
`1e-3*(1+|ref|)` because the two sides are float arithmetic in the same order and a pure relative metric
produces false failures near zero.

Mutations, all caught hard:

| mutation | failures |
|---|---:|
| sign extension dropped in `byte_s` | 19945 |
| nibble mask `0x0F0F0F0F` → `0x0F0F0F0E` | 16529 |
| final `dm4x*sumf_d - dm4y*sumf_m` sign flipped | 11893 |

## Status: not wired into the CUDA path yet

The module is gated and compiles, but it is **not called from `vecdotq.cuh`**. Wiring it means either
replacing the device function with a call into Goose-generated code inside the same translation unit — the
approach EXP188/EXP190 used for other kernels — or moving the tiling loop too.

I am stopping at the gate because the shape of the next step is a decision I should not take unilaterally:
calling this per element from inside the MMQ tile loop would put a boundary at a very hot site, and
EXP223 already showed that instruction-count changes there do not translate into time. The honest next
move is to wire it and measure with the kernel harness, which is the only instrument this machine has.

## Why this one was worth doing

Unlike the last several ports, this is not a helper: it is the arithmetic of the block that dominates the
census. Even unwired, having it gated means the wiring step is a mechanical change rather than a port.
