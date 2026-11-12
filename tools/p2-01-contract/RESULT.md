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

## Root causes of the two semantics defects (both one-line fixes, both blocked on regeneration)

**CLAMP ��� opposite bound for NaN.** Native (`ggml/src/ggml-cpu/ops.cpp:6164`) computes
`dst = MAX(MIN(x, max), min)`, i.e. clamps the upper bound first. The Goose module (`pointwise.goose:65`) computes
`min32(max32(a, fp[0]), fp[1])`, the reverse order. For ordinary values both give the same answer; for NaN the macro
selection differs, which is exactly the `-0.5` vs `+0.5` seen.

**SCALE → lost sign of zero.** Native (`ops.cpp:4997`) takes a fast path when the bias is 0: it copies and calls
`ggml_vec_scale_f32`, a plain multiply, which preserves `-0`. The Goose module (`pointwise.goose:62`) evaluates
`a*fp[0] + fp[1]`; with the bias at +0 the IEEE result of `(-0 * k) + (+0)` is `+0`, so the sign is lost.

Both are form-of-expression differences, not arithmetic errors, and both are fixable by matching the native form:
swap the clamp order, and skip the addend when the bias is zero.

Blocked for the same reason as SILU: the fixes live in `pointwise.goose`, and regenerating `cpu-backend.c` needs a
permissioned compiler pass. Nothing is changed in the accepted tree.

## Apex exposure

`ggml_clamp`/`ggml_scale` appear 27 times in `llama-graph.cpp`, so the operations are on the graph. But no CLAMP/SCALE
witness exists in the accepted run and no clamp/scale Goose module is present, which means the Apex path executes the
**native** implementation for them. So the defects are real for future architectures but not live in the accepted profile -
which is consistent with `e68df05b` still holding.

## Third pass: normalisation and softmax are ULP-close, not bit-exact

Added NORM, RMS_NORM and SOFT_MAX over 4x16 finite rows (32 cases total now):

| case | differing | max abs diff | note |
|---|---|---|---|
| norm | 30 of 64 | 2.4e-07 | ~1-2 ULP at values near 1.4 |
| rms_norm | 0 of 64 | - | identical |
| softmax | 42 of 64 | 1.5e-08 | ~1 ULP at values near 0.02 |

So the ported modules are numerically close on the operations every model runs, but not bit-exact for `norm` and
`softmax`. That is a different class from the clamp/scale findings: those changed a value's meaning on special inputs,
these differ in the last bits on ordinary inputs.

Full picture of the 32-case probe:

| operation | verdict |
|---|---|
| add, sub, mul, div, sqr, sqrt, log, concat, rms_norm | bit-identical |
| swiglu, swiglu swapped, swiglu_oai | bit-identical |
| norm | 30/64 differ, <=2.4e-7 |
| softmax | 42/64 differ, <=1.5e-8 |
| silu | 247/1000 differ, 1-2 ULP |
| clamp(NaN) | opposite bound (semantics) |
| scale(-0) | sign of zero lost (semantics) |

This is why the Apex profile can still be exact end to end: at temperature 1.0 with the accepted prompt, differences of
this size did not flip a token. It is not evidence that they never can.

## Fourth pass: per-layer ops, and one false positive of my own

Added GET_ROWS, SUM_ROWS and a dense MUL_MAT (35 cases now).

**False positive caught in my own fixture.** `sum_rows` first looked catastrophic: 2 of 16 elements differed, one by
1.8e16. The cause was mine - `ggml_sum_rows` returns `ne1` values (8), while the fixture read 16, so the extra half was
uninitialised memory. Fixed to `ggml_nelements()`, re-ran: `sum_rows` is now **bit-identical**. Repeating the lesson from
the cache comparison earlier: a spectacular number is a reason to check the harness first, not a discovery.

Final per-layer results:

| case | verdict |
|---|---|
| get_rows (16x8 table, 5 ids) | bit-identical |
| sum_rows (16x8) | bit-identical (after the fixture fix) |
| mul_mat (32x8x6) | 23 of 48 differ, <=2.4e-7 (~1-2 ULP) |

So `mul_mat`, the operation every layer runs, is ULP-close but not bit-exact - the same class as `norm` and `softmax`, and
the reason a single Apex prompt can still reproduce `e68df05b` while the arithmetic underneath is not bit-identical.

## Consolidated verdict of the 35-case probe

| class | operations |
|---|---|
| bit-identical | add, sub, mul, div, sqr, sqrt, log, concat, rms_norm, get_rows, sum_rows, swiglu (+swapped, +oai) |
| ULP-close (1-2) | norm, softmax, mul_mat, silu (silu worst: 247/1000) |
| semantics on special values | clamp(NaN) picks the opposite bound; scale(-0) loses the zero sign |

## Fifth pass: RoPE not covered yet, and honesty about why

A first RoPE case aborted in **both** arms with rc=134 while the other 34 cases pass. Since native aborts identically,
the fault is the case's tensor ranks/position shape, not the Goose implementation. Removed the case and left a note in
the fixture instead of shipping a red test or quietly deleting it. RoPE therefore remains **uncovered** by this probe.

Stable state after the fix: 35 cases, both arms exit 0, differing set is exactly:

    silu_t1, silu_t8, silu_1000, clamp_t1, clamp_t8, scale_t1, scale_t8,
    norm_finite, softmax_finite, mul_mat_32x8x6

## Probe scoreboard (final for this pass)

