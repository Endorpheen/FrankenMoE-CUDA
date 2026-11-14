# REPORT - EXP P4-12 meta backend, backend policy, threading

Base commit `df3ce73a`. Verdict: **ACCEPTED for the three slices listed here**,
with two recorded native defects and explicit remaining scope below.

## What was done

Three slices of the P4-12 scope were moved to Goose and gated against the real
native code, without a GPU:

| slice | exports | native source | cases |
|---|---|---|---|
| `mb_` meta device / buffer-type policy | 13 | `ggml-backend-meta.cpp` 54-392 | 24 |
| `rg_` registry identity | 3 | `ggml-backend-meta.cpp` 215-245, 346-371 | 7 |
| `th_` / `rt_` thread-count, cpumask, admission | 7 | `ggml.c` 8045-8065, `ggml-cpu.c` 2682-2712, 3384-3410, `config.cpp` 21-23, 155-159 | 36 |

Total: 23 exports, 67 differential comparisons, 12 mutations.

## Numbers

- Gate: 67/67 pass at `-O1` (`tests/gate.txt`). Gate run 11 ms after build.
- Mutations: 12/12 killed, half A 6/6, half B 6/6 (`tests/mutations.txt`).
  Every mutation was built at `-O0` and failed a named case.
- `goose check`: parsed and typechecked, 83 specializations.
- Memory impact on production: none. The functions allocate nothing; they
  write into caller buffers. The gate allocates one 4 KiB ggml context per
  tensor-dependent case group, inside the experiment only.
- Speed impact on production: none until the adapter call sites are switched;
  the functions are branch-only policy code.

## Correctness

The oracle is native, not a retyped copy, for 4 of 5 subsystems:

- meta device and buffer-type getters run on synthetic `ggml_backend_device`
  and `ggml_backend_buffer_type` handles with a controlled iface, so the real
  `ggml_backend_meta_device` registry and getters execute with no GPU;
- `ggml_threadpool_params_default/match` come from `libggml-base.so`;
- `bmoe::validate` is compiled from `runtime/core/src/config.cpp`;
- only `ggml_thread_cpumask_next`, `ggml_thread_cpumask_valid` and the
  graph-plan clamp are transcribed, because they are static. The transcription
  is line-equivalent and marked in `tests/fixture.cpp`.

Mutation coverage by slice: `mb_` 7 (M01-M07), `th_` 3 (M08-M10), `rt_` 1
(M11), `rg_` 1 (M12).

## Findings at the base commit

1. Meta device and buffer-type names are always `Meta()`. Both constructors
   read the by-value vector parameter after `std::move` into the member
   (`ggml-backend-meta.cpp` 62-77 and 256-265), and the moved-from vector is
   empty. The gate observed this before the port existed. The port reproduces
   it; the intended join is the follow-up.
2. A meta device always reports `host_buffer = false`,
   `buffer_from_host_ptr = false` and `events = false`, because `get_props`
   seeds those three flags false and only ANDs members (130-146). This is
   reproduced exactly.

Neither finding changes the port's structure: when upstream fixes the
constructors, `mb_name`/`mb_description` gain the join and the two name cases
start comparing real content.

## Artifacts

Experiment directory (lead copies into `experiments/` after assigning the id):

```
README.md
REPORT.md
source/runtime.goose
tests/fixture.cpp
tests/build.sh
tests/mutate.py
tests/gate.txt
tests/mutations.txt
```

Hashes are recorded in `README.md` (inputs, artifacts, toolchain). Generated
C and objects stay in `build/` and are not evidence.

## Integration step

1. Copy `source/runtime.goose` to `goose/cpu/goose-meta-backend.goose`.
2. Add the manifest entry (`goose/MANIFEST.json`) with the four native targets
   and regenerate via `tools/sync-goose-tree.py`.
3. Wire the adapter at the getter level in the order given in `README.md`,
   section "Integration step"; each call takes POD arrays projected from the
   native handles, and no ggml type crosses the Goose boundary.
4. Move `rt_*` to the config-validation module when that slice lands; the
   native error strings stay where they are.
5. Do not change production defaults. This is a port, not a tuning change.

## Remaining scope of P4-12

- `ggml-opt.cpp`: optimizer construction, epoch/fit policy and result
  aggregation. Separate slice.
- Meta `graph_compute` dispatch and split-state computation: needs real
  devices and the split-state callback contract.
- Meta buffer allocation, `init_tensor`, set/get/memset paths: need device
  memory.
- Backend init, events, comm and diagnostics: driver-bound.
- Strict source closure of `ggml-backend-meta.cpp` (the file is 2511 lines and
  this gate covers its device and buffer-type policy only).

## Risks

- The name/description cases pin a defect. If a rebase picks up an upstream fix,
  those cases will fail until the port gains the join; that failure is expected
  and is the signal to update the slice.
- `SIZE_MAX` is passed as `-1`; an adapter that forgets the signedness will
  mis-handle unlimited buffer types.
- `rg_key_eq` models handle identity, not pointer ordering. Any caller that
  relies on map iteration order is outside the port.
- The threading oracle for the three static helpers is a transcription; a
  divergence between the transcription and the daemon's actual `atomically`
  updated source would not be caught by this gate.
- `io_threads_max = 8` is read from `config.h`; a change there needs the case
  and mutation table updated together.
