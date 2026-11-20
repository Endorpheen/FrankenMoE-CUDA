# CUDA parity with EXP-1476

## Scope and evidence

Reference source is `e7c9155a01938b1825189240e7088620c6c7cd85`
(`v0.10.0-exp1476`). Reference CUDA paths below are relative to
`vendor/flint/ggml/src/ggml-cuda` at that revision, except where a
different directory is given. The pinned release executable and
libraries passed the archived manifest check. Its loaded maps include
`libggml-cuda`, cuBLAS and cuBLASLt from CUDA 12.6. The independent
comparison checkpoint is `f08110d1`, with an executable that declares
only `libc` and `libm` and resolves the system CUDA driver dynamically.

Evidence includes the matched 4,096-ID profiles, a complete reference
32,768/512 trace with whole CUDA Graph events and a second reference
trace with individual CUDA Graph nodes. All three used RTX 4070 and
the 4096/4096 launch profile. The complete 32K requests use the frozen
model and request SHA-256 values in
[the acceptance checklist](APEX-32K-ACCEPTANCE.md). The short profiles
use the first 4,096 IDs of that request and generate one ID. Nsight
timings diagnose activity and omit the final unprofiled parity claim.

**Status meaning.** PARITY requires an executed candidate path,
numerical evidence against the reference and a matching performance
and memory check. PARTIAL means an executed mechanism or verified
component exists, with a missing capability or an open parity check.
MISSING means the reference mechanism has no live independent
equivalent. INCORRECT means a comparable output fails its reference
check. Kernel and API duration sums in the appendices are observations.
Per-mechanism effects on rates and peak allocations require
paired ablations, which are open for every row.

## Mechanisms

**32 mechanisms: PARITY 0, PARTIAL 21, MISSING 10, INCORRECT 1.**

Group up to three related improvements. Give each change its own
short correctness, CUDA-error and numerical checks, then commit it
independently. Run one full Apex 32K/512 4096/4096 request after
three verified changes and compare prefill, decode, RAM, VRAM and
output against the previous correct checkpoint and EXP-1476.
Isolate any regression with the short tests and run changes
individually when needed. A critical failure or the first verified
generation repair calls for an immediate full run.

