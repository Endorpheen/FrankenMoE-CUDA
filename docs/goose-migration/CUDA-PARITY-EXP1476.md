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

After a material dependency change, use short sequential A/B runs on
identical inputs and settings. Use the best current verified checkpoint
as the control. Compare the combined candidate and, when necessary, the
individual toggles to isolate interactions. Record IDs, probabilities,
prefill, decode and the targeted hardware metrics. Enable a combination
in the working runtime only after it passes correctness and shows a real
end-to-end gain. Preserve each implementation and append new evidence to
the entry. Apply the full-request gate on the package schedule.

## Mechanisms

**32 mechanisms: PARITY 0, PARTIAL 22, MISSING 9, INCORRECT 1.**

Group three to five related improvements. Give each change its own
short correctness, CUDA-error and numerical checks, then commit it
independently. Run one full Apex 32K/512 4096/4096 request after
the package and compare prefill, decode, RAM, VRAM and
output against the previous correct checkpoint and EXP-1476.
Isolate any regression with the short tests and run changes
individually when needed. A critical failure or the first verified
generation repair calls for an immediate full run.
After the EXP-1528 diagnostic, use five short-verified improvements
before the next scheduled full request. An experimental change that
fails a short gate does not count toward that five.

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
| 3 | Activation packing. `mmq.cu` → direct or scatter `quantize_mmq_q8_1` in `quantize.cu`, then MMQ. DS4 carries a half scale and activation sum. D4 carries F32 scales. | Both direct and scatter templates execute in the reference. Independent MMQ uses the EXP180 packer for bounded, job-ordered DS4/D4 windows. | PARTIAL. Both formats execute on real Apex. Device scatter and unique-input packing remain open. Validate model-level parity and quantify duplicate packing. |
| 4 | MMQ stream-K. `mmq.cuh` partitions K and invokes `mul_mat_q_stream_k_fixup` to combine partial outputs. | Fix-up runs in the reference. Independent scalar kernels reduce K inside each output block. | MISSING. After the base tile passes, implement reference partition and reduction rules. Verify K tails, partial ordering and large segments separately from the base tile. |
| 5 | Quantized small-batch MMVQ. `ggml-cuda.cu` → `ggml_cuda_mul_mat_vec_q` → Q8_1 and `mul_mat_vec_q` in `mmvq.cu`. | Both `quantize_q8_1` and `mul_mat_vec_q` run. Goose uses its scalar Q4_K/Q6_K path during decode. | MISSING. Restore the appropriate small-batch integer-dot specializations and packing. Check the exact subtype, output and decode timing under the final graph. |
| 6 | Indexed small-batch MMVQ. `ggml_cuda_mul_mat_id` → `mul_mat_vec_q_moe_launch` → `mul_mat_vec_q_moe`. | The reference trace executes indexed MMVQ. Goose groups jobs on the host and feeds the scalar matrix kernel. | MISSING. Restore the routed MMVQ path and verify duplicate expert IDs, signed-scale arithmetic and first decode output. |
| 7 | Tensor Core dense GEMM. `ggml_cuda_mul_mat_cublas` selects the F32 `cublasSgemm` path with `CUBLAS_TF32_TENSOR_OP_MATH` in `common.cuh`. | The 4096/1 trace records 39 CUTLASS `s1688gemm` launches. `GOOSE_DENSE_MMA=1` selects 40 independent tiled TF32 MMA launches with F32 accumulation. | PARTIAL. The CUDA path and F32 error fixture pass. Apex output changes under the experimental flag and still differs from EXP-1476. Keep the flag off for the next baseline until the model-level numerical difference is resolved. |
| 8 | Dense small-shape dispatch. `mmf.cu` and `mmvf.cu` implement `mul_mat_f` and `mul_mat_vec_f`. The trace also records `dot_kernel`. | All three families appear in the trace. Goose does dense F32 FMA and shuffles using one output per warp. The exact library owner of `dot_kernel` remains open. | PARTIAL. Identify the dot route and reuse existing dot and warp code where equivalent. Recreate selected row/vector tiling and validate dimensions and backend selection by shape. |
| 9 | Device expert grouping. `ggml_cuda_mul_mat_id` / `mmq.cu` → `mm_ids_helper` → per-expert group ranges and Q8 scatter. | `mm_ids_helper` executes. Goose builds per-expert jobs and keeps a GPU SLRU, with host route synchronization. | PARTIAL. Feed active routes to device grouping and pack contiguous batches without losing expert ownership. Verify sparse, repeated and empty groups against the reference. |
| 10 | Fused MoE selection. `ggml-cuda.cu` `ggml_cuda_topk_moe_fusion` → `topk_moe_cuda`. | Reference executes selection fusion. Goose separately runs softmax, stable sort, gather, row sum and scaling. | PARTIAL. Combine equivalent operations while retaining tie order and normalized expert weights. Compare selected indices and scores before measuring speed. |
| 11 | Fused MoE combination. `ggml_cuda_match_moe_weighted_reduction` → `moe_weighted_reduction_f32`. | The reference fuses the weighted combination. Independent Goose uses the existing fused device body with bounded input windows and eliminates the separate weighted tensor. | PARTIAL. The 4096-token numerical fixture and real Apex trace pass the scoped gate. Full logits and peak memory acceptance remain open. |
| 12 | FP16 MMA Flash Attention. `fattn.cu` `ggml_cuda_flash_attn_ext` → shape selection → `fattn-mma-f16.cuh` `flash_attn_ext_f16`. | Reference prefill calls the MMA kernel even with Q4 KV storage. Goose uses Q4 scalar tiles plus a separate merge. | MISSING. Reuse Q4 cache and conversion leaves with the reference tiled MMA schedule and causal/mask behavior. Verify GQA, real head width 256 and long-context logits. |
| 13 | Small-query Flash Attention vector dispatch. `fattn.cu` → `flash_attn_ext_vec`. | Reference trace contains the vector path. Goose slices its scalar Q4 tiles for both prefill and decode. | MISSING. Restore the small-query specialization and validate softmax merging and decode latency at 32K. |
| 14 | Attention staging and bounds. `fattn-common.cuh` and `fattn-mma-f16.cuh` stage Q4_0 KV as FP16, use FWHT where selected, and derive KV bounds from the mask. | `dequantize_block_q4_0`, `fwht_cuda` and `flash_attn_mask_to_KV_max` execute. Goose now applies the reference Q/K/V and inverse-output rotations in the live Q4 KV graph. It tracks causal bounds in its KV transaction and reads Q4 directly. | PARTIAL. FWHT executes at widths 256 and 64 on Apex. Float16 staging/rounding, masked tails and generation parity still need validation. |
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
