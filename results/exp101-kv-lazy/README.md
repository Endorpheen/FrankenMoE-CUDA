# EXP-2026-10-30-101: lazy physical KV mapping

Measured Apex memory candidate, preserved for manual use. The author's five loaded pairs and Codex's independent clean qualification both observe4428 ->3188MiB sampled process VRAM (-1240MiB/-28.0036%). This is above EXP090, on the fixed32K workload; it is not an upstream comparison or an all-context memory guarantee. The broader compatibility and failure-mode matrix is incomplete, so this delivery does not claim universal production validation.

## Mechanism

Reserve the full KV virtual address range and map/zero physical backing as the active KV range grows. Configured context262144, KV precision, tensor addresses/shapes/strides and arithmetic remain unchanged. Physical mappings grow with use; larger requests retain more memory. The startup log's1440MiB KV size is logical capacity, not currently committed physical bytes. The existing EXP087 lazy compute reservation and EXP089/090 mmap registration remain in the bundle;120 expert ranges are registered in the independent qualification.

The prepared path defaults on in its supported CUDA/VMM/non-transposed offloaded-KV configuration; GGML_KV_LAZY_VMM=0 disables it. Source documents old allocation for unsupported configurations. Live disable/fallback, no-alloc, failure-injection, state-file roundtrip and Flint tests were not independently run by Codex. A physical mapping failure currently aborts; no mid-run fallback is claimed.

## Evidence and limits

Author: five fresh-process balanced pairs AB/BA/AB/BA/AB, Apex Q4_K_M, context262144, all40 CPU expert layers, batch/ubatch4096, threads8/16, masks0f0f/ffff, poll0, FA/Q4_0 KV, mmap, one slot,32 checkpoints, cache-ram0, no speculation/EHS. Each arm cold32768 plus identical repeat, up to128 outputs, seed123, temperature1/top_p0.95/top_k20/min_p0, EOS respected. VRAM4428 ->3188MiB in all five pairs. Some system swap-out windows and heavy CPU load exclude these timings from clean speed acceptance; paired timing deltas remain descriptive. Saved raw SSE/VRAM/resource evidence is hashed, not published as prompts or conversations.

Codex independent qualification, one A-B pair/four requests after Ghostty exit: cold single-response decode29.71974 ->28.88707tok/s, repeat28.06800 ->28.11341tok/s; sampled post-ready block VRAM peak4428 ->3188MiB. Both cold/repeat selected IDs, ordered top5 and probabilities match exactly, delta0. Every request has process swap0/system swap-out0/major faults0. This qualification supports preserved speed range and the memory result; it is not a new five-pair speed claim. Audio8 TTS remained loaded identically during these four requests and was stopped afterward for future measurements. Ghostty was already idle at the initial CPU sample, so its causal responsibility for the earlier slowdown is not established.

The author also records a24576 checkpoint branch with identical output and a261632-input/128-output capacity run matching archived EXP089 output exactly, with sampled candidate peak6388MiB. These are retained author results, not new Codex full-context or Flint runs. Near-full memory savings are not asserted.

## Source, runtime and manual launch

Apply patches/kv-lazy-vmm.patch after the four prerequisite patches in source-provenance.json on upstream source1b44e30775dcb2f93952116b05f89e5bd797fb00. The incremental patch dry-runs against the preserved pre-change files. Overlay source hashes and original build scripts are retained. Recorded build scripts describe past execution; redirect all output/dependency paths to a fresh directory before replaying them. Only libllama and libggml-cuda change; the other seven files match EXP090. Use the complete nine-file manifest rather than the executable's unchanged version/hash alone.

The tested local bundle is /home/end0/AI/LlamaServer/exp101-kv-lazy/run. launch-profiles.json records the exact Apex command and LD_LIBRARY_PATH. Free port8081 before starting it. Existing launchers and installed profiles were not switched. At delivery, the original EXP090 service was restored and all nine mapped hashes verified; switching to EXP101 is manual as requested. Old accepted builds remain available.

This commit preserves only EXP101. The six other OpenCode directions are not accepted runtime changes; their reports remain separate. Raw evidence, weights and binaries stay local. No hardware usage accounting, private instructions or helper conversations are published.