| # | Mechanism and EXP-1476 call path | Evidence and independent Goose state | Status, restoration and verification |
|---:|---|---|---|
| 1 | Q4_K MMQ. `ggml-cuda.cu` `ggml_cuda_mul_mat` / `ggml_cuda_mul_mat_id` → `mmq.cu` `ggml_cuda_mul_mat_q` → `mmq-load-tiles.cuh` and `mmq-vec-dot.cuh` → `mul_mat_q`. | Executed type 12 symbols contain SM89 `IMMA.16832.S8.S8`. With `GOOSE_MMQ=1`, Goose sends dense and routed 16×8 tiles to its K=32 MMA path. | PARTIAL. The integer-dot fixture and real 4,096-ID trace exercise the kernel. Full output parity and unprofiled performance remain open. |
| 2 | Q6_K MMQ, same backend chain with `ggml_cuda_mmq_vec_dot_q6_K_q8_1_mma`. | Executed type 14 symbols contain SM89 `IMMA.16816.S8.S8`. Goose candidate uses K=16 and the existing packed loader and signed per-16 scale. | PARTIAL. Dense and cached routes match the integer-dot fixture, and the real short trace reaches the tiled path. Validate full generation and small shapes separately. |
| 3 | Activation packing. `mmq.cu` → direct or scatter `quantize_mmq_q8_1` in `quantize.cu`, then MMQ. DS4 carries a half scale and activation sum. D4 carries F32 scales. | Both direct and scatter templates execute in the reference. Independent MMQ uses the EXP180 packer for bounded, job-ordered DS4/D4 windows. | PARTIAL. Both formats execute on real Apex. Device scatter and unique-input packing remain open. Validate model-level parity and quantify duplicate packing. |
| 4 | MMQ stream-K. `mmq.cuh` partitions K and invokes `mul_mat_q_stream_k_fixup` to combine partial outputs. | Fix-up runs in the reference. Independent scalar kernels reduce K inside each output block. | MISSING. After the base tile passes, implement reference partition and reduction rules. Verify K tails, partial ordering and large segments separately from the base tile. |
| 5 | Quantized small-batch MMVQ. `ggml-cuda.cu` → `ggml_cuda_mul_mat_vec_q` → Q8_1 and `mul_mat_vec_q` in `mmvq.cu`. | Both `quantize_q8_1` and `mul_mat_vec_q` run. Goose uses its scalar Q4_K/Q6_K path during decode. | MISSING. Restore the appropriate small-batch integer-dot specializations and packing. Check the exact subtype, output and decode timing under the final graph. |
| 6 | Indexed small-batch MMVQ. `ggml_cuda_mul_mat_id` → `mul_mat_vec_q_moe_launch` → `mul_mat_vec_q_moe`. | The reference trace executes indexed MMVQ. Goose groups jobs on the host and feeds the scalar matrix kernel. | MISSING. Restore the routed MMVQ path and verify duplicate expert IDs, signed-scale arithmetic and first decode output. |
| 7 | Tensor Core dense GEMM. `ggml-cuda.cu` selects cuBLAS routes, with `cublasGemmEx` for supported dense shapes. | The trace contains CUTLASS `Kernel2` tensor-op GEMMs and recorded cuBLAS dependencies. The independent F32 warp broadcasts one dot per output. | MISSING. Supply a tiled Tensor Core dense path with equivalent accumulation and tail handling. Compare actual F16/F32 shapes and isolate its timing after MMQ. |
| 8 | Dense small-shape dispatch. `mmf.cu` and `mmvf.cu` implement `mul_mat_f` and `mul_mat_vec_f`. The trace also records `dot_kernel`. | All three families appear in the trace. Goose does dense F32 FMA and shuffles using one output per warp. The exact library owner of `dot_kernel` remains open. | PARTIAL. Identify the dot route and reuse existing dot and warp code where equivalent. Recreate selected row/vector tiling and validate dimensions and backend selection by shape. |
| 9 | Device expert grouping. `ggml_cuda_mul_mat_id` / `mmq.cu` → `mm_ids_helper` → per-expert group ranges and Q8 scatter. | `mm_ids_helper` executes. Goose builds per-expert jobs and keeps a GPU SLRU, with host route synchronization. | PARTIAL. Feed active routes to device grouping and pack contiguous batches without losing expert ownership. Verify sparse, repeated and empty groups against the reference. |
| 10 | Fused MoE selection. `ggml-cuda.cu` `ggml_cuda_topk_moe_fusion` → `topk_moe_cuda`. | Reference executes selection fusion. Goose separately runs softmax, stable sort, gather, row sum and scaling. | PARTIAL. Combine equivalent operations while retaining tie order and normalized expert weights. Compare selected indices and scores before measuring speed. |
| 11 | Fused MoE combination. `ggml_cuda_match_moe_weighted_reduction` → `moe_weighted_reduction_f32`. | Reference makes one fused weighted combination. Goose materializes routed projections and reduces views over separate operations. | PARTIAL. Fuse route weights and reduction with the exact accumulation order. Test large MoE batches and measure removed intermediate storage and copies. |
| 12 | FP16 MMA Flash Attention. `fattn.cu` `ggml_cuda_flash_attn_ext` → shape selection → `fattn-mma-f16.cuh` `flash_attn_ext_f16`. | Reference prefill calls the MMA kernel even with Q4 KV storage. Goose uses Q4 scalar tiles plus a separate merge. | MISSING. Reuse Q4 cache and conversion leaves with the reference tiled MMA schedule and causal/mask behavior. Verify GQA, real head width 256 and long-context logits. |
| 13 | Small-query Flash Attention vector dispatch. `fattn.cu` → `flash_attn_ext_vec`. | Reference trace contains the vector path. Goose slices its scalar Q4 tiles for both prefill and decode. | MISSING. Restore the small-query specialization and validate softmax merging and decode latency at 32K. |
| 14 | Attention staging and bounds. `fattn-common.cuh` and `fattn-mma-f16.cuh` stage Q4_0 KV as FP16, use FWHT where selected, and derive KV bounds from the mask. | `dequantize_block_q4_0`, `fwht_cuda` and `flash_attn_mask_to_KV_max` execute. Goose tracks causal bounds in its KV transaction and reads Q4 directly. | PARTIAL. Match the reference tensor transforms in the exact selected path. Check Float16 rounding, packed bytes and masked tails before timing. |
| 15 | Flash partition reconciliation. `fattn-mma-f16.cuh` → stream-K general/uniform fix-up, plus vector combine-results. | All three reference families are present. Goose merge implements its current scalar partition geometry. | MISSING. Port compatible reconciliation with the MMA and vector schedules. Compare numerical reductions across tail partitions and multihead layouts. |
| 16 | GDN prefill Tensor Cores. `ggml_cuda_op_gated_delta_net_impl` → `gdn_single` in `gated-delta-net-mma.cu`. | Reference executes `gdn_single<16,128,1>`. Goose invokes `fg_cuda_gdn` → scalar `goose_gated_delta_net`. | MISSING. Restore the blocked recurrent MMA order and shared state update. Verify full and chunked hidden states for each layer, then prefill time and state-memory peaks. |
| 17 | GDN decode specialization and fused state cache. `ggml_cuda_op_gated_delta_net_fused_cache` → native scalar `gated_delta_net_cuda` or shape-selected MMA. | Reference executes scalar decode GDN and known fused cache code. Goose has CUDA-resident GDN state and a publish/consume path. | PARTIAL. Match the selected decode kernel's arithmetic and state snapshots against the reference across prefill → decode. Check for stale aliases and measure actual state moves. |
| 18 | SSM convolution and activation fusion. `ssm-conv.cu` dispatch plus `ggml_cuda_try_fuse` → `ssm_conv_long_token_f32` or `ssm_conv_f32`. | Both kernel families execute. Goose uses the Goose rolling-window leaf and separate activation on the shared arena. | PARTIAL. Restore the long-token tiling and selected SiLU fusion without altering the valid fallback. Verify convolution tail, persistent history and chunked equivalence. |
| 19 | Fused RMS/L2 normalization. `ggml_cuda_try_fuse` → RMS multiply/add/scale variants in `norm.cu` and supported RoPE/cache combinations. | Trace shows RMS variants. Goose has row normalization and separate vector operations. | PARTIAL. Fuse only traced compatible sequences after comparing epsilon and multiply/add order. Confirm reduction layout and avoid rounding regression. |
| 20 | Gated and unary activation fusion. `unary.cu` and `ggml_cuda_try_fuse` launch `unary_gated_op_kernel` and `unary_op_kernel`. | Both execute. Goose implements fused SwiGLU and several individual activations and keeps a CUDA activation arena. | PARTIAL. Verify the executed reference combinations, then match remaining graph fusion choices. Compare activations and adjacent gates before speed checks. |
| 21 | Broadcast arithmetic and reductions. CUDA operation dispatch → `k_bin_bcast`, `scale_f32`, `reduce_1Block_kernel`. | The reference runs optimized family variants. Goose implements broadcast vector and row-sum kernels for supported layouts. | PARTIAL. Restore shape-aware broadcasts/reductions where effect is measured. Verify offsets, F32/F64 reduction order and noncontiguous inputs. |
| 22 | RoPE execution. CUDA dispatch → `rope_multi` and its graph fusion with norms and cache. | `rope_multi` runs. Goose has positional transforms on GPU and CPU. | PARTIAL. Compare actual long prefix positions, mode flags and rotate/cache fusion. Check Q/K numerics at the first divergence. |
| 23 | Q4 cache writes. CUDA SET_ROWS dispatch → `k_set_rows_quant`. | Reference executes GPU Q4 quantization. Goose validates row updates and uses `goose_set_q4_rows` for dense supported spans, with ordered CPU sparse writes. | PARTIAL. Verify packed bytes and ordering against reference in the real cache. Measure writes across prefill and decode and retain safe repeated-index behavior. |
| 24 | Tensor copies, concatenation and gathers. CUDA transform dispatch → `concat_cont`, `concat_non_cont`, `cpy_scalar`, `k_get_rows_float` and vectorized gather. | All appear in reference trace. Goose has aligned lifetimes, GPU blits and gathers and host fallbacks. | PARTIAL. Match selected contiguous and strided paths while avoiding dead-tensor transfers. Check views, source lifetimes and numerical read-back. |
| 25 | Backend graph splitting and lifetime planning. GGML scheduler in `vendor/flint/ggml/src/ggml-backend.cpp` → `ggml-alloc.c` child/view counts, aligned best-fit, split/coalesced blocks and admissible in-place reuse. | Reference log records five buffers and eight same-size reallocations. Goose completes actual 4096/4096 using a ~1.010 GB lifetime arena and whole-block reuse. | PARTIAL. Retain the tested alias planner. Extend device lifetime ownership and compare backend memory peaks and retry behavior on identical profiles. |
| 26 | Expert tiers and pinned mapped weights. `vendor/flint/src/llama-model.cpp` → `llama-mmap-expert-pin.h` and CUDA buffer registration. | `cudaHostRegister` occurs 120 times in the reference session. Goose verifies all model pages resident, loads dense tensors and uses a GPU SLRU, with different upload scheduling. | PARTIAL. Associate registrations with the reference ranges, then restore explicit equivalent pinned transfer and expert-bank budgets. Verify real resident sizes, misses, bytes and swaps. |
| 27 | Selective transfer and device placement. Backend split execution and CUDA tensor set/get → asynchronous linear and 2-D copies. | Reference transfers selected ranges and uses D2D. Independent trace shows large output round trips through RAM and eager materialization of overwritten records. | PARTIAL. Keep live intermediate outputs on device and omit copies of expired records. Verify CPU fallback, overlapping views and request-level transfer bytes. |
| 28 | CUDA VMM pool. CUDA buffer allocator → `cuMemAddressReserve`, `cuMemCreate`, `cuMemMap`, `cuMemSetAccess`, release. | Reference executes 105 create/map operations and reserves a virtual address range. Goose grows Driver API buffers with `cuMemAlloc_v2` and `cuMemFree_v2`. | MISSING. Restore equivalent reusable device allocation and supported fallback. Check allocation limits, device address stability and VRAM peak on RTX 4070. |
| 29 | CUDA Graph capture and replay. `ggml-cuda.cu` graph evaluator → begin/end capture, instantiate, exec update and launch. | Whole-graph trace records over 20,000 launches across the session. Server reports 510 reuses. Goose rebuilds its graph and directly launches CUDA kernels. | MISSING. Establish stable graph keys, addresses and update boundaries before recording and replay. Check changing KV prefixes and buffer ownership before measuring decode overhead. |
| 30 | Streams and synchronization. CUDA scheduler and `ggml-cuda.cu` establish streams, events and CUDA Graph scopes. | Reference uses stream sync and async copies. Goose synchronizes the context for many operations during execution. | PARTIAL. Restore per-stream ordering and event dependencies after liveness and transfers are safe. Compare wall time without adding driver wait to GPU time. |
| 31 | Numerical prefill → decode continuation. Reference `llama-context.cpp` scheduler and CUDA graph compute produce logits for the shared sampler. | Identical 32K/512 reference responses verify text, ID and probability hashes. Goose completes all tokens but diverges at output index 1, before sampling. | INCORRECT. Identify earliest differing stage by matching per-layer tensors, committed Q4 KV, SSM/GDN and the first subsequent logits. Numerical repair precedes acceptance of any speed. |
| 32 | Device initialization and specialization loading. CUDA backend loader and scheduler select architectures, entry points and cuBLAS paths. | Trace records device info, driver lookups, cuLibrary entry resolution and allocation APIs. Goose loads its own PTX with the Driver API. | PARTIAL. Extend end-to-end executable selection for supported MMQ shapes and verify real device witness. Preserve independent module loading and rejected profiles. |

