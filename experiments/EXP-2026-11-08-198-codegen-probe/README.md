# EXP198: minimal measurement of the address-typing blocker

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

## What this does and does not show

It shows the mechanism is real and reproducible in a five-line kernel, and that the workaround
does not work. It does **not** give the size of the effect on a real kernel: here the gap is
+14%, while EXP180 and EXP186 measured roughly 2.5x on the full bodies, because those loop over
many blocks and the per-access cost compounds.

The one fix that would help is an emitter that emits typed `__restrict__` parameters. That is a
compiler change and is out of bounds for this work, which is why the blocker has stayed open.

## Reproducing

    cd source
    /usr/bin/nvcc -forward-unknown-to-host-compiler -ccbin=/usr/bin/g++-13 -O3 -std=c++17 \
      --generate-code=arch=compute_89,code=[compute_89,sm_89] -Xcompiler=-fPIC -c -o k2.o k2.cu
    cuobjdump -sass k2.o | awk '/Function : /{fn=$3} /\/\*[0-9a-f]{4}\*\//{c[fn]++} END{for(f in c) print f, c[f]}'
