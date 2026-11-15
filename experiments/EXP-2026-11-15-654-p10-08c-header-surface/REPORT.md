# REPORT — EXP-2026-11-15-659 (P10-08, device headers, C++ host headers, scheduler sequence)

Verdict: **ACCEPTED for the three slices** (S7 `devh_`, S8 `cpph_`, S9 `sched_`).
P10-08 moves from 6/6 to 9/9 slices; the two remaining items are not slices
(section 7).

## 1. What was frozen

### S7 `devh_` — the per-backend device headers (15 exports)

Fourteen headers, 14 macros and all 84 `GGML_BACKEND_API` declarations, each
with its header, arity, return class and every parameter class. Classes are
0 void, 1 bool, 2 signed integer, 3 `size_t`, 4 float/double, 5 pointer
(including pointer typedefs and function pointers), 6 other scalar
(`uint32_t`).

The macros a caller reads before it links:

| macro | value |
|---|---|
| `GGML_CUDA_MAX_DEVICES` | 16 |
| `GGML_VK_MAX_DEVICES` | 16 |
| `GGML_SYCL_MAX_DEVICES` | 48 |
| `GGML_CANN_MAX_DEVICES` | 16 |
| `GGML_RPC_MAX_SERVERS` | 16 |
| `RPC_PROTO_MAJOR/MINOR/PATCH_VERSION` | 6 / 0 / 0 |
| `GGML_CUDA_NAME` / `GGML_CUBLAS_NAME` | `CUDA` / `cuBLAS` |
| `GGML_VK_NAME` | `Vulkan` |
| `GGML_SYCL_NAME` | `SYCL` |
| `GGML_ET_NAME` | `ET` |
| `GGML_WEBGPU_NAME` | `WebGPU` |

`GGML_CUDA_NAME` is conditional in the header (`ROCm`, `MUSA` or `CUDA`). The
extractor records only the name and the fixture expands the live branch, so a
build that flips the condition is compared against what the compiler actually
produced.

The declaration surface per header, and the twelve capabilities derived from
it:

| header | declarations | capabilities |
|---|---:|---|
| `ggml-cuda.h` | 11 | init, reg, is, buffer_type, host_buffer_type, device_count, description, memory, host-buffer register/unregister |
| `ggml-metal.h` | 6 | init, reg, is, abort callback, family check, capture-next-compute |
| `ggml-vulkan.h` | 8 | init, reg, is, buffer_type, host_buffer_type, device_count, description, memory |
| `ggml-rpc.h` | 7 | init, reg, is, buffer_type, memory, add_server, start_server |
| `ggml-opencl.h` | 5 | init, reg, is, buffer_type, host_buffer_type |
| `ggml-hexagon.h` | 3 | init, reg, is — **no buffer type at all** |
| `ggml-sycl.h` | 14 | all except host-buffer register; **only header with split_buffer_type and comm** |
| `ggml-cann.h` | 8 | init, reg, is, buffer_type, host_buffer_type, device_count, description, memory (`int32_t` parameters) |
| `ggml-blas.h` | 4 | init, reg, is, set_n_threads |
| `ggml-et.h` | 9 | init, reg, is, buffer_type, host_buffer_type, device_count, description, memory, **guid** |
| `ggml-virtgpu.h` | 1 | reg only |
| `ggml-zdnn.h` | 2 | reg, buffer_type |
| `ggml-zendnn.h` | 4 | init, reg, is, set_n_threads |
| `ggml-webgpu.h` | 2 | init, reg |

Facts the table makes explicit and a name list alone would not:

- every one of the fourteen is loadable through the registry (`_reg` is the one
  capability no header lacks);
- `ggml-hexagon.h` exposes no buffer type, so a consumer cannot allocate from
  it directly;
- tensor-parallel communication (`split_buffer_type`, `comm_*`) exists only in
  `ggml-sycl.h`;
- `ggml-et.h` is the only header with a `guid`;
- `ggml-virtgpu.h` is registry-only: no init, no predicate, no buffer;
- a device index is `int` in CUDA and SYCL, `size_t` in Vulkan and ET, and
  `int32_t` in CANN; the RPC entry points take `uint32_t` plus a
  `const char *` endpoint, which is a different call shape, not a rename.