Reference-only kernel and API details are linked to the numbered
mechanisms below. Events that lack reliable attribution stay open.

## Selected numbers

The same-input 4,096-token comparison measured **104.375 s** for
Goose prefill and **2.020 s** for EXP-1476. Goose spent **60.690 s**
in matrix kernels and **27.162 s** in compute-window copies.
Reference MMQ plus Q8_1 packing took **0.448 s**. The quantized
matrix family is the first repair. See the
[earlier detailed comparison](APEX-CUDA-REGRESSION.md) for timing
windows, full-request diagnostic rates and memory samples.

The real independent 32K/512 checkpoint completes physical 4096/4096
at 32.071946 input tokens/s and 10.269880 decode steps/s, with
diagnostic high-water RSS of 23,883,316 KiB and sampled process VRAM
of 8,944 MiB. The output fails parity. Final acceptance requires
matching fresh A–B–B–A medians and valid responses. Historical floors
are 2,940.9 input tokens/s and 62.86 decode steps/s.

## First independent MMQ checkpoint

`GOOSE_MMQ=1` selects the SM80+ PTX image. Q4_K and Q6_K use the
existing packed arithmetic and Q8_1 quantizer with independent
`mma.sync` bindings. The default device image and small-shape
operation selection retain their previous paths while parity is
open. Go planner tiles stay within expert bank boundaries. The
Q8_1 staging window covers up to 512 routes. Strided vector
operands upload their logical rows when they need host staging.
Large attention operations can shrink their workspace wave while
preserving batch, cache and output geometry.

