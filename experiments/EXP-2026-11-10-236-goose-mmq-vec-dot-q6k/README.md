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

---

# The Q6_K port targets a path this GPU never takes

The copy-the-directory mechanism worked (`cubin ae8665ae…` versus stock `4c280687…`), and then the
comparison answered the question in an unexpected direction: **all 32 `mul_mat_q` kernels are
instruction-for-instruction identical to stock.**

## Why

    mmq.cuh:195  constexpr __device__ bool use_mma_data_layout() {
                     #if defined(AMD_MFMA_AVAILABLE) || defined(AMD_WMMA_AVAILABLE) || defined(TURING_MMA_AVAILABLE)
                         return true;
    #ifdef GGML_CUDA_MMA ...

`ggml_cuda_mmq_get_util_funcs` only reaches the `dp4a` branch when `use_mma_data_layout()` is **false**. On
sm_89 `TURING_MMA_AVAILABLE` is defined, so it is always **true**, and Q6_K uses
`ggml_cuda_mmq_vec_dot_q6_K_q8_1_mma` — the `IMMA.16816.S8.S8` path, 512 of those instructions in the
kernel. `vec_dot_q6_K_q8_1_impl_mmq` is never called, so patching it changes nothing, which is exactly what
the 0-difference function table shows.

## What that means

- **EXP236's port is correct and irrelevant to Apex on this GPU.** It is bit-exact and at parity on the
  harness (that measurement stands — it called the function directly), but the production kernel does not
  use that function at all.
- The Q6_K share of the census (9.7%) is **MMA code**, not `dp4a` code. Porting the `dp4a` variant of any
  `K`-quantised type on this GPU cannot affect it.
- The same reasoning applies to my EXP235 Q4_K inner product: Q4_K's `dp4a` path is also skipped on sm_89.
  Its parity measurement is real, but wiring it would be dead code here too.

## The larger lesson

This is the third time in this session that a port was correct and the question was whether it is
*reached*: EXP218 (`unicode_tolower`, 0 calls), EXP230/231 (shift paths, 0 calls), and now two MMQ kernels
that are bypassed by a compile-time branch rather than by a runtime condition. A census that attributes
time to `mul_mat_q` says nothing about *which template instantiation* of it runs.

Before porting any MMQ-family inner function, the first question is now `use_mma_data_layout()`: if the
target has MMA, the `dp4a` code is not what executes, and the MMA path is a different port entirely
(`IMMA`-based, with its own tile layout).

## State

EXP235 and EXP236 stand as measured, gated, bit-exact, **unreachable** implementations. They are kept
because the harness work is real and a non-MMA target would use them, but no wiring attempt should continue
on this machine.
