# EXP-2026-11-15-659 — P10-08 device headers, C++ host headers, scheduler sequence

Three model-free slices that continue `EXP-2026-11-15-649`: the fourteen
per-backend device headers, the C++ host header surface (`ggml-cpp.h` plus
`carrier/runtime/core/include`), and the scheduler eval-callback sequence with
the log and progress callbacks around it.

- Requirement: **P10-08** "Preserve public headers, FFI and binary
  interoperability" (`docs/goose-migration/tasks.json`), currently `PARTIAL`
  with 6/6 slices.
- Assigned base commit: `050354e9`. Header canon: `upstream/llama.cpp` at
  `0b5be7e4a` (git-ignored, anchored by the SHA-256s in `results/hashes.txt`).
- Carrier headers: `runtime/core/include/bmoe` at the same base commit; the
  requirement's `carrier/runtime/core/include/**` source glob resolves to this
  path in the workspace.
- Reference shared object for the log-routing arm:
  `build/exp054-dflash/bin/libggml-base.so.0.22.0` (`FG_SO_DIR` overrides).
- Toolchain: `goose-2026.3-string`, `gcc-13`/`g++-13` 13.4.0.
- No model, no GPU, no server, no production artifact was touched.

## Hypothesis

What EXP-649 left open is not more constants: it is the second half of the
public header surface, where the contract is not a number but a *shape* and a
*sequence*.

1. Fourteen device headers declare 84 functions that a loader resolves by name.
   A declaration that moves between headers, gains a parameter or changes its
   return class breaks a backend driver that was built against the old header,
   and nothing in the C ABI table of the earlier slices notices.
2. `ggml-cpp.h` is seven `unique_ptr` wrappers. Their contract is *which*
   release function each deleter calls and *when*: the wrong pairing is a
   double free of a different type, and a `shared_ptr` would release a shared
   handle twice. Separately, the carrier headers decide the engine's behaviour
   through defaults that no consumer sees at compile time.
3. The scheduler eval callback is a small exact state machine: ask in index
   order, compute the chunk, observe only what the caller asked for, and let a
   false observation stop the rest of the split. A plausible implementation and
   the real one differ in the edge cases (nothing wanted, cancel on an
   unobserved node, empty graph), which is exactly what a differential gate
   against a verbatim transcription can decide.

All three are decisions that can be frozen and checked without a model.

## Slices

| slice | module | freezes | exports |
|---|---|---|---|
| S7 `devh_` | `source/device-headers.goose` | the 14 device headers, their 14 macros (3 RPC protocol numbers, 4 device ceilings, 6 backend names, one CUBLAS name), all 84 `GGML_BACKEND_API` declarations with header membership, arity, return class and every parameter class, the 12-capability matrix and the per-header device ceiling | 15 |
| S8 `cpph_` | `source/cpp-headers.goose` | the 7 `ggml-cpp.h` RAII wrappers (release function, `unique_ptr` kind, stateless deleter, call counts on destroy/release/move), the 5 includes and the C++-only guard, 3 carrier enums, 55 defaults across `SamplingConfig`/`MoeStreamConfig`/`SpecConfig`/`RunConfig`/`SessionConfig`, 8 pod layout facts, `bmoe::version()` and the 3 `SpecConfig` predicates | 34 |
| S9 `sched_` | `source/sched-eval.goose` | the ask/observe/cancel machine over one split graph (event order, computed ranges, cancellation, empty graph, argument bounds), the two eval callback shapes, the 6 log levels and the log routing rule, and the progress-callback contract (null, abort, final 1.0) | 20 |

## The oracle

Nothing is compared against a hand copy where a machine can resolve it:

- `tests/extract_native.py` re-reads all fourteen device headers on every build
  and emits:
  - every macro, expanded by the fixture with its own compiler, so the
    conditional `GGML_CUDA_NAME` (`ROCm`/`MUSA`/`CUDA`) is the live branch;
  - every `GGML_BACKEND_API` declaration with its header index;
  - the macro-to-header map that the device-ceiling check uses.
  It also asserts that the scheduler anchors are still present and in the same
  order (`ask` exactly two call sites, `observe` exactly one, the view is
  `j0..j1+1`, the observe can `break`, `j0 = j1`), and that the loader still
  carries the data-progress call and the final `1.0` call. A future upstream
  change to the loop shape fails the oracle instead of silently drifting.
- `tests/fixture_devh.cpp` classifies each real declaration with `decltype`
  (arity, return class, parameter classes) and recomputes the capability matrix
  from the native name list with an independent `std::string` scan.