RTX 4070 executed five MMQ nodes in the dense/indexed fixture with
zero error against the independent integer-dot formula. Coverage
includes output tails, zero activation rows, repeated routes and
multiple packing windows. The 15-case vector fixture with 8×
stride and the mixed CPU/CUDA continuation regression also passed
on the default image.

One 4,096-ID Apex diagnostic on the new path measured
46.363 s prefill, or 88.346 tokens/s, versus 104.375 s and 39.243
tokens/s in the comparable scalar profile. Nsight recorded 3.160 s
MMQ, 0.098 s Q8_1 and 0.595 s residual scalar matmul, versus
60.690 s in the previous shared scalar kernel. Compute-window H2D
fell from 187,594,956,656 to 105,965,212,544 bytes. D2H remained
91,803,156,480 bytes. The short profile captured the executable
before the attention-wave allocation guard was added.

The short request's first ID differs from the reference. The
full 32K/512 attempt on this checkpoint failed to reserve a
268,435,456-byte dense MoE vector buffer. It captured no decode
rate. Keep this as a GPU-executed, numerically gated partial
checkpoint and continue memory and correctness work.

Raw data is in
`/home/end0/AI/LlamaServer/franken-goose-mmq.w1C6YB`, including
`mmq-profile.IpNHWO` and `apex-functional.DCqNyI`.

