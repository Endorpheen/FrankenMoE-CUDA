# test-backend-ops against the active runtime: current failure map

Run: `test-backend-ops test -b CUDA0`, runtime `runtime/exp196/run`, 2026-11-08.

Result after the GET_ROWS and RoPE fixes: the suite completes instead of aborting.

    full suite: rc=0, 0 FAIL, 2/2 backends passed

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
| `MUL_MAT` | 9 → **fixed** | `GGML_GOOSE_CUDA_MMVF` and, separately, `GGML_GOOSE_CUDA_MMVQ_Q4K` | EXP184 / EXP185 (batch bound, fixed) |
| `SILU_BACK` | 1 → **fixed** | `GGML_GOOSE_CUDA_ACTIVATIONS` | EXP166 (routed to native, fixed) |

## What the remaining failures have in common

All three are configurations outside the Apex profile, and each is the same class of gap that
GET_ROWS and RoPE had — a body that models one shape and is wrong for another rather than
declining to the native branch:

- `MUL_MAT` batched/permuted variants and `SILU_BACK`.
- `MUL_MAT` fails only on the batched and permuted variants (`bs=[3,2]`, `nr=[2,2]`,
  `per=[0,1,2,3]`); the plain `bs=[1,1]` shapes that Apex uses are fine.
- `SILU_BACK` is not a kernel Apex executes at all.

## Next steps, in order

1. `MUL_MAT` batched/permuted matvec — the last cluster, and it affects two modules.

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

## SILU_BACK: the body was never written

`ggml_cuda_op_silu_back` routed straight into `fg166_run(34, ...)`, but no `silu` appears
anywhere in `device.goose`, `device-generated.c` or `host.goose` — the backward pass was never
implemented, yet the route claimed it. `test-backend-ops` measured a 1.57 error, and the live
witness confirmed the mode ran (`m6=0x1`).

The route now calls the native function unconditionally. Apex does not execute this mode at
all (`m6=0x0` in the profile log), so nothing on the hot path changes.

    test-backend-ops test -o SILU_BACK -b CUDA0   ->  2/2 backends passed
    full suite                                    ->  18753 OK, 11 FAIL, no abort

Apex is unchanged, checked: the activation witness reads
`m0=0x2000400,m2=0x2000480,m6=0x0,m7=0x1`, identical to before the change, with `m6=0x0`
confirming the mode is not on the profile. Output still `e68df05b`, 2572.84 / 27.01.

## MUL_MAT: the discriminator narrowed

`-o MUL_MAT` runs 2468 cases; exactly 9 fail. `nr` is documented in the harness as "repeat in
dims 3 and 4", so `nr=[*,2]` means the fourth dimension is 2, and every failing case has it:

    type_a=f32,  m=16, n=1, k=256, bs=[1,1], nr=[1,2]
    type_a=f32,  m=16, n=1, k=256, bs=[3,2], nr=[1,2]
    type_a=f32,  m=16, n=1, k=256, bs=[3,2], nr=[2,2]
    type_a=f32,  m=16, n=1, k=4,   bs=[1,1], nr=[1,2]
    type_a=f32,  m=16, n=1, k=4,   bs=[3,2], nr=[1,2]
    type_a=f32,  m=16, n=1, k=4,   bs=[3,2], nr=[2,2]
    type_a=q4_K, m=16, n=1, k=256, bs=[1,1], nr=[1,2]
    type_a=q4_K, m=16, n=1, k=256, bs=[3,2], nr=[1,2]
    type_a=q4_K, m=16, n=1, k=256, bs=[3,2], nr=[2,2]

`ne3 == 2` on its own is not the cause: hundreds of cases with that dimension pass. The
combination that fails is `n=1` together with a batched fourth dimension, and it fails for both
the f32 module and the q4_K one.

This is where the next attempt should start: the grid dimension the module derives for the
non-ids path (`p.v[4][2] = ne3`) interacting with a single destination column.

### Resolution taken

Both eligibility predicates now also require `ne3 == 1`, the shape the profile uses, and
everything else goes native. That took `-o MUL_MAT` to 0 failures and the full suite to
**rc=0, 0 failures, 2/2 backends passed**, with the Apex witnesses unchanged at `paths=0x1`.

It bounds the port; it does not explain the mismatch. The body's handling of `n=1` together
with a batched fourth dimension is still unexplained, and the `ne3 == 1` bound is what keeps
it off the results until it is.

## Where the MUL_MAT mechanism is, and where it is not

The bound is in place and the suite is green, so this section is only about what is known
versus assumed, for whoever picks it up.

Known from measurement:

- `n=2` with `ne3=2` passes; `n=1` with `ne3=1` passes; `n=1` with `ne3=2` fails. The
  interaction is the pair, not either value.
- The f32 module gives a relative error around 0.95-1.6; the q4_K module gives `NaN`, which is
  what an out-of-range read produces from quantized data rather than merely a wrong number.

Known from the code:

- Both modules already required `ne13 == ne3`, so the predicate admitted the shape; the body
  believed it modelled it.
- Launch geometry is computed in Goose: `put(p,7,0,nrows); put(p,7,1,nch); put(p,7,2,ntok)`
  with `ntok` from `dst->ne[3]`, i.e. 2 in the failing cases.
- The per-element body takes `sample_dst` and derives `sample_x` from it, with the non-ids
  branch handling channel derivation through `channel_ratio` (host.goose, the block starting
  "Without ids native derives the source channel").

Not established: which of those address terms is wrong. The `NaN` on the quantized module
points at a sample or channel offset that leaves the buffer, but that is a direction, not a
diagnosis, and it is recorded as such.

Next step that would settle it: a GPU fixture with `k=256, m=16, n=1, ne3=2` for both
modules, comparing each output element against the native kernel and separating sample 0 from
sample 1. If only sample 1 is wrong, the sample offset is the culprit and the channel
arithmetic is exonerated; if both are, the column handling for `ncols_dst = 1` is.
