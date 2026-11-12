# Blocker: our line aborts on a long prompt inside the scheduler, and the base library is not reproducible

Status: local agent stopped after eight meaningful attempts, per the blocker rule. Escalated to Igor for the senior helper.

## Expected

Run with `-ngl 0`, `-c 2048`, `-np 1`: a 371-token prompt is served exactly as the flint line serves it
(`c5ca940bbe49eaa5` for the 8-token continuation). With no layer offloaded the scheduler has no reason to involve CUDA.

## Actual

    connection closed during the request; process aborts
    ggml_abort <- ggml_cuda_error("invalid argument", shared-cuda.cu:114)
              <- ggml_backend_cuda_buffer_set_tensor
              <- fg163_io <- fg163_gg_function_21
              <- ggml_backend_tensor_copy
              <- fg164_component <- ggml_backend_sched_graph_compute_async
              <- llama_context::graph_compute <- process_ubatch

A `-g` build of the scheduler object with a breakpoint on `ggml_backend_tensor_copy` prints the pair directly:

    COPY src=norm-0 dst=CUDA0#norm-0#0

so the scheduler creates a split-input copy tensor in the CUDA0 backend and copies a CPU activation into it.

## What was tested

1. **Prompt shape**: 5 tokens fine, 371 tokens aborts. Deterministic.
2. **Switches**: `GGML_GOOSE_SCHEDULER=0`, `GGML_GOOSE_CPU_BACKEND_API=0`, `GGML_GOOSE_BACKEND_BUFFERS=0`,
   `GGML_GOOSE_STREAM_CONTROL=0`, `GGML_GOOSE_ALLOCATOR=0`, `GGML_EXPERT_PINNED_RING=0`,
   `GGML_GOOSE_EXPERT_PIPELINE=0` -- all still abort with two CUDA errors.
3. **Isolation by library**: our libllama with flint's `libggml-base` serves the long prompt with zero CUDA errors; with our
   `libggml-base` it aborts. Same `libggml-cpu`, same `libggml-cuda`, same `libllama`.
4. **GPU hidden**: `CUDA_VISIBLE_DEVICES=""` serves the long prompt, so the abort needs a CUDA backend to exist.
5. **Source diff against flint** (hooks stripped): passes 2-4 of the split, `compute_splits`, `tensor_alloc`, both `reserve`,
   `ggml_backend_tensor_copy`, `backend_from_buffer`, `hv_tensor_copies` sizing and the `sched_new` prologue are line-identical.
   The substantive remainder is the expert-pipeline block (102 non-blank lines), inert because nothing calls
   `ggml_backend_sched_set_expert_pipeline`.
6. **Interposition**: an `LD_PRELOAD` wrapper on `ggml_backend_tensor_copy` never fires because the library is linked `-Bsymbolic`.
7. **Rebuild from recorded sources**: produces a different artifact (`95a29c73` vs deployed `e213fa19`), still aborts, and
   returns `9a260527` instead of the accepted `1da0f499` for the 5-token prompt. So a faithful rebuild changes numerics, and the
   deployed artifact cannot be recreated from what is checked in.
8. **Deployment inputs**: the library name is `sha256(H(scheduler object) + H(goose unit))`; recomputing it from current sources
   yields `bafe3955c9ea`, the deployed name. So the EXP164 pair in the deployment is byte-identical to current sources. The
   other five Goose objects (`shared-core.o`, `shared-meta.o`, `allocator.o`, `buffers-goose.o`, `stream-goose.o`) were
   missing from disk and were rebuilt today from their recorded steps.

## Remaining hypotheses

- One of those five objects changes scheduler-visible state (buffer usage flags, `buft` identity, host-pointer detection), so a
  split that flint keeps on CPU is offered to CUDA in our build.
- The deployed bytes of those five objects are not what their sources now produce, which would also explain the napkin math of
  a faithful rebuild changing numerical output.
- A `ggml_backend_expert_pipeline_v1` provider may exist in our CUDA backend, enabling a path that the flint base has no code
  for; the enable call was not found in our trees, but the flint llama may call it in an object we did not rebuild.

## Paths