## Large MoE vector workspace checkpoint

A 4096-token MoE product previously requested a contiguous
268,435,456-byte device staging allocation and failed. The planner
now slices large dense MUL operations along independent token
planes. Each window has a 16 MiB staging target and publishes its
own range to RAM. The physical microbatch is still 4096. Inputs
and output must be disjoint before the planner selects windows.
RTX 4070 executed three windows for the 4096-plane fixture and
matched every output including the tail on both CUDA images.

The full real request for this critical memory repair completed
with `GOOSE_MMQ=1` at physical 4096/4096, with cache reuse zero
and 512 output IDs. Prefill was **573.944 s / 57.092649
tokens/s**. Decode was **49.370 s / 10.350416 steps/s** on the
511-step denominator. Against the earlier independent scalar
diagnostic the prefill interval fell by 447.759 seconds. The
response discusses the repeated input log and differs from the
reference at output index 1 (ID 280 versus 198). The next-step
probability distribution also differs. Exact parity remains open.

High-water RSS was 23,945,668 KiB. The 304 VRAM samples recorded
8,696 MiB maximum for the process and 11,813 MiB for the card.
Process swap and system swap-out growth were zero. The full model
remained resident before and after the request.

Raw data:
`/home/end0/AI/LlamaServer/franken-goose-vector-windows.a9wPqw/apex-functional.qYHYlH`.
Executable SHA-256:
`bfb055048259d72514f0efff1919999391f86b766bd5c973bbecb6e73682e66d`.