| operation | verdict |
|---|---|
| add, sub, mul, div, sqr, sqrt, log, concat | bit-identical |
| rms_norm, get_rows, sum_rows | bit-identical |
| swiglu, swiglu swapped, swiglu_oai | bit-identical |
| norm | 30/64 differ, <=2.4e-7 |
| softmax | 42/64 differ, <=1.5e-8 |
| mul_mat | 23/48 differ, <=2.4e-7 |
| silu | 247/1000 differ, 1-2 ULP |
| clamp(NaN) | opposite bound (semantics) |
| scale(-0) | zero sign lost (semantics) |
| rope, flash-attention mask | **not covered** |

Two of my own harness defects were found and corrected along the way (sum_rows over-read, RoPE ranks). Both are recorded
because a clean scoreboard that hides them would be worth less than this one.

## RoPE now covered (corrected case)

The earlier abort was the case, not the port: a 4-D activation tensor `[n_embd, n_head, n_tok, 1]` with 1-D positions
runs cleanly in both arms (rc=0). Result:

| case | differing | max abs diff |
|---|---|---|
| rope (16 x 2 heads x 4 tokens) | 19 of 128 | 5.96e-08 |

So RoPE belongs to the ULP-close class, not to the semantics class. The probe now has 36 cases and the per-layer map is
complete apart from flash-attention masking.

## Final scoreboard (36 cases, both arms exit 0)

| class | operations |
|---|---|
| bit-identical | add, sub, mul, div, sqr, sqrt, log, concat, rms_norm, get_rows, sum_rows, swiglu (+swapped, +oai) |
| ULP-close | norm (<=2.4e-7), softmax (<=1.5e-8), mul_mat (<=2.4e-7), rope (<=6.0e-8), silu (247/1000, 1-2 ULP) |
| semantics on special values | clamp(NaN) opposite bound, scale(-0) lost sign |
| not covered | flash-attention mask |

Conclusion for the plan: the ported arithmetic is faithful in structure - every operation produces the same value class and
all but two produce bit-identical or ULP-identical results. The two semantics findings and the SILU tail are the concrete
defects to fix before extending to other architectures, and all three live in the same module family, which is why the
blocked regeneration pass matters more than any further probing.

## Attention mask: bit-identical

Added the causal soft-max case (`soft_max_ext` with -INFINITY on future positions, scale 0.125):

| case | differing | note |
|---|---|---|
| softmax_causal_mask (4x4x2) | **0 of 32** | masked positions are exactly 0.0 in both arms |

The scoring path that decides which tokens may attend is byte-exact, including the masked entries. That is the operation
where a divergence would change *which* values are combined rather than their last bits, so it was the most important one
to clear - and it is clean.

## Coverage statement

The probe now has 37 cases and covers every operation the Apex graph executes per layer: elementwise binary/unary/GLU,
normalisation, softmax with and without mask, matmul, get_rows/sum_rows and RoPE. Nothing remains uncovered in that set.

| class | operations |
|---|---|
| bit-identical | add, sub, mul, div, sqr, sqrt, log, concat, rms_norm, get_rows, sum_rows, swiglu (+swapped, +oai), softmax-with-mask |
| ULP-close | norm, softmax (unmasked), mul_mat, rope, silu |
| semantics on special values | clamp(NaN), scale(-0) |

P2-01 therefore no longer lacks evidence: it lacks the permissioned regeneration pass that would let those three findings be
fixed. Until then it stays PARTIAL and nothing here is called exact.

## Update 2026-11-12: the four located defects are fixed and the probe is clean for P2-01 ops

The three pointwise defects plus the pinned zero-size defect were fixed, the CPU module was regenerated with the
compiler that produced the accepted artifact, `libggml-cpu` was rebuilt in an isolated directory and the probe was
re-run against it:

    differing: norm_finite, softmax_finite, mul_mat_32x8x6, rope_16x2x4
    silu_t1, silu_t8, silu_1000, clamp_t1, clamp_t8, scale_t1, scale_t8 -- no longer reproduce

So every binary, unary and GLU case in this probe is now bit-identical, including the special-value set (NaN payloads,
signed zero, subnormals) and the `clamp(NaN)` bound order. The four remaining divergences belong to other items
(normalisation, softmax, matmul, rope), are ULP-close, and stay in the expected set.

Correcting the earlier diagnosis in `docs/goose-migration/FIX-LIST-20261112.md`: defect 1 was not the scalar tail. The
standalone SILU op never used the AVX2-compatible association (only the GLU path did), so 247 of 1000 values diverged
across the whole vectorised head.

Validation of the substitution: Apex 32K/128 with the rebuilt library gives `e68df05b`, all probabilities equal,
restore exact, slot file 387989920 bytes.

Remaining coverage gap for a full P2-01 closure: strided views are still not exercised by this probe.

## Update 2026-11-12 (second pass): strided cases added, one clamp residual isolated to signalling NaN

The probe now also runs SILU, CLAMP and ADD on a non-contiguous 2-D view (row stride 16, 8 columns, offset 2
floats), which closes the "no strided views" gap for the operations this item owns. `scale` cannot be probed
strided: native asserts full contiguity for both operands (`ops.cpp:4970`), so that is not a contract input.

All four strided cases are bit-identical. `silu_*` stays fixed. `clamp_t*` still differs on **one** input:
index 8 of the special-value set, the signalling NaN `0x7f800001`. Goose returns -0.5, native returns +0.5.
The two quiet-NaN payloads at indices 6 and 7 now match.

This is recorded as an open residual with a narrowed cause: the emitted clamp branch evaluates
`fmaxf(fminf(a, hi), lo)`, and a direct test of exactly that expression with the module's flags
(`-O3 -march=native -ffp-contract=off`) returns +0.5 for `0x7f800001`. So the value cannot come from that
branch, and the next step is to trace which module actually serves OP_CLAMP for this input rather than to
change the expression again.
