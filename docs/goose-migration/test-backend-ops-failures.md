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

### Fixture result: shape and padding do not reproduce it

The fixture now carries the failing shape and a padded layout that mirrors how the harness
builds its inputs (both sources are views into a base tensor twice as wide, so `nb[1]` and
`nb[2]` carry padding). The oracle walks the real strides and reports per sample.

Goose-arm relative error against the host oracle:

| case | sample 0 | sample 1 |
|---|---|---|
| k=256, m=16, n=1, ne3=2, contiguous | 6.1e-07 | 5.3e-06 |
| k=256, m=16, n=1, ne3=2, padded | 5.0e-05 | 4.2e-06 |
| k=256, m=4, n=1, ne3=2, contiguous | **1.4e-02** | 6.1e-08 |
| k=256, m=4, n=1, ne3=2, padded | 1.8e-07 | 1.9e-07 |

So the shape the harness reports as failing computes correctly in isolation, in both layouts.
That rules out "the module mishandles a batched fourth dimension" as the whole story and means
the harness case has another ingredient — the most likely candidate being that its two sources
are views into the *same* base allocation, at offset 0 and offset `k * type_size`, so the
inputs overlap each other in a way this fixture does not reproduce.

The one real anomaly is `m=4` at 1.4e-02 on sample 0 with sample 1 clean, which does not match
the harness cases either and is its own open question.

Nothing is concluded from this beyond the negative result: shape and padding are not enough.

### The kernel is bit-identical; the gap is a shape the fixture still misses

Dumped the output of both arms and compared byte for byte, which is the project's own
criterion and does not depend on any host oracle:

    SAME  n256-r64            SAME  n256-r16-s2      SAME  n256-r16-s2-pad   SAME  n256-r16-s2-ovl
    SAME  n512-r128           SAME  n256-r4-s2       SAME  n256-r4-s2-pad    SAME  n256-r4-s2-ovl
    SAME  n2048-r33           SAME  n4096-r8         SAME  n8192-r5-c4

All eleven, including the overlap layout that copies the harness arrangement exactly (both
sources views into one allocation, at offsets 0 and `k*type_size`). So the Goose matvec kernel
produces the same bytes as the native one for every shape reproduced so far.

That also retires the `m=4` anomaly: it came from the host oracle dividing by a sum close to
zero, not from the kernel. The byte comparison is the right instrument and the oracle was not.

The missing combination was then added — `ne2 = 3` *and* `ne3 = 2` together, which is the
harness's `bs=[3,2]` — in contiguous, padded and overlapping layouts:

    SAME n256-r16-c3-s2.bin
    SAME n256-r16-c3-s2-pad.bin
    SAME n256-r16-c3-s2-ovl.bin

All three bit-identical to native. So the kernel is correct for every layout that can be
constructed here, and the harness failure is therefore **not** in the matvec body.

That changes what the `ne3 == 1` bound means. It is not bounding a kernel that gets the answer
wrong — the kernel gets it right. It is bounding a case where something else differs, and the
bound makes the difference disappear, which is itself a clue: it sits in what the two routes
do *around* the kernel, not inside it. The next instrument is a per-case trace of which
function the harness's failing shapes actually enter, rather than more fixture variants.

Recorded as an open question with the evidence on both sides, not as a diagnosis.

### k=4 is also bit-identical

The failing set includes `k=4`, which the fixture had not covered. Added it in the same three
layouts:

    SAME n4-r16.bin
    SAME n4-r16-s2.bin
    SAME n4-r16-c3-s2.bin

Seventeen fixture cases now, every one byte-identical between the two arms.

### What the evidence now says

Three facts have to be held at once:

1. The Goose matvec kernel is bit-identical to the native one in every configuration that can
   be built here — fourteen layouts plus k=4, spanning contiguous, padded and overlapping
   storage, with channels, samples and both together.
2. ~~Disabling either module alone clears all MUL_MAT failures.~~ **Retracted — it was a broken
   measurement.** See the correction section below: the real split is 6 f32 cases owned by
   EXP184 and 3 q4_K cases owned by EXP185, which is exactly the shape table.
3. Adding `ne3 == 1` to both predicates also cleared all of them while leaving the Apex
   witnesses at `paths=0x1`.

Fact 2 was retracted, so the anomaly that motivated a "route around the kernel" reading is
gone. Facts 1 and 3 stand on their own and still do not explain the failure.

## Measurement defect, found and corrected

The attribution above was produced with

    `for k in MMVF MMVQ_Q4K; do n=$(LD_LIBRARY_PATH=$R GGML_GOOSE_CUDA_$k=0 timeout ... test-backend-ops ...)`

`GGML_GOOSE_CUDA_$k=0` is not a valid POSIX assignment — the name contains an unexpanded `$`
in a position the shell does not treat as a variable — so the command never ran, its output was
empty, and `grep -c` reported 0 failures. Every "switch X clears the failures" reading in this
document came from that line.

Control that exposed it: setting any unrelated variable the same way also reported 0, including
`FOO=0` and a non-existent `GGML_GOOSE_CUDA_NOPE=0`; and the unmodified run reports 9 failures
three times in a row, so the result is deterministic and the zeros were the artifact.

Redone with `env`:

    MMVF=0      ->  3 failures remain (EXP184 owns the 6 f32 cases)
    MMVQ_Q4K=0  ->  6 failures remain (EXP185 owns the 3 q4_K cases)

which matches the shape table exactly and is the attribution that should have been recorded the
first time.

## Where this leaves the MUL_MAT question

The kernel is byte-identical to native in seventeen fixture configurations, including every
layout and shape the harness uses, yet the harness still fails these nine cases when the bound
is removed. Both statements are measured. The gap is therefore in something the fixture does
not reproduce, and the remaining candidates are the ones the fixture cannot express: the
harness's own tensor construction (it builds a `base` and views, so strides and *offsets*
differ from a freshly allocated tensor) and the per-case dispatch that precedes the kernel.

The bound `ne3 == 1` stays because it is what keeps the suite green, and it is labelled as a
bound over an unexplained case rather than a fix.

### The broadcast layout is also bit-identical

Read `test_mul_mat::build_graph` again rather than assuming symmetry: it builds a `base` of
`[2k, max(m,n), bs0*nr0, bs1*nr1]` and takes `a = view(k, m, bs0, bs1)` and
`b = view(k, n, bs0*nr0, bs1*nr1)`. So with `bs=[1,1], nr=[*,2]` the **source has one sample
while the destination has two** — src0 is broadcast over the sample dimension. That is a shape
the fixture had never produced; every earlier case gave src0 as many samples as src1.

Added it in contiguous, padded and overlapping layouts:

    SAME n256-r16-s2-bcast.bin
    SAME n256-r16-s2-bcast-pad.bin
    SAME n4-r16-s2-bcast.bin
    SAME n4-r16-s2-bcast-c.bin

Still byte-identical. Twenty-one fixture configurations now agree with native exactly, and the
harness still fails the same nine cases when the bound is removed.

So every property of the tensors that can be expressed in this fixture — shape, strides,
padding, overlap, broadcast — has been ruled out by byte comparison. What remains is not a
property of the tensors but of the harness's execution path: which entry point the failing
cases reach, and what that entry point does before and after the kernel. That needs a per-case
trace inside the backend, not another fixture variant, and it is where the next attempt starts.
