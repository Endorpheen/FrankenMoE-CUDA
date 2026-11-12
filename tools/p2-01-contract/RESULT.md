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

## Cause of the SILU 1 ULP, located

- native: `ggml_vec_silu_f32` uses the AVX2 path `ggml_v_silu()`, i.e. the FMA polynomial + float-bit exponent
  construction (`ggml/src/ggml-cpu/vec.cpp:380`, `vec.h`),
- Goose: `fn silu(x:f32)->f32 { return x/((1 as f32)+exp32(-x)); }` with `exp32` bound to libm `expf`
  (`EXP-143/source/glu-rejected-contextual-u32.goose:8,89`).

So the difference is not a logic error and not a NaN problem: it is libm `expf` against the SIMD polynomial. One ULP at
x = -1 is the expected size of that gap; other inputs happen to agree.

## Repair route (reuse, not a rewrite)

EXP-143 already carries a byte-exact SIMD association for SWIGLU ("same FMA polynomial / float-bit exponent construction",
`glu-numerical-contract.json`, 9/9 wide cases). The same exponent construction can back SILU, since SILU is
`x * sigmoid(x)` and SWIGLU already computes exactly that sigmoid. That would make SILU byte-exact against native without
touching the native path or weakening any gate - it is a change inside the Goose module, followed by a rebuild and this
same probe.

Not done in this iteration: it is a Goose-source change plus regeneration, and it must not be rushed inside a measurement
pass. P2-01 stays PARTIAL with the cause and the route recorded.

## Refinement: the exact module already has a bit-exact path; the tail does not

`EXP-143/source/pointwise.goose`:

    fn silu_compat(p, x, col, width) {
        if p.ip[12]!=0 && p.x.kind==0 && p.d.kind==0 && col < (width/8)*8 {
            return x/(1 + avx2_exp_compat(-x));   // byte-exact SIMD association
        }
        return silu(x);                            // x/(1+expf(-x))  -> 1 ULP
    }

So the divergence is not an unknown: the bit-exact exponent construction is used for the wide part and the scalar tail
falls back to libm `expf`. That matches the EXP-143 contract, which claims 9/9 wide cases and says nothing about the tail.

Minimal repair, one expression in the Goose module: use `avx2_exp_compat` in the fallback too, i.e. make the tail take the
same association as the wide part, then regenerate and rerun this probe. No native change, no gate weakened.

Why it was not rushed here: regenerating the module needs the generator plus a rebuild, and this must be followed by the
same 19-case probe and an Apex regression. P2-01 stays PARTIAL with a precise, small, reusable fix identified.
