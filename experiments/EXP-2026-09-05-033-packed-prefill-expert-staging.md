# EXP-2026-09-05-033 — Packed prefill expert staging

Status: REJECTED

## Hypothesis

Bulk prefill spends substantial time submitting 17,886 small H2D copies of selected expert weight slices. For multi-token `MUL_MAT_ID` only, gathering each layer's selected expert slices into compact pinned staging buffers, transferring larger contiguous payloads, and remapping IDs to packed slots can reduce transfer submission overhead and allow later transfer/compute overlap.

## Scope

- Prefill only; single-token decode retains the existing path.
- Same expert IDs, quantization, router output, and arithmetic result.
- Isolated source and build directories only.
- No change to EHS default, model files, GGUF layout, or production launcher.

## Existing constraints

- EXP-032 measured 17,886 selected-expert H2D copies totaling 22,160.56 MiB for one 391-token bulk prefill. The H2D submission span was 3,634.692 ms of the 3.78 s bulk-prefill interval.
- The existing hot-store tier graph is intentionally disabled above eight tokens and is not a valid prefill switch.
- The old hot-store/cache direction was rejected for decode. This experiment does not re-enable it.
- Router IDs for layer N+1 are unavailable until layer N FFN has been evaluated. Cross-layer prediction is not assumed.

## Design gate

The scheduler already materializes the required boundary. In `ggml_backend_sched_compute_splits`, the selected-ID tensor is synchronized and read before `copy_experts` submits the expert slices for the corresponding `MUL_MAT_ID` node. No new router callback or graph synchronization is needed.

The implementation point is therefore the existing selected-expert copy branch in `ggml-backend.cpp`. A packed path must replace only its many sparse slice submissions; it must not alter graph topology or add a router boundary.

## Candidate implementation

1. Reuse the scheduler's already materialized and deduplicated selected IDs for one prefill `MUL_MAT_ID` node.
2. Copy its selected contiguous expert runs into a CUDA-pinned host buffer.
3. Submit one contiguous H2D payload per projection, then scatter it on the GPU back into the original expert offsets.
4. Keep the original IDs and `MUL_MAT_ID` layout; no routing or numerical computation changes.
5. Reuse the staging buffer only after the GPU event has completed; leave decode and all fallback paths unchanged.

## Correctness gate

- Full backend operation tests must pass.
- A deterministic fixed request must complete with the same router IDs and response token stream as the control build for non-speculative prefill evaluation.
- No CUDA errors, memory leaks, swap, or unexplained VRAM/RAM increase.

## Benchmark gate

- At least five sequential paired prefill runs, fixed prompt and model flags.
- Report prefill time separately from decode.
- Accept only a median improvement of at least 3% beyond normal variance, or a documented material reduction in H2D calls/time with no sustained regression.
- If the gather/remap cost offsets the reduced copy count, reject and preserve only this report and any useful patch.

## Results

- Isolated source tree: `work/llama.cpp-exp033-packed-prefill`.
- Isolated build: `build/exp033-packed-prefill`.
- New path was opt-in only through `GGML_PACKED_PREFILL_STAGE=1`; the normal path remained unchanged.
- Fixed configuration: RTX 4070, 12 CPU threads, 32K context, EHS disabled, 395-token prompt and deterministic 16-token response.
- Experimental prefill: 6502.22 ms, 60.75 tok/s.
- Control prefill: 3979.34 ms, 99.26 tok/s.
- Delta: +63.40% prefill time (slower); this is far beyond normal variation.
- Experimental decode: 19.83 tok/s; control decode: 20.85 tok/s.
- Both responses had identical content SHA-256: `24122785e34e88b37d241f7c4f1f303575ab88c7065d05cbc6c0f9a7c60085b7`.
- Both runs had identical response metadata relevant to determinism: 16 predicted tokens, 395 evaluated tokens, temperature 0, 11 MTP draft tokens, 9 accepted.
- No CUDA errors; VRAM returned to 3579 MiB after each server shutdown.

## Rejection reason

The implementation is mathematically correct but serializes the prefill transfer path. To safely reuse one pinned host staging buffer, it waits for the GPU event after every packed projection before refilling that buffer. The synchronization cost outweighs fewer H2D submissions by a large margin.

The useful patch is retained at `experiments/rejected/EXP-2026-09-05-033-packed-prefill-expert-staging.patch`. Do not revive this single-buffer design. A future attempt needs a bounded multi-slot pinned ring with events, so CPU gathering of the next payload can continue while the GPU consumes prior payloads; it must be tested first under the same fixed request.