## Kernel coverage

The exact EXP-1476 request passed content, ID and probability
checks again under both trace modes. Whole-graph capture reports
2,619.257 prefill tokens/s and 59.604 decode steps/s. Node capture
reports 2,632.479 and 57.546. Whole-graph capture hides most decode
nodes. The table uses node capture and includes startup warm-up.
Durations are device activity sums. Graph-node counts explicitly
separate kernels executed as part of CUDA Graphs.

The node trace covers **38 named families, 64 demangled
specializations and 920,033 launches**, including 895,931 from
CUDA Graph nodes. Quantized matmul variants in this workload are
Q4_K and Q6_K. MMQ ran 2,928 Q4_K and 464 Q6_K launches. D4 direct
packing ran 464 times, DS4 direct 2,304 times and DS4 scatter 624
times. Warm-up appears alongside the request. Two small kernels
around the prefill → decode boundary have open source attribution.

| Kernel family | Launches | Specializations | Graph nodes | Total GPU ms | Mechanism |
|---|---:|---:|---:|---:|---|
| `mul_mat_q` | 3,392 | 3 | 0 | 3,111.846 | 1–2 |
| `mul_mat_vec_q` | 200,740 | 9 | 199,363 | 2,493.988 | 5 |
| `flash_attn_ext_f16` | 90 | 2 | 0 | 1,393.709 | 12 |
| `flash_attn_ext_vec` | 5,130 | 2 | 5,100 | 1,207.552 | 13 |
| `gdn_single` | 240 | 1 | 0 | 465.086 | 16 |
| `quantize_mmq_q8_1` | 3,392 | 3 | 0 | 455.040 | 3 |
| `rms_norm_f32` | 99,686 | 4 | 97,426 | 363.022 | 19 |
| `unary_gated_op_kernel` | 57,511 | 3 | 56,118 | 352.213 | 20 |
| `concat_non_cont` | 330 | 1 | 30 | 321.865 | 24 |
| `moe_weighted_reduction_f32` | 20,872 | 1 | 20,403 | 265.777 | 11 |
| `k_bin_bcast` | 58,677 | 3 | 56,688 | 252.578 | 21 |
| `ssm_conv_long_token_f32` | 240 | 1 | 0 | 209.353 | 18 |
| `quantize_q8_1` | 201,091 | 1 | 199,480 | 183.273 | 5–6 |
| `mul_mat_vec_f` | 41,354 | 2 | 40,806 | 175.195 | 8 |
| `mm_ids_helper` | 936 | 1 | 0 | 154.024 | 9 |
| `k_get_rows_float_vec` | 15,660 | 1 | 15,303 | 101.250 | 24 |
| `cpy_scalar` | 20,880 | 1 | 20,403 | 65.012 | 24 |
| `fwht_cuda` | 20,880 | 2 | 20,400 | 63.614 | 14 |
| `Kernel2` | 312 | 1 | 0 | 61.048 | 7 |
| `k_get_rows_float` | 37,248 | 2 | 36,726 | 58.824 | 24 |
| `topk_moe_cuda` | 20,872 | 1 | 20,403 | 56.721 | 10 |
| `gated_delta_net_cuda` | 15,420 | 1 | 15,303 | 56.137 | 17 |
| `unary_op_kernel` | 36,862 | 2 | 35,736 | 27.924 | 20 |
| `rope_multi` | 10,440 | 1 | 10,200 | 22.845 | 22 |
| `k_set_rows_quant` | 10,440 | 1 | 10,200 | 22.308 | 23 |
| `ssm_conv_f32` | 15,420 | 1 | 15,303 | 21.325 | 18 |
| `concat_cont` | 15,330 | 1 | 15,273 | 20.393 | 24 |
| `dequantize_block_q4_0` | 180 | 1 | 0 | 11.819 | 14 |
| `mul_mat_vec_q_moe` | 351 | 2 | 117 | 10.484 | 6 |
| `flash_attn_combine_results` | 5,110 | 1 | 5,090 | 8.323 | 15 |
| `flash_attn_mask_to_KV_max` | 80 | 1 | 0 | 3.998 | 14 |
| `mul_mat_q_stream_k_fixup` | 480 | 1 | 0 | 3.200 | 4 |
| `flash_attn_stream_k_fixup_general` | 80 | 1 | 0 | 2.537 | 15 |
| `scale_f32` | 180 | 1 | 60 | 0.811 | 21 |
| `mul_mat_f` | 39 | 1 | 0 | 0.374 | 8 |
| `dot_kernel` | 39 | 1 | 0 | 0.052 | 8, exact owner open |
| `reduce_1Block_kernel` | 39 | 1 | 0 | 0.048 | 21, exact owner open |
| `flash_attn_stream_k_fixup_uniform` | 10 | 1 | 0 | 0.041 | 15 |