- `tests/fixture_cpph.cpp` classifies the seven typedefs with `std::is_same`,
  reads every default and offset from a compiled-in struct, and measures the
  deleter calls for real: the seven release functions are defined as
  linker-wrapped counters (`-Wl,--wrap=...`), so `which function, how many
  times, with which pointer` is observed rather than assumed. `bmoe::version()`
  is compiled twice, with and without `BMOE_VERSION`.
- `tests/fixture_sched.cpp` contains a verbatim transcription of
  `ggml-backend.cpp:1808-1837` and compares it with the port over every
  want-mask up to six nodes and every cancellation point. The log routing rule
  is measured on the reference `libggml-base.so` through
  `ggml_log_set`/`ggml_log_get`, and the progress contract is a transcription of
  `llama-model-loader.cpp:1570-1574` and `1722-1726`.

## Running it

```
bash tests/build.sh                 # all three gates at -O1
bash tests/build.sh sched mutate    # one gate, module compiled at -O0
python3 tests/mutate.py 1           # six mutations per slice
python3 tests/mutate.py 2           # the other six
```

The build directory is derived from the script location; no absolute scratch
path is written. Overrides: `FG_NATIVE`, `FG_CARRIER`, `FG_GOOSE`, `FG_SO_DIR`,
`FG_OUT`, `CC`, `CXX`.

## Gate

| slice | cases | values | `-O1` | `-O0` |
|---|---:|---:|---|---|
| `devh` | 143 | 620 | 0 failures | 0 failures |
| `cpph` | 78 | 435 | 0 failures | 0 failures |
| `sched` | 21 | 9482 | 0 failures | 0 failures |
| total | 242 | 10537 | 0 | 0 |

Every slice is above the ten-case floor. The summaries are byte identical at
both levels (`results/gate-o1.log`, `results/gate-o0.log`,
`results/gate-final.log`).

## Mutations — 36/36, halves 6/6 per slice

Module generated and compiled at `-O0`, fixture at `-O2`, one mutation at a
time, 18 mutations per half. The table with the decision and the number of
values each mutation broke is in `REPORT.md`.

## Assumptions

1. The header canon `upstream/llama.cpp` `0b5be7e4a`, the reference
   `libggml-base.so.0.22.0` and the carrier headers are local inputs; their
   SHA-256s in `results/hashes.txt` are the durable anchor. A missing input
   fails the oracle with an explicit message.
2. Declaration shapes are classified by kind. `int` vs `int32_t` is not a
   difference here; arity, pointer-ness and a by-value struct are. `uint32_t`
   is its own class (6), because the RPC and CANN entry points take it.
3. The capability matrix is a *derived* fact, not a table: the port scans its
   own declaration list, the fixture scans the native list, and the two scans
   are compared. A header that gains `_reg` gains the capability in both.
4. Struct sizes and offsets are x86-64 LP64 facts, asserted unconditionally by
   this gate. The two carrier structs that carry `std::string` are not laid out.
5. The log-routing arm calls `ggml_log_set` on the reference library. It
   restores the default callback before exiting; the gate is single-threaded
   and emits no log line while a probe callback is installed.
6. Callback shapes are frozen by kind; `ggml_log_level` is an enum and is
   classified as "other" without affecting the checked arity and return class.
7. Native abort/`GGML_ABORT` branches are not executed. Refusals are compared
   as refusals (`-1`).

## Integration step

The slices are complete and gate-clean outside the repository. Integration is
one move plus one ledger edit, done by the lead after reproduction:

1. Copy the three modules verbatim into `goose/misc/`: `device-headers.goose`,
   `cpp-headers.goose`, `sched-eval.goose`.
2. Copy `tests/` and `results/` into
   `experiments/EXP-2026-11-15-659-p10-08c-header-surface/`, and this README
   and `REPORT.md` with it.
3. In `docs/goose-migration/tasks.json`, set P10-08 `slices_total` and
   `slices_done` to 9, refresh `remaining` to the two non-slice items (host
   wiring; the ledger library's SONAME decision), point `coverage_doc` at the
   new REPORT, and refresh `status_totals`, `updated_at` and `work_front`.
4. No existing module, symbol table entry or production file changes.

## Boundaries

- No model, no GPU, no server, no benchmark, no production build was run.
- `work/llama.cpp-integration` was not touched.
- Nothing was committed; the workspace repository is unchanged.
- The three modules export 69 functions under the prefixes `devh_`, `cpph_` and
  `sched_`; none collides with the 8440 function names of the canonical
  `goose/` tree.
