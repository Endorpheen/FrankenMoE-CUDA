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

---

# Wiring attempt: patched header, different object, no SASS conclusion yet

I tried to wire this into the real build path: a copy of `vecdotq.cuh` in `integration/` with the Q6_K body
replaced by a call to the emission, compiled into `mmq-instance-q6_k.cu.o`.

## What worked

    rc 0 clean
    sha256 patched 802f5bce…   unpatched 9a8bc096…   stock build 6ba4392d…

The object **is** different from both my unpatched attempt and the stock build, so the override took
effect. Getting the include to resolve needed `-I` **before** the other flags: `mmq-vec-dot.cuh` includes
`"vecdotq.cuh"`, and a quoted include resolves in the includer's own directory first, so my copy was
otherwise ignored — the first two attempts silently built the stock header.

## What I could not conclude

`cuobjdump -sass` reports **0 `IDP.4A`** in the Q6_K `mul_mat_q` kernel in **both** the patched and the
stock object. So the hardware dot-product instruction is not what this template generates on sm_89 at all —
unlike the standalone `vec_dot_q4_K_q8_1_impl_mmq` harness of EXP235, where native emitted 16 of them.

Two readings, and I am not going to pick one without evidence:

1. this kernel's code path on this GPU shape does not use dp4a — the `fallback` template axis may select a
   different implementation than the one I measured standalone;
2. my SASS extraction is missing the kernel body again (it has been unreliable twice in this session).

Either way I have **no performance claim** for Q6_K in-chain, and I stopped rather than report the
instruction counts as evidence.

## The tooling problem worth recording

Extracting per-function SASS from a CUDA object with 64 kernels and fatbin sections has defeated me three
times this session: `cuobjdump -sass` on the `.o` shows only the `stream_k_fixup` functions, `--list-elf`
extracts a cubin whose paths I then failed to keep, and my awk function-boundary parsing has produced
inconsistent totals. The measurements that *are* trustworthy all came from a purpose-built harness where I
compile a small `.cu` with `-cubin` and disassemble that — which is exactly how EXP235's `IDP.4A` numbers
were obtained.

So: wiring needs its own harness, not post-hoc disassembly of the production object. Recorded instead of
guessing.

---

# Why the override never bound: three failed mechanisms, each ruled out by a measurement

I tried three ways to route the Q6_K call site into the emission. All three produced **byte-identical
cubins** (`sha256 4c280687e0a01b1a`, 4096 instructions in the kernel), and each failure has a distinct,
verified cause.

## 1. `-I` with a patched copy of `vecdotq.cuh` — never used

`mmq-vec-dot.cuh` does `#include "vecdotq.cuh"`. A quoted include resolves **in the includer's own
directory first**, then in `-I` paths. My copy sat in an `-I` directory, so the compiler always found the
original next to `mmq-vec-dot.cuh`. Putting `-I` first among the flags changes nothing; the includer's
directory is not in that ordering at all.

## 2. `-Dvec_dot_q6_K_q8_1_impl_mmq=..._native` — renames the call sites too

The macro applies to every occurrence, so the definition **and** the call in `mmq-vec-dot.cuh:1002` both
became `_native`. My replacement under the original name was therefore never called, and the resulting code
is exactly the native code. This is the subtle one: it looked like a successful rename, and the build was
clean.

## 3. `-include override.cuh` — works, but cannot fix either of the above

Verified that `-include` is honoured: forcing a header containing `#error FORCED` makes the build fail with
`FORCED seen: True`. So the mechanism is fine; the problem is what the header can express. It cannot
retroactively change a quoted include's resolution, and it cannot avoid the `-D` covering both sides.

## What would actually work

Copy the **whole** `ggml-cuda` directory into the experiment, patch the copy's `vecdotq.cuh`, and compile
from a source file inside the copy — then every quoted include resolves within the copy. That is heavy
(hundreds of files) but it is the only mechanism that does not depend on include-order tricks or macro
trickery, and it is what a promotion-time change would look like anyway.

Alternatively patch `mmq-vec-dot.cuh`'s single call site rather than the function definition, which needs
only one file copied *and* that file to be the includer — still the same directory problem.

## Recorded because

Three attempts, three clean builds, three byte-identical cubins. Without comparing hashes each time I would
have believed the override took effect — and the previous section's "the object hash differs" claim was
about a *different* build (an `-I` copy that did change something) while these do not. The lesson is the
same one this project keeps teaching: a clean build is not evidence that the change was used.