The capability matrix is derived twice: the port scans its own declaration
list, the fixture scans the native list with `std::string` operations, and both
matrices are compared cell by cell (14 × 12).

### S8 `cpph_` — `ggml-cpp.h` and the carrier headers (34 exports)

Seven RAII wrappers, seven release functions, and the ownership decisions a
consumer cannot see in the typedef:

| wrapper | owner | deleter calls |
|---|---|---|
| `ggml_context_ptr` | `ggml_context` | `ggml_free` |
| `gguf_context_ptr` | `gguf_context` | `gguf_free` |
| `ggml_gallocr_ptr` | `ggml_gallocr` | `ggml_gallocr_free` |
| `ggml_backend_ptr` | `ggml_backend` | `ggml_backend_free` |
| `ggml_backend_buffer_ptr` | `ggml_backend_buffer` | `ggml_backend_buffer_free` |
| `ggml_backend_event_ptr` | `ggml_backend_event` | `ggml_backend_event_free` |
| `ggml_backend_sched_ptr` | `ggml_backend_sched` | `ggml_backend_sched_free` |

All seven are `std::unique_ptr<T, D>` (a `shared_ptr` would copy the handle and
release it twice), the deleters are empty and return `void`, the header is
C++-only and pulls exactly five includes (`ggml.h`, `ggml-alloc.h`,
`ggml-backend.h`, `gguf.h`, `<memory>`).

The measured ownership behaviour, per wrapper: constructing from a raw pointer
releases nothing; destroying a live wrapper releases once, through its own
function, with the stored pointer unchanged; destroying an empty wrapper
releases nothing (`unique_ptr` semantics, not an explicit null check);
`release()` releases nothing; moving a live wrapper and destroying both ends
releases exactly once.

Carrier (`runtime/core/include/bmoe` at the same base):

- `bmoe::version()` returns `BMOE_VERSION` when the build defines it and
  `"unknown"` otherwise; both branches are compiled in the gate.
- `DenseWeightsMode` = 0 `Mmap`, 1 `Warmed`, 2 `Anonymous`, 3 `Pinned`.
- `DraftSource` = 0 `none`, 1 `mtp`, 2 `ngram`; `ThinkControl` = 0 `Template`,
  1 `Prefill`, 2 `None`.
- 55 defaults, frozen as values: `SamplingConfig{0.0f, 40, 0.95f,
  0xFFFFFFFF}`; `MoeStreamConfig` with `io_threads 4`, `o_direct true`,
  `cache_floor_mb 1536`, `row_stream_mb 64`, `predict_spec_max 2`,
  `dense_weights Anonymous`, `drop_renorm true`, and the five constexpr limits
  (`drop_low_topk_warn 4`, `cache_min_mb 1500`, `io_threads_max 8`,
  `prefetch_layers_max 8`, `route_ahead_max 8`); `SpecConfig` with
  `draft_max 3`, `ngram_min_match 3`, `ngram_max_match 12`,
  `draft_max_limit 8`, `ngram_match_limit 64`; `RunConfig` with
  `n_predict 128`, `n_threads 4`, `n_ctx 2048`, `think true`, and the prompt
  literal; `SessionConfig` with `n_batch 512` and `n_threads 4`.
- Three string-free pod layouts: `sizeof` 16 / 76 / 20 and five offsets.

The layout of the two structs that carry `std::string` (`RunConfig`,
`SessionConfig`) is deliberately not frozen: it is a libstdc++ fact, not a
header contract. Their defaults are frozen.

One asymmetry is recorded rather than "fixed": `SessionConfig::n_batch`
defaults to 512 while `session_config_from()` sets `n_batch = n_ctx`. The
default is what a caller who does not go through the mapping gets; the gate
freezes that number, not the mapping (which is compiled in `session.cpp`).

### S9 `sched_` — the eval sequence and the callbacks around it (20 exports)

The loop over one split graph is frozen as an exact machine. `sched_run(n,
want_mask, cancel_at)` returns the callback events as `index * 2 + phase`
(0 ask, 1 observe), the computed ranges as `start * 256 + end`, and a status.

