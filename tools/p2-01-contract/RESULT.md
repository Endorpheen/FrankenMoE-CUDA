# P2-01 contract probe: first results (2026-11-12)

Method: one C fixture built twice — once against the accepted Goose `libggml-cpu` (326 `fg` symbols) and once against the
untouched flint build (0 `fg` symbols) — then the raw output bytes are compared. Special-value set: +0, -0, +1, -1, +inf,
-inf, quiet NaN, a second NaN payload, signalling NaN, smallest subnormal, largest negative subnormal, 0.1. Threads 1 and 8.
GLU cases use a 2-row tensor and are run for SWIGLU, SWIGLU swapped and SWIGLU_OAI.

Reproduce:

    gcc-13 -O2 -I<llama.cpp-flint>/ggml/include fixture.c -o p201_goose  -L<staging> -lggml-cpu -lggml-base -lm -lpthread
    gcc-13 -O2 -I<llama.cpp-flint>/ggml/include fixture.c -o p201_native -L<llama.cpp-flint>/build-cuda/bin -lggml-cpu -lggml-base -lm -lpthread
    LD_LIBRARY_PATH=<staging>  ./p201_goose  goose.bin
    LD_LIBRARY_PATH=<flint/bin> ./p201_native native.bin
    cmp goose.bin native.bin

## Result: 17 of 19 cases byte-identical

| case | verdict |
|---|---|
| add, sub, mul, div (threads 1 and 8) | identical |
| sqr, sqrt, log (threads 1 and 8) | identical |
| swiglu, swiglu swapped, swiglu_oai | identical |
| silu (threads 1 and 8) | **1 ULP difference at x = -1** |

Exact bytes at x = -1: Goose `b1 b2 89 be` (-0.2689414322376251), native `b0 b2 89 be` (-0.26894140243530273).

So NaN payloads match (the criterion's NaN concern is satisfied for these operations), and the remaining difference is one
ULP of SILU at a single finite input, not a NaN or signed-zero problem.

## What this means for P2-01

- Not done yet: a 1 ULP difference is a real numerical divergence and the plan's rule is not to wave differences through.
- Two honest routes: (a) align the Goose SILU sigmoid/exp path with the native association so x = -1 matches bit for bit,
  or (b) record an explicit compatibility contract "SILU may differ by 1 ULP from native" and have it approved.
- Until one of those happens P2-01 stays PARTIAL; the probe itself is proof the other 17 cases are already exact.

## Coverage limits of this probe

Single-threaded and 8-thread runs only; element counts were small (12 and 24); no strided views yet (my first fixture
planned strides but the cases use contiguous 1-D tensors, so the stride dimension is still untested). These limits are
stated so the 17/19 result is not read as full-matrix evidence.
