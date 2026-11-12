# Address typing: the measured cause of the matmul gap

This is the compiler-feedback version of `experiments/EXP-2026-11-08-198-codegen-probe/README.md`,
kept in a tracked document because `COMPILER_FEEDBACK.md` at the repo root is locally excluded
(`.git/info/exclude`) and so cannot be committed. The same text was added there for the compiler
owner.

Everything in these documents says the remaining 40% of GPU time is blocked because the emitter
passes addresses as `int64_t`, so nvcc cannot use the read-only path or fold offsets. That claim
came from profiling real kernels. This is the smallest controlled test of the mechanism itself,
so the claim rests on something reproducible outside the assembly of two specific kernels.

## Method

Three variants of the same reduction — eight floats starting at a runtime-computed base, with
two stride multiplications — compiled with the project's exact nvcc flags
(`-ccbin=/usr/bin/g++-13 -O3 -std=c++17 --generate-code=arch=compute_89`):

1. `typed` — `const float * __restrict__`, indexed as `p[i0 + k]`.
2. `integer` — base passed as `uint64_t`, address rebuilt per access as
   `*(const float *)((const char *)(uintptr_t)p + off)`, which is what the emitter produces.
3. `ldg` — same integer base, but the read goes through `__ldg`, the obvious workaround.

## Result

    typed     56 instructions, 4 x LDG.E.CONSTANT
    integer   64 instructions, 4 x LD.E
    ldg       64 instructions, 4 x LDG.E.CONSTANT

The mechanism is exactly as described: the typed pointer yields the read-only load path, the
integer base yields the ordinary one, and the extra instructions are the address materialisation
(`ULDC.64` and `UIMAD` appear in `integer` and not in `typed`).

`__ldg` **does** restore the read-only path — but it does not reduce the instruction count at
all, because the base is still an integer and the offsets still have to be materialised. That is
an independent confirmation of the EXP186 note that `__ldg` made it slower rather than better
(504 → 552 there): it buys the cache path and pays the arithmetic.

## Two workarounds tried inside our own wrapper, both failed

The emitter is not something this work may change, but the CUDA wrapper is ours. So the obvious
next question is whether the wrapper can recover the typed path. Two attempts, both compiled and
measured:

1. **Locally restrict-qualified cast at the load.**

       const float * __restrict__ q = (const float * __restrict__)(const char *)(uintptr_t)p;
       return q[o/4];

   64 instructions, **0** `LDG.E.CONSTANT`. A restrict qualifier on a cast from an integer proves
   nothing — the compiler cannot know the pointee is unaliased, because an integer has no
   provenance.

2. **Typed `__restrict__` kernel parameter, converted once at entry.**

       __global__ void k(const float * __restrict__ a, ...) {
           const uint64_t base = (uint64_t)(uintptr_t)a;

   64 instructions, **0** `LDG.E.CONSTANT`. Same reason, one step later: the moment the pointer
   becomes an integer the provenance is gone, and it is gone for every subsequent load.

## What this does and does not show

It shows the mechanism is real and reproducible in a five-line kernel, and that the workaround
does not work. It does **not** give the size of the effect on a real kernel: here the gap is
+14%, while EXP180 and EXP186 measured roughly 2.5x on the full bodies, because those loop over
many blocks and the per-access cost compounds.

The one fix that would help is an emitter that emits typed `__restrict__` parameters **and** keeps
the addressing in pointer arithmetic rather than integers. That is a compiler change and is out of
bounds for this work, which is why the blocker has stayed open.

That conclusion is now tested rather than argued: four variants, two of them attempts to recover
the typed path from inside the wrapper, and only the fully typed one reaches
`LDG.E.CONSTANT` — 56 instructions against 64 for every integer-carrying variant.

## Reproducing

    cd source
    /usr/bin/nvcc -forward-unknown-to-host-compiler -ccbin=/usr/bin/g++-13 -O3 -std=c++17 \
      --generate-code=arch=compute_89,code=[compute_89,sm_89] -Xcompiler=-fPIC -c -o k2.o k2.cu
    cuobjdump -sass k2.o | awk '/Function : /{fn=$3} /\/\*[0-9a-f]{4}\*\//{c[fn]++} END{for(f in c) print f, c[f]}'

## Suggestion for the compiler side, in priority order

1. A typed-address mode in which a body can hold and index a pointer, so `p[i]` survives into the
   generated C rather than `base + i*sizeof(T)` cast from an integer. That is the only variant
   measured to reach `LDG.E.CONSTANT`.
2. Failing that, an emitter option that keeps a `__restrict__`-qualified pointer for the base and
   performs arithmetic as indices on it, even if the body's type remains integer at source level.

Nothing here asks for a compiler change to be made now, and no compiler source was touched. It
exists so that if the work is ever scheduled it starts from a five-line reproduction rather than
from two kernel dumps.
