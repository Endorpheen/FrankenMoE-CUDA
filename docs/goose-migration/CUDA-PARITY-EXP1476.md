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
quality evidence on common real requests and a matching performance
and memory check. Use focused tensor checks for an observed
correctness error. PARTIAL means an executed mechanism or verified
component exists, with a missing capability or an open parity check.
MISSING means the reference mechanism has no live independent
equivalent. INCORRECT means a comparable output fails its reference
check. Kernel and API duration sums in the appendices are observations.
Per-mechanism effects on rates and peak allocations require
paired ablations.

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
| EXP-1563, routed MMQ input residency | [Candidate patch and full pair](../../experiments/EXP-2026-11-20-1563-mmq-input-residency/README.md). Protecting the SwiGLU input removes 21,474,836,480 bytes in each direction and moves full prefill from 694.731 to 773.585 tok/s with matching responses. | Peak process VRAM rises from 8,524 to 8,748 MiB. The candidate run records 6,286 system swap-out pages with zero process swap. | Recheck when planned arena placement can keep the live input and result in the current capacity. Compare unchanged outputs and remeasure the 64 MiB transfers, VRAM and swap. |

After a material dependency change, use short sequential A/B runs on
identical inputs and settings. Use the best current verified checkpoint
as the control. Compare the combined candidate and, when necessary, the
individual toggles to isolate interactions. Record IDs, probabilities,
prefill, decode and the targeted hardware metrics. Enable a combination
in the working runtime only after it passes correctness and shows a real
end-to-end gain. Preserve each implementation and append new evidence to
the entry. Apply the full-request gate on the package schedule.

## Mechanisms

[The runtime performance audit](APEX-PERFORMANCE-AUDIT.md)
records cross-mechanism timing and the historical optimization
ledger, including lost KV laziness and expert upload overlap.

**32 mechanisms: PARITY 0, PARTIAL 31, MISSING 0, INCORRECT 1.**

Group three to five related improvements with a separate commit and
targeted correctness and CUDA checks for each. Evaluate real answers
on matched prompts before accepting faster paths. Run the full Apex
32K/512 profile when needed to qualify a package or a generation
repair. Keep the previous checkpoint and isolate observed
regressions with a focused test.

