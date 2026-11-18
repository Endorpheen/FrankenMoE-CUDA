# EXP-2026-11-17-937 — Recover the Maxwell dense checkpoint

Status: **RECOVERED for the fixed 4096 profile**. One lifecycle per arm. The sparse runtime and the shared Apex runtime remain separate.

## Runtime identity

The historical `maxwell-build-20261117/mx-warm.log` identifies **v2** for 1895.80 prefill and 65.92 single-response decode tok/s. `mx-v3b-2048.log` identifies **v3** for 1846.86 and 63.18. The first measurement predates the GGUF conversion that added 180 indexer tensors and an EOT token. The early and current GGUFs have the same tensor types and shapes for their 981 shared tensor names.

The v2/v3 executable has a `DT_RUNPATH` into `franken-recovery-20261116/support/bin`. v9 has a `DT_RUNPATH` into `llama.cpp-flint-cuda/build-cuda/bin`. With no library override, the loader selects those directories. The historical driver at `cce98d49:tools/same-load-bench.sh` explicitly sets `LD_LIBRARY_PATH` to the candidate directory. That reconstructs the likely historical selection. The historical 65.92 run has no surviving process maps in the inspected artifacts.

The new runs captured all eight application DSOs in each server's `/proc/PID/maps`. All came from that arm's candidate directory, including the v3 core and EXP906 CUDA library. See [v3 hashes](v3.sha256) and [v9 hashes](v9.sha256).

- v3 has the EXP906 CUDA DSO plus the Goose-backed core with `pin_mmap_experts`, graph bookkeeping, mmap bookkeeping and MoE-cache routines. v9 retains mmap, the MoE cache and Flash Attention. Its core lacks that expert registration call path and the v3 Goose bridges. Disassembly shows that its CUDA DSO defaults host registration to off. The old v11 swap retained the v9 core, leaving the registration caller absent.
- Both build caches specify Release, SM89, native CPU tuning, CPU repacking, CUDA graphs and Flash Attention. CPU DSO hashes differ. Both expose the Q4_0 CPU GEMV and dot-product variants. No performance attribution to individual CPU or CUDA kernels was established. The scheduler reported the same graph reuse counts for all three successful arms.
- With the current GGUF, v2 failed to finish startup. Its model constructor selects the MSA KV cache when indexer metadata is present, while its dense graph calls `build_attn_inp_kv()`. v3 selects the plain cache, which matches that graph. The v2 startup logged `FG408S_GRAPH_INPUT_PLAN` before stalling. Host permissions prevented live perf sampling and debugger attachment.

## Fixed short profile and gate

RTX 4070 12 GiB. Maxwell-V1-Flash Q4_0 GGUF SHA-256 `c2542c20bff7a227f010515c733135bcb0bc10e0366f36abed28e4e8f934d4bd`. All arms used the same fully resident file, 4096 context, 2048/2048 batch/ubatch, 8/16 threads, mmap, CPU experts, 4992 MiB expert cache, q4_0 K/V and Flash Attention. The saved 2048/128 request bytes have SHA-256 `954abc8d8bbe7c496550438109f378c07f3af952a453db86eb02f51e5680f78c`. Each measured lifecycle had one excluded warmup followed by three complete requests.

| Arm | Prefill tok/s, median [range] | Single-response decode tok/s, median [range] | RSS MiB | Process GPU MiB | Total GPU MiB |
|---|---:|---:|---:|---:|---:|
| v9 | 1251.29 [1189.38–1306.72] | 39.66 [38.87–41.21] | 21149.1 | 7502 | 11461 |
| v3, EXP906 default | 1767.72 [1677.31–1780.29] | 62.02 [55.46–62.53] | 21147.3 | 7504 | 11671 |
| v3, registration disabled | 1197.48 [1156.50–1247.37] | 44.47 [42.93–46.77] | 21146.7 | 7504 | 11519 |