- deployed base: `runtime/reconstructed/build/stage-run/libggml-base.so.0` (`sha256 e213fa19...`)
- link recipe: `experiments/EXP-2026-11-08-164-goose-scheduler/build-commands.json`, label `scheduler-base-link`
- scheduler source: `experiments/EXP-2026-11-08-164-goose-scheduler/source/backend-scheduler.cpp`
- diagnostic build: `experiments/EXP-2026-11-12-307-sched-debug/` (`-g` object, debug library, gdb script)
- probe log and evidence: `experiments/EXP-2026-11-12-304-p6-03-probe/README.md`
- isolation stage: `experiments/EXP-2026-11-12-305-sched-isolation/`

---

# Second blocker: signed zero in the ported make_qkx3_quants (P2-13, encoder direction)

Status: parked after three attempts, for the same senior-helper consultation as the scheduler blocker above.

## Expected

The Goose port of `make_qkx3_quants` reproduces the reference bit for bit, as every other ported quantizer does.

## Actual

`EXP330_QKX3 checks=39600 failures=55`, and every failure is one class: the returned minimum is `-0` in the reference and `+0` in
the port. Scales and all 32 levels match in every case.

## What was tested

1. The port's final line was `0 - min`, which yields `+0` for `min = +0` under IEEE; replaced with an explicit negation primitive
   `fg330q_neg` that returns `-v` and preserves `-0`, and the call was verified present in the generated C in all three places.
2. The gate still reports the same 55 failures after that change, so the final negation is not the source.
3. By elimination the sign is already lost upstream, in `this_min = (sum_l2*sum_x - sum_l*sum_xl)/D` or in the assignments that
   follow it, including the `this_min > 0` clamp.

## Why it is not papered over

The encoded block stores `dmin` as fp16, where `-0` and `+0` differ in bytes (`0x8000` versus `0x0000`), so this corner would
show up in the end-to-end `quantize_row_q4_K_ref` comparison that is the real acceptance for this helper.

## Paths

- `experiments/EXP-2026-11-12-330-p2-13-qkx3/` (Goose source, mechanically extracted oracle, fixture, README)
- `experiments/EXP-2026-11-12-329-p2-13-qp-quants/` (the other helper, green with a stated blind spot)

## Update: the rounding bug is fixed, and the signed-zero issue survives it

After `nearest_int` was corrected to this fork's formula (`(bits & 0x007fffff) - 0x00400000`), both helpers were re-run with matching
oracles:

- `EXP329_QP checks=12400 failures=0` -- `make_qp_quants` is green with the correct rounding.
- `EXP330_QKX3 checks=39600 failures=50` -- down from 55, still only the signed-zero class (`ref=-0 ours=0`).

So the signed-zero defect is **not** an artefact of the broken rounding helper; it survives a correct one. The blocker stands, with a
smaller count. Both fixtures and both bridges now use the fork's formula, which also means the earlier numbers for these two
experiments are superseded rather than merely annotated.

---

# Third blocker: IQ4_XS encoder super-scale does not match the reference (P2-13)

Status: parked for the same senior-helper consultation.

## Expected

Byte-identical encoding against the exported `quantize_row_iq4_xs_ref`, as the five other encoders achieve.

## Actual

`EXP336_IQ4XS_ENC checks=3944 failures=126`. First failure: s=1 (k=512), block 0. The block's stored `d` differs:

    native 0x204c -> +0.00839
    port   0xa052 -> -0.00844

Same magnitude class, opposite sign and a slightly different magnitude.

## What was tested

1. The per-block scale search matches a transcription bit for bit on this input (all eight block scales).
2. `max_scale = 0.270113` and `dsup = -0.008441` from the port and from the transcription, identically.
3. Neither sign convention for the initial `d0` reproduces the native value: `sgn=0` gives `dsup=-0.008441`, `sgn=1` gives
   `-0.008593`, while the native byte implies a negative `max_scale` of about `-0.2685`.
4. The 256-element case passes entirely; the failure starts at 512, so it is not a general sign inversion.
5. Earlier instrumentation attempts in this branch were themselves broken twice (a debug export that was never emitted because `main`
   did not reference it, and a comparison that used the port's own arrays as the reference), both now corrected.

## Why it is parked

Three attempts on the same branch, each producing a measurement rather than a guess, and the remaining difference is a specific
numeric relationship (`|max_scale|` 0.2685 versus 0.2701 with a sign flip) that reading the source has not explained. The end-to-end
gate cannot be made green by patching bytes at this point without knowing which of the two values the reference actually computes.

## Paths

- `experiments/EXP-2026-11-12-336-p2-13-iq4xs-encode/` (Goose source, bridge with the debug export, fixture, README with the full
  trail of corrections)
