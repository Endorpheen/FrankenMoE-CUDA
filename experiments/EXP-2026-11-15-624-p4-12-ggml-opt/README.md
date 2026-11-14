# EXP P4-12 (final slice): ggml-opt optimizer and training-loop decisions

Third and last slice of P4-12, covering `ggml-opt.cpp`. It ports the optimizer
and training-loop decisions that are pure integer or floating-point arithmetic;
tensor construction, graph building, backward expansion and backend scheduling
stay native. No GPU, no driver, no real weights, no production change.

Base commit: `98eaa70e` (2026-11-15). Native tree: `upstream/llama.cpp`.
Experiment id is provisional; the lead assigns it at integration.

## Inventory and scope

`ggml-opt.cpp` is 1094 lines. The split is by what the decision needs:

| part of the file | needs | in this slice |
|---|---|---|
| dataset shard sizes, shuffle range, batch shard mapping (86-219) | integers only | yes, S1 |
| epoch split, batch counts, callback bounds (881-924) | integers only | yes, S1 |
| fit period, validation split, shuffle triggers (999-1079) | integers, one f32 product | yes, S1 |
| build type, accumulate, momenta, param width, loss scale (322-547) | integers | yes, S2 |
| iter/opt_i updates, default params, optimizer name (549-582, 549-1094) | integers | yes, S2 |
| result loss/accuracy/ndata/pred/ncorrect aggregation (637-708, 782-877) | f32 inputs, f64 math | yes, S3 |
| `map_tensor`, `dup_graph`, graph duplication (262-320) | ggml tensors | no |
| loss graph construction, backward expand, optimizer step nodes (322-547) | ggml context, backend | no |
| `ggml_opt_alloc` allocator and `ggml_backend_sched_*` calls (725-780) | backend sched | no |
| progress bar formatting and stderr output (926-997) | none, but I/O only | no |
| tensor get/set and real training data movement (855-876) | backend buffers | no |

## Slices

Three atomic slices, 25 exported functions, one Goose file.

### S1 `op_` scheduling: dataset, epoch, fit

| export | native source |
|---|---|
| `op_nbatches` | `ggml_opt_epoch`, 899 |
| `op_batches_split` | 901-903 |
| `op_epoch_split` | `ggml_opt_fit`, 1020-1026 |
| `op_shard_bytes` | `ggml_opt_dataset_init`, 112, 116 |
| `op_shuffle_limit` | `ggml_opt_dataset_shuffle`, 158-160 |
| `op_batch_shard` | `ggml_opt_dataset_get_batch`, 179-182 |
| `op_should_shuffle` | `ggml_opt_fit`, 1041 |
| `op_should_reshuffle` | `ggml_opt_fit`, 1051 |

The validation split is the only float decision: native computes
`int64_t(((1.0f - val_split) * nbatches_logical)) * opt_period`, a float product
truncated to int64 and then widened by the physical period. The port keeps the
f32 product, so `0.3f` truncates identically.

### S2 `op_` step: optimizer step and accumulation

| export | decision |
|---|---|
| `op_opt_period` | logical batches per physical batch, 1020 |
| `op_build_type_next` | 30 (OPT) when `(opt_i+1) % period == 0`, else 20 (GRAD), 731-732 |
| `op_iter_advance` | iteration grows only on an optimizer graph, 825 |
| `op_opt_i_advance` | step index grows on every evaluation, 826 |
| `op_accumulate` | GRAD or above, except static + OPT + period 1, 328-329 |
| `op_need_momenta` | only AdamW optimizer graphs, 331-332 |
| `op_params_width` | 7 AdamW, 2 SGD, 506 |
| `op_loss_scale` | mean/MSE `1/(period*nelements)`, CE `1/period` above one, sum unscaled, 390, 408-410, 425 |
| `op_loss_per_dp` | sum is the only unscaled loss, 393/399/412/428 |
| `op_optimizer_code` | 0 adamw, 1 sgd, -1 undefined, 1085-1094 |
| `op_defaults` | build type OPT, period 1, AdamW, default callback, 245-260 |

Two base-commit behaviours are reproduced on purpose:

1. `opt_i` advances after validation evaluations too (line 826 is outside the
   backward branch). With period 3 and six evaluations the native step index is
   0, not the train-only 0 after four batches. The gate runs this exact case.
