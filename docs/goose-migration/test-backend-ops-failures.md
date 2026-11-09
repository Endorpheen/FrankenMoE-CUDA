# test-backend-ops against the active runtime: current failure map

Run: `test-backend-ops test -b CUDA0`, runtime `runtime/exp196/run`, 2026-11-08.

Result after the GET_ROWS and RoPE fixes: the suite completes instead of aborting.

    18748 cases OK, 16 FAIL, 0 aborts

Before those two fixes it aborted at GET_ROWS after 1115 cases, and RoPE aborted before its
first case.

## Attribution

Each cluster was attributed by disabling exactly one module and rerunning that operator, same
binary and same runtime:

| operator | failures | disabling this module clears them | so the defect is in |
|---|---:|---|---|
| `GET_ROWS` | 69 → **fixed** | `GGML_GOOSE_CUDA_GET_ROWS` | EXP176 (type gate, fixed) |
| `ROPE` | abort → **fixed** | `GGML_GOOSE_CUDA_ROPE` | EXP171 (`n_ctx_orig` gate, fixed) |
| `CPY` | 4 | `GGML_GOOSE_CUDA_CPY` | EXP177 |
| `MUL_MAT` | 11 | `GGML_GOOSE_CUDA_MMVF` and, separately, `GGML_GOOSE_CUDA_MMVQ_Q4K` | EXP184 / EXP185 |
| `SILU_BACK` | 1 | `GGML_GOOSE_CUDA_ACTIVATIONS` | EXP166 |

## What the remaining failures have in common

All three are configurations outside the Apex profile, and each is the same class of gap that
GET_ROWS and RoPE had — a body that models one shape and is wrong for another rather than
declining to the native branch:

- `CPY(type_src=f32,type_dst=i32)` and the reverse report errors of order 1.0 — the row is
  copied without the numeric conversion.
- `MUL_MAT` fails only on the batched and permuted variants (`bs=[3,2]`, `nr=[2,2]`,
  `per=[0,1,2,3]`); the plain `bs=[1,1]` shapes that Apex uses are fine.
- `SILU_BACK` is not a kernel Apex executes at all.

## Next steps, in order

1. `CPY` type conversion — smallest and fully reproducible with `-o CPY`.
2. `MUL_MAT` batched/permuted matvec — the larger of the two, and it affects two modules.
3. `SILU_BACK` — lowest priority; Apex never runs it, but it is the same defect class.

Each needs the same procedure already used twice: bound the Goose path by a predicate matching
what the body models, rebuild that object, relink the stages that reference it by path, rerun
the operator, then the paired Apex protocol.

## Note

Every one of these gaps was invisible to the per-stage gates because those gates were written
against the Apex profile, which is deliberate — port by fact. `test-backend-ops` is what turns
"covered for the profile" into "covered for the operator", and it is the instrument P3-19
existed to provide.