Memory was sampled after the last request. The default v3 gained +41.27% prefill and +56.38% decode over v9, with -0.009% RSS, +0.027% process GPU memory and +1.83% total GPU memory. Against the identical v3 with registration disabled, it gained +47.62% prefill and +39.48% decode, with +0.001% RSS, equal process GPU memory and +1.32% total GPU memory. Total GPU memory includes desktop processes. Prelaunch utilisation snapshots varied. The speed differences describe this pass and require balanced repeated lifecycles for a precise effect size.

The v3 comparison changed only `GGML_CUDA_REGISTER_HOST` between unset and `0`. Startup acquired 4,647,978 pages with the default and 3,072 pages when disabled, a difference of 17.72 GiB of existing resident pages on this host. Both lifecycles returned to the prelaunch pin balance of 4,103 pages after exit. All successful short arms had zero process swap and zero system swap-out delta. Each returned 2048 computed prompt tokens and 128 generated tokens with identical text SHA-256 `c881c3ba610b98f9670708c2e47aaea790231fed163f601ea0f7b9bcf18ca7f1`. The saved endpoint request did not return token IDs or logits. The scope of the gate is this completion workload.

## Dense checkpoint

Independent read-only byte copies are in `/home/end0/AI/LlamaServer/maxwell-dense-exp937-20261117/run`. All nine files passed SHA-256 verification, and `ldd -r` with the checkpoint directory in `LD_LIBRARY_PATH` resolved every application DSO there. The preserved v3 runtime handles the 4096 dense window. Preserve the original v9 package for sparse development and retain the accepted Apex launcher.

```bash
RUN=/home/end0/AI/LlamaServer/maxwell-dense-exp937-20261117/run
cd "$RUN"
sha256sum -c SHA256SUMS
LD_LIBRARY_PATH="$RUN" ldd -r ./llama-server
env -u GGML_CUDA_REGISTER_HOST -u GGML_CUDA_NO_PINNED LD_LIBRARY_PATH="$RUN" \
  "$RUN/llama-server" -m /home/end0/AI/LocalAI/Maxwell-V1-Flash-Q4_0.gguf \
  -c 4096 -ngl all -fit off --cpu-moe --moe-cache-mib 4992 -fa on \
  -ctk q4_0 -ctv q4_0 --load-mode mmap --alias bench -b 2048 -ub 2048 \
  -t 8 -tb 16 -np 1 --cpu-mask 0f0f --cpu-mask-batch ffff \
  --cpu-strict 1 --cpu-strict-batch 1 --poll 0 --cache-ram 0 \
  --ctx-checkpoints 32 --spec-type none --jinja --host 127.0.0.1 --port 9101
```

## Sparse diagnostic and Apex guard

The v9 Nsight Systems diagnostic used context 32768 and the saved 8192/2 request with SHA-256 `f2179f7c399bf4dd76f2563a2d47a9065fcc2aadac87a9453ef3da5a8d852d74`. It stopped in `cuMemCreate` with CUDA out of memory on the path `llama_kv_cache_msa_context::apply → llama_kv_cache::ensure_mapped → ggml_backend_cuda_lazy_map`. The trace has no complete GPU kernel or memcpy events. No long-context performance or correctness result was recorded. Raw trace SHA-256 `8d7444b3b238ec250a1008074f645892bd775a7a5e4bbcc74bdf7587f1a4b280`.

The smallest next probe should record the requested VMM chunk size and the committed bytes in the base and indexer KV caches, together with scratch allocations. Capture CUDA events on a complete, already verified long request before selecting a Goose kernel change. The current prefill graph builds an indexer score tensor and expands the block mask to per-token dimensions before masked Flash Attention. Their contribution to elapsed time remains unmeasured.

The accepted Apex EXP906 runtime matched its manifest. Its 32768/512 request bytes matched the frozen request. Two file reads left 9.46 of 21.71 GB in the page cache, and the Apex server was never started. The reference of 55.52 decode and 2625.21 prefill remains historical. This checkpoint introduces zero runtime source changes and does not replace the Apex runtime.
