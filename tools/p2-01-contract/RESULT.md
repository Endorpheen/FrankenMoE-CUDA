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

## Blocker for the SILU fix: no regeneration script in the tree

Checked before touching anything:

- `pointwise.goose` holds the fix point (`silu()` falls back to `expf`; `silu_compat` uses `avx2_exp_compat`),
- the experiment ships only generated artefacts (`generated/cpu-backend.c`, `cpu-control.c`, ...), and `cpu-backend.c`
  contains no `silu` symbol at all - the activation is inlined as `expf` expressions (e.g. line 3779),
- no generator script for this module exists under the experiment (`fg143_generate.py` is not present; earlier listing
  shows only fixtures, contract scripts and the generated outputs).

So the one-expression fix cannot be produced locally without re-running the Goose compiler to regenerate
`cpu-backend.c`. The compiler binary (v61) is available, but regenerating and rebuilding `libggml-cpu` touches the accepted
Apex path, and per the standing rules the compiler agent is not to be invoked without Igor's permission.

Recorded as a blocker with two exits:
1. permission to run a regenerate-and-rebuild pass (isolated copy first, probe after), then P2-01 can reach 19/19;
2. otherwise record the explicit contract "SILU scalar tail may differ by 1 ULP from native; the wide 8-lane path is
   byte-exact" and accept that as the compatibility contract the criterion allows.

Until one of these is chosen, P2-01 stays PARTIAL and no claim of exactness is made for SILU.

## Correction: the SILU divergence is wider than the first probe suggested

The first probe used 12 special values and found one differing element, which understated the problem. With the probe
extended to 1000 ordinary values:

| case | differing elements |
|---|---|
| silu, 12 special values, 1 thread | 1 of 12 (1 ULP) |
| silu, 12 special values, 8 threads | 1 of 12 (1 ULP) |
| **silu, 1000 values, 8 threads** | **247 of 1000**, differences of 1-2 ULP |

Examples from the 1000-value run: `0xb4b89ace` vs `0xb4b89acd`, `0xb583496c` vs `0xb583496e` (two steps).

So SILU does not match native on roughly a quarter of ordinary inputs, not on a single edge value. Everything else in the
extended probe is exact:

| case | verdict |
|---|---|
| add, mul at 1000 elements | identical |
| sqrt at 1000 elements | identical |
| sqr through a strided 2-D view | identical |
| swiglu, swapped, oai | identical |

That closes the "stride coverage" gap I flagged earlier and narrows the open problem to one activation. It also means the
1-ULP-contract option is weaker than it looked: a 25% hit rate is not a rare tail case, and calling it an accepted
compatibility contract would need that number stated explicitly.

## Scope of the SILU divergence: which models are actually exposed

Traced the graph builder (`llama.cpp-flint-cuda/src/llama-graph.cpp`, `build_ffn`):

- `LLM_FFN_SILU` **with** a parallel gate -> `ggml_swiglu_split` / `ggml_swiglu`, i.e. the GLU path, which this probe shows
  as **byte-identical**;
- `LLM_FFN_SILU` **without** a gate -> `ggml_silu` directly, which is the divergent path.

`qwen35moe` (Apex) declares `LLM_FFN_SILU` together with `LLM_FFN_PAR` (`models/qwen35moe.cpp:500,517,681,697`), so it
takes the GLU path. That is why the accepted profile still produces `e68df05b` with equal probabilities despite the SILU
difference - the divergent function is not on the Apex path.

Consequence, stated plainly:
- Apex is unaffected, and this is a structural reason, not luck.
- Any architecture that uses a bare SiLU FFN without a gate *is* exposed to the 247/1000 divergence.
- P2-01 therefore remains PARTIAL and is now also a prerequisite for those architectures, which is exactly the
  "readiness for other models" priority rather than an Apex detail.

## Second pass: two semantics defects, not ULP noise

Extended the probe to CLAMP, SCALE and CONCAT (29 cases total). New findings:

| case | Goose bytes | native bytes | meaning |
|---|---|---|---|
| clamp NaN | `000000bf` (-0.5) | `0000003f` (+0.5) | clamp picks the **opposite bound** for NaN |
| clamp NaN (2nd payload) | `000000bf` | `0000003f` | same |
| scale(-0) | `00000000` (+0) | `00000080` (-0) | **signed zero is lost** |

Both differ at both 1 and 8 threads. CONCAT is exact.

These are not rounding differences:
- the NaN cases return a different constant with a different sign, which is a semantic divergence in the clamp selection;
- `scale` loses the sign of zero, which breaks the IEEE-754 signed-zero contract that EXP-143's own note claims to have
  fixed "for the clamp/OAI selection" - it evidently does not cover the scale path.

## Why this matters beyond P2-01

Signed zero and NaN handling are exactly the "special-value contract" the acceptance criterion names. A model that
propagates a NaN through a clamp can see it become a finite bound value instead of staying NaN, i.e. an error silently
turns into a number. That is a correctness issue worth recording even though no observed Apex run hits it.

## Reproduce

Same two binaries as above; `clamp_t1`, `clamp_t8`, `scale_t1`, `scale_t8` are the failing case names in the raw stream.
