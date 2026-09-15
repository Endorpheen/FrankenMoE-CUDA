# EXP-2026-09-16-065: decode host-serialization readback - build and model-free gate

Status: candidate implemented, built, model-free gate PASSED. No model and no server was
started. No commit was made. This record precedes any decode A/B.

Design input: `experiments/designs/exp057/D1-host-serialization.md` (DEC-1). The note's named
site `ggml/src/ggml-backend.cpp:1673-1674` (weights-branch ids readback) is prefill-only. The
decode-relevant equivalent is the generic split-input copy fallback at
`ggml/src/ggml-backend.cpp:1745-1752`.

## 1. What was implemented and why only this part

Candidate implemented: route the **device -> host** split-input readback through the backend
async interface (`ggml_backend_tensor_get_async` on the source compute stream) and keep a
single wait, instead of a full source drain followed by a blocking per-thread-stream copy.

Original fallback body:

```
ggml_backend_synchronize(input_backend);            // full drain of the source backend
if (events[split_backend] != NULL) event_sync else ggml_backend_synchronize(split_backend);
ggml_backend_tensor_copy(input, input_cpy);          // device -> host: get() on cudaStreamPerThread
                                                     // + cudaStreamSynchronize(cudaStreamPerThread)
```

Two host-blocking waits were paid per device -> host crossing. Because
`ggml_backend_cuda_cpy_tensor_async` returns false when either side is not CUDA
(`ggml-cuda.cu:2716`), this fallback is the decode path for the router ids and the MoE input
activations that the CPU expert split consumes.

Correctness argument (device -> host, `input_backend` is the device):

- `ggml_backend_tensor_get_async(input_backend, ...)` enqueues the D2H on the source backend's
  own compute stream (`cuda_ctx->stream()`, `ggml-cuda.cu:2687`). It is therefore ordered behind
  every producer that wrote `input`, by ordinary single-stream ordering. The pre-drain is
  redundant for that purpose.
- The single `ggml_backend_synchronize(input_backend)` that follows waits for the copy and for
  all prior source-stream work, so `input_cpy->data` is valid before the CPU split reads it. The
  destination is a host backend (CPU) whose `synchronize` is a no-op and whose events are NULL
  (`ggml-cpu.cpp`), so dropping the redundant destination wait changes nothing.
- End state is identical to the original: after the readback the source backend is idle and the
  host buffer holds the same bytes. No new asynchronous work survives the readback, so no new
  aliasing window is opened. The bytes copied are the same pointer/length expression as
  `ggml_backend_tensor_get`: `tensor->data + offset`, `ggml_nbytes(input)`.

Gate: the change is behind `GGML_SCHED_READBACK_ASYNC` (default off, `=0` also disables). The
async branch additionally requires `input_backend->iface.get_tensor_async != nullptr`, so a
backend without the interface keeps the original blocking path.

Excluded on purpose, with reasons:

- Note Candidate A (allocate scheduler events when `n_copies == 1`): not implemented. With
  `n_copies == 1` every split of a backend shares one input-copy buffer, so the per-backend event
  is not an equivalent replacement for the per-backend host drain that protects that shared
  buffer; a clear no-use-after-overwrite proof was not reached. The note itself flags this as the
  change most likely to break scheduler semantics. The events branch is left unmodified, so the
  shipped `n_copies == 1` semantics are byte-for-byte the accepted runtime's.
- Note Candidate B host -> device half (`set_tensor_async` for CPU -> GPU activation upload): not
  implemented, for the smallest safe diff. Its only gain over the current code is removing one
  per-thread-stream wait, while it makes the lifetime of the transient host source depend on
  graph-order rather than a completed copy. Device -> host was the named readback path and has an
  airtight argument; the upload path was left blocking.

## 2. Trees and build

