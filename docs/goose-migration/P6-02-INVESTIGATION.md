# P6-02 investigation: CUDA-split abort on a long prompt

Own investigation. No external consultation: the defect is diagnosed and fixed
inside this repository. This note records what is measured, what is ruled out and
the next instrument.

## Reproduction

Profile: `-ngl 0 -c 2048 -np 1`, model Apex-35B-V1.0-Q4_K_M, 371 evaluated
tokens. Short prompts (5 tokens) are served correctly, the long one aborts.

    llama-server -m Apex-35B-V1.0-Q4_K_M.gguf -ngl 0 -c 2048 -np 1 ... 
    POST /completion with a ~371-token prompt

Observed: `connection closed during the request`, process aborts with

    ggml_abort <- ggml_cuda_error("invalid argument", shared-cuda.cu:114)
              <- ggml_backend_cuda_buffer_set_tensor
              <- fg163_io <- fg163_gg_function_21 <- fg163_gg_function_1
              <- ggml_backend_tensor_copy
              <- fg164_component <- ggml_backend_sched_graph_compute_async

A `-g` diagnostic base library with a breakpoint on `ggml_backend_tensor_copy`
prints the pair directly:

    COPY src=norm-0 dst=CUDA0#norm-0#0

So under `-ngl 0` the scheduler creates a split-input copy tensor in CUDA0 and
copies a CPU activation into it.

## Isolation already done

| experiment | result |
|---|---|
| our libllama + flint `libggml-base` | long prompt served, zero CUDA errors |
| our libllama + our `libggml-base` | aborts as above, same `libggml-cpu`/`libggml-cuda` |
| `CUDA_VISIBLE_DEVICES=""` | long prompt served: the abort needs a CUDA backend to exist |
| every Goose switch off (`GGML_GOOSE_SCHEDULER=0`, `GGML_GOOSE_CPU_BACKEND_API=0`, `GGML_GOOSE_BACKEND_BUFFERS=0`, `GGML_GOOSE_STREAM_CONTROL=0`, `GGML_GOOSE_ALLOCATOR=0`, `GGML_EXPERT_PINNED_RING=0`, `GGML_GOOSE_EXPERT_PIPELINE=0`) | still aborts with two CUDA errors |
| `LD_PRELOAD` interposer on `ggml_backend_tensor_copy` | never fires; the call is bound locally (`-Bsymbolic`) |
| replacing the base with flint's | fixes the abort but costs 28.6 s vs 22.8 s (~25 %, far beyond the 2 % rule) |

## What is textually identical to flint

Split passes 2-4, `compute_splits`, `tensor_alloc`, both `reserve` functions,
`ggml_backend_tensor_copy`, `ggml_backend_sched_backend_from_buffer` and the
`hv_tensor_copies` sizing are line-identical once the hook lines are stripped.
The whole-file diff is 253 changed lines out of about 2 600.

## The specific unknown

Our replacement scheduler is a newer revision than the code it replaces: it uses
`sched->expert_pipeline` in 16 places, while the flint `ggml-backend.cpp` in this
build has no such field. The pipeline pays for itself (that is why flint's base is
25 % slower), so it cannot simply be dropped.

Diagnostic plan (own work, in order):

1. Instrument `ggml_backend_sched_split_graph` in a diagnostic base to log every
   split's `backend_id`, its node names and the cause tag, under `-ngl 0` with the
   long prompt. Diff that log against the flint base on the same request.
2. If a split is assigned CUDA0, print `sched->n_backends`, each backend's name and
   device at `ggml_backend_sched_new`, and the `backend_from_buffer` result for the
   offending node. `-ngl 0` should leave only CPU and (maybe) an unused CUDA
   backend; the assignment order is the suspect.
3. Check whether the Goose scheduler hooks (`fg164_*`) or the expert-pipeline
   prologue run on this request at all: with the pipeline disabled they must be
   inert, and a diagnostic counter proves it.
4. Reduce to a scheduler-only C harness: one CPU source tensor, one copy node,
   `ggml_backend_sched_graph_compute` with a CUDA backend registered but untasked.
   If that reproduces, the fix belongs in the scheduler object; if not, the trigger
   is earlier in llama.cpp graph construction and the harness must mirror it.

## Artifacts to inspect

- deployed base: `runtime/baseline-artifacts/libggml-base.so.0.e213fa19`
  (sha256 `e213fa19…`; preservation note in
  `docs/goose-migration/P4-03-REPRODUCIBILITY.md`)
- scheduler replacement source:
  `experiments/EXP-2026-11-08-164-goose-scheduler/source/backend-scheduler.cpp`
- build recipe: `experiments/EXP-2026-11-08-164-goose-scheduler/build-commands.json`,
  label `scheduler-base-link`
- probe evidence: `experiments/EXP-2026-11-12-304-p6-03-probe/README.md`,
  `goose.log`, `native.log`
- isolation arms: `experiments/EXP-2026-11-12-305-sched-isolation/`
- diagnostic build and gdb script: `experiments/EXP-2026-11-12-307-sched-debug/`

## What the fix must satisfy

- the 371-token request is served exactly like the flint base line
  (`c5ca940bbe49eaa5` for the 8-token continuation);
