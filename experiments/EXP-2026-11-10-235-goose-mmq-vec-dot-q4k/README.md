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

---

# Wired, exact on GPU, and +19.2% slower — the clearest measurement in this whole line

I wired the module the way EXP182/EXP188 do it: the Goose emission is compiled **inside the same
translation unit** as a `__device__ __forceinline__` function, so the "boundary" is a C++ call that nvcc
inlines. No launch, no copy, no table. The file is `source/mmq-vd.cu`.

## Exactness on the real GPU

`fg235_check` computes the native function and the Goose function side by side on the same 65,536 inputs
and compares:

    FG235_KERNEL_MATCH n=65536 bad=0 worst_rel=0.000e+00

**Zero difference, not a tolerance pass.** `dp4a` is integer, the scales are the same floats in the same
order, so the two are bit-identical. The host gate (30000 checks) compares against a C++ transcription
with an absolute tolerance; this compares against the actual native device function and needs none.

## Timing — 12 runs, native and goose interleaved in both orders

    native median 174.5 ns   (min 174.0, max 176.0 across runs)
    goose  median 208.0 ns   (identical in every run)
    delta  +19.20%

This is the cleanest measurement I have produced: the native medians span 2 ns and the goose median is the
same value six times. **No load noise is visible at all**, because the two kernels differ only in the code
between two `cudaEventRecord`s in one process.

## What it means for the whole MMQ line

The port emits a function that is **bit-exact and 19% slower**, and this is with the boundary fully
inlined. That removes the last structural explanation I had been holding in reserve: it is not a launch
cost, not a copy, not a switch. The emission itself is slower code for the same arithmetic.

Combined with EXP223 — where a 23% instruction reduction bought no time — the picture is consistent:
the Goose emission is not optimising the same way nvcc optimises the hand-written CUDA, and the
difference shows up as ~10-20% on this kernel regardless of how it is called.

## Consequence

`vec_dot_q4_K_q8_1_impl_mmq` is **ready to promote on correctness** and **not promotable on speed**. It
stays unwired in the shipped path, and the module remains gated so that any future codegen work can be
measured against it with a one-line arm switch.

The honest summary of the 30% block: the arithmetic is now ported and proven bit-exact, and the blocker
has moved from "not ported" to "the compiler does not yet emit code competitive with hand-written CUDA
for this kernel" — which is a compiler question, not a migration one, and I have said so in the mailbox.

## Where the 19% comes from: no `IDP.4A`

SASS counts for the two kernels in the same binary, same arithmetic, same inlining:

| | instructions | dot-product opcode |
|---|---:|---|
| native | 133 | **16× `IDP.4A.S8.S8`** |
| goose | 741 | **0** |

The Goose kernel reconstructs each of the 32 byte products with `LOP3.LUT` (128), `SHF.R.U64` (96),
`IMAD.WIDE.U32` (64), `ISETP.GT.U32.AND.EX` (64) and `SEL` (64) — masks, shifts and selects standing in
for one hardware instruction. Native spends 16 `IDP.4A` for the same work.

`IDP.4A` is the four-way byte dot product: exactly what my source expresses as a loop of four
sign-extended products. nvcc recognises that pattern and folds it; the emission does not.

This is the first time I can attribute a port's slowdown to something specific and reproducible rather
than to a static count I later learn to distrust. EXP222's instruction surplus was my own dead branch;
EXP223 showed counts alone predict nothing. Here the difference is 5.6× concentrated in the inner
arithmetic, with a matching 19% time delta and no measurable noise.

Filed with the compiler owner as a defect report with the reproduction, the SASS evidence and two
possible fixes: fold the pattern, or expose the intrinsic as a declarable primitive (smaller, and it would
let me handle it on this side).

---

# The fix is on my side: declare `dp4a` as a primitive

I did not wait for the compiler. The emission already supports declaring an extern primitive, so the
question was whether `dp4a` can be one. It can, and it closes the gap completely.

## What changed

One function body in the Goose source. Instead of

    fn dp4a(a,b,acc) { loop of four sign-extended byte products }

declare it:

    extern "fg235v_dp4a" fn dp4a(a:i64, b:i64, acc:i64) -> i64;

and provide `__dp4a` in the `.cu` beside the other device services. Everything else in the module is
unchanged.

## Result — identical code, identical time

| | instructions | `IDP.4A` | median ns |
|---|---:|---:|---:|
| native | 133 | 16 | 173.8 |
| Goose, loop version | 741 | 0 | 208.0 (**+19.2%**) |
| **Goose, intrinsic version** | **133** | **16** | **174.2 (+0.29%)** |

Six interleaved runs each way:

    native  172.5 173.5 174.0 174.0 175.0 173.5   median 173.8
    goose   174.5 174.0 174.0 175.5 173.5 176.0   median 174.2

`+0.29%` median, `+0.48%` mean — inside noise, and the two arms produce **byte-identical SASS instruction
counts and the same 16 `IDP.4A`**, so this is not a lucky measurement: the compiler emitted the same code.

Bit-exactness re-verified on the GPU: `n=65536 bad=0 worst_rel=0.000e+00`.

## What this teaches about the migration

The 19% was never a compiler defect in the sense of bad codegen from a correct description. It was a
**description problem**: I wrote the arithmetic out longhand, and the emission faithfully produced
longhand code. Declaring the hardware operation as a primitive is the right expression of intent, exactly
as the modules already do for loads, stores and half conversions.

So my defect report to the compiler owner needs a correction, and I will send it: the foldable-pattern
request is the *weaker* option. The primitive route works today and needs nothing from them.

## Where this leaves the 30% block

`vec_dot_q4_K_q8_1_impl_mmq` is now bit-exact **and** at parity at kernel level. That is the first kernel
in the MMQ family where the Goose version does not lose. It is still not wired into the shipping path —
that needs the outer tiling loop and a full Apex measurement — but the reason it could not be wired is
gone.
