# EXP-2026-10-26-081: exact persistent prefix KV caching on Flint

Status: REJECTED for production promotion; experimental implementation retained. The covered functional and performance checks passed. The strict system-wide zero-swap gate was not met, and real-model MTP validation is still open. No commit or production switch was made.

## Hypothesis and scope

Persisting exact sequence states on local NVMe can avoid repeated 32K-64K prefill across agent sessions and server restarts. No approximate attention replay is allowed. A bounded cache must fail closed to a cold prefill when tokens, the model, context parameters, or integrity checks do not match.

Reference: [DeepInfer deployment Q&A](https://github.com/deepseek-ai/DeepInfer/blob/main/doc/inference_deployment_qa.md), especially KV cache storage and hits/recomputation. It describes persistent storage and metadata prefix matching; the complete storage manager is not published. This implementation uses local whole-sequence snapshots and existing llama.cpp serialization, not a port of the unpublished distributed manager or approximate SWA bounded replay.

Base: upstream `839a572a4cdecdb80d211297b8e7d7323a6e99d2` plus `patches/ornith-upstream-839a.patch`. Add `patches/persistent-prefix-kv.patch` on top. Isolated source: `work/llama.cpp-exp081-kv`; binary: `build/exp081-kv/bin/llama-server`.

The production integration checkout, existing source checkouts, working binaries, launchers, model file, AGENTS.md, CLAUDE.md and roadmap were not changed. Existing untracked user artifacts were left intact. All six test servers are stopped.

## Audit and implementation

- Stock llama.cpp already compares exact token prefixes, selects slots by similarity/LRU, preserves slot KV, and offers a bounded RAM prompt cache. `--cache-prompt` enables this request behavior; it does not provide automatic disk persistence.
- Stock slot save/restore already streams target state and appends context checkpoints plus draft/speculative state. A text-only save/erase/restore/resave was byte-identical: 307,188,544 bytes, SHA-256 `7e6dca0be46e5c51c5e1b89625c36a2625c05f711280b31de5a4c7c699f9ecab`.
- Add an automatic NVMe tier, searched for an exact longest token prefix that improves on the resident slot. Save at completed prefill and completed generation. Restore into the selected idle slot, then let existing SWA/checkpoint logic decide the exact replay boundary.
- Before loading KV, verify the token manifest key, exact integer tokens, payload length/SHA-256, and compatibility identity. The identity includes full SHA-256 of all model shards, the draft model when separate, executable bytes, argv, relevant GGML/LLAMA/CUDA/OMP environment values, effective context/RoPE/KV/batch parameters and placement. Environment and argv contents are hashed, not logged. Mismatches are misses, not approximate hits.
- Stream target KV directly to/from files. Use atomic temp-file publication, file/directory fsync, a private directory and an exclusive process lock. Keep at most 64 disk entries; evict by last use before allocating incoming disk space. Default quota 4096 MiB, maximum configurable quota 65536 MiB, with 2 GiB free-disk reserve. Checkpoint/draft auxiliary state is capped at 256 MiB per saved entry. The disk store does not retain KV vectors in RAM.
- Include token and speculative-state bytes in RAM-cache accounting; carry speculative state on RAM restoration as well. Skip shared RAM/disk cache use for configured LoRA, and exclude unsupported multimodal/control-vector/embedding paths from the disk tier.
- Add MTP `pending_h` carry serialization with a versioned format and exact dimension validation. Restore target, draft KV, checkpoints and speculative carry together. EHS, pinned ring and kernel patches remain in place; EHS stays off for these tests.
- Transfer Flint architecture/tokenizer support from local fork `1b44e30775dcb2f93952116b05f89e5bd797fb00`. Its existing local Jinja hex-escape fix and tests were copied into the isolated tree because the model template uses `\x00`. The original Flint runtime was only read.

## Controls and measured profile

`--cache-nvme-mib 0` disables the new persistent tier completely, including model hashing at startup. `--cache-nvme PATH` sets its directory; the default is the llama cache directory's `prefix-kv` subdirectory. `--cache-ram 0` disables the existing RAM tier; `--no-cache-prompt` or request `cache_prompt:false` disables request prefix reuse. New builds enable the persistent tier by default.

Use a dedicated directory per configuration. Compatibility is deliberately strict: changing even argv such as the port/log path changes identity. The current single-directory implementation clears incompatible entries and refuses a second owner; it does not share disk cache across simultaneously running servers. Legacy manual `/slots/restore` files keep their upstream format and do not independently enforce the new outer fingerprint. The new strict identity/integrity guarantee applies to the automatic NVMe tier.

Hardware: Ryzen 9 5950X, RTX 4070 12 GB, 60.7 GiB physical RAM. Model: `/home/end0/AI/LocalAI/flint-int4-q4_1.gguf`, 25,992,966,976 bytes. Text-only Flint, 49 layers, SWA 513, no native MTP head.

Profile: context 66560, one slot, batch/ubatch 2048/1024, CPU threads 8/16, GPU layers 99 with CPU MoE, Flash Attention, Q4_0 KV, fit off, EHS 0. A: cache RAM/NVMe 0 and cache_prompt false. B: RAM 512 MiB, NVMe 4096 MiB and cache_prompt true. Same tested binary for all measured arms. Seed 42, temperature 0, 128 output tokens for each long request. Prompts use exactly 32768/65536 tokens; continuations contain 32920/65688 tokens.

Six server starts and sixteen serial generation requests were approved and used; one start stopped before any generation because of the Flint template lexer issue. Runs were manually issued one at a time; no benchmark automation, parallel runs or background builds were used. A 32-output-token pilot checked inference only and is excluded from performance claims. The output limit truncated reasoning in these synthetic fixtures, so this is not a completed-task quality benchmark.

## Measurements

Client TTFT is measured from curl request-header send to the first non-empty content SSE chunk, ignoring early HTTP headers and keep-alives. It includes disk lookup, checksum and restore. Prefill throughput uses only evaluated tokens, not skipped tokens. Decode throughput is one response, not aggregate throughput. Full response time includes synchronous final disk writes. Every reported cell is one observation, not a median or confidence interval.

| Case | Input | Reused | Hit % | Client TTFT s | Prefill s | Evaluated prefill tok/s | Decode tok/s | Full response s |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| a-32 | 32768 | 0 | 0.00000 | 96.041593 | 96.031478 | 341.22 | 29.17 | 100.402808 |
| a-64 | 65536 | 0 | 0.00000 | 197.508801 | 197.488195 | 331.85 | 26.30 | 202.346874 |
| a-32-continue | 32920 | 0 | 0.00000 | 97.704343 | 97.695467 | 336.97 | 29.80 | 101.971828 |
| a-64-continue | 65688 | 0 | 0.00000 | 199.112881 | 199.093417 | 329.94 | 25.82 | 204.040787 |
| b-32-cold | 32768 | 0 | 0.00000 | 97.801777 | 97.790995 | 335.08 | 29.00 | 103.372268 |
| b-32-warm | 32768 | 32767 | 99.99695 | 0.078755 | 0.054757 | 18.26 | 30.01 | 7.899376 |
| b-32-continue | 32920 | 32895 | 99.92406 | 3.135357 | 2.166795 | 11.54 | 29.96 | 9.305609 |
| b-64-cold | 65536 | 0 | 0.00000 | 199.319127 | 199.097094 | 329.17 | 26.36 | 206.743372 |
| b-64-warm | 65536 | 65535 | 99.99847 | 0.134845 | 0.058467 | 17.10 | 26.48 | 7.286067 |
| b-64-continue | 65688 | 65663 | 99.96194 | 4.626884 | 2.712504 | 9.22 | 25.92 | 11.968275 |
| stock-32-cold | 32768 | 0 | 0.00000 | 96.295627 | 96.285853 | 340.32 | 29.93 | 100.542701 |
| stock-32-warm | 32768 | 32767 | 99.99695 | 0.056041 | 0.046588 | 21.46 | 28.86 | 4.463307 |
| restart-32 | 32768 | 32763 | 99.98474 | 2.716680 | 1.512823 | 3.31 | 28.96 | 8.407235 |
| restart-64 | 65536 | 65531 | 99.99237 | 2.341150 | 0.171049 | 29.23 | 26.18 | 9.551476 |
| reference-32 | 32768 | 32763 | 99.98474 | 0.223570 | 0.213333 | 23.44 | 28.99 | 4.610347 |

Warm prefix reuse already existed in stock llama.cpp. The warm-versus-cache-disabled speedup must not be attributed solely to the new NVMe implementation. The added benefit is automatic reuse after restart or loss/overwrite of the resident/RAM state.

The new tier adds synchronous snapshot/hash costs: the first B cold response takes about 3% longer end-to-end at 32K and 2% at 64K. The first warm response also writes a new generation snapshot, so total response time is 7.90/7.29 s versus the much shorter TTFT. Existing stock 32K warm total is 4.46 s. Whole snapshots duplicate prefixes on disk; no paged/delta compression or asynchronous I/O is implemented.

## Restart, memory and NVMe

- Ready-server restart TTFT: 32K 96.0416 -> 2.71668 s (35.35x); 64K 197.5088 -> 2.34115 s (84.36x). SWA safety replay evaluates five tokens; hit rates are 99.98474% and 99.99237%.
- Full model SHA-256 verification makes initial B startup 80.59 s and the measured restart 77.46 s, versus A startup 0.85 s. Including startup, the sums are about 96.89 -> 80.18 s at 32K (1.21x) and 198.36 -> 79.81 s at 64K (2.49x). These are sums of separately measured startup/TTFT intervals, not one continuously timed request queued through restart. Do not present 35x/84x as full restart-to-answer speedups.
- Before restart, only the test KV payload files received POSIX_FADV_DONTNEED; no system-wide drop_caches was used. Physical process read_bytes increased by 284.09 MiB for 32K and 536.06 MiB for 64K. Corresponding writes were 585.21/545.75 MiB. Thus the test exercised real NVMe reads, not only an OS page-cache hit.
- Peak process RSS: A 23.461 GiB, B 23.882 GiB, incremental peak about 430.6 MiB. Test-process VmSwap stayed 0 in all snapshots; the one-second watchdog was configured to stop on a 1 KiB process-swap increase. No CUDA/OOM/watchdog trip occurred.
- GPU model/context/compute reservation stayed 2012 MiB (1050+546+415). Whole-device observed VRAM peaked at 5735 MiB and includes other applications; this is not process-only VRAM. Device VRAM returned to 3501 MiB after the test servers stopped.
- System swap was already nearly full before the experiment. Global pswpout increased by about 635.88 MiB over the sampled interval, including idle gaps. Attribution was not established. A system-wide zero-swap claim is not supported; this strict promotion gate remains failed.
- Disk cache reached about 3.9 GiB and evicted older entries under its 4 GiB quota. Final byte usage is in summary.json; temporary payload reservation is made before writes. The cache has no extra VRAM residency.

## Correctness and limits

Both B cold outputs match their A cold controls in all 128 tokens. At 32K, B warm matches native prefix caching with NVMe disabled in every token and every returned top-5 log probability. The restart output matches native `/slots/restore` of the identical snapshot in every token/probability. Native cold, warm and checkpoint-restored paths are not mutually bitwise identical with this quantized model and different replay batch shapes; this difference is also present with the new tier disabled. No approximate attention replay was added.

Model-free tests pass for exact prefix matching, token mismatch, incompatible identity, payload/manifest corruption, restart, quota eviction, real LRU touch behavior, 64-entry cap, locking, failed writes and MTP carry roundtrip/malformed data. ASan/UBSan pass with LeakSanitizer disabled because the execution sandbox uses ptrace. Existing server-component tests: 39 tests / 124 assertions, no failures. Jinja tests: 382 tests / 1504 assertions, no failures; 8 existing skips.

Actual MTP inference was not tested: Flint has no native MTP head, and no extra model runs were authorized. MTP support is implemented at the state boundary and compiles, but must not be described as runtime-validated for an MTP model. Concurrent slot pressure and forced filesystem I/O failures were not model-level benchmarks. The production runtime is not promoted on this evidence.

## Artifacts and reproduction

`results/exp081-kv/metrics.json` contains all sixteen observations; `resources.json`, `summary.json`, `provenance.json`, `source-manifest.json`, `stock-cache-correctness.json`, `restart-correctness.json`, and `manual-roundtrip.json` carry the compact evidence. Raw requests, SSE, curl traces and resource snapshots are beside them. `SHA256SUMS` hashes these artifacts. Payload snapshots remain local under ignored build directories. No real user conversations were used.

Apply the published Franken port and then `patches/persistent-prefix-kv.patch` to the pinned upstream in a fresh source directory. Patch reconstruction was checked against a clean published-base snapshot: all 22 changed files reproduce the tested source byte for byte. Build options are the EXP-079 CUDA profile, with a separate build directory. Build `llama-server`, `test-server-kv-store`, `test-server-component` and `test-jinja`. Do not apply this patch to the production integration checkout.

Binary and patch hashes are recorded in provenance.json. The new source is not a standalone copy of all upstream code; the patch is the reproducible tracked deliverable. No Git commit was made, no production launcher was changed, and no additional runs are scheduled.
