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

## Deferred optimization registry

Measure eliminated work separately from request throughput. A change
that demonstrably reduces unnecessary work can be useful if correctness
holds and no unacceptable regression is observed. Keep the device or
resource metric distinct from measured prefill and decode. Record useful
interactions even when a standalone test shows no speed gain.

Add an entry when an experiment improves a kernel, transfer or API
metric but lacks a verified request-level gain or fails the numerical
gate. Keep its exact implementation in Git or in a patch tied to the
original base. Preserve measured results, explain the deferral and name
the dependency that would justify rerunning it.

| Experiment | Preserved implementation and measurements | Reason for deferral | Dependencies and next combination test |
|---|---|---|---|
| EXP-1527, dense TF32 MMA | `fc399bf0`, `GOOSE_DENSE_MMA=1`. [Dense diagnostic](#dense-tf32-mma-diagnostic) and [full package](#prefill-package-full-32k512-diagnostic). Matrix time 565.205 → 52.069 ms, short prefill 110.293 → 115.386 tokens/s. | The short first ID changes from 9495 to 37972. EXP-1476 returns 332. The full package also fails reference text, ID and probability parity. Its speed does not isolate Dense MMA. | Diagnose routed MoE, dense accumulations and the first divergent Apex state. After the numerical gate passes, compare the selected dense path alone and with restored MoE routing. Measure router and output timing and require reference response parity. |
| EXP-1531, deferred CUDA waits | [Candidate patch](deferred/EXP-1531-graph-waits.patch) against `7916697c`. [Original A–B–B–A](#deferred-cuda-wait-diagnostic): explicit waits 1,873 → 562, median prefill 22.875 → 22.882 s. | Originally, synchronous D2H absorbed the removed waiting. Copy counts and bytes stayed unchanged. | Rechecked with reusable arena gaps and selective eviction on `e37e8a2c`. [Combined A–B–B–A](#deferred-waits-with-selective-eviction) passed the short gate and enabled waits at the planned graph boundary. Full-request qualification is open. Reassess when pinned transfer or stream/event changes alter host readback. |
| EXP-1536, GDN MMA | Standalone `GOOSE_GDN_MMA=1`. [Kernel and model measurements](#gdn-mma-checkpoint). Thirty kernels took 2,503.240 → 171.978 ms and short prefill reached 210.866 tokens/s. | The short first ID changed from 9495 to 280. Both differ from EXP-1476 ID 332. On the same actual layer inputs the largest relative L2 errors were 1.58e-5 for output and 3.71e-5 for state. | Compare the earliest differing routed-expert IDs and logits against EXP-1476. Recheck with restored attention and routing on a numerically verified checkpoint. Keep the experimental flag off by default until the response gate passes. |
| EXP-1537, Flash Attention MMA | Standalone `GOOSE_FLASH_MMA=1`. [Kernel and model measurements](#causal-flash-attention-mma-checkpoint). Ten attention layers take 34.416 ms including Q4 staging, versus 2,738.669 ms for the scalar path. | The short first ID changes from 9495 to 271. On shared Apex inputs, relative L2 differences against the scalar path reach 6.44e-3. | Compare the same Q/K/V inputs to the EXP-1476 FP16 kernel, then restore stream-K scheduling and fix-up. Check isolated and combined GDN/Flash/Dense variants on the same short request. Keep the flag off by default pending response parity. |
| GDN + Flash + Dense MMA on the ten-change package | Source `6083f55c`; [current intermediate performance control](#intermediate-apex-performance-control). Combined short prefill 281.919 tokens/s. The full profiled request reached 283.535 tokens/s. | The full response begins to repeat the input. Its second ID is 1716 versus 198 in EXP-1476. Content and probability rows differ. | Use the exact opt-in package as the speed checkpoint. Use the EXP-1476 response for correctness. Recheck the individual kernels and switches as the numerical path changes. |
| EXP-1540, MMQ Stream-K | `GOOSE_MMQ_STREAM_K=1`. [Kernel and model measurements](#mmq-stream-k-checkpoint). The Apex 32/1 case executes 160 Stream-K and 160 fixup kernels and reduces MMQ device arithmetic time from 59.266 to 54.748 ms. | The 32/1 request changes probabilities and takes 913.819 ms versus 855.517 ms. The 4096/1 geometry chooses the regular MMQ path throughout. | Keep opt-in. Recheck with reference-sized MMQ tiles and routed dispatch after their numerical gate. A low-occupancy production shape or revised tile geometry can change the partition decision. |

After a material dependency change, use short sequential A/B runs on
identical inputs and settings. Use the best current verified checkpoint
as the control. Compare the combined candidate and, when necessary, the
individual toggles to isolate interactions. Record IDs, probabilities,
prefill, decode and the targeted hardware metrics. Enable a combination
in the working runtime only after it passes correctness and shows a real
end-to-end gain. Preserve each implementation and append new evidence to
the entry. Apply the full-request gate on the package schedule.

## Mechanisms

**32 mechanisms: PARITY 0, PARTIAL 25, MISSING 6, INCORRECT 1.**

Group three to five related improvements. Give each change its own
short correctness, CUDA-error and numerical checks, then commit it
independently. Run one full Apex 32K/512 4096/4096 request after
the package and compare prefill, decode, RAM, VRAM and
output against the previous correct checkpoint and EXP-1476.
Isolate any regression with the short tests and run changes
individually when needed. A critical failure or the first verified
generation repair calls for an immediate full run.
The current package extends the five planned changes by another five
before the next scheduled full request. An experimental change that
fails a short gate does not count. Focus the additional work on missing
computational kernels, starting with the existing Flash Attention MMA,
GDN MMA and MMQ Stream-K work. Change transfers only where needed to
connect or validate these kernels.

Prioritize Apex prefill. Start with dense GEMM (7), then examine
transfers and synchronization (24, 27, 30), Flash Attention (12,
14, 15), GDN/SSM (16, 18), and MMQ/MoE (4, 9, 10, 11). Reuse
the existing Goose implementations whenever they fit the active
shapes and numeric contract. Sequence work by measured impact and
dependencies. Resume decode-specific work after prefill recovery.

| # | Mechanism and EXP-1476 call path | Evidence and independent Goose state | Status, restoration and verification |
|---:|---|---|---|
| 1 | Q4_K MMQ. `ggml-cuda.cu` `ggml_cuda_mul_mat` / `ggml_cuda_mul_mat_id` → `mmq.cu` `ggml_cuda_mul_mat_q` → `mmq-load-tiles.cuh` and `mmq-vec-dot.cuh` → `mul_mat_q`. | Executed type 12 symbols contain SM89 `IMMA.16832.S8.S8`. With `GOOSE_MMQ=1`, Goose sends dense and routed 16×8 tiles to its K=32 MMA path. | PARTIAL. The integer-dot fixture and real 4,096-ID trace exercise the kernel. Full output parity and unprofiled performance remain open. |
| 2 | Q6_K MMQ, same backend chain with `ggml_cuda_mmq_vec_dot_q6_K_q8_1_mma`. | Executed type 14 symbols contain SM89 `IMMA.16816.S8.S8`. Goose candidate uses K=16 and the existing packed loader and signed per-16 scale. | PARTIAL. Dense and cached routes match the integer-dot fixture, and the real short trace reaches the tiled path. Validate full generation and small shapes separately. |
| 3 | Activation packing. `mmq.cu` → direct or scatter `quantize_mmq_q8_1` in `quantize.cu`, then MMQ. DS4 carries a half scale and activation sum. D4 carries F32 scales. | Both direct and scatter templates execute in the reference. Independent MMQ reuses the EXP180 packer and shares one Q8_1 row across routes when their input slot is identical. | PARTIAL. Both formats execute on Apex. Device scatter/grouping and model-level parity remain open. |
| 4 | MMQ stream-K. `mmq.cuh` partitions K and invokes `mul_mat_q_stream_k_fixup` to combine partial outputs. | Independent Goose partitions flattened output tiles and K blocks and applies backward partial reduction with `GOOSE_MMQ_STREAM_K=1`. GPU fixtures and a short Apex shape execute the fixup. | PARTIAL. Verify response parity and scheduling on reference-sized tiles. The current 4096 geometry selects regular tiles. |
| 5 | Quantized small-batch MMVQ. `ggml-cuda.cu` → `ggml_cuda_mul_mat_vec_q` → Q8_1 and `mul_mat_vec_q` in `mmvq.cu`. | Both `quantize_q8_1` and `mul_mat_vec_q` run. Goose uses its scalar Q4_K/Q6_K path during decode. | MISSING. Restore the appropriate small-batch integer-dot specializations and packing. Check the exact subtype, output and decode timing under the final graph. |
| 6 | Indexed small-batch MMVQ. `ggml_cuda_mul_mat_id` → `mul_mat_vec_q_moe_launch` → `mul_mat_vec_q_moe`. | The reference trace executes indexed MMVQ. Goose groups jobs on the host and feeds the scalar matrix kernel. | MISSING. Restore the routed MMVQ path and verify duplicate expert IDs, signed-scale arithmetic and first decode output. |
| 7 | Tensor Core dense GEMM. `ggml_cuda_mul_mat_cublas` selects the F32 `cublasSgemm` path with `CUBLAS_TF32_TENSOR_OP_MATH` in `common.cuh`. | The 4096/1 trace records 39 CUTLASS `s1688gemm` launches. `GOOSE_DENSE_MMA=1` selects 40 independent tiled TF32 MMA launches with F32 accumulation. | PARTIAL. The CUDA path and F32 error fixture pass. Apex output changes under the experimental flag and still differs from EXP-1476. Keep the flag off for the next baseline until the model-level numerical difference is resolved. |
| 8 | Dense small-shape dispatch. `mmf.cu` and `mmvf.cu` implement `mul_mat_f` and `mul_mat_vec_f`. The trace also records `dot_kernel`. | All three families appear in the trace. Goose does dense F32 FMA and shuffles using one output per warp. The exact library owner of `dot_kernel` remains open. | PARTIAL. Identify the dot route and reuse existing dot and warp code where equivalent. Recreate selected row/vector tiling and validate dimensions and backend selection by shape. |
| 9 | Device expert grouping. `ggml_cuda_mul_mat_id` / `mmq.cu` → `mm_ids_helper` → per-expert group ranges and Q8 scatter. | `mm_ids_helper` executes. Goose builds per-expert jobs and keeps a GPU SLRU, with host route synchronization. | PARTIAL. Feed active routes to device grouping and pack contiguous batches without losing expert ownership. Verify sparse, repeated and empty groups against the reference. |
| 10 | Fused MoE selection. `ggml-cuda.cu` `ggml_cuda_topk_moe_fusion` → `topk_moe_cuda`. | Goose selects eight experts directly and fuses gather, clamp, division and scaling in a single weights kernel. The live Apex snapshot matches the preceding graph byte-for-byte. | PARTIAL. Integrate softmax and selection with the weight output. Keep numerical parity and qualify full Apex. |
| 11 | Fused MoE combination. `ggml_cuda_match_moe_weighted_reduction` → `moe_weighted_reduction_f32`. | The reference fuses the weighted combination. Independent Goose uses the existing fused device body with bounded input windows and eliminates the separate weighted tensor. | PARTIAL. The 4096-token numerical fixture and real Apex trace pass the scoped gate. Full logits and peak memory acceptance remain open. |
| 12 | FP16 MMA Flash Attention. `fattn.cu` `ggml_cuda_flash_attn_ext` → shape selection → `fattn-mma-f16.cuh` `flash_attn_ext_f16`. | `GOOSE_FLASH_MMA=1` stages real Q4 KV as F16 and uses the existing `8×8` Goose process tile at all ten Apex attention layers. Each CTA finishes its complete causal query tile. | PARTIAL. Match EXP-1476 numerics, restore stream-K scheduling/fix-up and verify supported masks, long-context logits and full memory. |
| 13 | Small-query Flash Attention vector dispatch. `fattn.cu` → `flash_attn_ext_vec`. | Reference trace contains the vector path. Goose slices its scalar Q4 tiles for both prefill and decode. | MISSING. Restore the small-query specialization and validate softmax merging and decode latency at 32K. |
| 14 | Attention staging and bounds. `fattn-common.cuh` and `fattn-mma-f16.cuh` stage Q4_0 KV as FP16, use FWHT where selected, and derive KV bounds from the mask. | `dequantize_block_q4_0`, `fwht_cuda` and `flash_attn_mask_to_KV_max` execute. Goose now applies the reference Q/K/V and inverse-output rotations in the live Q4 KV graph. It tracks causal bounds in its KV transaction and reads Q4 directly. | PARTIAL. FWHT executes at widths 256 and 64 on Apex. Float16 staging/rounding, masked tails and generation parity still need validation. |
| 15 | Flash partition reconciliation. `fattn-mma-f16.cuh` → stream-K general/uniform fix-up, plus vector combine-results. | All three reference families are present. Goose merge implements its current scalar partition geometry. | MISSING. Port compatible reconciliation with the MMA and vector schedules. Compare numerical reductions across tail partitions and multihead layouts. |
| 16 | GDN prefill Tensor Cores. `ggml_cuda_op_gated_delta_net_impl` → `gdn_single` in `gated-delta-net-mma.cu`. | Reference executes `gdn_single<16,128,1>`. Independent Goose runs its `C=16, V=128` body with `GOOSE_GDN_MMA=1`. All 30 GDN layers execute it in a real Apex block. | PARTIAL. CPU and actual layer-input differential checks pass at the documented error levels. The short model output changes and still differs from EXP-1476. Validate full and chunked recurrent state and generation before default promotion. |
| 17 | GDN decode specialization and fused state cache. `ggml_cuda_op_gated_delta_net_fused_cache` → native scalar `gated_delta_net_cuda` or shape-selected MMA. | Reference executes scalar decode GDN and known fused cache code. Goose has CUDA-resident GDN state and a publish/consume path. | PARTIAL. Match the selected decode kernel's arithmetic and state snapshots against the reference across prefill → decode. Check for stale aliases and measure actual state moves. |
| 18 | SSM convolution and activation fusion. `ssm-conv.cu` dispatch plus `ggml_cuda_try_fuse` → `ssm_conv_long_token_f32` or `ssm_conv_f32`. | Both reference families execute. Goose applies SiLU inside its existing rolling convolution at all 30 Apex GDN layers. | PARTIAL. Restore the reference long-token schedule. Keep the fused/separate numerical fixtures and check persistent history at full context. |
| 19 | Fused RMS/L2 normalization. `ggml_cuda_try_fuse` → RMS multiply/add/scale variants in `norm.cu` and supported RoPE/cache combinations. | Goose connects its existing weighted RMS kernel at Qwen35 attention and GDN sites. The short Apex numerical snapshot matches the unfused graph byte-for-byte. | PARTIAL. Validate at full context and restore remaining compatible RMS/L2 and RoPE combinations. |
| 20 | Gated and unary activation fusion. `unary.cu` and `ggml_cuda_try_fuse` launch `unary_gated_op_kernel` and `unary_op_kernel`. | Both execute. Goose implements fused SwiGLU and several individual activations and keeps a CUDA activation arena. | PARTIAL. Verify the executed reference combinations, then match remaining graph fusion choices. Compare activations and adjacent gates before speed checks. |
| 21 | Broadcast arithmetic and reductions. CUDA operation dispatch → `k_bin_bcast`, `scale_f32`, `reduce_1Block_kernel`. | The reference runs optimized family variants. Goose implements broadcast vector and row-sum kernels for supported layouts. | PARTIAL. Restore shape-aware broadcasts/reductions where effect is measured. Verify offsets, F32/F64 reduction order and noncontiguous inputs. |
| 22 | RoPE execution. CUDA dispatch → `rope_multi` and its graph fusion with norms and cache. | `rope_multi` runs. Goose has positional transforms on GPU and CPU. | PARTIAL. Compare actual long prefix positions, mode flags and rotate/cache fusion. Check Q/K numerics at the first divergence. |
| 23 | Q4 cache writes. CUDA SET_ROWS dispatch → `k_set_rows_quant`. | Reference executes GPU Q4 quantization. Goose validates row updates and uses `goose_set_q4_rows` for dense supported spans, with ordered CPU sparse writes. | PARTIAL. Verify packed bytes and ordering against reference in the real cache. Measure writes across prefill and decode and retain safe repeated-index behavior. |
| 24 | Tensor copies, concatenation and gathers. CUDA transform dispatch → `concat_cont`, `concat_non_cont`, `cpy_scalar`, `k_get_rows_float` and vectorized gather. | All appear in reference trace. Goose has aligned lifetimes, GPU blits and gathers and host fallbacks. | PARTIAL. Match selected contiguous and strided paths while avoiding dead-tensor transfers. Check views, source lifetimes and numerical read-back. |
| 25 | Backend graph splitting and lifetime planning. GGML scheduler in `vendor/flint/ggml/src/ggml-backend.cpp` → `ggml-alloc.c` child/view counts, aligned best-fit, split/coalesced blocks and admissible in-place reuse. | Reference log records five buffers and eight same-size reallocations. Goose completes actual 4096/4096 using a ~1.010 GB lifetime arena and whole-block reuse. | PARTIAL. Retain the tested alias planner. Extend device lifetime ownership and compare backend memory peaks and retry behavior on identical profiles. |
| 26 | Expert tiers and pinned mapped weights. `vendor/flint/src/llama-model.cpp` → `llama-mmap-expert-pin.h` and CUDA buffer registration. | `cudaHostRegister` occurs 120 times in the reference session. Goose verifies all model pages resident, loads dense tensors and uses a GPU SLRU, with different upload scheduling. | PARTIAL. Associate registrations with the reference ranges, then restore explicit equivalent pinned transfer and expert-bank budgets. Verify real resident sizes, misses, bytes and swaps. |
| 27 | Selective transfer and device placement. Backend split execution and CUDA tensor set/get → asynchronous linear and 2-D copies. | Reference transfers selected ranges and uses D2D. Goose retains oversized activations, retires planned results and keeps unrelated live buffers during normal arena eviction. | PARTIAL. Extend reuse across adjacent oversized operations. Full-request transfer and memory parity remain open. |
| 28 | CUDA VMM pool. CUDA buffer allocator → `cuMemAddressReserve`, `cuMemCreate`, `cuMemMap`, `cuMemSetAccess`, release. | Reference executes 105 create/map operations and reserves a virtual address range. Goose grows Driver API buffers with `cuMemAlloc_v2` and `cuMemFree_v2`. | MISSING. Restore equivalent reusable device allocation and supported fallback. Check allocation limits, device address stability and VRAM peak on RTX 4070. |
| 29 | CUDA Graph capture and replay. `ggml-cuda.cu` graph evaluator → begin/end capture, instantiate, exec update and launch. | Whole-graph trace records over 20,000 launches across the session. Server reports 510 reuses. Goose rebuilds its graph and directly launches CUDA kernels. | MISSING. Establish stable graph keys, addresses and update boundaries before recording and replay. Check changing KV prefixes and buffer ownership before measuring decode overhead. |
| 30 | Streams and synchronization. CUDA scheduler and `ggml-cuda.cu` establish streams, events and CUDA Graph scopes. | Reference uses stream sync and async copies. Planned Goose graphs defer intermediate publication waits to the graph boundary, with blocking host reads. | PARTIAL. Extend independent stream and event dependencies. Verify full-request performance, error propagation and state visibility. |
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

## Q4 KV rotation checkpoint

EXP-2026-11-20-1524 reconnects the existing Goose FWHT warp body.
It follows EXP-1476 `llama_kv_cache` and `llm_graph_context::build_attn`
at `e7c9155a`: rotate Q and K after RoPE, rotate V before Q4_0
storage and apply the inverse V transform before the attention gate.
CPU and both CUDA images matched the Sylvester-matrix oracle exactly
for widths 64 and 256, five rows each, including the partial warp group.

The frozen short request executed 40 `goose_kv_fwht` kernels in
18.085 ms at physical 4096/4096. Prefill was 46.428 s, or 88.223
tokens/s. The earlier profiled MMQ observation was 46.363 s, or
88.346 tokens/s, on a different checkpoint. The runs do not
establish a speed or memory change. The first generated ID remains
different from the reference (Goose 9495, EXP-1476 332). This was
a one-token diagnostic without a decode throughput or RAM/VRAM
acceptance measurement. ELF dependencies remain libc and libm.

Raw data: `/home/end0/AI/LlamaServer/franken-goose-kv-fwht.TbKeRQ/mmq-profile.xv66Ks`.
Request SHA-256: `6aaecd1c3e3744ebc1d19039bc036fb808508b659dd44628618a0c27e9236973`.
Response SHA-256: `206b268a8bb9da0ac08a90f7f57a7d0c3560cfeab4618f9dd9cdaaf99c558bb6`.
Executable SHA-256: `8ec11f5f4ec138c5ecd7571f82b2b149e86f42c430902dbccf32c867cc23c023`.

## Covered activation overwrite checkpoint

EXP-2026-11-20-1525 skips downloads for dirty device records fully
covered by a dense, disjoint writer during output-only evaluation.
Overlapping inputs and partially covered records retain the existing
publication path. The CUDA fixture verifies full replacement, a
retained tail and an aliased input on both device images. Only the
full-replacement case drops the dirty record (256 bytes).

On the same 4096/1 input as EXP-1524, the first CUDA kernel to last
kernel window had 2,529 D2H copies and 89,922,142,208 bytes, down from
2,810 copies and 93,313,105,920 bytes. Recorded D2H device time fell
from 9.840 s to 9.427 s. H2D was unchanged at 105,306,630,016 bytes.
Prefill was 46.209 s, or 88.640 tokens/s, versus 46.428 s and 88.223
tokens/s. Both runs returned ID 9495 and the same text. These short
profiles measure transfer reduction. Stable throughput, full-request
generation, decode rate and RAM/VRAM still require acceptance.

Raw data: `/home/end0/AI/LlamaServer/franken-goose-overwrite.ybFFqc/mmq-profile.A1GnjM`.
Response SHA-256: `e6e320282997e1fd778f987084a0f69c828018df42f92c092820da1e1a691e85`.
Executable SHA-256: `7058ed51e0301df87c34524d74cef0df36668b71a8c64ac583183ba2acde0bf0`.

## Fused weighted expert reduction checkpoint

EXP-2026-11-20-1526 uses the canonical `fg179w_reduce` Goose body
and writes a single `[hidden,tokens]` result instead of materializing
the `[hidden,experts,tokens]` weighted tensor and summing its views.
The staging limit is 16 MiB for routed inputs per launch on Apex.
The physical microbatch remains 4096. The independent CPU oracle
matched every F32 result for one route and for 4096 tokens with
eight routes and a width tail. Both CUDA images exercised the
five-window fixture with a final partial window. The small-state
model mixed CPU/CUDA continuation had a maximum error of 1.19e-7.

The same real 4096/1 input ran 640 `goose_moe_weighted` launches in
19.919 ms. Prefill fell from 46.209 s to 37.137 s, or 88.640 to
110.293 tokens/s. H2D fell from 105,306,630,016 to 90,878,032,896
bytes. D2H fell from 89,922,142,208 to 71,198,769,152 bytes.
The 256 MiB weighted intermediate was removed from each graph
layer. This size describes a logical tensor, not peak process RAM
or VRAM. Both short runs produced the same token and text (9495,
` tail`). The reference token still differs. The full 32K/512
benchmark, peak memory and exact generation checks remain open.

Raw data: `/home/end0/AI/LlamaServer/franken-goose-moe-weighted.gjQFQa/mmq-profile.gcbhWG`.
Response SHA-256: `c3142113df588278d35a2108a43230b727818efa9acf74d1380832c881e74c63`.
Executable SHA-256: `6d30ef31d493ced9ba533c71233c6edd276d2063d211c28f45b8f69eea8a8e0a`.

## Dense TF32 MMA diagnostic

EXP-2026-11-20-1527 follows the independent MMQ tile organization
with a 16×8×8 F32-accumulating TF32 kernel for large F32 dense
projections. It resolves the leading TF32 value and two residual
cross-products. GPU fixtures cover odd dimensions and a 2048-wide
router-like shape, with maximum absolute FP64-reference error
5.04e-6. Operand and output fragments follow the
[NVIDIA PTX ISA](https://docs.nvidia.com/cuda/parallel-thread-execution/index.html#warp-level-matrix-fragment-mma-1688).

`GOOSE_DENSE_MMA=1` measured 52.069 ms across 40 Apex CUDA
launches, versus 565.205 ms for the corresponding scalar dense
launches. The EXP-1476 trace records 7.377 ms across 39 CUTLASS
launches. Prefill in the independent 4096/1 diagnostic took
35.498 s (115.386 tokens/s), versus 37.137 s (110.293 tokens/s)
without the dense flag. The first ID changes from 9495 to 37972
and EXP-1476 selects 332. The tested dense operation is available
for numerical investigation and remains off by default. The later
full diagnostic below enables it alongside the other changes.

Raw data: `/home/end0/AI/LlamaServer/franken-goose-dense-mma.vqcy2Z/mmq-profile.PJCwMh`.
Response SHA-256: `d8d141218cc32e9364c122c473e571e88222c48588a69a66bfa18f7033c44486`.
Executable SHA-256: `8d4f4371cea01543900846b245434d7c6f0ad693eedb57ac82784e7d44fc1a39`.

## Large CUDA activation checkpoint

EXP-2026-11-20-1528 retains a whole result larger than the 64 MiB
arena in its existing CUDA output allocation. This introduces no
new device allocation. Views can read this result directly on the
device. Host reads and reuse of the allocation first publish dirty
bytes. An independently verified disjoint, fully covering writer
may discard old bytes. Both device images passed a 67,125,248-byte
view-read test with a host sentinel, plus explicit publication and
buffer reassignment. The existing aliased-input and partial-write
checks also passed.

With the Dense MMA flag off in both runs and MMQ on, the identical
4096/1 Apex input took 25.272 s (162.078 tokens/s), down from
37.137 s (110.293 tokens/s). H2D dropped from 90,878,032,896 to
73,427,276,096 bytes and 26,990 to 26,260 copies. D2H changed from
71,198,769,152 to 71,601,422,336 bytes, and device D2H time from
7.558 s to 7.377 s. Both variants returned ID 9495 and ` tail`.
These are short profiles with the first-to-last kernel copy window.
Full-request speed, generation parity and memory remain separate gates.

Raw data: `/home/end0/AI/LlamaServer/franken-goose-large-activation.9mnAtU/mmq-profile.z4KQe7`.
Response SHA-256: `3749196097278a3605a0096b5d9d4755e487f2ec30699b24b0e9346d4e14d441`.
Executable SHA-256: `fa212c57aa64d49dfad999f3e868fd8b7543791acfc1d14a2baf8e387832481d`.

## Prefill package full 32K/512 diagnostic

Commit `903bc4e2` ran the frozen Apex Q4_K_M request on RTX 4070
with context 262,144, physical 4096/4096, no cached input, MMQ on
and the experimental Dense MMA enabled for this diagnostic. All
32,768 input IDs and 512 output IDs completed. Prefill was
390.083 s at **84.002538 tokens/s**. Decode was 43.072 s at
**11.863898 steps/s** across 511 steps. The previous independent
full diagnostic was 573.944 s / 57.092649 tokens/s and 49.370 s /
10.350416 steps/s. The new run shortens those intervals by 183.861 s
and 6.298 s. The EXP-1476 historical acceptance target remains
2,940.9 tokens/s prefill and 62.86 steps/s decode. The diagnostics
report single-request throughput. Matched reference parity is open.

The response discusses the input log, and all 512 probability rows
were returned. The first differing output is index 1 (Goose 1142,
reference 198). Text, IDs and probability hashes differ. The full
request does not pass the generation gate. The first 4,096-token
profile is a distinct workload and has different throughput.

The full model remained resident before and after (21,713,463,136
bytes and 5,301,139 pages). Process high-water RSS was 23,621,948 KiB
and the 211 samples recorded 8,568 MiB maximum process VRAM and
11,384 MiB whole-card VRAM. Process swap and system swap-out growth
were zero. Prefill arena size fell from 1,009,991,680 to 741,556,224
bytes. The previous independent diagnostic recorded 23,945,668 KiB
RSS and 8,696 MiB process VRAM. These are sampled diagnostic peaks.
There is no matched reference peak in this run. The candidate loads
the NVIDIA driver and system C libraries.

Raw data: `/home/end0/AI/LlamaServer/franken-goose-large-activation.9mnAtU/apex-functional.zQA8TW`.
Request SHA-256: `484a12de00ec057e4b78d480ebb185153bb10af607eb23192a72995ba3026d0f`.
Response SHA-256: `38b38b56b6e8aa57679dde3e2cae2eacd8ae32eb2c42ef768edfe3ffc1651722`.
Output IDs SHA-256: `d93f350741000ce81d3131ae7d82a4526527dcf4331b409ccf4d050e5bc18f13`.
Probability rows SHA-256: `992d83c84c347a9762cd907037f29b6d17a881e24c124e3e5238f6dac02e2227`.

## Final-consumer CUDA activation checkpoint

EXP-2026-11-20-1530 passes the host arena planner's tensor lifetimes
to the CUDA executor. The last consumer includes every view of the
same owner and the selected output remains live through host readback.
Only plan-owned temporaries have deadlines. Persistent state and
generic graphs retain their previous publication contract. Both CUDA
images verified a 67,125,248-byte activation stayed readable through
its last view and was then retired without updating the host sentinel.
The mixed CPU/CUDA KV/GDN continuation error remained 1.19e-7.

The sequential, profiled 4096/1 pair used MMQ on and Dense MMA off.
Prefill changed from 25.380 s / 161.388 tokens/s to 22.657 s /
180.784 tokens/s. D2H in the first-to-last CUDA kernel window fell
from 71,601,422,336 to 48,727,785,472 bytes (1,660 to 1,156 copies).
Device D2H time was 7.428 s before and 4.928 s after. H2D bytes and
copy count were unchanged. Text, output ID 9495 and all five reported
probabilities were identical. This was a short diagnostic with no
full-request throughput or memory qualification.

Control: `/home/end0/AI/LlamaServer/franken-goose-large-activation.9mnAtU/mmq-profile.qfUrRq`.
Candidate: `/home/end0/AI/LlamaServer/franken-goose-activation-lifetimes.UOpkCd/mmq-profile.Ngqd0s`.
Request SHA-256: `2d716460edf11494ce0fd16fe53241535b611c70774adc8fa3fe4619a6fcf81d`.
Candidate executable SHA-256: `9d57a6876152b08ed727a790a8b91c6d3d4679881852c4911195389bd57ffbc1`.

## Deferred CUDA wait diagnostic

EXP-2026-11-20-1531 tested deferring intermediate publication waits
to the planned graph boundary. CUDA operations used the legacy
default stream. Host reads still used blocking device-to-host copies.
[CUDA Driver API synchronization behavior](https://docs.nvidia.com/cuda/cuda-driver-api/api-sync-behavior.html).

Four sequential 4096/1 profiles in A–B–B–A order produced equivalent
text, token IDs and five probabilities. Prefill took 22.657 s and
23.093 s in A, versus 22.990 s and 22.774 s in B. The median changed
from 22.875 s to 22.882 s. H2D and D2H byte counts were unchanged.
Explicit context waits fell from 1,873 to 562 per run. Median time in
that API fell from 9.636 s to 3.798 s. Time in synchronous D2H calls
rose from 5.154 s to 10.754 s. API durations include waiting on
previous GPU work. The run shows redistributed waiting and no
prefill benefit. The change was rejected and does not count toward
the five verified improvements in this package.

Control profiles: `/home/end0/AI/LlamaServer/franken-goose-activation-lifetimes.UOpkCd/mmq-profile.Ngqd0s`
and `mmq-profile.rzm4yZ` in the same directory.
Candidate profiles: `/home/end0/AI/LlamaServer/franken-goose-graph-waits.OYG6LI/mmq-profile.c2mX9Y`
and `mmq-profile.nhsQOc` in the same directory.
Candidate executable SHA-256: `82bf7cce5d8ba9bd69a05ede48014d10b17efadf153df3b380f472b710a631f5`.

## Reusable CUDA arena gaps checkpoint

EXP-2026-11-20-1532 reuses aligned gaps and inactive metadata slots
in the 64 MiB arena. The adapter drains the arena when a whole
output cannot fit between live records. Both CUDA images verified
that a 32 MiB result stayed on device through eight 12 MiB
intermediates and its final view read. Existing overlap, oversized
output and mixed CPU/CUDA state checks passed.

On 4096/1, H2D fell from 73,427,276,096 to 70,810,030,400 bytes
(26,260 to 26,182 copies). Device H2D time was 6.408 s before and
6.008 s after. D2H rose from 48,727,785,472 to 48,794,894,336 bytes
(1,156 to 1,158 copies). Prefill took 22.760 s at 179.968 tokens/s
against the 22.657–23.093 s control range. The transfer change is
measured. Prefill improvement is within the observed variation.
Text, token and five probabilities were identical. No device buffer
was added. Peak process memory and full-request speed remain open.

Candidate profile: `/home/end0/AI/LlamaServer/franken-goose-activation-gaps.zxKUb9/mmq-profile.UQ3ctU`.
Candidate executable SHA-256: `2be0635954f65c803de606bbe744f941e8dfd08d2e1d8951564451ee4f955fbb`.

## Selective CUDA arena eviction checkpoint

EXP-2026-11-20-1533 scores eligible output intervals by the bytes
needed to evict and potentially reload overlapping live records. It
preserves unrelated records in the existing 64 MiB arena. In both
CUDA images a 32 MiB writer displaced only one of two 24 MiB live
results and the later consumer read both correctly. Oversized output,
gap reuse and mixed CPU/CUDA KV/GDN checks passed.

On the same 4096/1 input, prefill took 22.503 s at 182.018 tokens/s
versus 22.760 s at 179.968 tokens/s. D2H fell from 48,794,894,336
to 47,117,172,736 bytes (1,158 to 1,078 copies). H2D fell from
70,810,030,400 to 70,407,377,216 bytes (26,182 to 26,140 copies).
Text, token ID and five probabilities matched. No new device buffer
was added. This profile qualifies the transfer change. Repeat
performance and peak process memory remain full-request gates.

Candidate profile: `/home/end0/AI/LlamaServer/franken-goose-activation-eviction.8prNGA/mmq-profile.QapVkA`.
Candidate executable SHA-256: `4a177f7e5bda8ae8752d1e1bb327226050eebdca1c4955f617d2f6cdb6521014`.

## Deferred waits with selective eviction

EXP-2026-11-20-1534 applies the preserved EXP-1531 implementation
to `e37e8a2c` with reusable CUDA arena gaps and selective eviction.
The changed sources are byte-identical to those of the isolated
combination candidate. The eviction fixture and chunked model
continuation passed before profiling.

In 4096/1 A–B–B–A, the control took 22.503 s and 22.399 s. The
combination took 22.285 s and 22.282 s. Median prefill improved
from 22.451 s (182.440 tokens/s) to 22.283 s (183.813 tokens/s).
All four runs matched text, token ID and five probabilities. H2D
and D2H counts and bytes matched. Explicit waits again fell from
1,873 to 562. Blocking host reads still absorbed most of the removed
API wait. This comparison verifies a 0.168 s gain on the short
workload in the combined configuration. It does not isolate the
effect on full-context prefill or decode.

The planned graph path now checks queued CUDA work at graph exit,
including error exit. Generic execution and direct host publication
retain their existing waits. Peak memory and full-request correctness
remain open. No additional device buffer is used by this change.

Control profiles: `/home/end0/AI/LlamaServer/franken-goose-activation-eviction.8prNGA/mmq-profile.QapVkA`
and `mmq-profile.I4lJMQ` in the same directory.
Combination profiles: `/home/end0/AI/LlamaServer/franken-goose-eviction-waits.ZwSxaq/mmq-profile.NTa62g`
and `mmq-profile.2nTxqZ` in the same directory.
Executable SHA-256: `106e280f164e38cdb1955fdfc5a60cab98f9c3e2df3f9b23337197aa8f6a6f88`.

## Fused SSM convolution and SiLU checkpoint

EXP-2026-11-20-1535 connects the existing Goose SSM device leaf's
activation parameter in the Qwen35 graph and mirrors it in the CPU
fallback. Fused and separate outputs match bitwise at 1 and 4,096
tokens on CPU and both CUDA images, with 137 channels and two
sequences. The mixed-backend KV/GDN continuation error was 1.19e-7.

The same real 4096/1 input ran 30 SSM kernels and 30 fewer vector
kernels. Prefill changed from 22.282 s / 183.823 tokens/s to 21.763 s /
188.210 tokens/s. H2D fell from 70,407,377,216 to 66,380,839,136
bytes and D2H from 47,117,172,736 to 43,090,640,896 bytes. The
short answer, ID 9495 and all five probabilities matched the
preceding Goose checkpoint. Full-request speed, peak memory and
EXP-1476 generation parity remain open.

Candidate profile: `/home/end0/AI/LlamaServer/franken-goose-ssm-silu.1LvVP2/mmq-profile.lbLiJK`.
Executable SHA-256: `458d9850e798acc34bd8f29b92734f2c4ddef3d15e505adbd89048d05dabc357`.
Response SHA-256: `d2da1f35f6d5e1544177c571076704af3aea8bcdc8b1200b3fdc6012c6d561f2`.

## GDN MMA checkpoint

EXP-2026-11-20-1536 uses the existing Goose BF16 `C=16, V=128`
recurrence. OpenCL C provides only raw memory and PTX operations.
The SM80+ image enables it for qualified grouped F32 inputs with
scalar gates and one final state. The CUDA driver opts in to
93,312 bytes of dynamic shared memory. The 73-token, two-sequence
fixture exercised grouped keys, a partial chunk and final state.
Against the CPU recurrence, maximum absolute errors were 2.10e-8
for the output and 1.38e-7 for the final state.

Apex 4096/1 ran 30 MMA GDN kernels totaling 171.978 ms, compared
with 2,503.240 ms for 30 scalar kernels. Prefill took 19.425 s /
210.866 tokens/s, compared with 21.763 s / 188.210 tokens/s.
The first ID changed from 9495 to 280. EXP-1476 selects 332. An
isolated diagnostic ran both kernels on the same actual inputs
and initial state for each of 30 Apex layers. Maximum relative
L2 errors were 1.58e-5 for output and 3.71e-5 for final state.
Maximum absolute errors were 2.86e-5 and 7.92e-4. Both kernels
produced finite results throughout. These tests do not locate
the model-level divergence. The path remains opt-in and does
not count toward the current package's accepted improvements.

Short profile: `/home/end0/AI/LlamaServer/franken-goose-gdn-mma.ljN6DW/mmq-profile.squJAC`.
Layer differential: `/home/end0/AI/LlamaServer/franken-goose-gdn-audit.zqNjDD/mmq-profile.3UPTwP/nsys.log`.
Executable SHA-256: `b0231d453d115fa7101cdaa6288566f7135c6c2744c5b73350e1eaf353ed3a06`.
Response SHA-256: `f6b66a6000774215c3764dc6350df53a5ce80eb997d570525f5d27245f35456f`.

## Causal Flash Attention MMA checkpoint

EXP-2026-11-20-1537 uses the existing Goose `8×8` FP16 process tile
and async KV loaders. Q4 rows are staged through the existing decoder.
The causal mask is generated in the shared mask tile without a dense
host or device mask. The preset uses one CTA per complete query tile,
head width 256 and eight query heads per KV head. Stream-K and other
shapes remain separate. The 17-query, 57-key, 16:2 fixture exercised
both tails and measured a maximum absolute CPU error of 2.72e-5.

On Apex 4096/1, ten MMA calls took 34.117 ms and twenty Q4 stage
calls took 0.300 ms. The corresponding scalar tiles and merges took
2,738.669 ms. Prefill took 19.053 s / 214.977 tokens/s, compared
with 21.763 s / 188.210 tokens/s. The output ID changes from 9495
to 271. The independent attention calculation still needs reference
parity. In an isolated comparison on the same actual Q/K/V tensors
for each of ten Apex layers, relative L2 differences from scalar
attention ranged from 3.44e-3 to 6.44e-3, with no nonfinite values
and maximum absolute difference 0.168603. This is a differential
against independent scalar arithmetic. The path remains opt-in and
does not count toward the accepted package.

Short profile: `/home/end0/AI/LlamaServer/franken-goose-flash-mma.CpioRH/mmq-profile.guwzVl`.
Layer differential: `/home/end0/AI/LlamaServer/franken-goose-flash-audit.qfjatO/mmq-profile.d5MN40/nsys.log`.
Executable SHA-256: `754e480265e448918198295146422cb2125bddc7ef4284fd9707e2db909ce062`.
Response SHA-256: `709b803aa8f36448cbfb49eb3b5531abb636621b10af21515facb8224a5fd8c4`.

## Intermediate Apex performance control

The current intermediate speed control is `6083f55c`, the
GDN, Flash and Dense MMA combination with the
[ten-change package](#ten-change-experimental-package). The short
pair measured **281.919 tokens/s** at 14.529 s. Its numerical output
matched the preceding `0de1eff8` control. The
[full diagnostic](#ten-change-apex-32k512-diagnostic) reached
283.535 tokens/s and failed the response gate.
Enable it with these environment settings:

```sh
export GOOSE_MMQ=1 GOOSE_GDN_MMA=1 GOOSE_FLASH_MMA=1 GOOSE_DENSE_MMA=1
```

Use executable SHA-256
`194db934a930006f6cf3efd4063d4aacdf0a311646bc8d678d222dde6acae6b1`,
Apex Q4_K_M, context 262,144, physical 4096/4096, and the 4096/1
request with SHA-256
`2d716460edf11494ce0fd16fe53241535b611c70774adc8fa3fe4619a6fcf81d`.
The request uses seed 123, temperature 1, top-k 20, top-p 0.95 and
reports five log probabilities. This control is opt-in. EXP-1476
remains the correctness reference and the full-request gate remains open.
Leave `GOOSE_MMQ_STREAM_K` unset for this control.

The original EXP-1538 five-case matrix below used `3e616cb2`, executable
SHA-256 `754e480265e448918198295146422cb2125bddc7ef4284fd9707e2db909ce062`.
Five sequential Nsight runs used the same executable, request and memory
configuration. The Flash column includes KV staging or the scalar merge.
The Dense column covers the 40 Apex router projections. CUDA durations
are sums of device activity. H2D/D2H cover the first through last CUDA
kernel. RSS is process high-water, VRAM is the sampled peak.

| Variant | Prefill s | Tokens/s | GDN ms | Flash ms | Dense ms | H2D bytes | D2H bytes | RSS KiB | Process VRAM MiB | Card VRAM MiB |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Scalar mechanism control | 21.798 | 187.910 | 2,499.971 | 2,799.114 | 584.218 | 66,380,839,136 | 43,090,640,896 | 23,885,528 | 8,596 | 11,388 |
| GDN MMA | 19.336 | 211.837 | 171.778 | 2,814.737 | 583.417 | 66,328,587,320 | 43,090,640,896 | 23,885,500 | 8,596 | 11,387 |
| Flash MMA | 18.862 | 217.151 | 2,497.768 | 34.485 | 594.360 | 66,365,457,368 | 43,090,640,896 | 23,906,312 | 8,572 | 11,364 |
| GDN + Flash MMA | 16.510 | 248.085 | 165.009 | 35.745 | 584.822 | 66,388,725,296 | 43,090,640,896 | 23,905,132 | 8,572 | 11,347 |
| GDN + Flash + Dense MMA | 15.957 | 256.686 | 171.275 | 35.141 | 52.432 | 66,336,206,168 | 43,090,640,896 | 23,905,980 | 8,572 | 11,348 |

The individual GDN and Flash savings were 2.462 s and 2.935 s. Together
they saved 5.287 s, 0.110 s less than the additive prediction in this
single sequence. Dense saved a further 0.553 s at the same sampled process
VRAM peak. The five runs shared one memory configuration, so this series
measures Dense on top of the current memory work and does not isolate the
memory contribution. Each run kept all 5,301,139 model pages resident,
with zero process swap and zero system swap-out growth. There were 12,
11, 11, 10 and 9 memory samples respectively, including one post-request
sample in each count.

| Variant | Selected ID | Selected log probability | Top five IDs and log probabilities |
|---|---:|---:|---|
| Scalar mechanism control | 9495 | -0.1146200 | 9495 -0.1146200, 280 -3.3105867, 1866 -3.5725305, 14166 -4.2137613, 1901 -5.1594577 |
| GDN MMA | 280 | -0.1121834 | 280 -0.1121834, 387 -2.6394854, 605 -4.3271914, 198 -4.9755383, 271 -5.8182011 |
| Flash MMA | 271 | -2.0011609 | 20 -1.1342604, 198 -1.2773154, 271 -2.0011609, 29 -2.5919468, 220 -3.6042435 |
| GDN + Flash MMA | 271 | -2.3095357 | 303 -1.0703399, 198 -2.2048681, 271 -2.3095357, 37972 -2.8533170, 318 -3.6148250 |
| GDN + Flash + Dense MMA | 280 | -0.0385359 | 280 -0.0385359, 9495 -4.3836308, 198 -4.5553913, 79 -5.2518368, 605 -6.0141039 |

Matching selected IDs in two rows do not imply matching distributions.
Keep candidate flags off by default. Use the measured combination as a
speed control, with separate numerical checks and the existing package
schedule for the full Apex request.

Raw directories under `/home/end0/AI/LlamaServer/franken-goose-flash-mma.CpioRH`:

- `cuda-matrix-base.88bpsjq2`
- `cuda-matrix-gdn._nl2jjl9`
- `cuda-matrix-flash.svl0z1bs`
- `cuda-matrix-both.kd6dkqzf`
- `cuda-matrix-dense-both.2fydhi47`

Combined response SHA-256:
`ce276d3d12b5171ca3df5dab73ba52fba7598b0f031424c79b446fb3b21bd01f`.

## Byte-budgeted MMQ windows

EXP-2026-11-20-1539 uses the Goose MMQ tile plan to choose a window
from a 16 MiB packing target and the CUDA grid-Y bound. If the CUDA
allocation runs out of memory, the driver retries with smaller
windows. Expert boundaries, arithmetic, route order and physical
microbatch size are preserved. This reduces dispatch overhead and
leaves the independent stream-K implementation open. The GPU dense
and indexed Q4_K/Q6_K oracle matched exactly.

A fresh serialized 4096/1 pair used all three MMA flags on both
builds. Prefill changed from 16.033 s / 255.476 tokens/s to
15.799 s / 259.252 tokens/s. Each of MMQ pack and matmul fell
from 10,397 to 830 launches. MMQ matmul device time fell from
3,109.283 to 2,957.320 ms. Pack time fell from 95.628 to 89.121 ms.
H2D and D2H bytes were identical. Process RSS peaks were
23,905,636 and 23,885,044 KiB. Sampled process VRAM was 8,572 and
8,582 MiB. Both runs had zero process swap and system swap-out
growth. Content, output ID 280 and all five probabilities matched.
This passes the short package gate. EXP-1476 response and full
performance parity remain open.

Control: `/home/end0/AI/LlamaServer/franken-goose-flash-mma.CpioRH/cuda-matrix-dense-both.lvij361a`.
Candidate: `/home/end0/AI/LlamaServer/franken-goose-mmq-window.z2Wknq/cuda-matrix-dense-both.y2_5obc4`.
Executable SHA-256: `c6a801cd58aaa99efa7a92a220dd30d07f6864cf60e119134a14dd7d48fc2429`.
Candidate response SHA-256: `552eedccb6520e10a9bf642b24ab700cd7d78b363f89fbd12d8838f24e2de00a`.

## Numerical checkpoint for the combined speed control

Set `GOOSE_APEX_TRACE_DIR` to an existing, empty directory for a
separate diagnostic request. Use a new directory per request. The
Goose executor observes the existing layer frames before their
temporary storage can be reused. Each planned graph step writes
`step-<prefix>.bin` and `step-<prefix>.jsonl`. The manifest records
roles, layer, dtype, GGML shape and compact row ranges, byte offsets
and byte counts. A final completion record gives the prefix and
input token count. A missing completion record means the capture
is incomplete. Data is written in the native little-endian format
on the tested host. Keep captured tensor data outside Git.

The 4096/1 checkpoint uses byte-budgeted MMQ with GDN, Flash and
Dense MMA enabled. It contains embeddings, the first and last
rows of the norm, feed-forward and final output of each of 40
layers, all 1,310,720 expert IDs, and all 248,320 F32 logits for
the final token. It also contains active K/V rows for 10 attention
layers and history and recurrent states for 30 SSM/GDN layers.
The payload is 97,691,648 bytes. The manifest was checked for
all expected roles, contiguous byte spans, valid expert IDs and
finite logits. Logit argmax and sampled ID are both 280. Content
and all five probabilities match the captured speed control.
This run adds device-to-host reads and is for numerical comparison.

Capture: `/home/end0/AI/LlamaServer/franken-goose-numerical-checkpoint.dMq92a/numerical.4xbhLK`.
Binary data SHA-256: `099f8ebeabe1f6813f6c0d721803df3a4ab2b87c2b11903af0006222b8bda24b`.
Manifest SHA-256: `9bfd7b9be25c3d9e9843687cc94ff4f2b72358cd9b2049c11eaa4275de52be6e`.
Raw logits SHA-256: `02aefbb757ce92cb2324460ee44d21366900a61f8b62882b75f890fb1df57f1e`.
Diagnostic executable SHA-256: `d7f8095c8634caecc3b62421a55df9aae991ed93e3cfbca4d82ebedc687513a4`.

## MMQ Stream-K checkpoint

EXP-2026-11-20-1540 restores flattened tile/K partitioning and
backward partial reduction using the standalone 64×8 output
tile and the existing integer MMA body. The planner uses the
reference 90% wave-efficiency threshold and the actual 46 SMs.
Only the final fragment writes the result. Earlier fragments
write private worker slots. The regular path retains full-K sums.
The five-projection Q4_K/Q6_K fixture ran five Stream-K windows
and five fixups with maximum absolute error 1.49e-7. The final
compute-sanitizer run reported zero errors after correcting the
initial scratch slot stride.

On the same Apex prompt's first 32 tokens, 160 of 430 MMQ windows
used Stream-K. Total MMQ arithmetic including fixup fell from
59.266 to 54.748 ms. Prefill changed from 855.517 to 913.819 ms
and the selected ID was 321 in both runs. The five probability
rows differed. On 4096/1, all 830 windows used regular MMA. Text,
ID 280 and five probabilities matched the intermediate control.
Prefill was 15.938 s / 256.993 tokens/s at 8,582 MiB sampled VRAM.
This saves the implementation for future geometry and acceptance
work. The experiment does not increase the accepted package count.

32-token control: `/home/end0/AI/LlamaServer/franken-goose-mmq-window.z2Wknq/cuda-matrix-dense-both.azuvybtf`.
32-token candidate: `/home/end0/AI/LlamaServer/franken-goose-mmq-stream.3qNa4P/cuda-matrix-dense-both.02l9svtu`.
4096-token candidate: `/home/end0/AI/LlamaServer/franken-goose-mmq-stream.3qNa4P/cuda-matrix-dense-both.g7yjzq0j`.
Candidate executable SHA-256: `e8a6f1ae8afd9d035aa8a12a413fd140c2de723d1683c42a010ab4719d74eb41`.

## Shared MMQ activation rows

EXP-2026-11-20-1541 recognizes cached routed projections with one
input activation per token. Each token is packed once with the
existing Q8_1 leaf. The tile reads the same packed row for every
expert route using the original job's byte offset. The route order,
weight offsets and MMA arithmetic are unchanged. Separate input
slots, oversized packing requests and allocation pressure retain
the job-window path. The dense/indexed GPU fixture matched exactly
with ordinary MMQ. The shared + Stream-K fixture stayed within
1.49e-7 and compute-sanitizer reported zero errors.

In the matched combined Apex 4096/1 pair, pack and matrix launches
each fell from 830 to 510. Packed rows fell from 5,201,920 to
2,908,160, including 327,680 rows packed by 80 shared launches.
Total arithmetic tiles were unchanged. Pack device time fell from
89.185 to 43.877 ms and matrix time from 2,982.817 to 2,793.722
ms. The same first-to-last CUDA kernel window had identical H2D
and D2H bytes. Prefill was 15.867 s before and 15.893 s after.
This passes the short work-reduction gate, with no measured
request speedup in this pair. ID 280, text and all five reported
probabilities matched. Sampled process VRAM was 8,582 MiB in both
runs and process swap was zero.

The reverse pair completed the short A–B–B–A sequence. Control times
were 15.867 and 15.925 s. Candidate times were 15.893 and 15.575 s.
The medians were 15.896 and 15.734 s (257.674 and 260.324 tokens/s),
with overlapping ranges. All four responses and probability rows
matched. In a separate diagnostic run, the full 97,691,648-byte
numerical snapshot and manifest were byte-identical to the original
combined snapshot, including logits, expert IDs, layer rows and state.
The candidate is the new interim speed control.

Control: `/home/end0/AI/LlamaServer/franken-goose-mmq-window.z2Wknq/cuda-matrix-dense-both.objw61et`.
Candidate: `/home/end0/AI/LlamaServer/franken-goose-mmq-shared.TrPFV2/cuda-matrix-dense-both.livri7ys`.
Reverse control: `cuda-matrix-dense-both.qtr76i16` in the control build directory.
Reverse candidate: `cuda-matrix-dense-both.1suq6d3n` in the candidate build directory.
Numerical snapshot: `/home/end0/AI/LlamaServer/franken-goose-mmq-shared.TrPFV2/numerical.JLjr4r`.
Candidate executable SHA-256: `13d5f2828e4f079ab06613aa1ca4fe686eb5682ddc8a110bfb841c8a4050f806`.

## Direct eight-expert selection

EXP-2026-11-20-1542 shares the strict warp winner reduction from
EXP-171. On the 256-expert, eight-result, unmasked-softmax path,
one warp selects one row and the smallest expert ID wins equal
scores. Other shapes and generic sorting retain their existing
dispatch. The routing fixture verified equal inputs, a partial
four-warp group and exact CPU IDs. Compute Sanitizer reported
zero errors.

All 40 Apex 4096/1 routers used direct selection. Total selector
time fell from 15.310 to 0.628 ms and static shared memory from
16,896 bytes per CTA to zero. Prefill was 15.649 s before and
15.707 s after. CUDA transfer bytes were identical. Peak process
RSS was 23,884,940 and 23,885,144 KiB. Sampled process VRAM was
8,582 MiB in both runs with zero process swap. ID 280, text and
five probabilities matched. A separate 97,691,648-byte capture
and its manifest matched the interim control byte-for-byte.
This passes the short work-reduction gate. Full MoE fusion,
EXP-1476 generation and full-request speed remain open.

Control: `/home/end0/AI/LlamaServer/franken-goose-mmq-shared.TrPFV2/cuda-matrix-dense-both.pcl8oufl`.
Candidate: `/home/end0/AI/LlamaServer/franken-goose-router-topk.Fvuato/cuda-matrix-dense-both.2oz_xztl`.
Numerical snapshot: `/home/end0/AI/LlamaServer/franken-goose-router-topk.Fvuato/numerical.zEwphX`.
Executable SHA-256: `e454509024062f5b59bc92b7cbaef98628f70fe28cc698398765d87137701a2d`.

## Fused expert-weight normalization

EXP-2026-11-20-1543 replaces the separate gather, sum, clamp, divide
and optional scale stages with one Goose operation. It preserves the
existing selection and softmax, F64 accumulation, F32 rounding and
2^-14 clamp. The 37-route fixture covered the two-slot warp reduction,
clamping and scaling. Both CPU and CUDA matched the separate graph
bitwise. Compute Sanitizer reported zero errors. The small model's
mixed backend continuation error remained 1.19e-7.

On matched Apex 4096/1, the new graph ran 40 normalized gather
kernels, eliminating 120 net CUDA launches. H2D calls fell from
26,008 to 25,888 while D2H bytes stayed unchanged. The block took
15.632 s / 262.032 tokens/s versus 15.725 s / 260.484 tokens/s
in the preceding control. Sampled process VRAM was 8,582 MiB
and swap zero in both. Selected ID 280, text and probabilities
matched. A separate numerical payload and manifest matched all
97,691,648 bytes. This passes the work-reduction gate. Full-request
throughput and EXP-1476 response parity remain open.

Control: `/home/end0/AI/LlamaServer/franken-goose-router-topk.Fvuato/cuda-matrix-dense-both.dyfbhtfw`.
Candidate: `/home/end0/AI/LlamaServer/franken-goose-route-norm.C436JC/cuda-matrix-dense-both.knvpkl98`.
Numerical snapshot: `/home/end0/AI/LlamaServer/franken-goose-route-norm.C436JC/numerical.8exlSO`.
Executable SHA-256: `3ffebfc5f012a0bfab8122636d4f4f451710ef5be55f1a714a988b25fba6a024`.

## Weighted RMS in the Qwen35 graph

EXP-2026-11-20-1544 connects the existing Goose weighted RMS path
at the attention and GDN layer sites. The head RMS remains available
as a separate numerical checkpoint. The strided broadcast-weight
fixture matched fused and separate results bitwise on CPU and CUDA.
Compute Sanitizer reported zero errors. `GOOSE_RMS_MUL=0`,
`GOOSE_MOE_NORM=0` and `GOOSE_SSM_SILU=0` each restore their
separate graph path. The mixed CPU/CUDA model fixture passed with
each option disabled individually.

On one identical executable with all three experimental MMA flags,
Apex 4096/1 took 15.619 s / 262.243 tokens/s with separate RMS and
14.573 s / 281.072 tokens/s with weighted RMS. This removed 130
vector launches. H2D dropped from 66,336,182,488 to 61,000,992,568
bytes and D2H from 43,090,640,896 to 37,688,377,344 bytes. Sampled
process VRAM remained 8,582 MiB. ID 280, text and five probabilities
matched. The separate 97,691,648-byte numerical capture and manifest
matched the prior combination byte-for-byte. This is the tenth
short-verified improvement. Full-request qualification is separate.

Control: `/home/end0/AI/LlamaServer/franken-goose-rms-mul.sOoyid/cuda-matrix-dense-both._m0eovju`.
Candidate: `/home/end0/AI/LlamaServer/franken-goose-rms-mul.sOoyid/cuda-matrix-dense-both.r9xuapi6`.
Numerical snapshot: `/home/end0/AI/LlamaServer/franken-goose-rms-mul.sOoyid/numerical.rAeym5`.
Executable SHA-256: `f17cad7af7bfc3e543912f20c605b1cfc45aa0b4af20ee454e8f60f742a9e251`.

## Ten-change experimental package

The candidate uses the verified ten changes with the independent
optimization switches enabled, plus `GOOSE_MMQ=1`,
`GOOSE_GDN_MMA=1`, `GOOSE_FLASH_MMA=1`, and `GOOSE_DENSE_MMA=1`.
Leave `GOOSE_MMQ_STREAM_K` and `GOOSE_APEX_TRACE_DIR` unset for
the speed request. Each option below defaults to on and accepts
`0` to exercise the preceding mechanism in its place.

| Mechanism | Individual off switch |
|---|---|
| Last-consumer retirement | `GOOSE_ACTIVATION_LIFETIMES=0` |
| CUDA activation gap reuse | `GOOSE_CUDA_ARENA_REUSE=0` |
| Selective CUDA eviction | `GOOSE_CUDA_SELECTIVE_EVICTION=0` |
| Deferred waits | `GOOSE_CUDA_DEFER_WAITS=0` |
| Fused SSM SiLU | `GOOSE_SSM_SILU=0` |
| Byte-budgeted MMQ windows | `GOOSE_MMQ_WINDOW=0` |
| Shared Q8_1 rows | `GOOSE_MMQ_SHARED=0` |
| Direct Top-K | `GOOSE_MOE_TOPK=0` |
| Fused expert-weight normalization | `GOOSE_MOE_NORM=0` |
| Weighted RMS | `GOOSE_RMS_MUL=0` |

Selective eviction applies when gap reuse is active. The window
switch restores a 64-tile launch limit even if shared rows are
active. The three model graph switches restore the separate
operations. All ten off switches passed their targeted fixtures
individually. The combined build also passed activation eviction,
weighted RMS and MoE normalization fixtures with the switches on.

A new 4096/1 pair compared the published intermediate control with
the enabled package. Prefill was 15.501 s / 264.242 tokens/s versus
14.529 s / 281.919 tokens/s. H2D fell from 66,336,206,168 to
61,000,992,568 bytes. D2H fell from 43,090,640,896 to 37,688,377,344
bytes. Sampled process VRAM was 8,582 MiB in both runs, process
swap was zero, and ID 280, text and all five probabilities matched.
Use the frozen full request for the package diagnostic.

Short control: `/home/end0/AI/LlamaServer/franken-goose-mmq-shared.TrPFV2/cuda-matrix-dense-both.7rsy224s`.
Short package: `/home/end0/AI/LlamaServer/franken-goose-apex-package.KY0RZy/cuda-matrix-dense-both.cxs3gezu`.
Package executable SHA-256: `194db934a930006f6cf3efd4063d4aacdf0a311646bc8d678d222dde6acae6b1`.

## Ten-change Apex 32K/512 diagnostic

The enabled package ran one frozen Apex Q4_K_M request with context
262,144, physical 4096/4096, 32,768 evaluated prompt IDs and 512
returned IDs. No prompt tokens were cached. Nsight recorded CUDA
and OS runtime activity with node-level graph tracing. The older
Goose full checkpoint was a non-Nsight run. The EXP-1476 node trace
is from a previous session and its memory snapshots are from the
archived unprofiled run. These comparisons are diagnostic.

| Metric | Previous full Goose | Ten-change full Goose | EXP-1476 archived reference |
|---|---:|---:|---:|
| Prefill s | 390.083 | 115.569 | 12.448 (node trace) |
| Prefill tokens/s | 84.003 | 283.535 | 2,632.479 (node trace) |
| Decode s | 43.072 | 40.355 | 8.880 (node trace) |
| Decode steps/s, 511 steps | 11.864 | 12.663 | 57.546 (node trace) |
| RSS high-water KiB | 23,621,948 | 23,943,212 | 21,874,576 (archived process status) |
| Sampled process VRAM MiB | 8,568 | 8,622 | unavailable |
| Whole-card VRAM MiB | 11,384 sampled peak | 11,414 sampled peak | 11,031 after requests |
| Process swap KiB | 0 | 0 | 0 |

All 5,301,139 model pages remained resident at start and finish.
There were 78 memory samples and zero system swap-out growth.
The prefill arena was 739,459,072 bytes with 13 blocks. Historical
unprofiled EXP-1476 targets remain 2,940.9 prefill tokens/s and
62.86 decode steps/s. Keep the established same-session acceptance rule.

| CUDA copies between first and last recorded kernels | Ten-change Goose count / bytes | EXP-1476 node trace count / bytes |
|---|---:|---:|
| H2D | 1,156,051 / 522,686,099,072 | 173,187 / 136,901,438,100 |
| D2H | 28,367 / 302,015,238,912 | 21,505 / 1,950,097,760 |
| D2D | 0 / 0 | 15,972 / 10,454,296,320 |

The previous full Goose checkpoint has no CUDA copy trace. The
short matched pair in the package section supplies the incremental
copy comparison. Nearly all candidate D2H occurred before the first
scalar GDN launch (301,508,012,032 bytes), an approximate phase
boundary. Prefill MMQ spent 22.354 s in 4,080 matrix launches.
The subsequent decode-dominated region spent 10.031 s in scalar
quantized matmul and 7.688 s in scalar Q4 attention tiles. These
device activity sums are not additive with CPU wait time.

The first differing output is index 1, Goose ID 1716 versus reference
198. The previous full Goose selected 1142 at that position. The new
response begins by repeating text from the input log. Content, all
output IDs as a sequence and probability rows fail the reference gate.
Retain this faster checkpoint with explicit experimental MMA flags.
The final acceptance checklist remains open.

Raw run: `/home/end0/AI/LlamaServer/franken-goose-apex-package.KY0RZy/cuda-full-dense-both.vbzm5qhq`.
Request SHA-256: `484a12de00ec057e4b78d480ebb185153bb10af607eb23192a72995ba3026d0f`.
Response SHA-256: `18571316dd6445ed2318a0abff9236defe446dd827e0d3faeec282748a45eb9b`.
Validation: `validation.json` in the raw run directory.
Nsight report SHA-256: `8bc18131ceafb8828efed2820a03504ced1e23bda9677ee00d6be8e6b1e52fd3`.

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
