# EXP-2026-10-30-089: resident mmap expert registration

Accepted performance candidate above EXP087. This compares two versions of our preserved fork, not current upstream llama.cpp. Apex-35B-V1.0 Q4_K_M, Ryzen5950X/RTX4070, context262144, CPU MoE, batch/ubatch4096, threads8/16, existing masks, FA/Q4_0 KV, one slot and no speculation. Each fresh process receives the same cold32768-token native completion, up to128 outputs, seed123, EOS respected, top5 probabilities. Workloads, arithmetic and EXP087 lazy reservation remain unchanged.

## Five balanced manual pairs

Order: AB, BA, AB, BA, AB. Qualification is excluded. Every arm has a fresh process and empty prefix cache; normal startup warmup is retained. No competing model, helper, build or benchmark runs alongside inference. A read-only100ms monitor brackets each request.

| Metric | EXP087 | EXP089 |
|---|---:|---:|
| Median client time to first generated token | 23.512496s | 14.522125s |
| Median actual prefill elapsed | 23.502098s | 14.512139s |
| Median prefill throughput | 1394.259tok/s | 2257.972tok/s |
| Median single-response decode | 28.95297tok/s | 28.72410tok/s |
| Request peak RSS range | 22208720–22209324KiB | 22212040–22212908KiB |
| Median server-loaded timestamp | 2.346460s | 3.803180s |

Median paired changes: client TTFT-37.97135%, prefill rate+61.25485%, decode-1.12915%. All5/5 pairs win TTFT by at least37.66%. Paired percentage medians need not equal a ratio of arm medians. Pair5 decode is-6.325%; the pre-stated acceptance uses the median, which stays within3%. This is single-response throughput, not aggregate throughput.

Every primary request has process swap0, system swap-out0 and major faults0. All640 generated IDs per arm, ordered top5 IDs and probabilities match exactly, delta0. Qualification independently observes4428MiB model-process VRAM after both32K requests, preserving the EXP087 saving; VRAM was not sampled throughout every formal pair, so do not call that a new five-pair peak series. Near-full context later observes4950MiB during prefill; memory saving depends on graph size. Primary RAM increase is only several MiB.

Startup is mandatory work outside request timing: the median loaded timestamp rises about1.457s. Load/context allocation-only probes likewise show2.113s control versus3.536s with automatic registration. Even including this setup, the measured warm-file-cache first32K request benefits. Cold disk loading and other memory configurations are not covered by the speed claim.

## Mechanism and limits

Previously CPU expert weights remained pageable mmap memory. The new path registers the already mapped expert pages for GPU transfers. It changes no tensor bytes, addresses, buffer types, shapes, quantization or arithmetic and makes no second weight copy. Physical pointer queries confirm120 expert tensors/19503513600 bytes registered automatically; the CPU embedding remains pageable. The standalone full-host probe registers all121 ranges/19790073856 aligned bytes in1.339s, shows identical RSS/Anonymous before and after, then releases every range successfully and restores all pointers to unregistered memory. The speed result and physical residency evidence support avoiding pageable transfer staging; no new Nsight bandwidth attribution is claimed.

Production registration is default on only for Linux, ordinary resident mmap MoE expert tensors in the verified single-GPU+CPU causal Flash Attention/default single-sequence path with operation offload and no pipeline or MoE cache. Require union<=half physical RAM, MemAvailable>=union+8GiB and MemFree>=8GiB. Nonresident pages are not faulted in merely to pin them. Missing diagnostics, low budget, unsupported contexts or any registration failure retain the old pageable path. Low MemFree can disable the improvement even when large reclaimable file cache makes MemAvailable high. This deliberate conservative policy is not a promise of acceleration for all launches/models.

The model owns successful registrations, releasing them before buffers and mappings. Partial failure rolls back only this attempt's successful ranges. Storage is allocated before registration, and cleanup does not allocate. Existing backend callback lookup keeps CUDA details out of the model core. GGML_MMAP_EXPERT_PIN=0 disables the feature. GGML_CUDA_REGISTER_HOST=0 disables CUDA registration too; explicit existing settings retain precedence, and default respects GGML_CUDA_NO_PINNED.

## Correctness and compatibility

- ASan/UBSan model-free checks: alignment/overflow/merging, budget boundaries, failure at every range, rollback, repeated release and exception cleanup pass.
- Apex cold/24K-shared branch/repeat/early16K: exact selected IDs/ordered top5/probabilities, cache counts0/24576/32764/4096. Early branch naturally stops at EOS after4 outputs in both versions.
- Cold261632 at configured262144 context:128 exact output IDs/top5/probabilities, delta0, process swap0/system swap-out0/major faults0. Compare to checksum-verified preserved EXP087 output; this is a functional capacity check, not a paired full-context speed claim.
- Flint131072 profile: all four old cold/branch/repeat/early cases match128 IDs/top5/probabilities exactly; cache counts0/24576/32767/4096. No swap-out. Numerous file major faults after model switching exclude timings from performance claims. An early HTTP503 is excluded. The branch HTTP request queued behind the cold request on the sole slot; execution remained serial and those client times are not speed evidence.
- Disable probe: all121 host tensors unregistered; selected IDs/top5/probabilities match. This functional request after model switching did incur367084KiB process swap and166723 system swap-out pages; it does not pass the performance resource gate and is not an acceptance timing. Preserve this observation rather than discarding it. The new candidate's ten primary requests and capacity gate separately pass their resource gates.
- Metadata/no-alloc estimates are identical to EXP087. Nonvirtual internal declaration leaves public model object layout and vtable unchanged. Prior kernels/cache implementation and accepted builds are preserved; unsupported combinations fall back rather than being declared validated.

## Delivery

Apply patches/flint-apex-prefix-baseline.patch, then patches/lazy-pp-reservation.patch, then patches/mmap-expert-registration.patch on source commit1b44e30775dcb2f93952116b05f89e5bd797fb00. Patch replay reproduces every compiled source hash. Only libllama and libggml-cuda change versus EXP087; the seven remaining runtime components, including the executable, CPU backend, server/cache implementation and EXP087 ggml-base, are identical. See runtime-manifest.json for the exact nine-file set. CUDA device kernel objects remain reused; the rebuilt control's only difference is nonloaded NVCC filename metadata, with loaded code/data/fatbins identical.

The isolated runtime is /home/end0/AI/LlamaServer/exp089-mmap-register/build/bin. Keep its shared libraries together and use the recorded LD_LIBRARY_PATH. launch-profiles.json records supported commands; installed Desktop/runtime profiles are untouched. The saved working service on8081 runs this runtime, but its activation log reports pageable fallback because current free RAM does not satisfy the conservative headroom threshold; do not claim the measured speedup is active in that launch. Old builds remain available for rollback. No further optimization experiment is automatically scheduled after this goal.

Raw prompts, conversations, private instructions, weights and binaries are excluded. local-evidence-manifest.json hashes local raw evidence; public summaries and probes are retained here.