The decisions, each one an edge case a plausible implementation gets wrong:

- **Ask order is index order, one ask per visited node.** If nothing is wanted,
  a six-node split produces exactly six asks, one computed chunk `[0, 6)`, and
  no observations.
- **A false ask is not a cancellation.** The ask return value means "observe
  this node"; answering no to every node computes the whole graph in one chunk.
- **Observe happens after the chunk is computed and synchronized**, and only
  for a node that was wanted. It is one call per wanted node, not per chunk.
- **Cancellation is possible only at an observed node.** `cancel_at` pointing
  at an unwatched node never fires, because that node is never observed.
- **A cancelled split keeps what it computed and asks nothing more.** The
  chunks already recorded stay recorded.
- **An empty split calls back zero times** and returns success, not an error.
- Out-of-contract arguments (`n < 0`, `n > 32`, `cancel_at < -1`,
  `cancel_at >= n`, an empty output buffer) return `-1` instead of writing.

The two callback shapes are frozen with it (`ggml_backend_sched_eval_callback`
arity 3, `(tensor *, bool, void *) -> bool`; `ggml_backend_eval_callback` arity
4, `(int, tensor *, tensor *, void *) -> bool`).

Log arm: six `ggml_log_level` values `0..5`, `ggml_log_callback` arity 3
returning void, and the routing rule **measured on the reference library**:

- a fresh process already has a callback installed — the library's built-in
  default, which writes to stderr, so the slot is never null;
- `ggml_log_set(nullptr, ...)` does **not** clear the slot: it reinstalls the
  built-in default and clears the user pointer;
- a non-null callback receives the lines instead.

The header comment says "If this is not called, or NULL is supplied,
everything is output on stderr"; the implementation expresses that as a default
callback, not as an empty slot. The port models the implementation: the log
route is "default" for an unset or null registration, and
`sched_log_null_restores_default()` is `1`.

Progress arm (`llama_progress_callback` arity 2, `(float, void *) -> bool`):

- a null callback is never invoked (0 calls);
- each tensor point is invoked once before that tensor is written;
- `false` stops the load at that point, so an abort at point `k` produces
  `k + 1` calls and no final call;
- a load that is not stopped ends with one extra call at exactly `1.0`, even
  when there were no tensor points.

## 2. Gate

| slice | cases | values | `-O1` | `-O0` |
|---|---:|---:|---|---|
| `devh` | 143 | 620 | 0 failures | 0 failures |
| `cpph` | 78 | 435 | 0 failures | 0 failures |
| `sched` | 21 | 9482 | 0 failures | 0 failures |
| total | 242 | 10537 | 0 | 0 |

Every slice is above the ten-case floor. The `sched` case count is low because
one case is an exhaustive family: the eval machine is compared with a verbatim
transcription for every `n = 0..6`, every want-mask and every cancellation
point (1, 4, 12, 32, 80, 192 and 448 runs of the loop for `n = 0..6`, 769 in
total), and the progress arm for every `n = 0..5` and abort point. The summaries are byte identical at both
levels.

Boundary and refusal checks in the gates: every constant read in the wrong
direction (`devh_const_num` on a string macro, `devh_const_text` on a numeric
one), `-1` and past-the-end indices for every accessor, a declaration name that
is absent from the port, a capability index out of range, a header index out of
range, `sizeof`/`offsetof` lookups past the table, `sched_run` with
`n = -1`, `n = 33`, `cancel_at = -2`, `cancel_at = n` and an empty event
buffer, and progress arguments outside the contract.

The native arms are not reimplementations of the port:

- `devh`: the fixture expands the real macros and classifies the real
  declarations with `decltype`, and re-derives the capability matrix with an
  independent scan;
- `cpph`: the seven release functions are linker-wrapped counters
  (`-Wl,--wrap=...`), and every default is read from a compiled-in struct;
- `sched`: the reference is a transcription of the C loop, and the log rule is
  read from `libggml-base.so` through `ggml_log_get`.

