# REPORT - P4-12 final slice: ggml-opt optimizer decisions

Base commit `98eaa70e`. Verdict: **ACCEPTED for the three slices listed here**,
with explicit remaining scope below. This closes the last open slice of P4-12
(the ledger listed `ggml-opt.cpp optimizer and the backend-optimization
decisions` as remaining after EXP617).

## What was done

`ggml-opt.cpp` (1094 lines) was split by dependency. All decisions that only
need integers or floating-point arithmetic moved to Goose; everything that
needs a ggml tensor, a context, a backend buffer or a scheduler stayed native.

| slice | exports | native source | cases |
|---|---|---|---|
| S1 scheduling: dataset, epoch, fit | 8 | `ggml-opt.cpp` 86-219, 881-924, 999-1079 | 58 |
| S2 step: build type, accumulate, momenta, scale | 11 | 245-260, 322-547, 725-735, 782-826, 1085-1094 | 38 |
| S3 result: loss, accuracy, counters | 6 | 637-708, 782-877 | 36 |

Total: 25 exports, 132 differential comparisons, 12 mutations.

## Numbers

- Gate: 132/132 at `-O1` (`tests/gate.txt`); 132/132 with the port at `-O0`.
  Gate run 19-23 ms after build, whole build under 1.5 s warm.
- Mutations: 12/12 killed, half A 6/6, half B 6/6 (`tests/mutations.txt`).
  Every mutation was built at `-O0` and failed a named case.
- `goose check`: parsed and typechecked, 85 specializations, no warnings.
- Memory impact on production: none. The ported functions allocate nothing and
  write into caller buffers. The gate's own model context is 4 MiB and the
  compute context 32 MiB, inside the experiment only.
- Speed impact on production: none until adapter call sites are switched; the
  functions are branch and arithmetic code no slower than the native lines.
- GPU: not used. CPU backend only, for the tiny oracle model.

## Correctness

The oracle is the native file, not a retyped copy: `tests/oracle.cpp` includes
`ggml-opt.cpp` from the base tree, so the real struct definitions, the real
`ggml_opt_epoch`, `ggml_opt_alloc`, `ggml_opt_eval`, `ggml_opt_result_loss`,
`ggml_opt_result_accuracy`, `ggml_opt_dataset_*` and `ggml_opt_default_params`
execute inside the gate. This is why fields such as `iter`, `opt_i`,
`grad_accs`, `grad_m`, `opt_step_params` and `loss_per_datapoint` can be
compared directly instead of being inferred.

Coverage by evidence type:

- executed native code: batch split, callback order and bounds, `iter` and
  `opt_i` after a real epoch, accumulate/momenta/param-width, loss scale read
  from the built loss node, all loss types and periods, dataset shard bytes,
  batch shard mapping, defaults, optimizer names, result loss/accuracy/ndata;
- transcription, marked in the fixture: the `val_split` product (1025-1026) and
  the shuffle limit (158-160). Both are cross-checked against an observable
  native invariant: a native epoch run at the transcribed split, and the native
  shuffle's untouched suffix;
- contract case with no native counterpart: `op_ndata_next` returning -1 on a
  mismatch, because native reaches the same condition through `GGML_ASSERT`.

Mutation coverage by slice: S1 6 (M01-M06), S2 4 (M07-M10), S3 2 (M11-M12).
Every mutation kills a different decision.

## Findings at the base commit

1. Validation evaluations advance `opt_i`. `ggml_opt_eval` advances the step
   index unconditionally (line 826) while `iter` advances only for the
   optimizer graph (825). For period 5, three train batches and three validation
   batches give `iter = 1` and `opt_i = 1`; a train-only model would predict
   `opt_i = 3`. The port reproduces the executed behaviour and the gate pins it.
   This looks like an upstream quirk, not a design choice; if it is fixed, the
   `epoch_state_p5` case and mutation M08 are the signal.
2. `ggml_opt_result_loss` returns the sum for the SUM loss type (line 678:
   `*loss = loss_per_datapoint ? mean : sum`). The function name and the
   per-datapoint path suggest a mean; the code returns a sum. Reproduced.
3. The per-datapoint rescale and the squared term are float operations before
   the double accumulator. Doing them in double shifts the result by ~1e-8; the
   gate caught this while the port was being written and the final code matches
   the native precision.

## Artifacts

Experiment directory (lead copies into `experiments/` after assigning the id):

```
README.md
REPORT.md
source/ggml-opt.goose
tests/oracle.cpp
tests/fixture.cpp
tests/build.sh
tests/mutate.py
tests/gate.txt
tests/mutations.txt
```

SHA-256 values are recorded in `README.md`. Generated C, objects and gate
binaries stay in `build/` and are not evidence.

## Integration step

1. Copy `source/ggml-opt.goose` to `goose/cpu/goose-opt.goose`.
2. Add the manifest entry (`goose/MANIFEST.json`) with native target
   `ggml/src/ggml-opt.cpp` and regenerate via `tools/sync-goose-tree.py`.
3. Wire the adapter at decision level in the order given in `README.md`,
   section "Integration step". No ggml type crosses the Goose boundary; arrays
   are projected to `int64_t`, `float` and `double` views.
4. Replace the matching native expressions and keep the surrounding I/O,
   allocation and graph code unchanged. This is a port, not a tuning change;
   production defaults must not move.

## Remaining scope of P4-12

- `ggml-opt.cpp` graph construction, backward expansion, optimizer step nodes,
  allocator and scheduler calls: intentionally native.
- Progress bar, stderr and time reporting: native.
- Meta `graph_compute`, split-state callback, buffer allocation and
  `init_tensor`/set/get/memset: device-bound, still native (EXP617 scope).
- Backend init, events, comm and diagnostics: driver-bound.
- Strict source closure of `ggml-backend-meta.cpp` (2511 lines); EXP617 covers
  its device and buffer-type policy only.
- Ledger: `slices_total` 4, `slices_done` 4, `remaining` keeps the three
  device-bound items above and must drop the `ggml-opt.cpp` line.

## Risks

- The f32 split arithmetic depends on float evaluation order. The gate pins it
  at `-O1` and `-O0`; a build with `-ffast-math` or a different contraction
  choice could move the truncation at `val_split = 0.3`. Keep the port's
  product in one f32 expression.
- `op_result_loss` and `op_result_accuracy` depend on double `sqrt` from libm;
  the adapter must link `-lm`.
- The `opt_i` case pins the current line-826 behaviour. An upstream fix makes
  `epoch_state_p5` and M08 fail; that is the intended signal, not a flake.
- The port returns optimizer codes, not strings. An adapter that maps codes to
  names must keep the native string table for unknown ids.
- The gate's model is one shape. It exercises the formulas, not arbitrary
  tensor layouts; shapes do not enter any ported decision, but a future change
  that makes a formula shape-dependent would need a new case.
- `op_ndata_next` has no native oracle because native aborts on the mismatch;
  the -1 contract is covered by a direct case, not by a differential one.