- the Apex acceptance keeps `e68df05b`, exact ids and probabilities;
- decode stays within the 2 % rule of the accepted 41.62 / 43.04 tok/s and VRAM
  does not grow beyond the recorded figure.

## Fresh reproduction (2026-11-13, own run)

Our line, `-c 2048 -ngl 0 -np 1`, 1 800-character prompt, port 18094:

    READY=1
    ALIVE_AFTER_REQUEST=1
    SERVER_EXIT=134
    HTTP_TAIL=HTTP=000
    CUDA_ERRORS=2
    ABORT=1
    LEFT_PROC=0

The backtrace is the same one recorded before: `ggml_backend_cuda_buffer_set_tensor`
→ `fg163_io` → `fg163_gg_function_21` → `ggml_backend_tensor_copy` →
`fg164_component` → `ggml_backend_sched_graph_compute_async`. The copy runs through
the Goose scheduler module, so the defect is in our scheduler code, not in the
C++ wrapper around it.

Script and result: `experiments/EXP-2026-11-13-465-p6-02-repro/`.

## Where the search stands

`ggml_backend_sched_backend_from_buffer` in both revisions returns the first
backend that supports the buffer *type* and the op, and the Goose port
(`fg164` `from_buffer`) mirrors it through the exact bridge
`ggml_backend_supports_buft` + `ggml_backend_supports_op`. The failing copy tensor
is named `CUDA0#…`, so either the buffer type passed in is not the one the tensor
holds, or the backend order makes CUDA0 the first match.

Next probe, in this order:

1. At the breakpoint from EXP307, print `sched->n_backends`, each backend's name,
   and the offending tensor's `buffer->buft` plus the name returned by
   `ggml_backend_buft_name`. If CUDA0 answers true for a CPU buft, the bridge or
   the field offset for `buft` is wrong.
2. Print `sched->backend_ids` for the source and the created copy to see which
   index `from_buffer` returned and for which buffer.
3. Only after that, decide between a field-offset bug in the Goose ABI and an
   ordering bug in `ggml_backend_sched_new`.

## Probe results (2026-11-13, diagnostic base EXP307)

`ggml_backend_sched_new` reports two backends in this order:

    backend[0] = CUDA0
    backend[1] = CPU

`ggml_backend_tensor_copy` on the failing request shows the source buffer of every
offending copy is a CUDA buffer even though the run is `-ngl 0`:

    TENSOR_COPY src=norm-0 dst=CUDA0#norm-0#0        sbuf=CUDA_Host  dbuf=CUDA0
    TENSOR_COPY src=blk.0.ssm_norm.weight dst=...    sbuf=CPU_Mapped dbuf=CUDA0
    TENSOR_COPY src=node_55 dst=CPU#node_55#0        sbuf=CUDA0       dbuf=CUDA_Host

So the scheduler is not inventing a CUDA destination: `norm-0` is already stored in
a **CUDA_Host** buffer, the CUDA backend supports that buffer type, and
`backend_from_buffer` therefore returns CUDA0. Per-layer SSM norms are correctly in
`CPU_Mapped`; the global/output norms are the ones placed in CUDA host memory.

This moves the target: the question is not "why does the scheduler pick CUDA" but
"why does weight placement put `norm-*` in a CUDA host buffer with `-ngl 0`". The
placement decision is the Goose module behind `FG404S_WEIGHT_PLACEMENT`, and the
buffer-type support answer comes from the base library's device traits - the half
the isolation test already identified as faulty.

Next probes, in this order:

1. Enable the placement diagnostics and log the decision (and the device/buft it
   names) for `norm-0` and `output_norm.weight` under `-ngl 0`.
2. Compare the same log against the flint base: if flint also reports CUDA_Host for
   those tensors, the divergence is in the scheduler; if flint reports CPU_Mapped,
   the divergence is in the placement decision or the device trait table.
3. Once the wrong decision is identified, fix it in the module (or in the trait
   mapping), rebuild the diagnostic base, and require both prompt shapes plus the
   Apex acceptance.

## Placement-flag test (2026-11-13)

Running the same request with `GGML_GOOSE_WEIGHT_PLACEMENT=0`, which restores the
native `select_weight_buft` branch, still aborts:

    p602-native-placement READY=1 HTTP=000 EXIT=134 CUDA=2 ABORT=1 LEFT=0

So the Goose placement decision is not the trigger. What remains is the input to
that decision: the per-layer buffer-type list and the device capability answers.
Under `-ngl 0` the list should contain only the CPU buffer type, yet `norm-0` ends
up in `CUDA_Host`, so either the layer's device is CUDA (offload count misread) or
the CPU entry was rejected by `weight_buft_supported` and CUDA was taken instead.

Next probes:

1. At `select_weight_buft`, print the list contents (device name + buft name) and
   the returned buft for `norm-0` and `output_norm.weight`.
2. If the list contains CUDA, print `n_gpu_layers` and the layer device assignment
   at load time; that is the actual divergence from `-ngl 0`.
3. If the list is CPU-only, print `weight_buft_supported` and the
   `ggml_backend_dev_supports_op` answer for the norm op on the CPU device.

## Status

Open, worked locally. The next instrument is the split-assignment log described
above; the scheduler-only harness follows from what it shows.