First restore the remaining reference mechanisms with shape-aware
dispatch and live RTX 4070 traces. Reuse existing Goose implementations
for compatible graph shapes. Every row requires live execution,
correctness, quality, performance and memory evidence to reach PARITY.
Use the [measured gaps](#external-computation-stack-and-measured-gaps)
to prioritize parity work. Record the final Apex 32K/512 baseline after
all 32 rows pass and the new runtime meets the EXP-1476 performance
target in a fresh paired comparison. The archived target is 2,940.9
prefill and 62.86 decode tokens/s.

| # | Mechanism and EXP-1476 call path | Evidence and independent Goose state | Status, restoration and verification |
|---:|---|---|---|
| 1 | Q4_K MMQ. `ggml-cuda.cu` `ggml_cuda_mul_mat` / `ggml_cuda_mul_mat_id` → `mmq.cu` `ggml_cuda_mul_mat_q` → `mmq-load-tiles.cuh` and `mmq-vec-dot.cuh` → `mul_mat_q`. | [Dense and routed 16-by-64 tiles](#q4_k-16-by-64-cooperative-tiles) reuse an LDSM fragment across two warp accumulators. [Aligned packing windows](#aligned-dense-mmq-windows) retain shared dispatch under the existing budget. | PARTIAL. Extend other shapes and improve remaining tail utilization. Check full quality and speed parity. |
| 2 | Q6_K MMQ, same backend chain with `ggml_cuda_mmq_vec_dot_q6_K_q8_1_mma`. | [Dense and routed 16-by-64 Q6_K](#q6_k-16-by-64-cooperative-tiles) reuse LDSM fragments and signed scales across two warp accumulators. Both execute on real Apex in the reference accumulation order. | PARTIAL. Qualify more shapes and close full quality and speed parity. |
| 3 | Activation packing. `mmq.cu` → direct or scatter `quantize_mmq_q8_1` in `quantize.cu`, then MMQ. DS4 carries a half scale and activation sum. D4 carries F32 scales. | Both direct and scatter templates execute in the reference. Independent MMQ reuses the EXP180 packer and shares one Q8_1 row across routes when their input slot is identical. The Q6_K diagnostic adds the reference D4 reciprocal sequence. | PARTIAL. Both formats execute on Apex. Device scatter/grouping and model-level parity remain open. |
| 4 | MMQ stream-K. `mmq.cuh` partitions K and invokes `mul_mat_q_stream_k_fixup` to combine partial outputs. | Independent Goose partitions flattened output tiles and K blocks and applies backward partial reduction with `GOOSE_MMQ_STREAM_K=1`. GPU fixtures and a short Apex shape execute the fixup. | PARTIAL. Verify response parity and scheduling on reference-sized tiles. The current 4096 geometry selects regular tiles. |
| 5 | Quantized small-batch MMVQ. `ggml-cuda.cu` → `ggml_cuda_mul_mat_vec_q` → Q8_1 and `mul_mat_vec_q` in `mmvq.cu`. | `GOOSE_MMVQ=1` enables the existing Goose Q4_K/Q6_K dot products and Q8_1 packer for single-column projections. [CUDA trace and replays](#independent-mmvq-decode-checkpoint). | PARTIAL. The qualified shapes execute and improve short decode. Validate full context and other small-batch specializations. |
| 6 | Indexed small-batch MMVQ. `ggml_cuda_mul_mat_id` selects indexed `mul_mat_vec_q` or `mul_mat_vec_q_moe`. | The same Goose MMVQ option handles one or eight cached routes using the existing expert plane offsets. All three isolated expert projections match. | PARTIAL. Check remaining routed launch variants, shared packs and full acceptance. |
| 7 | Tensor Core dense GEMM. `ggml_cuda_mul_mat_cublas` selects the F32 `cublasSgemm` path with `CUBLAS_TF32_TENSOR_OP_MATH` in `common.cuh`. | The reference uses CUTLASS TF32. The [Goose router mode](#apex-tf32-router-and-expert-selection) matches its unsplit and split-K results and executes 40 times in Apex 4096/1. | PARTIAL. The tested router shapes match. Verify other shapes and the full response before promotion. |
| 8 | Dense small-shape dispatch. `mmf.cu` and `mmvf.cu` implement `mul_mat_f` and `mul_mat_vec_f`. The trace also records `dot_kernel`. | All three families appear in the trace. Goose does dense F32 FMA and shuffles using one output per warp. The exact library owner of `dot_kernel` remains open. | PARTIAL. Identify the dot route and reuse existing dot and warp code where equivalent. Recreate selected row/vector tiling and validate dimensions and backend selection by shape. |
| 9 | Device expert grouping. `ggml_cuda_mul_mat_id` / `mmq.cu` → `mm_ids_helper` → per-expert group ranges and Q8 scatter. | `mm_ids_helper` executes. Goose builds per-expert jobs and keeps a GPU SLRU, with host route synchronization. | PARTIAL. Feed active routes to device grouping and pack contiguous batches without losing expert ownership. Verify sparse, repeated and empty groups against the reference. |
| 10 | Fused MoE selection. `ggml-cuda.cu` `ggml_cuda_topk_moe_fusion` → `topk_moe_cuda`. | Goose selects eight experts directly and fuses gather, clamp, division and scaling in a single weights kernel. The live Apex snapshot matches the preceding graph byte-for-byte. | PARTIAL. Integrate softmax and selection with the weight output. Keep numerical parity and qualify full Apex. |
| 11 | Fused MoE combination. `ggml_cuda_match_moe_weighted_reduction` → `moe_weighted_reduction_f32`. | The reference fuses the weighted combination. Independent Goose uses the existing fused device body with bounded input windows and eliminates the separate weighted tensor. | PARTIAL. The 4096-token numerical fixture and real Apex trace pass the scoped gate. Full logits and peak memory acceptance remain open. |
| 12 | FP16 MMA Flash Attention. `fattn.cu` `ggml_cuda_flash_attn_ext` → shape selection → `fattn-mma-f16.cuh` `flash_attn_ext_f16`. | `GOOSE_FLASH_MMA=1` stages real Q4 KV as F16 and uses the existing `8×8` Goose process tile at all ten Apex attention layers. [Stream-K](#flash-attention-stream-k-restoration) also executes on eligible causal shapes. | PARTIAL. Verify the full package, other reference tile sizes and supported masks. |
| 13 | Small-query Flash Attention vector dispatch. `fattn.cu` → `flash_attn_ext_vec`. | [Goose vector decode](#vector-flash-attention-for-q4-decode) executes its Q4_0 path, including a masked tail and partition merge, and improves the 4096/129 decode median. | PARTIAL. Check 32K/512 and other small-query or masked profiles. |
| 14 | Attention staging and bounds. `fattn-common.cuh` and `fattn-mma-f16.cuh` stage Q4_0 KV as FP16, use FWHT where selected, and derive KV bounds from the mask. | `dequantize_block_q4_0`, `fwht_cuda` and `flash_attn_mask_to_KV_max` execute. Goose applies the reference rotations in the live Q4 KV graph. [Bounded single-query loops](#bounded-vector-attention-loops) reduce full vector Attention GPU time from 9.052 to 3.509 s. | PARTIAL. FWHT executes at widths 256 and 64 on Apex. Float16 staging/rounding, masked tails and full quality still need validation. |
| 15 | Flash partition reconciliation. `fattn-mma-f16.cuh` → stream-K general/uniform fix-up, plus vector combine-results. | The [standalone MMA path](#flash-attention-stream-k-restoration) selects and executes both Goose fixups on RTX 4070 with real Apex. Vector decode retains its existing combine-results path. | PARTIAL. Include Stream-K in the next package qualification. Check 32K quality and memory, then tune the choice of workers and tile size. |
| 16 | GDN prefill Tensor Cores. `ggml_cuda_op_gated_delta_net_impl` → `gdn_single` in `gated-delta-net-mma.cu`. | Reference executes `gdn_single<16,128,1>`. Independent Goose runs its `C=16, V=128` body with `GOOSE_GDN_MMA=1`. All 30 GDN layers execute it in a real Apex block. | PARTIAL. The [first layer's real scores and state](#first-recurrent-sigmoid-and-silu) now match bitwise. Validate full and chunked recurrent state and generation before default promotion. |
| 17 | GDN decode specialization and fused state cache. `ggml_cuda_op_gated_delta_net_fused_cache` → native scalar `gated_delta_net_cuda` or shape-selected MMA. | Reference executes scalar decode GDN and known fused cache code. Goose has CUDA-resident GDN state and a publish/consume path. | PARTIAL. Match the selected decode kernel's arithmetic and state snapshots against the reference across prefill → decode. Check for stale aliases and measure actual state moves. |
| 18 | SSM convolution and activation fusion. `ssm-conv.cu` dispatch plus `ggml_cuda_try_fuse` → `ssm_conv_long_token_f32` or `ssm_conv_f32`. | Both reference families execute. Goose fuses SiLU and now executes its [long-token schedule](#reference-long-ssm-convolution) on all 30 recurrent layers of the short prefill with the diagnostic selector enabled. | PARTIAL. Verify persistent history across multiple batches and the final decode profile. |
| 19 | Fused RMS/L2 normalization. `ggml_cuda_try_fuse` → RMS multiply/add/scale variants in `norm.cu` and supported RoPE/cache combinations. | Goose connects its existing weighted RMS kernel at Qwen35 attention and GDN sites. The short Apex numerical snapshot matches the unfused graph byte-for-byte. | PARTIAL. Validate at full context and restore remaining compatible RMS/L2 and RoPE combinations. |
| 20 | Gated and unary activation fusion. `unary.cu` and `ggml_cuda_try_fuse` launch `unary_gated_op_kernel` and `unary_op_kernel`. | Both execute. Goose implements fused SwiGLU and several individual activations. The opt-in [sigmoid and SiLU](#first-recurrent-sigmoid-and-silu) match the first recurrent layer's reference arithmetic. | PARTIAL. Verify other gates and remaining graph fusion choices. |
| 21 | Broadcast arithmetic and reductions. CUDA operation dispatch → `k_bin_bcast`, `scale_f32`, `reduce_1Block_kernel`. | The reference runs optimized family variants. Goose implements broadcast vector and row-sum kernels for supported layouts. | PARTIAL. Restore shape-aware broadcasts/reductions where effect is measured. Verify offsets, F32/F64 reduction order and noncontiguous inputs. |
| 22 | RoPE execution. CUDA dispatch → `rope_multi` and its graph fusion with norms and cache. | `rope_multi` runs. Goose has positional transforms on GPU and CPU. | PARTIAL. Compare actual long prefix positions, mode flags and rotate/cache fusion. Check Q/K numerics at the first divergence. |
| 23 | Q4 cache writes. CUDA SET_ROWS dispatch → `k_set_rows_quant`. | Reference executes GPU Q4 quantization. Goose validates row updates and uses `goose_set_q4_rows` for dense supported spans, with ordered CPU sparse writes. | PARTIAL. Verify packed bytes and ordering against reference in the real cache. Measure writes across prefill and decode and retain safe repeated-index behavior. |
| 24 | Tensor copies, concatenation and gathers. CUDA transform dispatch → `concat_cont`, `concat_non_cont`, `cpy_scalar`, `k_get_rows_float` and vectorized gather. | All appear in reference trace. Goose has aligned lifetimes, GPU blits and gathers and host fallbacks. | PARTIAL. Match selected contiguous and strided paths while avoiding dead-tensor transfers. Check views, source lifetimes and numerical read-back. |
| 25 | Backend graph splitting and lifetime planning. GGML scheduler in `vendor/flint/ggml/src/ggml-backend.cpp` → `ggml-alloc.c` child/view counts, aligned best-fit, split/coalesced blocks and admissible in-place reuse. | Reference log records five buffers and eight same-size reallocations. Goose completes actual 4096/4096 using a ~1.010 GB lifetime arena and whole-block reuse. | PARTIAL. Retain the tested alias planner. Extend device lifetime ownership and compare backend memory peaks and retry behavior on identical profiles. |
| 26 | Expert tiers and pinned mapped weights. `vendor/flint/src/llama-model.cpp` → `llama-mmap-expert-pin.h` and CUDA buffer registration. | [Goose SLRU and mmap registration](#expert-mmap-registration-and-slru) execute in real Apex. The bounded mode registers 28 ranges, transfers 6,459,162,624 bytes from pinned sources and improves the no-swap 4096/129 median. | PARTIAL. Validate full context, quality and performance against EXP-1476. Full registration remains rejected on this host. |
| 27 | Selective transfer and device placement. Backend split execution and CUDA tensor set/get → asynchronous linear and 2-D copies. | The [adaptive arena](#adaptive-cuda-activation-arena), [resident vector path](#resident-wide-vector-multiplication) and [last-use matrix input staging](#device-staging-for-the-last-use-matrix-input) remove the large temporary prefill publications. | PARTIAL. Investigate remaining expert uploads, dispatch packets and synchronous host transfers. Full memory and speed parity remain open. |
| 28 | CUDA VMM pool. CUDA buffer allocator → `cuMemAddressReserve`, `cuMemCreate`, `cuMemMap`, `cuMemSetAccess`, release. | [Scratch VMM](#cuda-vmm-scratch-restoration) grows stable MMQ and Attention buffers. [Lazy KV VMM](#lazy-q4-kv-vmm-restoration) saves 1,240 MiB on full Apex 32K/512 and preserves generation through a short request after reset. | PARTIAL. Qualify the larger package, full-capacity model growth and other supported layouts. |
| 29 | CUDA Graph capture and replay. `ggml-cuda.cu` graph evaluator → begin/end capture, instantiate, exec update and launch. | [Goose vector Attention capture](#cuda-graph-replay-for-vector-attention) executes the two-node vector/merge graph and updates it with current pointers and key counts. The hybrid trace records over 20,000 graph launches across the full workload. | PARTIAL. Qualify 32K and extend capture to other dependency-safe decode sequences. |
| 30 | Streams and synchronization. CUDA scheduler and `ggml-cuda.cu` establish streams, events and CUDA Graph scopes. | Planned Goose graphs [defer matrix waits](#deferred-matrix-waits), queue [small pinned packets asynchronously](#asynchronous-h2d-packets), and [overlap selected expert uploads](#overlapped-expert-projection-uploads) through a separate CUDA stream. | PARTIAL. Reduce remaining blocking uploads and qualify larger independent graph scopes. |
| 31 | Prefill → decode continuation. Reference `llama-context.cpp` scheduler and CUDA graph compute produce logits for the shared sampler. | The earlier full Goose response repeated input text. The newer checkpoint completes the [normal quality suite](#apex-response-quality-checkpoint) and the vector Attention cases. | INCORRECT on the last full quality diagnostic. Revalidate the current checkpoint on the full real workload. Investigate a specific reproduced behavioral defect. |
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

## Reference RMS at the first tensor boundary

EXP-2026-11-20-1545 connects `goose-cuda-normalization/device.goose`
to the standalone graph with `GOOSE_RMS_REFERENCE=1`. For the first
Apex layer, EXP-1476 (`e7c9155a01938b1825189240e7088620c6c7cd85`)
dispatches `rms_norm_f32<512,1,...>` from `norm.cu`. It reduces F32
squares across a block and applies `rsqrtf`. The standalone vector
kernel used an F64 warp sum and reciprocal square root via division.
The selected Goose kernel uses the existing F32 block reduction and
`rsqrt.approx`. CPU and CUDA fixtures exercised weighted and unweighted
RMS. Compute Sanitizer reported zero errors on the CUDA fixture.

The reference observer captured the first 124 IDs of the 128-ID
request before its expert-cache assertion interrupted the diagnostic
run. Goose evaluated those same 124 IDs at physical 4096/4096.
The sampled input embeddings already matched bitwise. The following
comparison uses tensors captured before the assertion.

| First-layer boundary | Earlier Goose | Reference RMS enabled |
|---|---:|---:|
| `attn_norm` differing F32 values (253,952 total) | 113,691 | **0** |
| `attn_norm` max absolute error | 3.8146973e-6 | **0** |
| QKV max absolute error | 1.3351440e-5 | 1.3351440e-5 |
| QKV relative L2 error | 1.91e-7 | 1.811958e-7 |
| GDN output relative L2 error | 8.4100463e-5 | 6.0680383e-5 |
| Differing first-layer expert ID slots (992 total) | 5 | 5 |

The next observed mismatch is at flat index 1 of
`linear_attn_qkv_mixed-0`. Both short 124/1 diagnostics selected ID
1142. On a sequential 4096/1 pair with MMQ, GDN MMA, Flash MMA and
Dense MMA enabled, the checkpoint path took 14.333 s (285.779 tok/s)
and selected ID 280. The reference RMS path took 14.350 s
(285.431 tok/s) and selected ID 37972. Both recorded 8,582 MiB
sampled process VRAM and zero process swap. RSS high-water was
23,884,492 versus 23,884,780 KiB. Nsight recorded 191 launches of
`goose_rms_reference` taking 44.766 ms on the device. Keep this
selector opt-in while response correctness remains open.

- Reference tensors: `/home/end0/AI/LlamaServer/franken-apex-reference-layers.e5xq75bf/tensors`.
- Earlier Goose tensors: `/home/end0/AI/LlamaServer/franken-apex-goose-first-layer-layers.9kozm1qk/tensors`.
- Reference RMS tensors: `/home/end0/AI/LlamaServer/franken-apex-goose-reference-rms-layers.g4j3vpm_/tensors`.
- Short pair: `/home/end0/AI/LlamaServer/franken-goose-rms-reference.rTDLHd/cuda-matrix-dense-both.ezw_9pb6` and `cuda-matrix-dense-both.af1q54gb` in the same build directory. Executable SHA-256 `33de2cc3761eae47c4044acb01d97ffb6811ec30a8aa9bcff6c6fd7fecb0e6b1`.

### Existing device code and standalone selection

This wiring audit uses the standalone build manifest, dispatch
sources and the recorded full Apex trace above. Mechanism statuses
stay in the main map.

| Mechanism | Existing Goose source | Observed standalone path and missing link |
|---|---|---|
| 5–6, Q4_K/Q6_K MMVQ | `goose-cuda-mmvq-q4k/device.goose`, `goose-cuda-mmvq-q6k/device.goose` | Wired for the qualified [dense and indexed decode shapes](#independent-mmvq-decode-checkpoint). The earlier full trace spent 10.031 s in 261,118 scalar matrix launches in the decode-dominated region. |
| 9, MoE grouping | `goose-cuda-mm-ids/{device,host}.goose` | Standalone groups matrix planes on the host and uses shared Q8 rows in MMQ. The device grouping/scatter source needs its own dispatch and route-layout verification. |
| 13, Flash decode | `goose-cuda-flash-attention/vector-device.goose` | Connected to single-query Q4_0 decode with its partition merge. The active trace and boundary tests are in the [vector decode checkpoint](#vector-flash-attention-for-q4-decode). |
| 15, Flash Stream-K | `goose-cuda-flash-attention/{mma-entry-device,mask-fixups-device}.goose` | [Both prefill fixups](#flash-attention-stream-k-restoration) are connected behind the package selector. The reference uses 80 general and 10 uniform Stream-K fixup launches on the full workload. |
| 18, long SSM convolution | `goose-cuda-ssm-convolution/device.goose::ssm_conv_long` | The [reference SSM mode](#reference-long-ssm-convolution) now selects the existing tiled body for eligible four-tap graphs and matches the first Apex convolution and SiLU. The short trace records 30 long-kernel launches. |
| 19, FP32 RMS | `goose-cuda-normalization/device.goose::rms_row` | Connected through the opt-in selector above. Verified bitwise on the first Apex norm. |

## Reference Q6_K at the QKV boundary

EXP-2026-11-20-1546 compared Apex's first QKV projection using the
bitwise-identical RMS output and `blk.0.attn_qkv.weight` (Q6_K,
2,048 by 8,192) from the same GGUF. An isolated CUDA graph used the
frozen EXP-1476 libraries and reproduced all 1,015,808 values from
the reference layer capture. Nsight observed D4 packing, 46 CTAs
of `mul_mat_q<(ggml_type)14,128,false,(ggml_prec)30>` and its Stream-K
fixup on sm89.

The arithmetic comparison separated three effects:

1. Goose applied both scales to each 16-element integer dot. The
   reference CUDA path in `mmq-vec-dot.cuh` combines two such dots
   with signed scales in I32, accumulates four D4-scaled terms for
   each 128-element half with FMA, then applies the weight scale
   with another FMA.
2. D4 packing produced identical int8 values and 2,724 differences
   among 7,936 F32 scales. EXP-1476's SM89 disassembly uses
   `MUFU.RCP`, multiplies by 127, then applies `MUFU.RCP` again.
   Goose's previous packer used two rounded divisions.
3. With reference scales and FMA order, a single 2,048-element
   accumulation still differed in 327,878 values. Replaying the
   original 128-row tiles, 46 worker partitions and backward
   fixup reproduced every value.

`GOOSE_MMQ=1 GOOSE_MMQ_Q6_REFERENCE=1` selects a separate Q6_K
entry for eligible dense, 128-row-aligned projections on SM89.
It reuses the packed loaders and int8 MMA. Each output warp
reproduces the reference partial sums in registers, without
allocating a global fixup buffer for this path.

With `GOOSE_RMS_REFERENCE=1` the actual 124-ID Apex graph reproduces
both the first RMS and QKV tensors byte-for-byte. On the isolated
projection with the same Apex weights and activations, the live
CUDA graph reached MMQ and Compute Sanitizer reported zero errors.
The next observed mismatch is `conv_output_silu-0`, with 496,733
differing values and maximum absolute error 1.9073486e-6.
The `linear_attn_out-0` relative L2 error fell from 6.0680383e-5 to
4.5949156e-5. The same five of 992 first-layer expert ID slots differ.

| Sequential Apex 4096/1 diagnostic | Ordinary Q6_K | Reference Q6_K |
|---|---:|---:|
| Prefill time, s | 14.434 | 14.292 |
| Prefill, tokens/s | 283.781 | 286.587 |
| RSS high-water, KiB | 23,884,772 | 23,884,968 |
| Sampled peak process VRAM, MiB | 8,582 | 8,582 |
| Sampled peak whole-card VRAM, MiB | 11,377 | 11,390 |
| Process swap, KiB | 0 | 0 |
| System swap-out increase, pages | 6,216 | 159 |
| First token | 37972 | 37972 |
| First token log probability | -0.0439093 | -0.0801412 |

The common request uses physical 4096/4096, a fresh context, FP32
reference RMS and MMQ/GDN/Flash/Dense MMA. The candidate executed
40 `goose_mmq_q6_reference` launches totaling 343.546 ms. The
ordinary MMQ entry still used 72 registers per thread. This single
profiled pair had different system swap-out and supplies diagnostic
timings. Decode and response correctness remain open.

- Build and raw evidence: `/home/end0/AI/LlamaServer/franken-goose-q6-reference.cSIziF`.
  The executable SHA-256 is `cd11ae8244342c1d6f5b03b3bba9070688cd49c01477ed9243b72ec95a4f2700`.
  The run directories are `cuda-matrix-dense-both.kr0x9587` and
  `cuda-matrix-dense-both.gtehhqmh`. Request SHA-256
  `2d716460edf11494ce0fd16fe53241535b611c70774adc8fa3fe4619a6fcf81d`.
- Goose layer capture: `/home/end0/AI/LlamaServer/franken-apex-goose-q6-final-layers.qctxqfjm/tensors`.
  The reference QKV SHA-256 is `13612e80f796c369125e191cf63b86d24bfac5f252dc66d72d0a54e77e6806e1`.

Complete the tensor diagnosis before the full audit of existing
Goose CUDA kernels. For each applicable Apex mechanism, record
its build and graph binding, a CUDA trace witness and the numerical
gate in this map. The audit feeds the full 32K/512 acceptance run.

## Reference long SSM convolution

EXP-2026-11-20-1547 reuses the existing Goose `ssm_conv_long`
body. With `GOOSE_SSM_REFERENCE=1`, the standalone graph selects it
for four taps, channel groups of 128 and more than 32 tokens.
The kernel uses 32-token shared-memory tiles and bounds the final
tile to the input extent. The compiled EXP-1476 SM89 kernel shows
four `FFMA.FTZ` accumulations, `MUFU.EX2` and an approximate
reciprocal for SiLU. These operations are explicit device
primitives in the Goose path.

Using the captured first-layer Apex QKV and actual GGUF weights,
both the standalone convolution and the fused SiLU matched
the reference for 33, 65 and 124 tokens (270,336, 532,480 and
1,015,808 FP32 values respectively in each mode). Compute
Sanitizer reported zero errors. The live 124-ID Apex graph
also matched all 1,015,808 values of `conv_output_silu-0`.
The next observed mismatch remains downstream of the convolution.
The captured `linear_attn_out-0` has max absolute error 2.2947788e-5
and relative L2 error 4.2930902e-5. Five of 992 expert ID slots
still differ. Capture Q/K normalization, gate, beta and the
recurrent output to isolate the first preceding mismatch.

| Sequential Apex 4096/1 diagnostic | Serial SSM | Long SSM |
|---|---:|---:|
| CUDA convolution launches | 30 | 30 |
| CUDA convolution time, ms | 110.705 | 24.902 |
| Prefill time, s | 14.400 | 14.077 |
| Prefill, tokens/s | 284.435 | 290.961 |
| RSS high-water, KiB | 23,884,628 | 23,885,164 |
| Sampled peak process VRAM, MiB | 8,582 | 8,582 |
| Sampled peak whole-card VRAM, MiB | 11,385 | 11,376 |
| Process swap, KiB | 0 | 0 |
| System swap-out increase, pages | 126 | 698 |
| First token | 37972 | 280 |

The profiled pair used physical 4096/4096, no reused prompt
tokens, reference RMS and Q6_K, and MMQ/GDN/Flash/Dense MMA.
The long path used 17,920 shared bytes and 25 registers per
thread. The different first token keeps this mode opt-in while
the response gate is open. This pair provides no decode result.

- Build and runs under
  `/home/end0/AI/LlamaServer/franken-goose-ssm-reference.1wy4IG`.
  Executable SHA-256 `3922f3230010398fd770beb2189efcd84f2ae576e9864c5c97873147fc1e9284`.
  Run directories `cuda-matrix-dense-both.lika0hlf` and
  `cuda-matrix-dense-both.p1k4kz82`.
- Goose layer capture under
  `/home/end0/AI/LlamaServer/franken-apex-goose-ssm-reference-layers.0v8prrd0/tensors`.
  Reference fused SSM tensor SHA-256
  `3a4687304ca86a30ca9f696a18e2b44024f9ad754c0737cb5ccd3db431e8ed7a`.

## First recurrent Q and K normalization

EXP-2026-11-20-1548 makes the vector sum-of-squares order explicit
in the existing Goose RMS kernel. SM89 disassembly of the
EXP-1476 32-thread RMS+scale specialization shows a rounded
square of the second component, followed by three `FFMA.FTZ`
instructions for the first, third and fourth components.
The prior standalone PTX rounded each product independently.

The reference observer captured the first 124 IDs before the
expert-cache assertion interrupted its diagnostic request, as in
the preceding capture. Goose used those same IDs. The first RMS,
QKV, and SSM convolution+SiLU remain byte-exact. The weighted
RMS fixture passed on CPU and CUDA, with zero Compute Sanitizer
errors.

| First-layer boundary, 124 IDs | Before explicit FMA | After |
|---|---:|---:|
| Q after RMS+scale, differing FP32 values | 19,305 of 253,952 | 0 |
| K after RMS+scale, differing FP32 values | 17,031 of 253,952 | 0 |
| GDN output max absolute error | 3.0994415e-6 | 1.9073486e-6 |
| Final GDN state max absolute error | 2.3841858e-5 | 2.7656555e-5 |
| `linear_attn_out-0` max absolute error | 2.2947788e-5 | 1.9788742e-5 |

Compare the recurrent output and the materialized persistent
state separately. The resident Goose path can write the state
directly and leave the unused output tail unmaterialized.
The parallel GDN inputs still differ: `a_softplus` has 515
differences, `gate` has 489 and `beta_sigmoid` has 1,748
(3,968 values in each). Z has 416,341 differences in
507,904 values. Among the five differing expert ID slots,
four belong to two reordered pairs. The set changes once,
at input index 51 (expert 106 versus 223).

| Sequential Apex 4096/1 diagnostic | Earlier RMS | Explicit FMA |
|---|---:|---:|
| Prefill, tokens/s | 289.524 | 289.596 |
| Reference RMS GPU time, ms (191 launches each) | 43.236 | 43.395 |
| RSS high-water, KiB | 23,884,864 | 23,884,520 |
| Sampled peak process VRAM, MiB | 8,582 | 8,582 |
| Sampled peak whole-card VRAM, MiB | 11,387 | 11,380 |
| Process swap, KiB | 0 | 0 |
| System swap-out increase, pages | 267 | 131 |
| First token | 280 | 20 |

Both profiled runs use physical 4096/4096, reference RMS, Q6_K
and SSM, and all four experimental MMA selectors. The existing
experimental settings remain opt-in. Decode and full response
correctness are open.

- Candidate build
  `/home/end0/AI/LlamaServer/franken-goose-qk-reference.mMaQVd`,
  executable SHA-256 `cf515485794c260477a481c070e9204ede25893a246ff6d00be9cc17bd712863`.
  Short candidates under `cuda-matrix-dense-both.70blc6n_`
  and control under
  `/home/end0/AI/LlamaServer/franken-goose-ssm-reference.1wy4IG/cuda-matrix-dense-both.xit70f3_`.
- Tensor captures under
  `/home/end0/AI/LlamaServer/franken-apex-reference-gdn-layers.sam0tjof`,
  `/home/end0/AI/LlamaServer/franken-apex-goose-gdn-layers.2jwhn0rw`
  and `/home/end0/AI/LlamaServer/franken-apex-goose-qk-reference-layers.1ukbak9i`.

## First recurrent Q4_K projections

EXP-2026-11-20-1549 extends the existing reference-order MMQ
path to dense Q4_K with `GOOSE_MMQ_Q4_REFERENCE=1` on SM89.
Apex layer 0 has Q4_K `ssm_alpha`, `ssm_beta` and `attn_gate`
weights. Isolated EXP-1476 CUDA graphs reproduced their recorded
124-ID outputs byte-for-byte. The trace showed eight workers
and a fixup for each 32-row projection and 46 workers with a
fixup for the 4,096-row Z projection.

On these activations DS4 packing was already identical. The
SM89 DS4 sequence uses two approximate reciprocals, as does the
existing D4 reference packer. The opt-in path uses that sequence
for DS4, makes the two FMA accumulations explicit and reuses
the Q6_K reference tile partition and backward fixup in registers.
An independent reduction with the recorded launch geometry
reproduced all three projections.

| Live Apex first layer, 124 IDs | Differing values after Q4_K change | Total values |
|---|---:|---:|
| `alpha` projection | 0 | 3,968 |
| `beta` projection | 0 | 3,968 |
| Z projection | 0 | 507,904 |
| Softplus and gate, each | 0 | 3,968 |
| `beta_sigmoid` | 418 | 3,968 |
| GDN output | 60,127 | 507,904 |
| GDN persistent state | 104,508 | 524,288 |

Compute Sanitizer found zero errors on all three CUDA projection
replays with the Apex weights. Maximum error is 5.9604645e-8 for
`beta_sigmoid`, 5.9604645e-7 for the recurrent scores and
2.8133392e-5 for the final persistent state. First-layer expert set
selection still differs at input index 51.

| Sequential Apex 4096/1 diagnostic | Earlier Q4_K | Reference Q4_K |
|---|---:|---:|
| Prefill time, s | 14.137 | 14.224 |
| Prefill, tokens/s | 289.734 | 287.964 |
| RSS high-water, KiB | 23,883,792 | 23,884,860 |
| Sampled peak process VRAM, MiB | 8,582 | 8,582 |
| Sampled peak whole-card VRAM, MiB | 11,378 | 11,369 |
| Process swap, KiB | 0 | 0 |
| System swap-out increase, pages | 176 | 179 |
| First token | 20 | 1156 |

Both runs used reference RMS, Q6_K and SSM at physical
4096/4096, with four experimental MMA selectors enabled.
The trace records 310 `goose_mmq_q4_reference` launches, totaling
1,227.433 ms. The earlier Q4_K mode remains the speed control.
Keep the reference Q4_K selector for numerical diagnosis.

- Candidate build and raw data under
  `/home/end0/AI/LlamaServer/franken-goose-gdn-q4-reference.dU50Rf`.
  Executable SHA-256 `689859b65962ce055b8f4361114b95fe2aea2c2baaa196c52e401e3ce1a92a9d`.
  Short run directories `cuda-matrix-dense-both.ohrwnin4` and
  `cuda-matrix-dense-both.qyewdhb2`.
- Live diagnostic tensors under
  `/home/end0/AI/LlamaServer/franken-apex-goose-gdn-q4-reference-layers.wmyaiiwo`.

## First recurrent sigmoid and SiLU

EXP-2026-11-20-1550 adds `GOOSE_CUDA_ACTIVATION_REFERENCE=1`
for F32 sigmoid and SiLU in the existing Goose vector kernel.
The reference SM89 sigmoid uses `MUFU.EX2`, addition and
`MUFU.RCP`. The gated SiLU also multiplies the reciprocal
by its input before applying the gate. The earlier standalone
operations used rounded division. The candidate reuses the
reciprocal primitive already used by reference SSM and MMQ.

The first-layer beta CUDA replay matched all 3,968 probabilities
bitwise with zero Compute Sanitizer errors. In the live 124-ID
Apex graph, sigmoid alone matched the beta, recurrent output
and persistent state. With the final sigmoid and SiLU mode,
every checked tensor through the first recurrent output
projection matches EXP-1476:

| Boundary | Bitwise matches | Total FP32 values |
|---|---:|---:|
| Beta sigmoid | 3,968 | 3,968 |
| GDN scores | 507,904 | 507,904 |
| Materialized GDN state | 524,288 | 524,288 |
| `linear_attn_out-0` | 253,952 | 253,952 |
| Post-attention norm, sampled first and last rows | 4,096 | 4,096 |

The first layer still differs in five expert ID slots, including
one different expert set at input index 51. Compare the complete
router input, logits and selection. The earlier Q/K, alpha,
gate, Z, RMS, QKV and fused SSM checkpoints remain bitwise exact.

| Sequential Apex 4096/1 diagnostic | Earlier activations | Reference activations |
|---|---:|---:|
| Prefill time, s | 14.297 | 14.276 |
| Prefill, tokens/s | 286.490 | 286.913 |
| RSS high-water, KiB | 23,885,164 | 23,884,476 |
| Sampled peak process VRAM, MiB | 8,582 | 8,582 |
| Sampled peak whole-card VRAM, MiB | 11,380 | 11,388 |
| Process swap, KiB | 0 | 0 |
| System swap-out increase, pages | 98 | 8 |
| First token | 1156 | 11 |

The CUDA trace records 30 `goose_gdn_mma` and 631
`goose_vector_ops` launches in each run. The request uses
physical 4096/4096, reference RMS/Q4_K/Q6_K/SSM and the four
experimental MMA selectors. The same 4096 prompt IDs were
evaluated with no cache reuse. Generation remains a separate
acceptance gate and the activation selector stays opt-in.

- Candidate build and raw evidence under
  `/home/end0/AI/LlamaServer/franken-goose-activation-reference.EfN4Tf`.
  Executable SHA-256 `17ec85e21a036a42369cf5014a6d9a0b928aaa498afdf0600adb1a16fb7fa6ca`.
  The run directories are `cuda-matrix-dense-both.w1tpxrum`
  and `cuda-matrix-dense-both.7idajz91`.
- Live diagnostic tensors under
  `/home/end0/AI/LlamaServer/franken-apex-goose-activation-reference-layers.3ohgn0ej`.

## Apex TF32 router and expert selection

EXP-2026-11-20-1551 isolated the first remaining expert
selection error. All 253,952 values entering the first router
matched. The ordinary Goose compensated TF32 path differed in
31,727 of 31,744 logits (max 0.0016851425). At input index 51,
reference logits for experts 106 and 223 were -3.3826885 and
-3.3832430. The earlier Goose values were -3.3830009 and
-3.3829997. The ranking changed before softmax.

The frozen reference's isolated router replays execute
CUTLASS TF32 `s1688gemm`. The 124-row input executes
`64x64_16x6_tn_align4` with grid 8×1×4, then
`splitKreduce_kernel`. The 4096-row input executes
`64x128_32x3_tn_align4` with grid 128×1×1. The Goose
`GOOSE_DENSE_MMA_REFERENCE=1` mode reuses its existing
TF32 MMA and the measured split order for the Apex router
shape (SM89, 2,048 input features, 256 experts, 124 or
4,096 inputs). Other shapes continue through the existing
CUDA dispatch. The mode runs within the independent image.

The live 124-ID model graph matches all 31,744 logits and
all 992 selected expert ID positions, including expert 106
at index 51. Softmax probabilities still differ by at most
1.4901161e-8. The first and last sampled output rows of layer 0
differ by at most 2.9802322e-8 and 3.7252903e-9 respectively.
The isolated router replays match all 31,744 and 1,048,576
logits at 124 and 4096 input rows. The 4096 replay uses repeated
captured Apex rows to exercise that geometry. Compute Sanitizer
found zero errors on both shapes.

| Sequential Apex 4096/1 diagnostic | Compensated TF32 | Reference router |
|---|---:|---:|
| Prefill time, s | 14.432 | 14.351 |
| Prefill, tokens/s | 283.814 | 285.410 |
| Dense MMA time, ms (40 launches) | 54.878 | 58.053 |
| RSS high-water, KiB | 23,884,940 | 23,885,184 |
| Sampled peak process VRAM, MiB | 8,582 | 8,582 |
| Sampled peak whole-card VRAM, MiB | 11,377 | 11,385 |
| Process swap, KiB | 0 | 0 |
| System swap-out increase, pages | 0 | 0 |
| First token | 11 | 16305 |

The matched live tests use physical 4096/4096 and the same
diagnostic options from EXP-1550. Nsight shows 40 launches of
`goose_dense_mma_reference` in the candidate. This pair measures
short prefill. The full response, decode and context acceptance
remain open.

- Candidate build and traces under
  `/home/end0/AI/LlamaServer/franken-goose-router-reference.e5bVFu`.
  Executable SHA-256 `593a46fbc568c4ae7eb30d6ba76171a820dedcf5a467e0f6b992d084cfe2d489`.
  The run directories are `cuda-matrix-dense-both.tbpu27ge`
  and `cuda-matrix-dense-both.g5gbb6os`.
- Live tensors under
  `/home/end0/AI/LlamaServer/franken-apex-reference-moe-layers.j3zgs9_t`
  and
  `/home/end0/AI/LlamaServer/franken-apex-goose-router-reference-layers.f5yrjvcd`.

## Apex indexed expert MMQ

EXP-2026-11-20-1552 traced the first large residual error
back to layer 0's MoE down projection. For input index 33,
the reference and Goose SwiGLU activations differed by at
most 1.1920929e-7, the Q6_K down result differed by 2.632942e-5,
and the MoE output by 6.6715293e-6. The graph has Q4_K gate
and up (2,048 features, 512 rows) and Q6_K down (512
features, 2,048 rows), all with 256 experts and eight routes
per input.

Isolated EXP-1476 CUDA graphs exactly reproduced the
recorded expert projections. Their SM89 trace shows
`mm_ids_helper<8>`, direct or scatter Q8_1 packing, and
1,024, 1,024 and 4,096 MMQ CTAs respectively. The tiles
cover the whole K range and the trace has no Stream-K
fixup. The full reference trace likewise assigns a complete
K range to these expert tiles. Goose reuses the reference
Q4_K/Q6_K accumulation and packing order for these indexed
shapes, including its existing shared-row and expert-cache
dispatch.

A 124-ID CUDA fixture using the actual mapped GGUF weights,
selected routes and captured input rows matched every bit of
the 507,904 up, 507,904 gate and 2,031,616 down values.
The trace counters show three cached MMQ projections.
Compute Sanitizer found zero errors.

In live Apex 124/1, gate and up also match bitwise. The
down error at index 33 falls to 3.7252903e-9. The remaining
SwiGLU arithmetic differs by up to 2.3841858e-7 and its
downstream quantization still exposes an outlier at input
index 19. The isolated down replay shows the distinction
between a discrepancy from its input and its own arithmetic.

| Sequential Apex 4096/1 diagnostic | Earlier indexed math | Reference indexed math |
|---|---:|---:|
| Prefill time, s | 14.284 | 14.308 |
| Prefill, tokens/s | 286.747 | 286.270 |
| Reference Q4_K launches | 310 | 390 |
| Reference Q6_K launches | 40 | 80 |
| Ordinary MMQ launches | 160 | 40 |
| RSS high-water, KiB | 23,885,640 | 23,885,396 |
| Sampled peak process VRAM, MiB | 8,582 | 8,582 |
| Sampled peak whole-card VRAM, MiB | 11,376 | 11,375 |
| Process swap, KiB | 0 | 0 |
| System swap-out increase, pages | 0 | 0 |
| First token | 16305 | 332 |

Candidate ID 332 and emitted `**` match the historical
EXP-1476 `mmq-reference.J7xT6p` response to the same
4,096 prompt IDs and sampling configuration. The historical
request has `n_probs=0` and the candidate has `n_probs=5`.
This establishes first-token agreement for the diagnostic
request. Full generation, response probabilities and
decode performance remain open.

- Candidate build and tests under
  `/home/end0/AI/LlamaServer/franken-goose-indexed-mmq-reference.DWuAqs`.
  Executable SHA-256 `52398641656da3a1e7595a610075388d161197bef5271b14a155943963217711`.
  The comparison uses `cuda-matrix-dense-both.xqsp5lm1` in that
  build and `cuda-matrix-dense-both.hmobnect` in the preceding
  router build.
- Live tensors under
  `/home/end0/AI/LlamaServer/franken-apex-goose-indexed-reference-layers.hca3mdi0`
  and
  `/home/end0/AI/LlamaServer/franken-apex-reference-experts-layers.cib3wbi6`.

## Numerical checkpoint before response-quality tests

EXP-2026-11-20-1553 extends the existing reference activation
mode to the real expert SwiGLU. The frozen `ggml_cuda_op_swiglu`
invokes `unary_gated_op_kernel<&op_silu>`. Its SM89 arithmetic
applies the same approximate reciprocal already used in the
Goose SiLU reference mode and then multiplies by the up value.
This change reuses the existing vector dispatch and packed or
split source addressing. Select it with
`GOOSE_CUDA_ACTIVATION_REFERENCE=1`.

On the 124-ID live tensor comparison, first-layer gate, up,
SwiGLU, down and the ungated shared expert output all match
bitwise. SwiGLU matches 507,904 values and down matches
2,031,616. This removes the quantization outliers at input
indices 19 and 33. An isolated CUDA SwiGLU test on those
exact Apex tensors also matches, with zero Compute Sanitizer
errors.

| Remaining checked boundary | Max absolute error |
|---|---:|
| Layer 0 router softmax | 1.4901161e-8 |
| Layer 0 eight normalized routing weights (468 of 992 differ) | 4.4703484e-8 |
| Layer 0 weighted expert output | 7.450581e-9 |
| Layer 0 gated shared expert | 1.8626451e-8 |
| Layer 0 complete FFN | 1.8626451e-8 |
| Layer 1 input RMSNorm | 3.8146973e-6 |
| Layer 1 projected recurrent output | 7.804483e-4 |
| Layer 1 complete FFN | 8.2837045e-4 |

This ends the broad tensor-by-tensor numerical investigation.
The next acceptance work compares coherent real responses
from independent Goose and EXP-1476 on common queries,
settings and input IDs. The records above retain the focused
probes needed if a response shows a concrete regression.

- Build `/home/end0/AI/LlamaServer/franken-goose-swiglu-reference.3MLAsn`,
  executable SHA-256
  `712c98d8cd8d540c5076074f614b31f73631669c45704aca395a423d02ef3fb2`.
  `reference/` contains the CUDA replay, diagnostic observer
  sources, standalone comparison and recorded residuals.
- Live Goose tensors under
  `/home/end0/AI/LlamaServer/franken-apex-goose-swiglu-reference-layers.2qfhifxt`.
- Reference tensors under
  `/home/end0/AI/LlamaServer/franken-apex-reference-experts-layers.cib3wbi6`.
  The first 124 IDs are the common captured microbatch.

## Apex response-quality checkpoint

EXP-2026-11-20-1554 compares three real Apex requests with
EXP-1476 using the same Q4_K_M weights, input IDs, request
bytes and greedy sampling. Both use context 262,144,
batch/ubatch 4096/4096, fresh context per request and normal
EOS. The tokenizer's no-thinking chat template was rendered
once and EXP-1476 supplied the prompt IDs for both endpoints.

The first Goose run exposed the terminal EOG token in text
and probability pieces. The sampler had already stopped
correctly. The HTTP response now skips that token when
decoding text and emits empty EOG text and bytes in
probability rows. Token IDs and counts remain available.
An EOG sampled exactly at the generation limit now reports
`stop_type=eos`. All 36 HTTP regression requests passed.
The same three real requests preserved every one of 1,083
Goose token IDs across the fix. Text matches the previous
Goose responses after removing the trailing EOG marker.

| Request | EXP-1476 | Independent Goose |
|---|---|---|
| Strict JSON, 94 prompt IDs | 34 output IDs, EOS | 34 output IDs, EOS. Identical text and IDs. |
| Grounded incident report in Russian, 175 prompt IDs | 320 output IDs, limit. Correct facts and two investigative steps, with extra sections. | 219 output IDs, EOS. Correct facts, two steps and an extra title. |
| Long technical handoff, 202 prompt IDs | 766 output IDs, EOS, 526 words. | 830 output IDs, EOS, 558 words. |

All six response texts after the fix are nonempty. The raw
scan found no replacement/control characters, visible special
markers, complete prompt echo or adjacent repeated-token loops.
Both JSON answers provide the expected fields and values inside
a Markdown fence, contrary to the strict formatting instruction.
Both long answers have the requested sections in order plus an
extra title, exceed the requested 520-word ceiling and include a
contradictory detail about duplicate records. Goose states that
a retry creates a second record, then explains that the unique
index prevents it. EXP-1476 describes a second worker committing
the same event despite that index. Both answers are readable and
complete. The sample supports using this checkpoint for further
CUDA work. Keep behavioral checks on subsequent changes and
investigate only concrete new failures.

On the long request, the endpoint reported 17.008 output tok/s
for Goose and 74.582 for EXP-1476. The measured prompt rates
were 143.871 and 349.253 tok/s at 202 prompt IDs. Peak process
RSS was 22,878,660 KiB in Goose over the suite. Its post-request
GPU samples were 8,018–8,020 MiB, and process swap was zero.
These are quality diagnostics with different output sequences.
The full 32K/512 correctness, memory and speed gates remain open.

The working checkpoint uses `GOOSE_MMQ=1`, `GOOSE_GDN_MMA=1`,
`GOOSE_FLASH_MMA=1`, `GOOSE_DENSE_MMA=1`,
`GOOSE_RMS_REFERENCE=1`, `GOOSE_MMQ_Q4_REFERENCE=1`,
`GOOSE_MMQ_Q6_REFERENCE=1`, `GOOSE_SSM_REFERENCE=1`,
`GOOSE_CUDA_ACTIVATION_REFERENCE=1`, and
`GOOSE_DENSE_MMA_REFERENCE=1`. The independent executable
links against libc and libm. The frozen EXP-1476 only runs
as the isolated comparison.

- Build `/home/end0/AI/LlamaServer/franken-goose-quality-eos.yIp48K`,
  executable SHA-256
  `47b441c51e41966f333c22169e1a292e33a8cd82e1df4178b48fdaf815c2ece9`.
- The reproducible requests, scripts, answers, hashes and
  `summary.jsonl` are under `quality/paired.gT0Q00` in that
  build. The pre-fix run is under
  `/home/end0/AI/LlamaServer/franken-goose-swiglu-reference.3MLAsn/quality/paired.061R1P`.

## Independent MMVQ decode checkpoint

EXP-2026-11-20-1555 connects the existing Goose Q4_K and
Q6_K MMVQ dot products and the non-MMQ Q8_1 packer.
`GOOSE_MMVQ=1` selected the integer `dp4a` path for
qualified one-column and cached eight-route projections during
the initial comparison. The accepted selector defaults to this
path on devices with the SM80+ image. Use `GOOSE_MMVQ=0` for
the prior scalar path.
MMQ stays responsible for batch matrix multiplication.
Explicit bit-pattern casts correct the Q6_K signed mask and
packed halfword inputs. The driver-loadable PTX binds the
packer multiply-high operation to the CUDA instruction.

Five isolated model-weight projections matched all 36,864
reference values, using two dense projections and three
indexed projections. The live cache path reported three
cached projections in the expert fixture. Compute Sanitizer
found zero errors. In the Apex request the candidate
launched 55,169 each of `goose_mmvq_matvec` and
`goose_mmvq_quantize`. The remaining scalar matrix kernel
ran 10,280 times, down from 65,449.

| Sequential profiled Apex 4096/129 | Scalar decode | Goose MMVQ |
|---|---:|---:|
| Prefill, tokens/s | 278.651 | 285.193 |
| Decode, steps/s (128 decode steps) | 16.198 | 18.625 |
| Scalar quantized matrix GPU time, ms | 2,521.801 | 204.342 |
| MMVQ matvec and pack GPU time, ms | 0 | 818.839 |
| RSS high-water, KiB | 23,885,904 | 23,885,464 |
| Sampled process VRAM, MiB | 8,582 | 8,582 |
| Sampled whole-card VRAM, MiB | 11,372 | 11,372 |
| Process swap, KiB | 0 | 0 |
| System swap-out increase, pages | 3,302 | 0 |

This pair uses physical 4096/4096 and the response-quality
checkpoint flags. The timed diagnostic forces a fixed output
length and the token sequences differ. The pair has different
system swap-out. Full throughput acceptance remains open.

All three previous normal quality prompts remain readable
and stop on EOS with the option enabled. The JSON still has
correct values inside a Markdown fence. The Russian report
preserves the facts and two investigation steps. The 560-token
long answer contains a consistent commit/crash/retry/ack example
and an operational check. It is 369 words, below its requested
430–520 words. The scan found no leaked special markers,
replacement/control characters, complete prompt echo or repeated
token loops.

- Build `/home/end0/AI/LlamaServer/franken-goose-mmvq.irQR6J`,
  executable SHA-256
  `346d7c89e80b65b01217ab986d2e3f7b58010e4da7c12f01861608265d59b37c`.
  Traces and responses under `cuda-matrix-dense-both.tyew48t0`
  and `cuda-matrix-dense-both.oi4nsn5a`. Common request SHA-256
  `3edd77e6dcdea9ab06f9205b86818223648d8293e85d7257151a0ca78f64ba73`.
  Normal quality artifacts under `quality/paired.osGxwk`.

## Expert mmap registration and SLRU

Historical EXP-906 (`3989367d`) restored host registration for
resident, read-only mmap ranges. Its own control and candidate
measured 2,019.28 and 2,625.21 prompt tok/s, and 42.59 and
55.52 decode tok/s. That measurement describes its historical
pair. The independent Goose comparison uses its own matched
checkpoint.

The original standalone SLRU already shares a VRAM bank across
layers of each compatible weight geometry. On this RTX 4070 the
4,992 MiB cap yields 5,234,122,752 allocated bytes (4,991.648 MiB)
after whole-slot rounding, corresponding to 1,283 Q6_K-down
group slots and 1,479 Q4_K-down group slots. One miss fills
all three planes of an expert. Hits do not upload weights.
The eviction invalidates the victim layer's slot map before
reassigning the slot. The cache fixture covers three-plane
fills, reuse, cross-layer invalidation, separate groups and
recovery from an interrupted upload. A CUDA fixture matches the
streamed result after its warm cache's source weights are zeroed.

Goose builds the page-aligned mmap spans from the validated
expert tensor layout, merges overlaps, checks host RAM and
residency and owns the registration order and rollback.
The C boundary calls `cuMemHostRegister_v2` and
`cuMemHostUnregister` with portable, read-only registration.
The rollback probe successfully reacquires a page after a
failure on a later span.

The full-registration diagnostic reused the validated
MMVQ checkpoint and the same physical 4096/4096 request.
The registered arm acquired 120 ranges totaling
19,504,005,120 bytes. The CUDA trace attributes 47,664 H2D
transfers and 30,516,117,504 bytes to pinned sources. Both
arms had 129,210 hits, 15,888 fills and 13,126 evictions.
SLRU uploaded 30,516,117,504 expert bytes per arm.

| Profiled Apex 4096/129 diagnostic | Pageable weights | Full registration |
|---|---:|---:|
| Prefill, tokens/s | 288.155 | 295.732 |
| Decode, steps/s (128 steps) | 18.664 | 21.702 |
| Host time in H2D calls, ms | 10,775.172 | 9,104.770 |
| RSS high-water, KiB | 23,886,128 | 23,817,068 |
| Sampled peak process VRAM, MiB | 8,582 | 8,582 |
| Minimum sampled system MemAvailable, KiB | 43,922,904 | 27,993,736 |
| Peak process swap, KiB | 0 | 17,056 |
| System swap-out growth, pages | 0 | 822,505 |

The token IDs, probability rows and response content all
match. The registered arm started with 36 fewer resident
model pages and recovered to 5,301,139 of 5,301,139 by the
end of the request. Full registration is rejected for this
host configuration because of swap and incomplete startup
residency. The bounded candidate admits whole ranges under
an explicit host pin budget and rechecks model residency
after CUDA placement.

- Full-registration raw pair under
  `/home/end0/AI/LlamaServer/franken-goose-expert-pin.jZVzQg`,
  directories `cuda-matrix-dense-both.4cf2o6ft` and
  `cuda-matrix-dense-both.kp1am68l`.

The accepted bound is 4,096 MiB of host registration, with
whole spans selected across the expert mmap. It leaves the
VRAM budget unchanged. Registration respects the host
headroom checks and the runtime revalidates model residency
after CUDA placement. `GGML_CUDA_REGISTER_HOST=0` disables
registration. `GOOSE_EXPERT_PIN_MIB` sets the host budget.
`GGML_CUDA_NO_PINNED` remains an opt-out when the register
switch is unset.

| Profiled 4096/129 A–B–B–A | Pageable A | Bounded registration B |
|---|---:|---:|
| Prefill pair, tokens/s | 284.588, 286.935 | 291.492, 292.118 |
| Prefill median, tokens/s | 285.762 | 291.805 |
| Decode pair, steps/s | 18.447, 18.600 | 19.170, 19.237 |
| Decode median, steps/s | 18.524 | 19.203 |
| Highest RSS high-water, KiB | 23,885,756 | 23,885,640 |
| Sampled peak process VRAM, MiB | 8,582 | 8,582 |
| Lowest sampled system MemAvailable, KiB | 49,166,516 | 44,850,816 |
| Process swap, KiB | 0 | 0 |
| System swap-out growth, pages | 0 | 0 |

All four runs start and finish with all 5,301,139 model pages
resident. Both B runs register 28 ranges covering 4,227,973,120
bytes. The CUDA traces confirm 10,951 H2D transfers and
6,459,162,624 bytes from pinned sources per B run.
The median host time in H2D calls fell from 10,854.646 to
10,366.942 ms. All four responses match in content, token IDs
and reported probabilities. Each shows 129,210 cache hits,
15,888 fills and 13,126 evictions.

The final build selects this bounded registration without
the enable flag. The independent runtime completes the same
three normal quality prompts as the MMVQ checkpoint. Their
answers and IDs match that checkpoint; all end on EOS with
no leaked markers or loops. The 202-prompt-ID, 560-output-ID
quality request reports 161.497 prefill and 22.130 output
tok/s. Its shape differs from the 4096/129 performance
diagnostic. Full 32K/512 acceptance has its own gates.

- A–B–B–A traces, requests, replies and cache telemetry under
  `/home/end0/AI/LlamaServer/franken-goose-expert-pin-bounded.vzu6Js`.
  Raw directories in order
  `cuda-matrix-dense-both.uvj3ebm2`, `cuda-matrix-dense-both.fmqw8yif`,
  `cuda-matrix-dense-both.03nazd2w`, and `cuda-matrix-dense-both.bicuz4ir`.
  Executable SHA-256 `ddfd8d2dbede001d76ebf81263cce910b6891334c8c1b3395d1ddfffeed86010`.
- Final executable and quality results under
  `/home/end0/AI/LlamaServer/franken-goose-expert-pin-default.fGBi00`,
  `quality/paired.bazMBk`.
  Executable SHA-256 `ce7ff3c3f225f235b863a958100a1aeacd8f76b9bf1e59f526c70d6344a1eeac`.

### Host pin budget at full Apex 32K/512

EXP-1567 runs an unprofiled A–B–B–A on the EXP-1566 binary,
with batch/ubatch 4096/4096. Only the host pin budget differs.
The GPU SLRU bank is 5,234,122,752 bytes in all four runs,
with 560,324 hits, 77,707 fills and 74,945 evictions per run.

| Metric | A, 4,096 MiB | B, 4,800 MiB |
|---|---:|---:|
| Prefill pair, tokens/s | 806.187, 800.598 | 810.558, 805.991 |
| Prefill median, tokens/s | 803.393 | 808.274 |
| Decode pair, steps/s | 21.294, 21.189 | 21.513, 21.400 |
| Decode median, steps/s | 21.241 | 21.457 |
| CUDA-registered host bytes | 4,227,973,120 | 4,982,968,320 |
| Registered host ranges | 28 | 33 |
| Highest process RSS high-water, KiB | 22,935,876 | 22,935,956 |
| Peak process VRAM, MiB | 8,512 | 8,512 |
| Median of per-run minimum system MemAvailable, KiB | 42,709,860 | 41,701,842 |
| Process swap, KiB | 0, 0 | 0, 0 |
| System swap-out growth, pages | 0, 17,257 | 0, 0 |

All runs retain 5,301,139 of 5,301,139 model pages before
and after inference. Content, all 512 token IDs, probability
rows and stop reason match. The existing opening prompt echo
persists. The 4,800 MiB setting pins another 720 MiB for
median gains of 4.882 prefill and 0.216 decode steps/s.
Background activity differs across runs, including system
swap-out during the last control. Keep the 4,096 MiB
default by decision. The follow-up pair was cancelled and
excluded.

Evidence and the runner are stored under
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/host-pin-exp1567`.
Run directories in order: `apex-host-pin-4096.dr70wskm`,
`apex-host-pin-4800.8dhlz7s6`, `apex-host-pin-4800.r6294_xt`
and `apex-host-pin-4096.zwjzczw_`.
Executable SHA-256
`11e32a5e0607ee5567e82fa80247f39716538451ce95b046eec2ffc093c91607`.

## Vector Flash Attention for Q4 decode

EXP-2026-11-20-1557 connects the existing D=256, Q4_0
vector body and combine kernel to the independent KV graph.
The decode path bounds a partial tile before any K or V load
and gives masked tail positions zero softmax mass. Single
and split-partition cases use the existing vector math. The
prefill MMA path remains selected for qualified prefill.
`GOOSE_FLASH_VEC=0` restores scalar Q4 Attention.

Five CPU-versus-CUDA probes at 1, 127, 128, 129 and 4,097
keys give a maximum absolute error of 0.000269771. Compute
Sanitizer reports zero errors. In each treatment run Nsight
shows 1,280 `goose_flash_vec_q4` and 1,280
`goose_flash_vec_combine` calls replacing the scalar decode
tile and merge routines.

| Profiled Apex 4096/129 A–B–B–A | Scalar A | Vector B |
|---|---:|---:|
| Prefill samples, tokens/s | 293.450, 295.041 | 291.384, 294.030 |
| Prefill median, tokens/s | 294.246 | 292.707 |
| Decode samples, steps/s | 19.512, 19.333 | 20.879, 20.816 |
| Decode median, steps/s | 19.422 | 20.847 |
| Decode Attention GPU time, median ms | 962.895 | 342.473 |
| Highest RSS high-water, KiB | 23,886,104 | 23,886,480 |
| Sampled peak process VRAM, MiB | 8,582 | 8,582 |
| Process swap, KiB | 0 | 0 |
| System swap-out growth, pages | 0 | 0 |

Both treatments use the bounded host registration checkpoint.
The vector mode changes token IDs, as expected from its
quantized query math. All three normal quality requests
remain coherent and end on EOS, with no full prompt echo,
invalid characters, leaked markers or repeated-token loop.
The Russian answer retains the recorded incident facts
and investigation steps. The 523-word long response includes
an extra title and a contradictory sentence about a duplicate
row despite the unique index. EXP-1476 has a similar
contradiction. On a fixed direct follow-up both runtimes
correctly answer ONE ROW and explain why the database
rejects the second insert. Preserve the exact texts and judge
subsequent optimizations on real response quality.

The one-request default-selector check reports 291.449
prefill and 20.583 decode steps/s and records the same
1,280 vector and combine launches. The 35 CUDA entries in
this compiled SM80+ image include 28 observed in this
profile. The inactive entries are the compensated dense
variant, the two MMQ Stream-K entries, the two scalar
attention entries and the separate route gather and sort
entries. They are alternatives to this request's selected
routes. This image inventory does not cover every unlinked
Goose module or all Apex profiles.

- A–B–B–A evidence under
  `/home/end0/AI/LlamaServer/franken-goose-flash-vec.qjrwRz`.
  In order the runs are `cuda-matrix-dense-both.9i17vqbf`,
  `cuda-matrix-dense-both.6j1vk9k1`,
  `cuda-matrix-dense-both._sa0v5bx` and
  `cuda-matrix-dense-both.e50ip44u`.
  Normal responses under `quality/paired.piD7ig` and
  the factual follow-up under `quality/fact.w4UKNQ`.
- Default build
  `/home/end0/AI/LlamaServer/franken-goose-flash-vec-default.fZdoEo`,
  executable SHA-256
  `9dd7df1347b5fed7f3695f63204c9f7f8bad993ca6318e8c82f57234d4d11665`.
  Default trace under `cuda-matrix-dense-both.m4h0eixz`.

## Full Apex diagnostic after MMVQ, SLRU and vector Attention

The independent build from EXP-1557 evaluated all 32,768
prompt IDs and generated 512 output IDs at 4096/4096,
context 262,144 and zero reused prompt tokens. This is one
profiled diagnostic. It uses the canonical recorded request.

| Metric | Current independent Goose |
|---|---:|
| Prefill time | 112.641 s |
| Prefill | 290.905 tok/s |
| Decode time | 30.922 s |
| Decode, 511 steps | 16.526 steps/s |
| Process RSS high-water | 23,945,336 KiB |
| Sampled peak process VRAM | 8,622 MiB |
| Sampled peak whole-card VRAM | 10,824 MiB |
| Process swap and system swap-out growth | 0 |

All 5,301,139 pages remain resident before and after the
request. The 72 memory samples record zero process swap.
The 5,234,122,752-byte SLRU bank reports 560,324 hits,
77,707 fills and 74,945 evictions, with 149,376,491,520
bytes uploaded. Bounded host registration covers
4,227,973,120 bytes in 28 ranges.

The response gives a relevant capacity analysis and advice.
It starts by repeating input text and the request before
the analysis. No replacement or unexpected control
characters occur. The full behavior gate stays open.

Using the first scalar GDN launch as the approximate
prefill/decode boundary, prefill records
469,156,832,336 bytes H2D and 301,508,012,032 D2H.
Host time inside H2D and D2H calls is 48.577 and
37.541 s. These API durations can include waits.
The two reference-order MMQ kernels account for
20.186 s of GPU activity. Removing all matrix arithmetic
alone would leave the large transfer and dispatch cost.

Decode records 5,110 vector Attention calls taking
9.041 s, or 17.692 ms per decode step. The 4096/129
diagnostic used about 2.67 ms per step in vector Attention
and merge. This is a major contributor to the lower long
decode rate. Both engines have 255-register vector entries.
The full Goose trace uses 64 partitions per head, while the
traced EXP-1476 specialization uses 11. Measure partition
geometry and keep the previously qualified short path.
Host time inside H2D calls is 20.037 s in the decode-dominated
phase, including waits. Device and host times overlap and
should not be added.

- Raw trace
  `/home/end0/AI/LlamaServer/franken-goose-flash-vec-default.fZdoEo/cuda-full-dense-both.34z9__mg`.
- Request SHA-256
  `484a12de00ec057e4b78d480ebb185153bb10af607eb23192a72995ba3026d0f`.
- Response SHA-256
  `05a704b7f35c3576794f692252c3fbecfdd5a50a2336ece25464230622971962`.
- Nsight SHA-256
  `42c42d12ddf3eccce5af92b006d9b2e0dd445021c8f620d0e31804821d889ae9`.

## Adaptive CUDA activation arena

EXP-2026-11-20-1558 tests 384 MiB in the existing device
activation arena. The Goose lifetime, alias and eviction
planner continues to select live ranges. The default runtime
starts with a 64 MiB arena and tries to widen only when a
planned large output needs it. It publishes the existing
live records before replacing the device allocation. Small
graphs retain the original behavior. An explicit
`GOOSE_CUDA_ACTIVATION_MIB=64` selects the previous budget.

The original eviction fixture passes. A second CUDA fixture
keeps two 128 MiB results and one 64 MiB result live in VRAM
and reads their views correctly while the corresponding host
buffers stay unmaterialized. Compute Sanitizer reports no
errors.

| Profiled Apex 4096/129 A–B–B–A | 64 MiB | 384 MiB |
|---|---:|---:|
| Prefill samples, tokens/s | 286.748, 286.625 | 627.785, 635.763 |
| Prefill median, tokens/s | 286.686 | 631.774 |
| Decode median, steps/s | 20.672 | 20.601 |
| Highest RSS high-water, KiB | 23,886,648 | 23,589,992 |
| Sampled peak process VRAM, MiB | 8,582 | 8,500 |
| Process swap and system swap-out growth | 0 | 0 |

All four replies match in text, IDs and probability rows.
In the first pair, prefill H2D is 61,041,128,656 vs
18,990,675,152 bytes and D2H is 37,689,370,624 vs
4,703,856,640 bytes. The saved Nsight records identify
the exact active kernels and transfers.

The adaptive-default build completes the identical full
Apex 32K/512 request at 4096/4096:

| Full profiled diagnostic | Previous 64 MiB build | Adaptive build |
|---|---:|---:|
| Prefill time, s | 112.641 | 50.648 |
| Prefill, tokens/s | 290.905 | 646.980 |
| Decode time, s | 30.922 | 31.240 |
| Decode, steps/s (511 steps) | 16.526 | 16.357 |
| Prefill H2D, bytes | 469,156,832,336 | 132,887,422,032 |
| Prefill D2H, bytes | 301,508,012,032 | 37,657,454,592 |
| Process RSS high-water, KiB | 23,945,336 | 23,646,068 |
| Sampled peak process VRAM, MiB | 8,622 | 8,540 |
| Process swap and system swap-out growth | 0 | 0 |

Both full responses match exactly, including their quality
limitation noted above. All 5,301,139 model pages stay resident.
The two full traces are development diagnostics. The same-build
A–B–B–A establishes the short gain. The remaining full
prefill D2H is dominated by 320 copies of 64 MiB and 960
copies of 16 MiB after vector operations. The next prefill
work should match their producer/consumer chains and avoid
those host publications when they are unnecessary.

The [isolated EXP-1476 comparison](../../experiments/EXP-2026-11-20-1559-hybrid-activation-arena/README.md)
changed zero CUDA compute allocations. Its graph allocator
already requested more than 384 MiB for each prefill block.

- Same-build A–B–B–A under
  `/home/end0/AI/LlamaServer/franken-goose-wide-arena.yr2YF0`.
  In run order `cuda-matrix-dense-both.5eo3mlp2`,
  `cuda-matrix-dense-both.4t3ah_j_`,
  `cuda-matrix-dense-both.i0uwgyvx`, and
  `cuda-matrix-dense-both.h0gtjg65`.
- Adaptive executable and full trace under
  `/home/end0/AI/LlamaServer/franken-goose-wide-arena-default.ollura`,
  `cuda-full-dense-both.poal0fdd`.
  Executable SHA-256 `209f7731abde844d2479529b5eb3c893de0c0978e55c6bcc56cec3ae9393a670`.
  Response SHA-256 `6dfee12a7e73fb75361a6c749fa3c37824fc62e6805f7ab22a3926415ae3c591`.
  Nsight SHA-256 `987099d3c1511211830de20858093f6c4042e61f8a80cf647be84de8ff32d6e2`.

## Arena size comparison at Apex 32K/512

EXP-2026-11-20-1560 compares 384 and 512 MiB in the same
independent executable. The request and all runtime settings
match, with only `GOOSE_CUDA_ACTIVATION_MIB` changed. The
profile is Apex Q4_K_M on RTX 4070, context 262,144 and
batch/ubatch 4096/4096. The request evaluates 32,768 input
IDs and returns 512 output IDs without prompt reuse.

| Profiled A/B | 384 MiB | 512 MiB |
|---|---:|---:|
| Prefill, tok/s | 646.076 | 652.184 |
| Decode, steps/s (511 steps) | 16.328 | 16.301 |
| Prefill and decode time, s | 82.015 | 81.592 |
| H2D, bytes | 191,097,039,232 | 191,097,039,232 |
| D2H, bytes | 38,131,127,040 | 38,131,127,040 |
| Prefill H2D, bytes | 132,753,204,304 | 132,753,204,304 |
| Prefill D2H, bytes | 37,623,900,160 | 37,623,900,160 |
| Process RSS high-water, KiB | 23,645,904 | 23,645,976 |
| Sampled peak process VRAM, MiB | 8,540 | 8,668 |
| Sampled peak whole-card VRAM, MiB | 10,748 | 10,880 |
| Process swap, KiB | 0 | 0 |
| System swap-out growth, pages | 0 | 0 |
| SLRU hits | 560,324 | 560,324 |
| SLRU fills | 77,707 | 77,707 |
| SLRU evictions | 74,945 | 74,945 |

Both runs keep all 5,301,139 pages resident before and after
generation. Each trace contains 42 process memory samples.
The expert bank stays at 5,234,122,752 bytes, with
149,376,491,520 bytes of expert uploads. The response text,
all IDs and all reported probability rows match each other.
The shared result preserves the earlier full diagnostic's
opening repetition and substantive capacity analysis.

Keep the 384 MiB configuration. The 512 MiB sample saves
0.424 s in the combined time while using 128 MiB more
process VRAM, with equal transfer counts and volume.
This single pair establishes no mechanism-level gain for
the larger allocation under the memory requirement.

- Raw pair and reproduction script under
  `/home/end0/AI/LlamaServer/franken-goose-wide-arena-default.ollura`.
  A is `cuda-full-dense-both.e5ygvndq`, B is
  `cuda-full-dense-both.uz39urpe`.
- Common executable SHA-256
  `209f7731abde844d2479529b5eb3c893de0c0978e55c6bcc56cec3ae9393a670`.
- Common content SHA-256
  `3a824300f79ea0f8bfa94c646dd23f0903626b8582ea7c3d8c2b058176335079`.
- A Nsight SHA-256
  `ca28de5bc050732b28c2603a89b235f28022b498a9cebadb5229c432e77faffa`.
- B Nsight SHA-256
  `63ffcf3a356e887b40c6c83b97349e5757f03ecabb13c38d1ed06f478547874f`.

## External computation stack and measured gaps

The fresh hybrid A arm in EXP-1559 and the independent
384 MiB A arm in EXP-1560 use the same canonical Apex
32K/512 request on RTX 4070. Both traces use 4096/4096
and context 262,144. Their profiled prefill rates are
2,618.408 and 646.076 tok/s. Their decode rates are 56.733
and 16.328 tok/s. The phase split uses the first scalar
GDN after the final prefill GDN kernel and is approximate.
Kernel activity and host API durations can overlap.

The hybrid's process map loads `libllama`,
`libllama-server-impl`, `libllama-common`, `libmtmd`,
`libggml`, `libggml-base`, `libggml-cpu`,
`libggml-cuda`, the tiled MoE library, CUDA Runtime,
cuBLAS/cuBLASLt, the system CUDA driver and libstdc++.
The independent executable's ELF dependencies are libc
and libm. It loads the system CUDA Driver API dynamically.
The existing C tensor descriptors, quant data types and
primitive substrate are statically included in its
standalone archive. The graph and kernels discussed here
use the independent runtime.

| Measured area | Hybrid EXP-1476 | Independent Goose | Restoration priority |
|---|---:|---:|---|
| Prefill quantized matrix GPU time | 3.137 s in `mul_mat_q` | 21.223 s across the three MMQ entries | Highest compute priority. Reuse the Goose packed loaders and MMA coefficients with cooperative tile reuse and reference-sized MoE grouping. |
| Prefill D2H | 1.442 GB | 37.624 GB | Immediate. The remaining large publications are 320 × 64 MiB and 960 × 16 MiB. Reuse the device vector path with valid live arena ranges. |
| Prefill H2D | 84.097 GB | 132.753 GB | Match producer/consumer lifetimes, scatter and selected expert uploads. Measure before attributing the excess to one subsystem. |
| Decode vector Attention GPU time | 1.216 s | 9.055 s | Highest decode kernel priority. Test the existing one-query vector body with the measured 11-part geometry against 64. Check evicted ranges and register spills independently. |
| Decode quantized matrix GPU time | 2.492 s MMVQ | 3.002 s MMVQ plus 0.800 s scalar | Verify the remaining routed and fused MMVQ shapes. Keep the achieved Q4_K/Q6_K path. |
| Prefill Dense GEMM | CUTLASS `s1688gemm` through cuBLAS | Independent TF32 router, 0.463 s | The traced Apex router shapes are already restored. Check other dense shapes after the larger losses. |
| Recurrent prefill GPU time | 0.468 s GDN MMA | 1.352 s GDN MMA | Existing mechanism executes. Tune the measured residual after MMQ and transfers. |

The hybrid runs 20,914 CUDA Graph launches. Its trace
contains 895,931 graph-node launches and about 29,471
direct launch calls. The independent trace contains zero
graph-node launches and 1,088,448 driver launch calls,
spending 2.933 s in those direct driver calls across the
session. It also records 291,671 context synchronizations,
with 27.155 s in the API including device waits.
The hybrid records 62,355 stream synchronizations, with
17.104 s including waits. These are attribution bounds,
not a measured speedup from Graph capture. Stable
allocation addresses, stream boundaries, and per-token
state keys are required before restoring capture. Goose
`goose-graph-lifecycle.goose` concerns the model graph
container. The standalone runtime now captures the
[vector Attention pair](#cuda-graph-replay-for-vector-attention)
through the Driver API.

The hybrid VMM pool uses two address reservations, 105
create calls and 105 map calls. Recorded create/map API
time is about 2.50 ms across the session. The independent
path uses 36 Driver API allocations and 24 frees, totaling
about 10.23 ms in those calls. The latest candidate connects
the [Goose planner to scratch VMM](#cuda-vmm-scratch-restoration).
The measured allocation calls do not explain the throughput gap.

The hybrid GGML scheduler reuses its CUDA compute
allocations above 1 GiB and performs 10.454 GB D2D in
prefill. The independent lifetime planner and 384 MiB
arena removed most of the earlier D2H. Its remaining
16 MiB copies align with the four explicit vector windows
at each of 240 recurrent-layer calls. The 64 MiB group
requires a separate lifetime check. Work on the 16 MiB
windows first, retaining the existing CPU-visible fallback
for graph shapes that require it.

Ready compute material includes
`goose-mmq-load-q4k/device.goose`,
`goose-mmq-load-q6k/device.goose`,
`goose-mmq-mma-coeff/device.goose`, and the Flash
`mma-entry-device.goose` / `mask-fixups-device.goose`
modules. The [main map](#mechanisms) tracks live
selection and evidence per mechanism.

## Resident wide vector multiplication

EXP-2026-11-20-1561 removes the forced host publication of
the recurrent gated multiplication when the GPU already
owns both operands. The original path uses four 16 MiB
vector windows for a full 64 MiB result, publishes each
to RAM and reloads the combined tensor for the next
matrix operation. The accepted path calls the existing
wide Goose vector kernel once when both operands are
resident, the graph owns a fresh output, and the existing
arena allocator can return a slot without eviction.
`GOOSE_CUDA_WIDE_VECTOR=0` enables the previous route.

A targeted CUDA fixture computes and consumes a 64 MiB
result while both 64 MiB operands remain live and the host
result stays unmaterialized. Compute Sanitizer reports
no errors.

| Profiled Apex 4096/129 A–B–B–A | Windows | Resident multiplication |
|---|---:|---:|
| Prefill samples, tok/s | 643.403, 643.171 | 694.073, 695.933 |
| Prefill median, tok/s | 643.287 | 695.003 |
| Decode median, steps/s | 20.889 | 20.851 |
| Prefill H2D, bytes | 18,990,675,152 | 16,977,390,512 |
| Prefill D2H, bytes | 4,703,856,640 | 2,690,590,720 |
| 16 MiB D2H copies in prefill | 120 | 0 |
| Highest RSS high-water, KiB | 23,590,632 | 23,524,240 |
| Sampled peak process VRAM, MiB | 8,500 | 8,484 |
| Process swap and system swap-out growth | 0 | 0 |

All four responses match in content, IDs and probabilities.
The default selector removes the same 120 copies on the
short request and records 689.356 tok/s prefill.

The independent default build also completes the full
32K/512 request at 4096/4096 with the same text, 512 IDs
and probabilities as the 384 MiB control in EXP-1560.
The full run takes 46.618 s in prefill and 30.876 s in
decode, or 702.909 prefill tok/s and 16.550 decode
steps/s. Exact model residency and SLRU counters persist.
RSS high-water is 23,580,412 KiB, sampled peak process
VRAM is 8,524 MiB and process swap and system swap-out
growth are zero. The opening input repetition recorded
in EXP-1560 remains visible.

Prefill H2D is 116,646,927,184 bytes and D2H is
21,517,772,800 bytes. The whole 960-copy 16 MiB group
is gone. The largest residual prefill publication is 320
copies of 64 MiB, totaling 21,474,836,480 bytes.
[EXP-1563](../../experiments/EXP-2026-11-20-1563-mmq-input-residency/README.md)
identifies an arena eviction between SwiGLU and routed MMQ
and measures its removal in an isolated candidate.

- Short A–B–B–A under
  `/home/end0/AI/LlamaServer/franken-goose-resident-vector.UsIPSk`,
  in order `cuda-matrix-dense-both.52573zah`,
  `cuda-matrix-dense-both.j9ukwb1l`,
  `cuda-matrix-dense-both.waa3_4bz`,
  and `cuda-matrix-dense-both.wblyfbf1`.
- Default executable and full run under
  `/home/end0/AI/LlamaServer/franken-goose-resident-vector-default.DpA97A`,
  `cuda-full-dense-both.9f8p85uk`.
  Executable SHA-256 `a2c4facd806f73910e3c3c8db4371522eaecc85544276014c322638ccb9dc79f`.
  Nsight SHA-256 `76ef051ff2f3881e04cf254cd7cccf8a10cd014aef14ca2a00e1bdbfbfc2298a`.

## Device staging for the last-use matrix input

EXP-2026-11-20-1564 follows EXP-1563. Layout instrumentation
finds 388.125 MiB live including the down projection result
at all 40 expert SwiGLU eviction sites in one 4096-token
block. This exceeds the 384 MiB arena. EXP-1476 schedules
placement from graph lifetimes and reuses parent storage
when the operator, views and layouts allow it (`ggml-alloc.c`,
`ggml_gallocr_allocate_node`).

Goose checks the complete input span against the planned
last-consumer index and the selected eviction window. On an
exact hit it relocates the stale input to the existing matrix
staging allocation with `cuMemcpyDtoD_v2` and releases its
arena record. The original placement covers unmatched views,
unpinned lifetimes and device staging out-of-memory. The
selector is active by default with
`GOOSE_CUDA_STAGE_MATRIX_INPUT=0` as rollback. Eight direct
lifetime/overlap cases pass. The 4096/1 comparison changes
40 copies in each PCIe direction to 40 D2D copies, preserves
content, IDs and probabilities, and leaves 8,484 MiB peak
process VRAM. Prefill changes from 694.653 to 752.958 tok/s.

The full profiled A/B uses the same binary and the EXP-1563
workload and settings. Only `GOOSE_CUDA_STAGE_MATRIX_INPUT=0`
versus `=1` differs.

| Metric | A | B |
|---|---:|---:|
| Prefill, tok/s | 696.703 | 766.649 |
| Prefill, s | 47.033 | 42.742 |
| Decode, steps/s (511 steps) | 16.526 | 16.468 |
| Decode, s | 30.921 | 31.029 |
| H2D, whole trace bytes | 176,385,876,304 | 154,911,039,824 |
| D2H, whole trace bytes | 22,025,992,960 | 551,156,480 |
| 64 MiB host round trips | 320 | 0 |
| 64 MiB device copies | 0 | 320 |
| 64 MiB copy GPU duration sum, s | 4.382 | 0.086 |
| Process RSS high-water, KiB | 23,580,636 | 23,514,964 |
| Sampled peak process VRAM, MiB | 8,524 | 8,524 |
| Sampled peak whole-card VRAM, MiB | 10,720 | 10,725 |
| Process swap and system swap-out growth | 0 | 0 |

Both responses have the same content, all 512 IDs, probability
rows and stop reason. The previously recorded opening echo
persists. Both retain 5,301,139 resident model pages and the
same SLRU hit, fill and eviction counts. This pair qualifies
the transfer mechanism at 4096/4096. Decode varies by 0.058
steps/s across the single profiled pair. Q4_K and Q6_K
reference-shaped MMQ still consume 15.788 and 4.480 s on
B, and long vector Attention consumes 9.057 s.

The full traces are under
`/home/end0/AI/LlamaServer/franken-goose-matrix-d2d.iiJE4H` in
`cuda-full-dense-both.9q_cnmjo` and `cuda-full-dense-both.7d_2a_yj`.
Executable SHA-256
`5a25efca2b6b035b11c3d45c69330bf570ba56a48023271172ee008eb843c642`.
Nsight SHA-256 values
`da867fbb9ec87db7d595f7b86763bac87e739dc1e5c4339af4dd8095d5525b44`
and `4e47cdbc580e294a699048e230b11618c3a85623663c76ecf04ae7d627cd9293`.
Shared response content SHA-256
`3a824300f79ea0f8bfa94c646dd23f0903626b8582ea7c3d8c2b058176335079`.

## Bounded vector Attention loops

EXP-2026-11-20-1565 retains the existing Q4_0
single-query score, quantization, softmax and V accumulation
helpers. It iterates over 32 keys per warp in the original
order instead of explicitly expanding all 32 copies of the
score and value control flow. Every tail read retains its
key bound. The standalone partition geometry is 64 at 32K.

On SM89 SASS, the vector kernel goes from 10,616 to 1,612
static instructions, 1,021 to 158 `ISETP`, 623 to 120
`BRA`, and 41 `LDL` plus 40 `STL` to zero local-memory
instructions. Offline `ptxas` reports 78 registers and zero
stack or spill bytes. Nsight records 255 to 80 registers per
thread at runtime. The frozen EXP-1476 vector entry has
65 `ISETP`, 19 `BRA` and no `LDL` or `STL`. Static
counts describe the compiled bodies and do not count dynamic
loop executions.

CUDA memcheck reports zero errors for 1, 127, 128, 129,
4,097, 32,768 and 32,769 keys. The maximum absolute error
against the CPU graph on the two longest fixtures is
4.617e-5. The full A/B uses the EXP-1564 D2D checkpoint,
the same RTX 4070, request and 4096/4096 configuration
on both sides, with one sequence and no prompt cache hits.

| Metric | A | B |
|---|---:|---:|
| Prefill, tok/s | 764.830 | 766.833 |
| Prefill, s | 42.844 | 42.732 |
| Decode, steps/s (511 steps) | 16.556 | 20.329 |
| Decode, s | 30.866 | 25.136 |
| Vector Q4 Attention GPU time, s | 9.052 | 3.509 |
| Vector Q4 Attention launches | 5,110 | 5,110 |
| Vector reconciliation GPU time, s | 0.051 | 0.051 |
| H2D, whole trace bytes | 154,911,039,824 | 154,911,039,824 |
| D2H, whole trace bytes | 551,156,480 | 551,156,480 |
| D2D, whole trace bytes | 21,474,836,480 | 21,474,836,480 |
| Process RSS high-water, KiB | 23,514,832 | 23,514,076 |
| Sampled peak process VRAM, MiB | 8,524 | 8,524 |
| Sampled peak whole-card VRAM, MiB | 10,718 | 10,716 |
| Process swap and system swap-out growth | 0 | 0 |

The measured decode interval decreases by 5.730 s and the
vector Q4 kernel time decreases by 5.543 s. All 512 output
IDs, content, probability rows, stop reason, SLRU counters
and 5,301,139-page model residency match. The existing
request echo remains. This pair qualifies the kernel change.
End-to-end EXP-1476 performance and full quality gates
remain open. Long vector Attention still costs 3.509 s
compared with 1.216 s in the earlier frozen hybrid trace.

The A trace is under
`/home/end0/AI/LlamaServer/franken-goose-matrix-d2d.iiJE4H/cuda-full-dense-both.xjqjf6c8`.
The B trace is under
`/home/end0/AI/LlamaServer/franken-goose-flash-loop.IoydtL/cuda-full-dense-both.8rbn_uvp`.
B executable SHA-256
`aaf817e26b5d9b0bf2aef522e986d9314bd52158b183dfdf4592ac0ad885ee56`.
A and B Nsight SHA-256 values
`659df8aa28a5425b3f9d172ba0bdc78bd1bb3b710da8297b28278aa4c4dd256a`
and `cc55d87a0dff360e29425ccfabee688b2ab7f6185cbf0b4192f9e309e05a7174`.

## Cooperative dense Q4_K MMQ

EXP-2026-11-20-1566 restores the EXP-1476 cooperative
weight staging mechanism in standalone Goose for the
aligned 4096-column dense Q4_K path on SM89. The reference
128-by-128 tile uses 57,856 bytes of dynamic shared
memory. Goose starts with a 16-by-32 tile. Four warps
stage 16 weight rows and share the decoded weight groups
and FP32 representations of the half-rounded scale/minimum
coefficients. The existing Q8_1 packer and reference K
partition order remain. All four warps share the K split
because the 32 columns stay inside one 128-column reference
tile. The device binding implements the CUDA shared loads
and LDSM/MMA instructions. The ordinary path covers other
shapes, including routed batches. Use
`GOOSE_MMQ_Q4_WIDE_SHARED=0` for the control or rollback.

The initial weight-only staging experiment uses 4,096
shared bytes and changes the dense group from 0.855 to
0.831 s in the short pair. Staging coefficients as well
uses 5,120 shared bytes and changes that group from
0.860 to 0.669 s in the subsequent same-binary pair.
Short prefill changes from 767.273 to 796.103 tok/s.
Two dense Q4_K matrix fixtures compare 4,718,592 FP32
outputs bit for bit and CUDA memcheck reports zero errors.

The full pair uses the EXP-1565 Attention and EXP-1564
device staging paths, the same request and 4096/4096
launch profile. Arm A sets the rollback flag. Arm B uses
the default. CUDA traces show 1,360 calls to
`goose_mmq_q4_dense_shared`, 5,120 shared bytes per block
and 110 registers per thread. The compiled SM89 entry
contains LDSM.

| Metric | A | B |
|---|---:|---:|
| Prefill, tok/s | 773.834 | 804.407 |
| Prefill, s | 42.345 | 40.736 |
| Decode, steps/s (511 steps) | 20.433 | 20.333 |
| Decode, s | 25.008 | 25.132 |
| Dense 4096-column Q4_K group, s | 6.853 | 5.333 |
| All Q4_K reference-order MMQ, s | 15.420 | 13.914 |
| Q6_K reference-order MMQ, s | 5.010 | 5.002 |
| Long vector Q4 Attention GPU time, s | 3.508 | 3.509 |
| H2D, whole trace bytes | 154,911,039,824 | 154,911,039,824 |
| D2H, whole trace bytes | 551,156,480 | 551,156,480 |
| Process RSS high-water, KiB | 23,513,972 | 23,513,680 |
| Sampled peak process VRAM, MiB | 8,524 | 8,524 |
| Sampled peak whole-card VRAM, MiB | 10,708 | 10,708 |
| Process swap and system swap-out growth | 0 | 0 |

All 512 IDs, probability rows, content, stop reason, SLRU
counters and 5,301,139-page model residency match.
The existing opening prompt echo persists. Prefill saves
1.609 s with 1.520 s less device time in the targeted
group. Decode varies by 0.100 steps/s across the single
profiled pair. Full CUDA and response parity remain open.

The weight-only evidence is under
`/home/end0/AI/LlamaServer/franken-goose-mmq-shared.iVYM1M`.
The final executable and full pair are under
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj`,
in `cuda-full-dense-both.f6q9p_m7` and
`cuda-full-dense-both.4bt53wno`. Executable SHA-256
`11e32a5e0607ee5567e82fa80247f39716538451ce95b046eec2ffc093c91607`.
A and B Nsight SHA-256 values
`24a024a83d96391b1b42fa40f7357ecdba10d89fe5d63f303a69813b69d71fb9`
and `1ad53cd07b7ccc58ba012fc78660bd8f990a9b35aad83bbdd4348d7af285efba`.

## Cooperative routed Q4_K MMQ

EXP-1568 applies the cooperative Q4_K weight stage to the
SM89 indexed gate/up projections with 512 output rows and
2,048 input elements. The Goose job planner groups at most
32 adjacent jobs with the same expert plane offset. The
packing budget accounts for that limit, including when
shared Q8_1 inputs fall back to per-route packing. Four
warps cooperatively stage weights and coefficients. Tail
lanes read a valid packed slot and still participate in the
barriers. Only the live routes store results.
`GOOSE_MMQ_Q4_ROUTED_SHARED=0` selects the eight-route control.

`integration/goose-standalone/tests/mmq-routed-shared.c` exercises
226 routes across ten experts, with tails on both sides
of the 8- and 32-route boundaries. All 115,712 values match
bitwise for each of the two packing modes. Compute Sanitizer
memcheck reports zero errors. SM89 SASS contains LDSM, with
5,120 shared bytes, 110 registers and zero spill bytes.

The short Apex pair removes 0.043 s from routed Q4_K
(0.728 to 0.685 s). Prefill is 786.305 and 794.107 tok/s.
The full pair uses the same executable, request and 4096/4096
launch profile. Arm A sets the rollback flag. Arm B uses
the default. Both complete all 512 response IDs.

| Metric | A | B |
|---|---:|---:|
| Prefill, tok/s | 795.573 | 802.785 |
| Prefill, s | 41.188 | 40.818 |
| Decode, steps/s | 20.287 | 20.343 |
| Decode, s | 25.189 | 25.119 |
| Routed Q4_K gate/up device time, s | 5.754 | 5.282 |
| Routed Q4_K gate/up launches | 640 | 640 |
| Routed Q4_K gate/up thread blocks | 21,326,720 | 22,745,792 |
| H2D, whole trace bytes | 154,911,039,824 | 154,864,119,008 |
| D2H, whole trace bytes | 551,156,480 | 551,156,480 |
| Process RSS high-water, KiB | 23,513,964 | 23,514,240 |
| Sampled peak process VRAM, MiB | 8,524 | 8,524 |
| Sampled peak whole-card VRAM, MiB | 10,742 | 10,741 |
| Process swap and system swap-out growth | 0 | 0 |

Both responses match in content, IDs, probability rows
and stop reason. The previously reported opening echo
persists. Model residency and all SLRU counters match.
The routed stage saves 0.472 s of kernel time, and
prefill saves 0.370 s. The additional 1,419,072 blocks
expose the cost of partially filled 32-route tiles.

The raw reports, executable and responses are under
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/routed-q4-exp1568`.
Full runs: `cuda-full-dense-both.fz8s60fp` and
`cuda-full-dense-both.qlv8j1jl`. Executable SHA-256
`6d6e4b569e2c4d83b17c78dbfb60eff62a45266f300290e9cf8f7222c148ff22`.
Nsight SHA-256 values
`2fe15a9425255d3495a8dbcf151296ed6621c1e9d981e943f696cde33e026d4b`
and `941d936a5e7eb9f13d6e7678b56a422ff2106bffdabf7058a30b6e75fa4b339c`.

## Flash Attention Stream-K restoration

EXP-1569 schedules the existing FP16 MMA tile across a
flattened query-tile by key-tile range. The Goose planner
uses two workers per SM and the reference 5% rounding
rule. It selects a uniform, general, or whole-tile path
according to the shape. The fixup uses the original
Goose `mask-fixups-device.goose` reductions and layout.
The implementation reuses the MMQ pack allocation for
fixup metadata and partials. It keeps the causal bound,
supports partial query tiles and writes neutral partials
for fully masked ranges. A barrier protects shared
memory across successive tiles of one worker.

The fixture `integration/goose-standalone/tests/flash-stream-k.c`
checks 16 query heads, two KV heads and Q4_0 storage.
Uniform (17 queries, 199 keys, prefix 17) compares 69,632
values to the whole-tile GPU path, with maximum absolute
error 0.000280. General (289 queries, 333 keys, prefix 45)
compares 1,183,744 values, with maximum absolute error
0.001769 and relative L2 0.0004531. Both pass CUDA
memcheck. General also passes racecheck on the final
build with zero errors and warnings.

RTX 4070 executes ten 92-worker Stream-K and ten general
fixup launches on the paired Apex 4096/129. A 120/1
Apex probe executes ten 90-worker Stream-K and ten
uniform fixup launches for 30 output tiles. The uniform
probe uses the available 8-by-8 tile. EXP-1476's trace
also includes a 4-by-8 uniform shape.

| Profiled Apex 4096/129 | Whole-tile A | Stream-K B |
|---|---:|---:|
| Prefill, tok/s | 789.053 | 790.191 |
| Decode, steps/s | 21.554 | 22.106 |
| Flash MMA and fixup device time, ms | 50.796 | 74.568 |
| Process RSS high-water, KiB | 23,484,572 | 23,485,240 |
| Sampled peak process VRAM, MiB | 8,484 | 8,484 |
| Process swap and system swap-out growth | 0 | 0 |

Both answers identify the missing measurements and
queue-growth risk. Both repeat part of the input. IDs,
probabilities and SLRU activity differ, so the pair
cannot attribute a decode gain to Stream-K. Both retain
all 5,301,139 model pages. The final barrier build
repeats B's response, IDs and probabilities with 8,484
MiB peak process VRAM and zero swap. Enable
`GOOSE_FLASH_STREAM_K=1` alongside `GOOSE_FLASH_MMA=1`
for package qualification. The existing whole-tile
selector remains the default pending the full quality
and memory check.

The profiled pair, uniform probe and final confirmation
are stored under
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/flash-stream-exp1569`.
The A/B directories end in `5hlaxrgu` and `3t3tehhf`.
The uniform and final confirmations end in `q786tnov`
and `_uvs7o46`. Final executable SHA-256
`bb0bc9cb57e490c762ad4032e353afc13ecf35912d0f5b6c7529e1c76b9b53bf`.

## CUDA VMM scratch restoration

EXP-1570 reuses the Goose VMM growth arithmetic in the
standalone runtime. The MMQ packed activation buffer and
Attention scratch each reserve up to 32 GiB of virtual
address space. Physical pages are created on demand and
mapped at the end of the current extent, rounded to the
device granularity. The policy leaves the VA stable during
growth. The Driver API bindings handle the map, access
permissions, synchronization, unmap and release effects.
Unsupported drivers and failed VMM allocations keep the
existing ordinary allocation path. Enable
`GOOSE_CUDA_VMM=1` for package qualification.

Eight boundary cases cover alignment, reuse, growth,
invalid granularity and the upper bound. An RTX 4070
fixture runs two Attention sizes in the same process,
compares both outputs bitwise and passes CUDA memcheck.
The two original base addresses stay fixed while the
regions grow from 2 to 14 MiB and from 4 to 6 MiB.
The diagnostic log confirms release of both addresses.
The runtime defers the first Goose planner call until
the CPU module has been initialized and registers
cleanup after CUDA initialization.

| Profiled Apex 4096/129 with Flash Stream-K | Ordinary A | VMM B |
|---|---:|---:|
| Prefill, tok/s | 800.468 | 802.489 |
| Decode, steps/s | 22.160 | 22.127 |
| Process RSS high-water, KiB | 23,484,576 | 23,484,816 |
| Peak process VRAM, MiB | 8,484 | 8,484 |
| Peak whole-card VRAM, MiB | 10,699 | 10,699 |
| Process swap and system swap-out growth | 0 | 0 |
| Ordinary CUDA allocations | 28 | 24 |
| VMM reserves / maps / unmaps / address frees | 0 / 0 / 0 / 0 | 2 / 4 / 4 / 2 |

Both arms return identical content, token IDs, probability
rows and stop reason. Both retain all 5,301,139 model
pages and the same GPU SLRU activity. The single profiled
pair qualifies the mechanism on the short request.
The full quality and memory gates remain open.

The fixture, pair and executable are under
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/vmm-exp1570`.
The A/B directories end in `nvcd9prq` and `yduladvb`.
Executable SHA-256
`817a222ceb430ce990f8b79c48044c9a1b1d10c5d8564c67f5d1b44fb966a9ba`.

## CUDA Graph replay for vector Attention

EXP-1571 captures the partitioned Q4_0 vector Attention
kernel and its combine kernel on one blocking CUDA stream.
The stream preserves ordering with kernels and copies on
the legacy stream. The captured executable retains the
two kernel nodes. Replay updates both nodes' parameters,
including the current Q/K/V and output addresses, KV
length, strides packet and launch geometry. The Goose
shape selector retains direct launch for a single part.
Missing Driver API support, capture failure or a rejected
update selects the direct pair. An error after attempting
graph submission propagates to the caller.

The six one-query fixtures vary KV length across the
128-key boundary, change the partition count and grow
the scratch buffer. Each result matches the direct path
bit for bit. Nsight records one capture and five graph
launches with eight kernel parameter updates. CUDA
memcheck records zero errors.

| Profiled Apex 4096/129 with VMM and Flash Stream-K | Direct A | Graph B |
|---|---:|---:|
| Prefill, tok/s | 788.828 | 801.516 |
| Decode, steps/s | 22.189 | 22.132 |
| Process RSS high-water, KiB | 23,484,856 | 23,484,736 |
| Peak process VRAM, MiB | 8,484 | 8,484 |
| Peak whole-card VRAM, MiB | 10,695 | 10,695 |
| Process swap and system swap-out growth | 0 | 0 |
| Direct CUDA launch API calls | 269,616 | 267,058 |
| Graph captures and instantiations | 0 | 1 each |
| Graph launch API calls | 0 | 1,280 |
| Graph kernel parameter updates | 0 | 2,558 |

Both responses match in content, IDs, probability rows and
stop reason. Model residency and all SLRU counters match.
The trace proves replay and parameter updates in the live
runtime. The single profiled pair does not establish a
speed gain. Enable `GOOSE_CUDA_GRAPH=1` for package
qualification. Capture of larger decode sequences remains
open.

The raw reports and executable are under
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/cuda-graph-exp1571`.
The A/B directories end in `p6wd6yp_` and `dji5vyxt`.
Executable SHA-256
`cdf0b20c4c197f08b38a53329ed0192cb0b6e697a68dae564847bb7f6c12dd6c`.

## Full Flash Stream-K, VMM and CUDA Graph diagnostic

EXP-1572 compares the same EXP-1571 executable on the frozen
32K/512 request. Both arms use context 262,144, batch/ubatch
4096/4096, host pin 4,096 MiB, the existing SLRU bank and
the 384 MiB activation arena. Arm B enables Flash Stream-K,
VMM scratch growth and vector Attention capture. Arm A
disables all three. This is diagnostic evidence for the live
mechanisms and their interaction.

| Profiled full Apex on RTX 4070 | A | B |
|---|---:|---:|
| Prefill, tok/s | 797.362 | 797.355 |
| Decode, steps/s | 20.224 | 20.080 |
| Prefill, s | 41.096 | 41.096 |
| Decode, s | 25.268 | 25.448 |
| Flash prefill compute and fixup, s | 2.841 | 2.395 |
| General Flash fixups | 0 | 80 |
| VMM reserves / maps / unmaps | 0 / 0 / 0 | 2 / 11 / 11 |
| CUDA Graph launches / updates | 0 / 0 | 5,110 / 10,218 |
| Context synchronizations | 291,671 | 291,673 |
| H2D, whole trace bytes | 154,864,119,008 | 156,503,583,128 |
| D2H, whole trace bytes | 551,156,480 | 551,156,480 |
| Expert cache fills | 77,707 | 78,687 |
| Process RSS high-water, KiB | 23,514,996 | 23,527,992 |
| Peak process VRAM, MiB | 8,524 | 8,524 |
| Peak whole-card VRAM, MiB | 10,736 | 10,737 |
| Process swap and system swap-out growth | 0 | 0 |

Both arms keep all 5,301,139 model pages resident. Both
answers explain queue buildup and suggest measuring the
service and applying bounded admission. Both begin by
repeating a fragment of the request. IDs, probabilities,
SLRU activity and response text diverge. Keep the prior
production checkpoint. The package does not pass final
quality or speed acceptance.

### Transfer and scheduling attribution

The EXP-1476 scheduler splits graphs and orders asynchronous
copies with backend events (`ggml-backend.cpp`,
`ggml_backend_sched_split_graph` and allocation/compute
of the splits). Its CUDA evaluator runs
`ggml_cuda_try_fuse` and uses streams and fork/join events.
The standalone Goose executor shares its planned activation
arena and defers selected waits to graph completion.

In the B trace 220,242 context waits immediately follow
MMVQ, 41,200 follow scalar matmul, 15,570 follow tensor
blits and 10,380 follow Q4 cache writes. These are counts
by preceding API and kernel correlation ID. The 25.397 s
aggregate includes elapsed device work. It bounds time
spent inside the calls and does not establish avoidable
host latency. Matrix launch paths still synchronize after
each projection even in a planned graph.

The remaining D2H is 508,559,360 bytes from 512 full
logits rows, 41,943,040 bytes from 320 routing ID
transfers and 654,080 bytes from 20,440 small transfers.
The earlier 64 MiB round trips remain absent. The
additional 980 expert fills correlate with the higher
H2D in arm B. The two-node replay leaves most CUDA
dispatch in the direct path. It records 1,077,590 direct
launch API calls versus 1,087,728 in A.

Both traces and replies are under
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/restoration-exp1572`.
The A/B directories end in `35aidzzm` and `vzrecq47`.
Nsight SHA-256 values
`c7bd5611b5ed362dd755a7ed1c2336350666f65f6266e8860311796caa6e32ac`
and `4f079c805abbf881f7eac7085faf023dae5c5f20ea9152f69e5187797cb21c5b`.

## Deferred matrix waits

EXP-1573 extends the existing planned-graph completion
fence to MMQ, MMVQ, dense MMA and scalar matrix launches.
The legacy stream orders intermediate buffers and output
publication. Blocking host reads retain visibility. Failed
launches and unplanned execution retain their synchronous
handling. Use `GOOSE_CUDA_DEFER_MATRIX_WAITS=0` to restore
per-matrix waiting.

| Profiled Apex 32K/512, 4096/4096 | A | B |
|---|---:|---:|
| Prefill, tok/s | 792.132 | 797.870 |
| Prefill, s | 41.367 | 41.069 |
| Decode, steps/s | 20.047 | 20.882 |
| Decode, s | 25.490 | 24.471 |
| Context synchronization calls | 291,671 | 26,469 |
| Time in context synchronization calls, s | 25.244 | 0.961 |
| Time in H2D calls, s | 31.637 | 54.745 |
| H2D, whole trace bytes | 154,864,119,008 | 154,864,119,008 |
| D2H, whole trace bytes | 551,156,480 | 551,156,480 |
| Process RSS high-water, KiB | 23,514,836 | 23,514,764 |
| Peak process VRAM, MiB | 8,524 | 8,524 |
| Peak whole-card VRAM, MiB | 10,785 | 10,785 |
| Process swap and system swap-out growth | 0 | 0 |

Both responses match in content, IDs, probability rows and
stop reason. The previously recorded opening echo persists.
The short pair also matches every response field, and decode
improves from 21.551 to 22.092 steps/s. The binary with the
default selector repeats the B response and 6,579 context
synchronizations on the short request. Most of the time
removed from the context wait API moves into H2D. Full
prefill saves 0.298 s and decode saves 1.019 s.

The full pair and default checkpoint are under
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/matrix-waits-exp1573`.
The A/B directories end in `fjd09uke` and `jy05j2cj`.
The default confirmation ends in `y3oyyjpj`.
Default executable SHA-256
`12b2b058862b745846391e681205a364441d3c4cb2b3db0218604250d29f2f25`.

## Lazy Q4 KV VMM restoration

EXP-1575 restores the EXP-101 physical-memory policy for
standalone causal Q4 KV. The Goose state manifest identifies
the owning tensor. Each K/V tensor obtains a stable virtual
range sized for the complete tensor. Bound accesses grow
and clear the mapped prefix through the existing VMM
planner. Reads, writes and diagnostic copies keep the same
byte layout and fail before using a failed map. Recurrent
state keeps an eager allocation. Reset clears the mapped
KV high-water and recurrent state, and teardown unmaps
the pages and frees the reservations. Devices without VMM
support keep eager allocation. Set `GOOSE_KV_LAZY_VMM=0`
to select the eager path.

The fixture checks zeroed pages, preservation of a written
prefix during growth, the final partial page and reset of
both KV and recurrent state. It passes CUDA memcheck.
The short Apex trace records 20 mappings and exact output.
The full 32K/512 trace records 100 maps and 100 unmaps
across 20 KV reservations. The logical context remains
262,144 with 4096/4096. The SLRU bank is exactly
5,234,122,752 bytes in both arms.

| Same-binary full Apex | Eager A | Lazy B |
|---|---:|---:|
| Profiled prefill, tok/s | 798.897 | 802.982 |
| Profiled decode, steps/s | 20.848 | 21.046 |
| Profiled ready process VRAM, MiB | 7,962 | 6,522 |
| Profiled peak process VRAM, MiB | 8,524 | 7,284 |
| Profiled process RSS high-water, KiB | 23,514,884 | 23,515,244 |
| Profiled system swap-out growth, pages | 44,854 | 0 |
| Unprofiled reverse-pair prefill, tok/s | 804.446 | 806.801 |
| Unprofiled reverse-pair decode, steps/s | 21.807 | 22.594 |
| Unprofiled peak process VRAM, MiB | 8,512 | 7,272 |
| Unprofiled process RSS high-water, KiB | 22,936,224 | 22,937,952 |
| Unprofiled system swap-out growth, pages | 0 | 0 |

Process swap is zero in both pairs. All full answers,
tokens and probability rows match their controls. SLRU
counts, profiled H2D/D2H and all 5,301,139 model pages
also match. The existing opening echo persists. The
unprofiled pair serves a second 4096/129 request with
zero cached prompt tokens in each process. Its response,
IDs and probability rows also match. Peak VRAM falls
1,240 MiB in both pairs. The system swap activity in
the profiled control limits a speed attribution from
that pair. The unprofiled pair confirms the memory
improvement without a measured throughput regression.

The default build also completes a short Apex 4096/129
with scratch VMM and CUDA Graph enabled. Nsight sees 22
reservations, 24 maps and 1,280 graph launches. Its
output matches the standalone lazy run, with 7,084 MiB
peak process VRAM and zero process swap or system swap-out.

The executable and evidence are under
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/lazy-kv-exp1575`.
The profiled A/B directories end in `crx43jaz` and
`57g3pd3y`. The unprofiled A/B directories end in
`i807ja2q` and `me1sdsm_`. Default executable SHA-256
`d0e953021463e61c394d420ce274bd91490acc0fad30c6a622a35b79a0d24f9f`.

## Asynchronous H2D packets

EXP-1576 moves eligible uploads up to 4 KiB onto the
asynchronous CUDA copy path in planned graphs. The existing
Goose pinned-pool module assigns aligned slots in a single
1 MiB pinned host buffer. Each packet copies its original
bytes into that buffer before launch. The GPU copy stays
on the legacy stream to preserve dependency ordering,
including with the blocking Attention replay stream.
The host buffer is reused after graph completion. A failed
completion disables reuse. Allocation failure, disabled
pinning, unplanned execution and capacity exhaustion
retain the blocking copy path. Expert weight uploads
retain their current scheduling.

The saturation fixture submits 4,097 matrices, reuses
the buffer in a second graph, and submits one matrix
outside the graph. It verifies the results, 12,309 async
copies, three blocking copies after exhaustion, three
blocking copies outside the graph and a complete
allocation/free lifecycle. CUDA memcheck reports zero
errors.

Both paired full Apex 32K/512 blocks use batch/ubatch
4096/4096, context 262,144, the same model and executable,
default lazy KV and the unchanged expert budgets. Only
`GOOSE_CUDA_ASYNC_PACKETS` differs.

| Metric | Blocking A | Async B |
|---|---:|---:|
| Profiled prefill, tok/s | 798.127 | 796.619 |
| Profiled decode, steps/s | 20.790 | 23.413 |
| Profiled decode, s | 24.579 | 21.825 |
| Profiled synchronous / asynchronous H2D calls | 1,153,478 / 0 | 299,832 / 853,646 |
| Profiled host time in both H2D APIs, s | 54.818 | 45.154 |
| Profiled context wait API time, s | 0.948 | 6.779 |
| Profiled H2D whole trace bytes | 154,864,119,008 | 154,864,119,008 |
| Profiled D2H whole trace bytes | 551,156,480 | 551,156,480 |
| Profiled peak process VRAM, MiB | 7,284 | 7,284 |
| Profiled system swap-out growth, pages | 32,927 | 0 |
| Unprofiled reverse-pair prefill, tok/s | 802.004 | 805.183 |
| Unprofiled reverse-pair decode, steps/s | 22.062 | 25.012 |
| Unprofiled reverse-pair decode, s | 23.162 | 20.430 |
| Unprofiled peak process VRAM, MiB | 7,272 | 7,272 |
| Unprofiled process RSS high-water, KiB | 22,936,584 | 22,938,524 |
| Unprofiled system swap-out growth, pages | 0 | 0 |

Process swap is zero in both pairs. All answers, token
IDs, probability rows, model residency and SLRU activity
match. The existing opening echo persists. Some of the
H2D API time moves to context waits. The unprofiled pair
shows a 2.733 s decode improvement with no detectable
prefill regression on that pair and unchanged peak VRAM.
The host buffer uses 1 MiB of pinned memory.

The default build repeats the short Apex response with
212,707 async copies and 1,280 graph launches alongside
scratch and lazy-KV VMM. Peak process VRAM is 7,084 MiB
and process swap and system swap-out growth are zero.
Set `GOOSE_CUDA_ASYNC_PACKETS=0` for the blocking rollback.
The historical expert transfer and compute overlap
remains a separate mechanism.

Evidence and binaries are under
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/async-packets-exp1576`.
The profiled A/B directories end in `5hulo71g` and
`_gtst4av`. The unprofiled A/B directories end in
`jk3bvqtp` and `5urojrcy`. The final default short run
ends in `orvg3r6w`. Default executable SHA-256
`60ec1dfc5a737634195a2b7b21d8e344d1b5651455c07187760b98d409a51670`.

## Overlapped expert projection uploads

EXP-1577 restores the EXP-107 copy/compute overlap
mechanism for the independent resident SLRU bank. When
a planned, deferred multi-expert graph fills a slot,
Goose copies the current projection first. Other planes
from known registered source ranges are queued in a
bounded host job list. A CUDA event protects reuse of
the selected slots. After the current matrix's descriptors
and kernels are queued, the driver submits the staged
copies on a nonblocking stream. The next expert consumer
waits on the upload-complete event. Pageable spans and
small expert sets use ordinary uploads. The cache uses
the existing GPU bank and the same host pin budget.

The SLRU fixture and GPU eviction fixture verify projection
selection, slot ownership, same-group cross-layer eviction
and failure reset. The GPU test executes eight event records
and eight event waits and passes CUDA memcheck.

The first short version issued 1.403 GB on the side stream
before the matrix launch and showed zero kernel overlap.
In its first staged wave the matrix's first kernel started
4.328 ms after the side copy began. Submitting the staged
wave after the matrix descriptors and kernels creates
61.291 ms of observed copy/kernel intersection in the
short 4096/129 trace.

| Full Apex 32K/512, same binary and 4096/4096 | Ordinary A | Overlap B |
|---|---:|---:|
| Profiled prefill, tok/s | 797.946 | 807.536 |
| Profiled prefill, s | 41.065 | 40.578 |
| Profiled side-stream H2D bytes | 0 | 9,515,630,592 |
| Profiled side-stream copy/kernel intersection, s | 0 | 0.440 |
| Profiled total H2D bytes | 154,864,119,008 | 154,864,119,008 |
| Profiled total D2H bytes | 551,156,480 | 551,156,480 |
| Profiled peak process VRAM, MiB | 7,284 | 7,284 |
| Profiled system swap-out growth, pages | 1,919 | 57,502 |
| Unprofiled reverse-pair prefill, tok/s | 803.911 | 814.122 |
| Unprofiled reverse-pair prefill, s | 40.761 | 40.249 |
| Unprofiled reverse-pair decode, steps/s | 24.949 | 25.226 |
| Unprofiled peak process VRAM, MiB | 7,272 | 7,272 |
| Unprofiled process RSS high-water, KiB | 22,938,660 | 22,938,588 |
| Unprofiled system swap-out growth, pages | 0 | 0 |

Both pairs preserve the response, all 512 token IDs,
probability rows, SLRU counters and full model residency.
Process swap remains zero. The earlier prompt echo persists.
The profiled pair confirms the mechanism with system swap
activity. The unprofiled pair confirms a 0.511 s prefill
gain with unchanged peak VRAM. Decode has no staged expert
uploads under the current selector, so its timing difference
does not establish a decode gain.

The default build on short Apex repeats the response with
2,378 side-stream copies and 1,280 CUDA Graph launches.
Scratch VMM and lazy KV are active, peak process VRAM is
7,084 MiB and swap is zero. Set
`GOOSE_CUDA_EXPERT_OVERLAP=0` for the blocking rollback.
The remaining pageable and current-projection uploads
still need separate optimization.

The executable and evidence are under
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/expert-overlap-exp1577`.
The profiled A/B directories end in `sz4upaih` and
`wx7o6i72`. The unprofiled A/B directories end in
`fetjzwqg` and `zc3wtc0r`. The default confirmation
ends in `xeujmkr_`. Default executable SHA-256
`49b03df6edf0db31b5b36eea385eb432e97a032347bc1d6c7ea1647a4ae4d92b`.

## Cooperative Q6_K MMQ

EXP-1578 extends the existing shared dense and routed
dispatch to Q6_K on SM89. The Goose loader expands Q6_K
low and high words into 16 weight groups, staging signed
scales and conversion factors with the fragments. Four
warps reuse the tile via LDSM.x2. The D4 reciprocal packer
and FP32 accumulation order stay unchanged. The qualified
dense path uses 4,096 columns with aligned 128-row groups.
The indexed path covers 2,048 output rows and 512 input
elements. Both use a 16-by-32 output tile and preserve the
existing fallback dispatch. Set `GOOSE_MMQ_Q6_SHARED=0`
to select the reference-order narrow tile.

The dense fixtures compare 4,718,592 FP32 results.
The shared routed fixture checks 226 routes and 462,848
Q6_K results for each packing mode. Q4_K is checked through
the same dispatch fixture. All comparisons are bitwise
equal and CUDA memcheck reports zero errors. SM89 SASS
contains LDSM.x2. The Q6_K entries use 4,416 shared bytes,
113 or 115 registers and zero spill bytes. The Q4_K shared
entries retain 5,120 shared bytes and 110 registers.

| Same-binary full Apex 32K/512, 4096/4096 | Narrow A | Shared B |
|---|---:|---:|
| Profiled prefill, tok/s | 807.032 | 831.755 |
| Profiled prefill, s | 40.603 | 39.396 |
| Profiled Q6_K MMQ GPU time, s | 5.024 | 4.053 |
| Profiled Q6_K dense and routed shared launches | 0 / 0 | 320 / 320 |
| Profiled H2D bytes | 154,864,119,008 | 154,852,393,496 |
| Profiled peak process VRAM, MiB | 7,284 | 7,284 |
| Unprofiled reverse-pair prefill, tok/s | 814.272 | 834.303 |
| Unprofiled reverse-pair prefill, s | 40.242 | 39.276 |
| Unprofiled reverse-pair decode, steps/s | 24.970 | 25.191 |
| Unprofiled peak process VRAM, MiB | 7,272 | 7,272 |
| Unprofiled process RSS high-water, KiB | 22,938,688 | 22,938,812 |
| Process swap and system swap-out growth | 0 | 0 |

Both pairs preserve content, all 512 IDs, probability
rows, SLRU activity and full model residency. The previous
opening echo persists. Q4_K retains the same launches and
geometry in both traced arms. The shared Q6_K selectors
apply to prefill on this request. Decode differs only as
an observed paired timing. The unprofiled B runs with the
default selector and saves 0.966 s of prefill.

The evidence and default executable are under
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/q6-shared-exp1578`.
Profiled A/B directories end in `fla2k9lx` and `c95jysst`.
Unprofiled A/B directories end in `3yhos541` and `f219g233`.
Default executable SHA-256
`7c35a7399c6a1bd25273f689408824163765b51e4f0b2361d1edd8512e54dedb`.

## Aligned dense MMQ windows

EXP-1579 rounds the dense staging window down to a whole
number of cooperative 32-route blocks. The Goose planner
keeps windows below four tiles unchanged. It never rounds
up or increases the 16 MiB packing budget. The narrow
path remains available if an aligned launch is ineligible.
`GOOSE_MMQ_ALIGN_WINDOW=0` selects the previous windows.

The original Q4_K path split the 4,096 columns of the
2,048-row projection into 455 and 57 eight-route tiles.
Neither piece met the four-tile shared launch guard.
The aligned path splits them into 452 and 60. The two
GPU calls use the 16-by-32 shared entry and retain the
existing K split and accumulation order.

Eight planner cases and 8,388,608 dense FP32 output
values pass. CUDA memcheck records zero errors. Short
Apex records the targeted 80 calls in the narrow entry
with 0.355 s GPU time, then in the shared entry with
0.283 s GPU time. Short prefill is 833.681 and 848.074
tok/s. The traced copy totals and peak process VRAM
match.

| Unprofiled reverse full Apex 32K/512, 4096/4096 | Previous A | Aligned B |
|---|---:|---:|
| Prefill, tok/s | 836.847 | 847.266 |
| Prefill, s | 39.157 | 38.675 |
| Decode, steps/s | 25.108 | 25.211 |
| Peak process VRAM, MiB | 7,272 | 7,272 |
| Process RSS high-water, KiB | 22,938,892 | 22,938,832 |
| Process swap and system swap-out growth | 0 | 0 |

Both arms return identical content, all 512 IDs and
probability rows. SLRU activity and all 5,301,139 model
pages match. The existing opening echo persists. The
default B saves 0.482 s of full prefill. The window change
does not execute in decode.

The fixtures, traces and default executable are under
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/mmq-aligned-exp1579`.
Unprofiled A/B directories end in `a82avi54` and `hedrpskx`.
Default executable SHA-256
`50e0a660facd18f74513361ed65962b89d73aee2b76b6ed74f05373c718e1710`.

## Q4_K 16-by-64 cooperative tiles

EXP-1580 extends shared Q4_K from 32 to 64 routes per
16-row block on qualified SM89 shapes. Each warp has
two eight-route accumulators and reuses a single LDSM
fragment and scale set for both MMA operations. Dense
64-column blocks stay inside a 128-column reference
partition and use the same backward fixup. The planner
rounds packing windows down and retains the 16 MiB
budget. Routed tails skip unused warp halves and keep
the CTA barriers. The 16-by-32 and narrow paths remain
available. Set `GOOSE_MMQ_Q4_X64=0` for rollback.

The dense cases compare 8,912,896 FP32 results. Routed
cases compare 115,712 values per packing mode across
expert groups including 65 routes. All compare bitwise.
CUDA memcheck reports zero errors. Offline SM89 SASS
uses the same 5,120 shared bytes with 126 dense and 114
routed registers per thread, and zero spill bytes.

| Same-binary full Apex 32K/512, 4096/4096 | 16-by-32 A | 16-by-64 B |
|---|---:|---:|
| Profiled prefill, tok/s | 842.496 | 905.629 |
| Profiled prefill, s | 38.894 | 36.183 |
| Profiled dense shared Q4_K GPU time, s | 7.413 | 5.651 |
| Profiled routed shared Q4_K GPU time, s | 5.197 | 4.339 |
| Profiled dense shared Q4_K blocks | 32,440,320 | 16,220,160 |
| Profiled routed shared Q4_K blocks | 22,745,792 | 12,441,664 |
| Profiled H2D bytes | 154,852,393,496 | 154,844,665,400 |
| Profiled peak process VRAM, MiB | 7,284 | 7,284 |
| Profiled system swap-out growth, pages | 0 | 2,265 |
| Unprofiled reverse-pair prefill, tok/s | 842.482 | 898.410 |
| Unprofiled reverse-pair prefill, s | 38.895 | 36.473 |
| Unprofiled reverse-pair decode, steps/s | 25.073 | 25.090 |
| Unprofiled peak process VRAM, MiB | 7,272 | 7,272 |
| Unprofiled process RSS high-water, KiB | 22,938,992 | 22,938,992 |
| Unprofiled process swap and system swap-out | 0 | 0 |

Both pairs preserve content, all 512 token IDs,
probability rows, SLRU activity and full model
residency. The existing opening echo persists.
The unprofiled pair confirms 2.421 s lower prefill
latency with the new default. The profile measures
2.620 s less GPU time in the shared Q4_K families.

Evidence and binaries are under
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/mmq-x64-exp1580`.
Profiled A/B directories end in `mx8mvfng` and `lr4uflj8`.
Unprofiled A/B directories end in `ny2_a6wf` and `158x45t5`.
Default executable SHA-256
`70e8b33fc476cbe0669e5ef9b47758c9d2465e1bb8e2578a681d3add0c0d919b`.

## Q6_K 16-by-64 cooperative tiles

EXP-1581 extends the existing Q4_K 64-route planner to
the qualified dense and routed Q6_K shapes. Each warp
reuses the two LDSM.x2 weight fragments and signed
scales of a D4 group pair for two eight-route accumulators.
Both shared entries use the same Q6_K accumulation and
half-block finalization helpers. Dense K partition order
and quantizer stay unchanged. Routed tails skip unused
warp halves while preserving CTA barriers. The verified
16-by-32 rollback is `GOOSE_MMQ_Q6_X64=0`.

The dense cases compare 4,718,592 FP32 results.
The routed fixture checks 226 routes and 462,848 values
per packing mode. All compare bitwise. CUDA memcheck
reports zero errors. On SM89 the 64-route entries use
4,416 shared bytes and 128 dense or 126 routed registers,
with zero spill bytes.

| Short Apex 4096/129 CUDA trace | 16-by-32 A | 16-by-64 B |
|---|---:|---:|
| Prefill, tok/s | 913.564 | 947.907 |
| Dense Q6_K GPU time, s | 0.267 | 0.174 |
| Routed Q6_K GPU time, s | 0.226 | 0.165 |
| Dense Q6_K blocks | 1,269,760 | 634,880 |
| Routed Q6_K blocks | 2,904,064 | 1,619,328 |
| Peak process VRAM, MiB | 7,084 | 7,084 |

| Unprofiled reverse full Apex 32K/512, 4096/4096 | 16-by-32 A | 16-by-64 B |
|---|---:|---:|
| Prefill, tok/s | 905.283 | 934.390 |
| Prefill, s | 36.196 | 35.069 |
| Decode, steps/s | 25.215 | 25.233 |
| Peak process VRAM, MiB | 7,272 | 7,272 |
| Process RSS high-water, KiB | 22,939,148 | 22,939,140 |
| Process swap and system swap-out growth | 0 | 0 |

Both pairs preserve content, IDs, probability rows, SLRU
activity and full model residency. The full response
matches EXP-1580, including the existing opening echo.
The unprofiled pair confirms 1.128 s lower prefill latency
with the new default. The Q6_K shared tiles do not execute
in decode on this request.

Evidence and binaries are under
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/q6-x64-exp1581`.
Short A/B directories end in `n2f9vfet` and `fcpq6hc_`.
Full A/B directories end in `8p0be036` and `sklg3axw`.
Default executable SHA-256
`471702a96c6b8987c46cb357063dc6e8d867670013f5c17fa373f058ac6fd3a0`.

## Long Attention partition probe

EXP-2026-11-20-1562 holds the existing vector kernel fixed
and tests 64 versus 11 maximum partitions, matching the
observed partition count of the frozen hybrid. The isolated
same-binary microbench initially favors 11 partitions. At
32,768 keys the median kernel time is 1.543 versus 1.222 ms.
At 4,097 keys the two launch counts are 33 and 11, taking
0.186 versus 0.165 ms per kernel. Both pass their numerical
comparison with the CPU operation.

The real same-build Apex pair at 4096/4096 and 32K/512
does not reproduce the microbench gain:

| Profiled full Apex | 64 parts | 11 parts |
|---|---:|---:|
| Prefill, tok/s | 689.890 | 692.118 |
| Decode, steps/s | 16.573 | 15.781 |
| Vector Attention GPU time, s | 9.052 | 9.291 |
| Partition combine GPU time, s | 0.051 | 0.013 |
| Expert fills | 77,707 | 83,740 |
| Process RSS high-water, KiB | 23,579,860 | 23,580,188 |
| Sampled peak process VRAM, MiB | 8,524 | 8,524 |
| Process swap and system swap-out growth | 0 | 0 |

The 11-part answer asserts growing latency and increasing
retries without evidence from the request. The 64-part
answer explicitly says those measurements are absent.
The altered generation also changes expert traffic, so
the whole decode difference cannot be assigned to the
kernel alone. Retain 64 parts. The probe switch and
binary stay isolated for reproduction. This rules out
copying the partition count alone as a sufficient
restoration of the hybrid's Attention performance.

- Same-build pair and microbench under
  `/home/end0/AI/LlamaServer/franken-goose-attention-parts.UmYXXz`.
  A is `cuda-full-dense-both.pa18o0ow` and B is
  `cuda-full-dense-both.hpwbdd7s`.
- Executable SHA-256
  `e3e2b90468a534ca817f666f62ed8c7ff823a570377a0196da23f7a25c87a322`.
  Patch and harness are in `reproduction/`.

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