2. `ggml_opt_result_loss` returns the sum, not the mean, for the SUM loss type
   (line 678). The name is misleading; the port follows the code.

### S3 `op_` result: aggregation

| export | decision |
|---|---|
| `op_result_loss` | per-datapoint rescale, mean, standard error, NaN under two batches, 658-691 |
| `op_result_accuracy` | ratio and binomial standard error, NaN when undefined, 699-708 |
| `op_ndata_next` | batch consistency check then increment, 850-852 |
| `op_pred_len` | prediction vector growth, 862-864 |
| `op_ncorrect_next` | sticky -1 sentinel, 867-876 |
| `op_accuracy_defined` | sentinel predicate, 699 |

The rescale is a float multiply before the double sum, and the squared term is
a float multiply, exactly as in the native code. Doing both in double differs
by about 1e-8 and fails the gate.

## Gate

132 comparisons: 58 in S1, 38 in S2, 36 in S3. The oracle is `ggml-opt.cpp`
itself, included into `tests/oracle.cpp` and compiled into the gate, so struct
fields and static helpers are reachable without a retyped copy. The oracle
builds one 2x3 `mul_mat` model and runs a real `ggml_opt_epoch` on the CPU
backend over synthetic data, for four configurations (periods 1, 2, 3, 5, with
and without validation), so batch split, callback order, `iter` and `opt_i` are
observed from executed native code.

Two values are not reachable through the public API and are transcribed from
the native lines, marked `TRANSCRIPTION` in the fixture:

- the `val_split` arithmetic (1025-1026), cross-checked against a native epoch
  run at the same split index;
- the shuffle limit (158-160), cross-checked against the native invariant: the
  suffix past the limit stays identity after a real `ggml_opt_dataset_shuffle`.

```
summary pass=132 fail=0 total=132
```

Mutation gate: 12 mutations, two halves of six, port rebuilt at `-O0` for every
mutation. All 12 are killed by a named case; see `tests/mutations.txt`.

```
half A: killed 6/6
half B: killed 6/6
```

## Layout

```
source/ggml-opt.goose   the three slices, exports op_
tests/oracle.cpp        includes the native ggml-opt.cpp, tiny CPU run
tests/fixture.cpp       native oracle vs port, 132 comparisons
tests/build.sh          gate build (-O1) and run, root derived from the script
tests/mutate.py         12 mutations in two halves, -O0 builds
tests/gate.txt          recorded gate output
tests/mutations.txt     recorded mutation results
```

Generated C, objects and gate binaries stay under `build/` and are not part of
the evidence; the Goose source plus these results reproduce them.

Reproduce:

```
bash tests/build.sh
python3 tests/mutate.py --half A --jobs 1
python3 tests/mutate.py --half B --jobs 1
```

## Integration step

The port is a new canon module; production code is not touched.

1. File: `goose/cpu/goose-opt.goose`, all 25 exports with their current names.
   A new file is needed because no canon module owns `ggml-opt.cpp`, and
   `goose/cpu/` already carries the backend API, allocator and job-pool
   modules. Order inside the file: S1, S2, S3 as written.
2. `goose/MANIFEST.json`: add an entry with `goose` set to the new path,
   `native_targets` = `ggml/src/ggml-opt.cpp`, and `experiment` set to the
   accepted EXP id. Regenerate with `tools/sync-goose-tree.py`.