## CUDA API coverage

The node trace contains **35 API names and 326,401 calls**. The
groups below cover each recorded name. API time is host time inside
the call, including waits. The largest observations were 17,163.950
ms in 62,354 stream synchronizations, 1,342.211 ms in 120 host
registrations, 1,164.472 ms in 20,914 CUDA Graph launch calls and
942.735 ms in 211,165 asynchronous memcpy calls. The two launch
APIs recorded 29,471 calls totaling 109.349 ms.

| Mechanisms | Recorded API names, with version suffixes omitted |
|---|---|
| 25, 26, 28 | `cudaMalloc`, `cudaFree`, `cudaHostRegister`, `cudaMallocHost`, `cudaFreeHost`, `cuMemAddressReserve`, `cuMemCreate`, `cuMemMap`, `cuMemSetAccess`, `cuMemRelease`, `cuMemGetAllocationGranularity` |
| 23, 25, 27 | `cudaMemcpyAsync`, `cudaMemcpy2DAsync`, `cudaMemsetAsync`, `cuMemsetD8` |
| 29 | `cudaGraphLaunch`, `cudaGraphInstantiate`, `cudaGraphExecUpdate`, `cudaGraphDestroy`, `cudaGraphExecDestroy`, `cudaStreamBeginCapture`, `cudaStreamEndCapture` |
| 30 | `cudaStreamSynchronize`, `cudaStreamCreateWithFlags`, `cudaEventCreateWithFlags` |
| 32 | `cudaMemGetInfo`, `cudaLaunchKernel`, `cuLaunchKernel`, `cuLibraryLoadData`, `cuKernelGetFunction`, `cuGetProcAddress`, `cuInit`, `cuLibraryGetKernel`, `cuModuleGetLoadingMode`, `cudaGetDriverEntryPoint` |

The same record contains 173,804 H2D transfers totaling 138,267,677,864
bytes, 21,506 D2H transfers totaling 1,951,091,040 bytes, and 15,972
D2D transfers totaling 10,454,296,320 bytes. There are also 165
device-memory fills. These session totals include startup activity.
Per-mechanism peak RAM and VRAM remain open until matched ablations.

## Evidence locations

Root: `/home/end0/AI/LlamaServer/franken-goose-apex-4096.v1C1cI`.

- `reference-nodes.aPAmFV/trace.nsys-rep`, SHA-256
  `6b0f9b8f4d8f83ce892e14c2eca5239dc330cb4d003c09a4425ccd4e0ad3adb3`.
  Its SQLite tables `CUPTI_ACTIVITY_KIND_KERNEL`,
  `CUPTI_ACTIVITY_KIND_RUNTIME`, and `StringIds` are the source for
  the coverage and duration tables in this document.
- `full-reference.NWyFxv` contains the exact-request whole-graph trace,
  verified response and control log with the 510 reuse counter.
- `mmq-profile.GQtAIU` and `mmq-reference.J7xT6p` contain the
  4,096-ID CUDA comparisons.
- `apex-functional.jl8ydH` contains the real independent 4096/4096
  request, response, buffer reuse, residency and memory samples.