The oracle also asserts the source anchors on every build: the two ask call
sites, the single observe call site, `j0..j1+1` in `ggml_graph_view`, the
`synchronize`, the `break`, `j0 = j1`, the loader's data-progress call and its
final `1.0` call. An upstream change to the loop shape fails the oracle with an
explicit message instead of leaving a stale model behind.

## 3. Mutations — 36/36, halves 6/6 per slice

Module generated and compiled at `-O0`, fixture at `-O2`, one mutation at a
time, 18 mutations per half. "Caught by" is the number of gate values that
broke.

| # | slice | mutation | caught by |
|---|---|---|---|
| 1-01 | devh | `GGML_CUDA_NAME` text becomes `CUDA2` | 1 value |
| 1-02 | devh | `GGML_CUDA_MAX_DEVICES` 16 → 8 | 1 value |
| 1-03 | devh | RPC major version 6 → 7 | 1 value |
| 1-04 | devh | a declaration is filed under the wrong header | 2 values |
| 1-05 | devh | a declaration loses a parameter | 1 value |
| 1-06 | devh | a pointer return becomes void | 1 value |
| 1-07 | cpph | the scheduler deleter calls `ggml_backend_event_free` | 1 value |
| 1-08 | cpph | the context wrapper becomes `shared_ptr` | 1 value |
| 1-09 | cpph | a deleter is declared stateful | 7 values |
| 1-10 | cpph | a live wrapper is released twice | 1 value |
| 1-11 | cpph | `release()` also releases | 1 value |
| 1-12 | cpph | the sampling `top_k` default 40 → 32 | 1 value |
| 1-13 | sched | the first ask is reported as an observation | 768 values |
| 1-14 | sched | the computed range loses its end | 768 values |
| 1-15 | sched | the scheduler observes the node it was told not to | 768 values |
| 1-16 | sched | the observe callback can no longer cancel | 321 values |
| 1-17 | sched | the event count is written one too large | 769 values |
| 1-18 | sched | a successful load drops its final progress call | 6 values |
| 2-01 | devh | `GGML_SYCL_MAX_DEVICES` 48 → 64 | 1 value |
| 2-02 | devh | the host-buffer capability answers `_buffer_type` | 2 values |
| 2-03 | devh | the `reg` capability looks for `_init` | 2 values |
| 2-04 | devh | the SYCL ceiling table disagrees with the macro | 1 value |
| 2-05 | devh | a header name changes | 1 value |
| 2-06 | devh | a numeric macro answers with a string | 8 values |
| 2-07 | cpph | `cache_floor_mb` 1536 → 1024 | 1 value |
| 2-08 | cpph | the wrapper size 8 → 16 | 7 values |
| 2-09 | cpph | the header loses an include | 1 value |
| 2-10 | cpph | `version()` invents a number without the define | 2 values |
| 2-11 | cpph | `is_mtp` accepts the ngram draft | 2 values |
| 2-12 | cpph | the carrier layout loses a field slot | 1 value |
| 2-13 | sched | the last node of a split is never asked | 226 values |
| 2-14 | sched | the node limit doubles | 1 value |
| 2-15 | sched | an aborted progress load reports one call too few | 15 values |
| 2-16 | sched | a null progress callback is still invoked | 21 values |
| 2-17 | sched | a null log callback routes to a listener | 1 value |
| 2-18 | sched | the error log level becomes a warning | 1 value |

Halves: 6/6 per slice in both halves, no survivors, no build breaks.

Two mutations were replaced after their first run. "A cancellation skips a
node that was computed" survived: the branch it edits is only reached when no
node is wanted, where `j1 == n - 1` and both `j0 = j1 + 1` and `j0 = j1 + 2`
leave the loop. It was replaced by "the event count is written one too large",
which produces 769 failures. "A declaration loses a parameter" first targeted a
zero-arity entry and changed nothing measurable; it was moved to a three-arity
entry.

## 4. Compatibility

- `goose check` passes for each module: 80, 96 and 82 specialisations.
- The 69 exported functions (`devh_` 15, `cpph_` 34, `sched_` 20) were compared
  against all 8440 function names in the canonical `goose/` tree: zero
  collisions, so no existing module is shadowed.