3. Native adapter, one call site per decision, in this order:
   - `ggml_opt_dataset_init` -> `op_shard_bytes(nbytes_data, nbytes_labels,
     ndata, ndata_shard, ne_label > 0, out)`; `out[0]` is `nbs_data`, `out[1]`
     is `nbs_labels`;
   - `ggml_opt_dataset_shuffle` -> `op_shuffle_limit(idata, ndata_shard)`,
     then `std::shuffle` over that many leading shards;
   - `ggml_opt_dataset_get_batch` and `_host` -> `op_batch_shard(perm, ibatch,
     shards_per_batch, k)` for the shard index of each copy;
   - `ggml_opt_build` -> `op_accumulate(build_type_alloc, static_graphs,
     opt_period)`, `op_need_momenta(build_type_alloc, optimizer)`,
     `op_params_width(momenta)` for the `opt_step_params` length, and
     `op_loss_scale(loss_type, opt_period, nelements)` for the loss scale;
   - `ggml_opt_alloc` -> `op_build_type_next(opt_i, opt_period)`;
   - `ggml_opt_eval` -> `op_iter_advance(iter, allocated_graph == gb_opt)`,
     `op_opt_i_advance(opt_i, opt_period)`, `op_ndata_next(prev, ndata,
     loss.size())`, `op_pred_len`, `op_ncorrect_next`;
   - `ggml_opt_result_loss` / `_accuracy` -> `op_result_loss` /
     `op_result_accuracy`, called with the recorded float losses and the
     stored period and per-datapoint flag;
   - `ggml_opt_default_params` -> `op_defaults(loss_type, out)`;
   - `ggml_opt_optimizer_name` -> `op_optimizer_code(id)` or keep the string
     switch (the port returns codes, the strings stay native);
   - `ggml_opt_fit` -> `op_epoch_split(ndata, nbatch_logical, nbatch_physical,
     val_split, out)` for `opt_period`, `nbatches_logical`, `ibatch_split`,
     `idata_split`, then `op_should_shuffle` and `op_should_reshuffle`;
   - `ggml_opt_epoch` -> `op_nbatches(ndata, ndata_batch)` and
     `op_batches_split(ndata, ndata_batch, idata_split)`.
4. Pass `val_split` as `float`. The split arithmetic is f32 in native; an
   adapter that widens to double changes the truncation at 0.3 and 0.7.

## Compatibility checks

- `goose check source/ggml-opt.goose`: parsed and typechecked, 85
  specializations, no warnings.
- Gate: 132/132 at `-O1` against the base tree; 132/132 with the port at
  `-O0` (`bash tests/build.sh mutate`); `tests/gate.txt` is the recorded run.
- Mutations: 12/12, half A 6/6 (all S1), half B 6/6 (four S2, two S3).
- No duplicate exports: no `op_` symbol exists anywhere under `goose/` at the
  base commit (`grep -rl` over the tree returns nothing).
- The generated ABI is plain C11 (`sl_i64`, `sl_f32`, `sl_f64`), so the adapter
  calls it from C++ without a shim. `sqrt` is the only libm dependency.

## Limits

- No graph construction, backward pass, optimizer graph nodes, allocator or
  backend scheduling: those need a live ggml context and a backend.
- No progress-bar formatting, no stderr or time output.
- No tensor data movement (`ggml_backend_tensor_set/get`) and no real training.
- The gate proves the decisions on one model shape (input 2, output 3, physical
  batch 4), four periods and datasets up to 24 points. Shapes outside that do
  not change any ported formula.
- Two expressions are transcriptions, as listed under Gate.

## Hashes (SHA-256)

Inputs, base `98eaa70e`:

```
e35fd27856e37f3611d3bc6f17ee963be51bfe92bec96fad0a9fa2c7ebbe9c46  upstream/llama.cpp/ggml/src/ggml-opt.cpp
3586de1bc8a934b5c72339e2b6937b0641e8f149b512231e666f67de0736eea2  upstream/llama.cpp/ggml/include/ggml-opt.h
8cce13c85f19330096002140c401601bcf050fe7c16c255ef83e33d0eb7ad3dc  libggml-base.so (oracle library)
```

`work/llama.cpp-integration/ggml/src/ggml-opt.cpp` is byte-identical to the
upstream copy above (same SHA-256); it was read only, and the gate compiles the
upstream path.

Artifacts:

```
be36304ca700a26753906e6e63c6525d9a69c0020ec6aabe29408be305a388c5  source/ggml-opt.goose
482e39a8f2f99852daa45d50edbf78b7a930c446eb12c0910792e51583a13aaa  tests/oracle.cpp
024ecd55421ffa9a36ebfcc547b05ecf1b96b2c77fe102bc36d0c456226f4541  tests/fixture.cpp
80e3d17d64424df48a3d5be0fb673fea303696ee945b7f56b33097979c60635d  tests/build.sh
d411c19a8929282ff0b413ee42e3de30d372100640b49bae186c04c18db789f3  tests/mutate.py
c0b2c8845d7c48d8c8f24c81a95c94d035b1b54a29d2b14df516367e923fecc4  tests/gate.txt
faa07d9d9897baa3e9e324a7b9d4bbe519c9e1cb35dd6b79a86af023ed04e0d2  tests/mutations.txt
```

Toolchain: goose runtime `1e714e6146bc`, gcc/g++ 13.4.0. Generated port C:
183045 bytes, not committed.