- Source: `work/llama.cpp-exp065-hostser`, an exact copy of `work/llama.cpp-exp046-default`.
- Only one file differs between the trees: `ggml/src/ggml-backend.cpp`
  (exp046 sha `eb985111f709f6fcd895316eba9c6b7d8e5bf2512dce5b1518e6c7ff07081f41`,
   exp065 sha `9f102a02f18be9d5389c2ce1d16cd4ffd9929a8963832e2b9b6f683e894f710c`).
- Build: `build/exp065-hostser`, same configure flags as `build/exp046-default-runtime`
  (Unix Makefiles, Release, shared libs, CUDA arch 89, OpenMP on, OpenSSL on, server on,
  tests on). The normalized `CMakeCache.txt` is byte-identical to exp046's after replacing only
  the source/build paths (normalized sha `b3a6f489630c978f2c23fffd6aa91b79153c6e171d4f70af77b38f7f04854381`).
- Build command: `cmake --build build/exp065-hostser -j10 --target llama-server test-backend-ops`,
  exit 0, wall 4:13.72, max RSS 694172 KiB. Only 2 warnings, both pre-existing
  (`-Wmissing-field-initializers` in `ggml-backend-meta.cpp` and `ggml-cpu.cpp`), unrelated to
  this change; `ggml-backend.cpp` compiled warning-free.

Binaries:

| Artifact | sha256 |
| --- | --- |
| `bin/llama-server` | `ddbe62ddebc474f5e0a503d49219953d1561d48c64abaa8c508da4c906f05b78` |
| `bin/test-backend-ops` | `cc787ca0b2d5e46778b6f62f2388939c36d53a602bc97fec9aaa9eb6ad1d66e8` |
| `bin/libggml-cuda.so` | `73b12a4a4825ed5fc595f9ee2ca90e8d845b422e178168103c8b8cd6730b2e27` |
| `bin/libggml.so` | `9992518441c2043ba4f3f78b4ad3ba44b20d2d5cab25e7c582d7f2d754c8c98f` |
| `bin/libggml-base.so` | `cfe405e433d183c857d79281fc237050772c0fc65bd7252bf01c2e7acfd3f6dc` |

## 3. Exact diff

Patch: `EXP-065-ggml-backend.patch`, sha256
`55a0a7642869a448ad0da7e5cb3fb171fa2127e09eb001d5202264e023818321`.

```
--- a/ggml/src/ggml-backend.cpp
+++ b/ggml/src/ggml-backend.cpp
@@ -1743,13 +1743,27 @@
                     // try async copy, but if not possible, we can still use a sync copy without synchronizing the dst backend, since we handle the synchronization here with multiple copies and events
                     // TODO: add public function to facilitate this, since applications do not have direct access to the backend interface
                     if (!split_backend->iface.cpy_tensor_async || !split_backend->iface.cpy_tensor_async(input_backend, split_backend, input, input_cpy)) {
-                        ggml_backend_synchronize(input_backend);
-                        if (sched->events[split_backend_id][sched->cur_copy] != NULL) {
-                            ggml_backend_event_synchronize(sched->events[split_backend_id][sched->cur_copy]);
+                        // EXP-065: device -> host readback on the source stream, then a single wait
+                        static const bool readback_async = []() {
+                            const char * v = getenv("GGML_SCHED_READBACK_ASYNC");
+                            return v != nullptr && strcmp(v, "0") != 0;
+                        }();
+                        const bool d2h_async = readback_async &&
+                            !ggml_backend_buffer_is_host(input->buffer) &&
+                            ggml_backend_buffer_is_host(input_cpy->buffer) &&
+                            input_backend->iface.get_tensor_async != nullptr;
+                        if (d2h_async) {
+                            ggml_backend_tensor_get_async(input_backend, input, input_cpy->data, 0, ggml_nbytes(input));
+                            ggml_backend_synchronize(input_backend);
                         } else {
-                            ggml_backend_synchronize(split_backend);
+                            ggml_backend_synchronize(input_backend);
+                            if (sched->events[split_backend_id][sched->cur_copy] != NULL) {
+                                ggml_backend_event_synchronize(sched->events[split_backend_id][sched->cur_copy]);
+                            } else {
+                                ggml_backend_synchronize(split_backend);
+                            }
+                            ggml_backend_tensor_copy(input, input_cpy);
                         }
-                        ggml_backend_tensor_copy(input, input_cpy);
                     }
                 }
             }
```