- No header declaration is redefined: the port carries values and shapes only.
- The workspace repository was not modified; no production file, build flag or
  launcher changed. No model, GPU or server was started.

## 5. Assumptions

1. `upstream/llama.cpp` `0b5be7e4a`, the carrier headers and
   `libggml-base.so.0.22.0` are local inputs, anchored by the SHA-256s in
   `results/hashes.txt`; a missing input fails the oracle loudly.
2. Shapes are classified by kind. `int` vs `int32_t` is not a break here;
   arity, pointer-ness and by-value structs are. Pointer type identity
   (`ggml_tensor *` vs `void *`) is not checked — a source-compatibility
   concern, not an ABI one.
3. The capability matrix is derived on both sides from the declaration lists;
   it is not a table that could drift from them.
4. The layout facts are x86-64 LP64 with `gcc-13`; the two `std::string`
   structs are excluded on purpose.
5. The log arm mutates the process-global logger. It installs a probe, records
   the pointer identity, then restores the default before returning; the gate
   is single-threaded and produces no log line while the probe is installed.
6. `sched_run` models one split graph. Cross-split behaviour (events, copies,
   the `next_copy` rotation) is outside S9 and stays with host wiring.
7. The progress model counts calls, not values. The `size_done / size_data`
   float division is not exercised, because a load with zero tensor bytes does
   not reach the loop.
8. Native `GGML_ABORT`/nullptr branches are not executed; refusals are
   compared as refusals.

## 6. Conflicts and risks

- **The carrier headers are internal.** `runtime/core/include/bmoe` is this
  workspace's runtime, not upstream llama.cpp. Their defaults are behaviour,
  not ABI: changing `io_threads` or `dense_weights` compiles everywhere and
  changes results. Freezing them in the same module as `ggml-cpp.h` is a
  packaging decision (one "C++ host headers" slice); if the carrier moves to a
  separately versioned component, S8 splits.
- **The scheduler model is one split.** A real run interleaves splits and
  backend events; the sequence inside a split is what a trace consumer sees, and
  that is what S9 freezes.
- **The log test touches a global.** Running the gate inside a host process
  with a pre-installed logger would see that logger as the "default". The gate
  runs as its own process and restores the default on exit.
- **`ggml_log_set(nullptr)` is not a clear.** The header comment is easy to
  read as "null means stderr, the slot is empty"; the implementation reinstalls
  the built-in callback. Consumers that compare the stored pointer with
  `nullptr` after clearing are wrong, and this report records the real rule.
- **Reference `.so`.** The `sched` slice needs `libggml-base.so.0.22.0` for the
  log arm; `FG_SO_DIR` overrides the path and a missing file fails the gate.
- **Overlap with EXP-649.** `bkh_` already froze the eval typedef shape; S9
  repeats the shape next to the sequence and additionally freezes the backend
  eval callback and the log/progress contracts. No value is duplicated except
  the one callback shape, deliberately, so the sequence has its callback.

## 7. What is still open for P10-08

1. Wiring the nine accepted slices into the runtime and CLI hosts (the
   header-derived tables are not consulted by production code yet).
2. The decision about the ledger library's SONAME
   (`libllama-exp371.so.0` vs upstream `libllama.so.0`), which is an owner
   decision recorded by EXP-649.
3. The batch-ownership contract (`llama_batch` fields and lifetime across an
   async decode), which is not a header-only fact.

## 8. Evidence

```
results/gate-o1.log      all three gates, -O1, 0 failures
results/gate-o0.log      all three gates, module at -O0, identical summaries
results/gate-final.log   final clean run
results/mutate-1.log     mutations 1-18, 18/18 caught
results/mutate-2.log     mutations 19-36, 18/18 caught
results/hashes.txt       sha256 of every artifact, native input and .so
```

Base: assigned `050354e9` (the checkout advanced by three documentation-only
commits during the work; no header, source or carrier file changed, verified by
the hashes). Header canon `upstream/llama.cpp`
`0b5be7e4a25862bc2777d0c47eae18788a8c963a`. Reference build
`build/exp054-dflash/bin` (ggml 0.22.0). Toolchain `goose-2026.3-string`,
`gcc-13`/`g++-13` 13.4.0.
