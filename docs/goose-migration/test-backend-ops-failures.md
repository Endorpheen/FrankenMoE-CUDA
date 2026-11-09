# test-backend-ops against the active runtime: current failure map

Run: `test-backend-ops test -b CUDA0`, runtime `runtime/exp196/run`, 2026-11-08.

Result after the GET_ROWS and RoPE fixes: the suite completes instead of aborting.

    18751 cases OK, 13 FAIL, 0 aborts

Before those two fixes it aborted at GET_ROWS after 1115 cases, and RoPE aborted before its
first case.

## Attribution

Each cluster was attributed by disabling exactly one module and rerunning that operator, same
binary and same runtime:

| operator | failures | disabling this module clears them | so the defect is in |
|---|---:|---|---|
| `GET_ROWS` | 69 → **fixed** | `GGML_GOOSE_CUDA_GET_ROWS` | EXP176 (type gate, fixed) |
| `ROPE` | abort → **fixed** | `GGML_GOOSE_CUDA_ROPE` | EXP171 (`n_ctx_orig` gate, fixed) |
| `CPY` | 4 → **fixed** | `GGML_GOOSE_CUDA_CPY` | EXP177 (type gate, fixed) |
| `MUL_MAT` | 11 | `GGML_GOOSE_CUDA_MMVF` and, separately, `GGML_GOOSE_CUDA_MMVQ_Q4K` | EXP184 / EXP185 |
| `SILU_BACK` | 1 | `GGML_GOOSE_CUDA_ACTIVATIONS` | EXP166 |

## What the remaining failures have in common

All three are configurations outside the Apex profile, and each is the same class of gap that
GET_ROWS and RoPE had — a body that models one shape and is wrong for another rather than
declining to the native branch:

- `MUL_MAT` batched/permuted variants and `SILU_BACK`.
- `MUL_MAT` fails only on the batched and permuted variants (`bs=[3,2]`, `nr=[2,2]`,
  `per=[0,1,2,3]`); the plain `bs=[1,1]` shapes that Apex uses are fine.
- `SILU_BACK` is not a kernel Apex executes at all.

## Next steps, in order

1. `MUL_MAT` batched/permuted matvec — the larger of the two, and it affects two modules.
2. `SILU_BACK` — lowest priority; Apex never runs it, but it is the same defect class.

Each needs the same procedure already used twice: bound the Goose path by a predicate matching
what the body models, rebuild that object, relink the stages that reference it by path, rerun
the operator, then the paired Apex protocol.

## Note

Every one of these gaps was invisible to the per-stage gates because those gates were written
against the Apex profile, which is deliberate — port by fact. `test-backend-ops` is what turns
"covered for the profile" into "covered for the operator", and it is the instrument P3-19
existed to provide.

## CPY: what the fix was, and a build trap worth recording

The element path gated on element **size** (`e0 != 4 || e1 != 4`), not on type. `f32` and `i32`
are both four bytes, so `f32 -> i32` took the raw word copy and never converted. The fix
declines when the types differ:

    if ti(s0,2,0) != ti(s1,2,0) { return false; }

The first rebuild did **not** take effect, and the reason matters beyond this stage:
`build_runtime.py` compiles the checked-in `source/host-generated.c`, so a `.goose` edit only
reaches the binary after `emit_sources.py` regenerates it. The object timestamps looked fresh
and the link line referenced them by path, which is exactly why the failure looked
unexplained. Running `emit_sources.py` before `build_runtime.py` turned it green.

The GET_ROWS and RoPE fixes were not affected: both were edits to C/C++ (`cuda-runtime.cu`,
`cuda-bridge.cuh`), not to a `.goose` source.

    test-backend-ops test -o CPY -b CUDA0   ->  2/2 backends passed
    full suite                              ->  18751 OK, 13 FAIL, no abort

Apex is unaffected, checked rather than assumed: `FG177C_CUDA_CPY host=0x4,paths=0x1`,
`FG171R host=0x1,modes=0x8,variants=0x50` and `FG176G host=0x3,paths=0x3` are all unchanged,
output is still `e68df05b`, and the run reads 2576.97 / 26.73 at load 2.07 — in line with the
native 2549.14 / 25.90 pair taken under the same conditions.

## MUL_MAT: what the 12 failures actually are

Exact shapes from the run, all with `n=1`:

| type_a | m | n | k | bs | nr | result |
|---|---:|---:|---:|---|---|---|
| f32 | 16 | 1 | 256 | 1x1 | 1x2 | error 0.95 |
| f32 | 16 | 1 | 256 | 3x2 | 1x2 | NaN |
| f32 | 16 | 1 | 256 | 3x2 | 2x2 | NaN |
| f32 | 16 | 1 | 4 | 1x1 | 1x2 | error 0.42 |
| f32 | 16 | 1 | 4 | 3x2 | 1x2 | error 1.31 |
| f32 | 16 | 1 | 4 | 3x2 | 2x2 | error 1.62 |
| q4_K | 16 | 1 | 256 | 1x1 | 1x2 | NaN |
| q4_K | 16 | 1 | 256 | 3x2 | 1x2 | NaN |
| q4_K | 16 | 1 | 256 | 3x2 | 2x2 | NaN |

Both modules decline-then-fall-back correctly when their switch is off, so the failures are in
what they compute, not in what they replace.

The important part for the next attempt: EXP184 already carries an eligibility predicate
(`cuda-runtime.cu`, `const bool eligible = ...`) that the Goose body consults, and these cases
pass it. So the fix is not "decline more shapes" — the predicate believes it models them and
does not. Reading the predicate against the table is where the next step starts.

Also worth noting: the plain `bs=1x1` case fails as well as the batched ones, so batching alone
is not the discriminator, and `n=1` is not either — the Apex profile runs `mul_mat_vec_f` four
times per pass successfully with much larger `k`.

The witness confirms the path rather than the dispatch: running `-o MUL_MAT` leaves

    FG184M_CUDA_MMVF   host=0x21,paths=0x1
    FG185Q_CUDA_MMVQ_Q4K host=0x21,paths=0x1

`paths=0x1` is the launched-route bit, so the Goose kernels did execute on these cases. That
settles the question this note exists to answer: it is a computation mismatch inside the body,
not a case that should have declined and did not.

No change was made here. The predicate currently admits these shapes, and narrowing it by
guesswork would either be wrong or would silently reduce coverage; the failure needs to be
traced inside the body first.
