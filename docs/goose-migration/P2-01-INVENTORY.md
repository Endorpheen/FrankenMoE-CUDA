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