## 4. Model-free gate

### 4a. test-backend-ops, affected op

`build/exp065-hostser/bin/test-backend-ops -o MUL_MAT_ID`, exit 0.

- 869/869 tests passed; 2/2 backends passed (CUDA0, CPU).
- Log: `tests/test-backend-ops.log`.

### 4b. Accepted-runtime scheduler harness (EXP-041 default-on ring, re-run on exp065)

`tests/run-model-free-exp065-ring.sh` (8 modes: ring default/ON/OFF/alloc-fail + sched
default/ON/OFF/alloc-fail). 0 FAIL lines in all 8 logs.

- Ring checks: 9/9, 9/9, 8/8, 9/9.
- Sched checks: 4/4, 4/4, 3/3, 3/3.
- Output equality: `sha256(out-sched-{default,on,off,fail}.bin)` is
  `39505119b144ea286e865119b397eecff75578881c24c4aa1cec9bd642e91510` for all four modes, the
  accepted EXP-038/040/041/043/062/064 reference. See `tests/log-sched-sha.txt`.

### 4c. Decode-shaped readback check (the changed path)

`tests/run-model-free-exp065.sh`, built from `tests/test-exp065-sched.cpp`:

- `prefill` mode reproduces the same accepted prefill sha
  `39505119b144ea286e865119b397eecff75578881c24c4aa1cec9bd642e91510`.
- `decode` mode: batch 3, host MoE weights, `ids` and activations in CUDA buffers, no offload
  hint, so the scheduler confirms `MUL_MAT_ID runs on the CPU` with `single CPU split` and both
  CUDA inputs cross device -> host.
  - flag off output sha `faab2b50b2feab884a731c04033313513a8765252933698606c2d65730942778`
  - flag on  output sha `faab2b50b2feab884a731c04033313513a8765252933698606c2d65730942778`
  - byte-identical.
- Path-taken proof (gdb breakpoint count on `ggml_backend_tensor_get_async` in the decode run):
  flag off -> 0 calls, flag on -> 2 calls (the two CUDA inputs). This shows the new branch is
  actually executed when enabled and is not executed when disabled.
- Logs: `tests/log-prefill.txt`, `tests/log-decode-off.txt`, `tests/log-decode-on.txt`,
  `tests/log-sha.txt`.

## 5. Scope and next step

This record proves build identity, byte-correctness, and that the target branch executes. It does
**not** measure decode performance. A paired decode A/B (flag off vs on, same binary) is the next
step and requires explicit approval under AGENTS.md hard rule 1.

Expected effect, from the design note: roughly 1-4 percent of the decode step, central about 2
percent; below or at the 2 percent acceptance threshold.

## 6. Files created

- `work/llama.cpp-exp065-hostser/` (tree copy; one edited file `ggml/src/ggml-backend.cpp`)
- `build/exp065-hostser/` (build tree)
- `results/archive/EXP-2026-09-16-065/EXP-065-ggml-backend.patch`
- `results/archive/EXP-2026-09-16-065/REPORT.md`
- `results/archive/EXP-2026-09-16-065/tests/test-exp065-ring.cpp`
- `results/archive/EXP-2026-09-16-065/tests/test-exp065-sched.cpp`
- `results/archive/EXP-2026-09-16-065/tests/run-model-free-exp065-ring.sh`
- `results/archive/EXP-2026-09-16-065/tests/run-model-free-exp065.sh`
- `results/archive/EXP-2026-09-16-065/tests/{log-*.txt,out-*.bin,test-exp065-ring,test-exp065-sched}`
- `results/archive/EXP-2026-09-16-065/tests/{configure.log,build.log,test-backend-ops.log,cmakecache-exp046.normalized,cmakecache-exp065.normalized}`
