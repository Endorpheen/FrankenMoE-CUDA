# P2-01 inventory: binary, unary and GLU

Status: inventory step (not acceptance). Basis: `ggml.h` operation list against Goose sources under `experiments/*/source`.

## Explicit `GGML_OP_*` mentions in Goose sources (12 of 21)

ADD, ADD1, SUB, MUL, DIV, SQR, SQRT, LOG, SUM, CLAMP, SCALE, FILL.

## GLU / activation family: implemented under lowercase names

A first pass keyed on `GGML_OP_*` alone missed these, which would have overstated the gap:

| name | files mentioning it |
|---|---|
| silu | 61 |
| relu | 35 |
| swiglu | 31 |
| gelu | 29 |
| geglu | 23 |

Representative sources: `EXP-136-goose-runtime/source/ggml/src/ggml-cuda/unary-goose.cu`,
`EXP-138-goose-cpu-backend/source/cpu-moe/ggml-cpu-goose-moe.c`, `.../cpu-moe-tiled/ggml-cpu-goose-tiled.c`.

## Coverage of the whole op set

105 operations in `ggml.h`; 103 of them are mentioned by at least one Goose source. That is a name-level inventory, not a
correctness claim.

## What still blocks acceptance of P2-01

1. Per-operation, per-type, per-stride correctness evidence: the existing gates cover selected families (EXP138/EXP143,
   AVX2 SWIGLU 9/9 byte-exact), not every variant.
2. The NaN payload contract: the criterion names it explicitly and it is not yet closed. Current state is that NaN
   propagation differs from the native path in payload bits for at least some variants; that must either be fixed or
   documented as an explicit compatibility contract before the item can be called done.
3. Signed-zero behaviour is recorded as fixed for the clamp/OAI selection, but not systematically checked across the
   remaining unary variants.

Next: build a generated matrix (op x type x stride) with a bit-exact comparison against the native reference, plus a NaN
payload probe, and accept P2-01 only on that evidence.

## Measured map (2026-11-12): 37 contract cases, Goose vs untouched native build

Tool: `tools/p2-01-contract/fixture.c`, built twice (accepted `libggml-cpu` with 326 `fg` symbols, and the flint build with
0), raw bytes compared. Reproduction and byte evidence: `tools/p2-01-contract/RESULT.md`.

| class | operations | evidence |
|---|---|---|
| bit-identical | add, sub, mul, div, sqr, sqrt, log, concat, rms_norm, get_rows, sum_rows, swiglu (+swapped, +oai), softmax-with-causal-mask | 0 differing bytes |
| ULP-close | norm 30/64 (<=2.4e-7), softmax 42/64 (<=1.5e-8), mul_mat 23/48 (<=2.4e-7), rope 19/128 (<=6.0e-8), silu 247/1000 (1-2 ULP) | byte diffs |
| semantics on special values | clamp(NaN) returns the opposite bound (-0.5 vs +0.5); scale(-0) loses the zero sign | byte diffs at 1 and 8 threads |

Cause of each finding is located in the Goose source and in the native code:
- clamp: native `MAX(MIN(x,max),min)` vs Goose `min(max(a,lo),hi)` - reverse order, differs only for NaN;
- scale: native skips the addend when bias is 0 (plain multiply, keeps -0); Goose evaluates `a*k+0` and `(-0)+(+0)=+0`;
- silu: wide part uses the bit-exact `avx2_exp_compat`, the scalar tail falls back to libm `expf`.

All three fixes live in `pointwise.goose` and need a permissioned regenerate-and-rebuild pass; the compiler agent is not to
be invoked without Igor's permission. Two harness defects of my own were found and fixed on the way (sum_rows over-read,
RoPE ranks) and are recorded in RESULT.md rather than hidden.

Apex exposure: `qwen35moe` declares `LLM_FFN_SILU` with `LLM_FFN_PAR`, so it takes the bit-exact GLU path; `clamp/scale`
have no Goose module and execute natively there. That is why `e68df05b` holds while these findings stay open.
