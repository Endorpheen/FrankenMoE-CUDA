# EXP-2026-10-26-082: memory-only prefix recovery on Flint

Verdict: ACCEPTED for the scoped latency-reduction and exact-KV-state hypothesis. Experimental only: production promotion and an atomic commit remain pending user review/approval. This does not establish general task-quality equivalence. The production runtime is unchanged; no next experiment was started.

## Question and useful DeepInfer mechanism

The user requested an investigation of efficient KV reuse and a practical RAM/VRAM-only implementation, followed by tests on the actual working launch profile. EXP-081 and its disk-cache build are retained separately and are not the basis of this implementation.

[DeepInfer's Q&A](https://github.com/deepseek-ai/DeepInfer/blob/main/doc/inference_deployment_qa.md) describes prefix matching plus state-cache recovery. Its bounded SWA replay is approximate, and decoder prefill savings depend on DeepSeek's YOCO/CED architecture. These are not generic drop-in optimizations for Flint. [The released runtime](https://github.com/deepseek-ai/DeepInfer#readme) targets DeepSeek-V4.1-Flash on SM100/Ascend and does not include the full service KV-cache system. Copying its storage architecture, kernels or model-specific computation shortcut would not reproduce its results on a 4070/Flint.

The portable idea used here is retaining bounded attention-window states in memory. This implementation uses exact native state serialization; it does not truncate attention approximately, quantize KV further, or write snapshots to disk.

## Actual local bottleneck

Native Flint/llama.cpp keeps ordinary KV for the active slot, but intentionally skips most mid-prefill SWA checkpoints. A branch may share many token IDs while lacking the corresponding SWA window. The native code then logs `forcing full prompt re-processing due to lack of cache data` and resets cached tokens to zero.

The A measurements demonstrate this with native prefix caching enabled: a 32K request sharing 24K repeats all 32768 tokens; a 64K request sharing 48K repeats all 65536 tokens. This is not a comparison against `cache_prompt:false` or `--no-cache-prompt`.

## Implementation

Source overlay: `work/llama.cpp-exp082-ram-kv`. Runtime: `build/exp082-ram-kv/bin/llama-server`. Patch: `patches/flint-memory-prefix-checkpoints.patch`.

- `tools/server/server-prefix-checkpoints.h`: bounded scheduling/eviction policy, exact window bounds, prefix eligibility and next-token resume. Default on; `GGML_SWA_PREFIX_CHECKPOINTS=0` restores native behavior. `GGML_SWA_PREFIX_CHECKPOINT_MIB` sets a 0-512 MiB checkpoint-payload budget; default 256 MiB. Invalid settings disable the extension. Zero disables it.
- `tools/server/server-context.cpp`: preserve SWA snapshots at existing prefill-batch boundaries, without reducing batch/ubatch. Keep full-attention KV in its native VRAM cache. Bound native plus new checkpoint payloads before allocation and retain useful early/latest anchors under pressure. Clamp metadata to the actual serialized SWA window. Restore a fully processed checkpoint at its next position, instead of unnecessarily replaying its already-completed last token.
- The extension is restricted to text-only, single-slot Flint with SWA, no adapters/control vectors and no speculative context/implementation. Other modes keep native behavior. Existing `--ctx-checkpoints 0` and `--swa-full` also bypass the new path. MTP code and libraries are unchanged; no MTP speedup is claimed.
- `tests/test-prefix-checkpoints.cpp` and `tests/CMakeLists.txt`: prefix mismatch, exact SWA bounds, resume, supported-mode guards, memory quota, eviction under a simulated 128K history, disable/invalid-setting tests. ASan/UBSan passed; LeakSanitizer was disabled because of the sandbox's ptrace environment.
- `tests/test-prefix-checkpoint-state.cpp`: GPU model-state roundtrip test. It serializes into vectors in RAM only. No `.bin`, slot-save or NVMe KV files are used.

The original working binary/source remain byte-identical to the pre-experiment hashes. Seven model/kernel/common libraries are copied byte-for-byte from the actual working build; only the server-context overlay and server implementation are rebuilt, with the original g++-13 flags. The isolated binary and rebuilt server implementation are linked against copied libraries. This avoids a different CUDA/kernel build as a performance confound. See build-provenance.json, library-manifest.json, provenance.json and the saved local build command list.

## Profile and execution

Both arms use the user's settings: Flint GGUF, alias flint, context 131072, all dense GPU layers, CPU MoE, fit off, Flash Attention, Q4_0 K/V, batch/ubatch 4096/4096, threads 8/16, one slot, `--cache-ram 0`, Jinja, reasoning budget 4096, temperature 1, top-p 0.95, top-k 20, min-p 0. Test API port 18282 and a separate log path avoid the working port. Each paired request uses the same fixed seed 42, exact token IDs, top-5 probability reporting and 128 generated tokens. Sampling temperature was not changed to zero.

A uses `GGML_SWA_PREFIX_CHECKPOINTS=0`; B uses the default extension behavior. Native `cache_prompt:true` is enabled in both. `--cache-ram 0` disables the native whole-prompt RAM tier, not the already-existing attention-window checkpoints; it is left unchanged.

Four server starts and twelve manually issued serial HTTP generation requests were completed: A32, B32, A64, B64. Each run uses cold, late-branch and early-branch requests. Late branches share exactly 24576/49152 tokens and change one subsequent token. Early branches share 5120 tokens and have 16384 total input tokens. Two additional standalone model-state probes checked restore/rewind ordering; no extra HTTP generation was used. No production server was stopped and no benchmark automation or concurrent build was used.

## Measurements

TTFT is curl request-header send to the first non-empty content SSE event; early HTTP headers/keep-alives are ignored. Total time includes all 128 output tokens. Prefill throughput counts only actually evaluated tokens. Decode is single-response throughput, not aggregate throughput. Each cell is one observation; no median, confidence interval or general speedup claim is implied.

| Case | Input | Cached | Evaluated | Client TTFT s | Prefill s | Evaluated tok/s | Decode tok/s | Total s |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| a32-cold | 32768 | 0 | 32768 | 27.213054 | 27.200367 | 1204.69 | 27.26 | 31.877810 |
| a32-branch | 32768 | 0 | 32768 | 27.098117 | 27.087142 | 1209.73 | 25.96 | 31.995433 |
| a32-early | 16384 | 0 | 16384 | 13.373823 | 13.367579 | 1225.65 | 29.21 | 17.724663 |
| b32-cold | 32768 | 0 | 32768 | 27.073491 | 27.060654 | 1210.91 | 27.09 | 31.766328 |
| b32-branch | 32768 | 24576 | 8192 | 7.507719 | 7.496528 | 1092.77 | 27.89 | 12.068037 |
| b32-early | 16384 | 4096 | 12288 | 10.012612 | 10.006238 | 1228.03 | 29.38 | 14.338676 |
| a64-cold | 65536 | 0 | 65536 | 58.782875 | 58.759053 | 1115.33 | 25.76 | 63.722310 |
| a64-branch | 65536 | 0 | 65536 | 58.748250 | 58.726286 | 1115.96 | 25.46 | 63.752820 |
| a64-early | 16384 | 0 | 16384 | 13.332342 | 13.324197 | 1229.64 | 28.03 | 17.867224 |
| b64-cold | 65536 | 0 | 65536 | 58.050554 | 58.026808 | 1129.41 | 25.60 | 63.021968 |
| b64-branch | 65536 | 49152 | 16384 | 16.541685 | 16.519105 | 991.82 | 24.69 | 21.698695 |
| b64-early | 16384 | 4096 | 12288 | 10.032988 | 10.026737 | 1225.52 | 29.64 | 14.321715 |

The primary latency result is a real native-cache-enabled A/B:

| Workload | Native TTFT | RAM-checkpoint TTFT | TTFT speedup | Whole-response speedup |
| --- | ---: | ---: | ---: | ---: |
| 32K, 24K shared | 27.098 s | 7.508 s | 3.61x | 2.65x |
| 64K, 48K shared | 58.748 s | 16.542 s | 3.55x | 2.94x |

B reuses 75% of input tokens in these late branches; it evaluates 8192/16384 instead of 32768/65536. The early-branch improvement is smaller: about 1.33x TTFT, because the nearest retained checkpoint covers 4096 of the shared 5120 tokens.

Cold prefill is essentially unchanged: 27.200 -> 27.061 s at 32K, 58.759 -> 58.027 s at 64K, both within 1.3%. These small differences are not claimed as cold-prefill acceleration. The optimization does not make new tokens inherently cheaper and does not speed up the first request after restart.

## Memory, disk and safety

Peak RSS A=25.816 GiB, B=25.950 GiB; incremental peak about 136.7 MiB. Logged peak checkpoint payloads are approximately 19.74 MiB native versus 157.92 MiB candidate, under the 256 MiB budget. GPU model/context/compute reservation stays 3774 MiB in both arms. Whole-device VRAM peaks A=7748 MiB / B=7737 MiB include unrelated applications and are not a claimed GPU saving.

Test-process VmSwap was zero in every resource snapshot. Global pswpout was 24966666 pages both before and after the full series, including the final state probe: no additional swap-out was observed. The system already had occupied swap; do not describe it as an entirely swap-empty machine. No OOM occurred.

No KV payload file was written. Process write_bytes stayed at log-sized values (tens of KiB), not snapshot-sized values. Checkpoints live in existing RAM vectors; full attention KV remains in native VRAM. They are lost on server shutdown. This does not add cross-restart persistence, a full session-tree cache, or deduplication of arbitrary old branches. Existing native manual slot-file APIs remain available but were not used.

## Correctness evidence and limitations

32K and 64K cold A/B outputs match in all 128 generated tokens and every returned top-5 probability. The extra checkpoints did not change cold computations.

The GPU model-state probe processed 24K, captured full-prefix and SWA states in RAM, advanced through 32K, restored SWA and removed the divergent tail in exactly the server's API order. Full prefix state before/after is 208825244 bytes, SHA-256 `d3ca24859b29163ba68c3968b6c1e2712e0115a065141b97d561922378cfdccb` on both sides. SWA-only state is 10349108 bytes, SHA-256 `5f2c8adb3a53b742125d19311b1ec5b15ae6e4c98086a544e252ddadbee1b643` on both sides. The reverse API order was checked separately and also matched. Thus the retained/recovered KV data is bit-exact, not an approximate reconstruction.

Branch outputs/probabilities are not bit-identical to a complete fresh prefill at temperature 1 with this quantized runtime. This is explicitly recorded in generation-comparison.json. Exact state recovery does not establish batch/layout-invariant floating-point inference or equal task accuracy. The 128-token cap often ends during reasoning, so this is not a completed-agent-task quality benchmark. No general generation-quality guarantee or unconditional production promotion is made. Further quality evaluation needs a separate user-approved task; none was started.

No ordinary repeat-with-identical-live-SWA speedup is claimed; that native path was already fast. No improvements for other architectures, speculative decoding or multi-slot traffic are claimed; those modes bypass the extension.

## Artifacts and lifecycle

- `metrics.json`: all twelve raw observations; `comparison.json`, `summary.json`, `resources.json`: ratios and resources.
- `generation-comparison.json`, `state-roundtrip.json`, `state-roundtrip-rewind-first.json`: correctness results and hashes.
- `provenance.json`, `source-manifest.json`, `library-manifest.json`: source/build preservation and patch hashes.
- Requests, SSE, curl traces and logs are beside these summaries. `SHA256SUMS` covers the saved evidence. No real user conversation was used.
- The patch reconstructs all five changed/new files byte-for-byte against the current working Flint source snapshot. Existing local Jinja fixes are prerequisites already present in that working fork; they were not changed or repackaged here.
- All test processes are stopped. The working runtime, working launch command, model, EXP-081 build, AGENTS.md, CLAUDE.md and roadmap are unchanged. No Git commit was made without approval. The experiment lifecycle remains pending any user-approved atomic commit; no next experiment was started.
