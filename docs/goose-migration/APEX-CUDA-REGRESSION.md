# Apex CUDA regression audit

The comparison uses the same first 4,096 IDs of the frozen EXP-1476
request, physical batch/ubatch 4096/4096, context 262,144, Q4 KV and
one generated ID on RTX 4070. Each binary ran alone under Nsight
Systems 2024.5.1. The immutable reference is EXP-1476 tag
`v0.10.0-exp1476`, source commit
`e7c9155a01938b1825189240e7088620c6c7cd85`. It ran solely as a
reference. Goose still uses its own CPU executor, so this comparison
locates program differences and does not qualify speed parity.

| Trace | Reference | Independent Goose |
|---|---:|---:|
| Prefill wall time | 2,019.725 ms | 104,374.602 ms |
| CUDA kernels after startup | 4,145 | 2,773 |
| Sum of CUDA kernel durations | 783.732 ms | 66,932.028 ms |
| H2D in compute window | 9,976 copies, 13,748,362,792 bytes, 574.422 ms | 26,159 copies, 187,594,956,656 bytes, 17,501.654 ms |
| D2H in compute window | 139 copies, 229,248,128 bytes, 16.345 ms | 2,080 copies, 91,803,156,480 bytes, 9,660.653 ms |
| CPU time in recorded CUDA launch calls | 4,105 calls, 23.178 ms | 2,772 calls, 28.077 ms |
| CPU time in recorded CUDA synchronization calls | 341 stream synchronizations, 1,313.869 ms | 1,971 context synchronizations, 66,632.112 ms |

The candidate compute window spans its first to last CUDA kernel. The
control excludes warm-up by starting at the first request's
`rms_norm_f32`, after the 591 ms idle gap in the trace. The final
GPU kernel sets each end. Linux refused host IP sampling with
`perf_event_paranoid=4`. CUDA API spans record time inside the driver,
including GPU wait. They overlap GPU work and should be read separately.

| Operation | EXP-1476 execution | Independent Goose execution | Missing mechanism and evidence |
|---|---|---|---|
| Quantized matmul | `ggml_cuda_mul_mat_q` → `quantize_mmq_q8_1` → `mul_mat_q`. The request ran 424 MMQ launches in 390.792 ms and 424 activation quantizations in 56.918 ms. | `cuda_matrix_planes` → `fg_cuda_plane` or `fg_cuda_expert_batch` → `goose_quantized_matmul`. The shared kernel, including its F32 cases, ran 511 launches in 60,690.386 ms. | One block computes one result, reconstructs Q4_K/Q6_K as F32 and reloads its activation for that output. The reference stages Q8_1 once per input region and shares tiles across outputs. Disassembly of the traced Ada MMQ symbols shows Q4_K `IMMA.16832.S8.S8` and Q6_K `IMMA.16816.S8.S8`. The Goose PTX contains neither MMA nor DP4A. |
| Dense matmul | The CUDA dispatcher can select MMQ, MMV or cuBLAS. Executed evidence includes 39 CUTLASS tensor-op GEMM launches totaling 7.377 ms, plus dense vector kernels. The reference mapped cuBLAS and cuBLASLt. | F32 dispatch reaches `matmul_f32` under the same CUDA launch name. One warp computes one output with scalar FMA and shuffles. | The dense fast path lacks a tiled Tensor Core GEMM. The shared Goose kernel name prevents a separate time attribution to F32 from this trace. |
| MoE experts | `ggml_cuda_mul_mat_id` selects quantized MMQ or small-batch MMVQ. `mm_ids_helper` ran 117 times (19.274 ms), and direct and scatter Q8_1 kernels both ran. The captured request also uses 117 `mul_mat_vec_q_moe` launches. | Compact Top-K and the Goose SLRU produce a list of active jobs with expert-bank offsets. The batched launch still invokes one CUDA block per output element of one job. | Device grouping alone cannot replace per-expert MMA tiling and fused packing. The Goose host resolves cache/route jobs. The reference trace shows device ID grouping and Q8_1 scatter. |
| Attention | `ggml_cuda_flash_attn_ext` selects the FP16 MMA flash kernel after staging, even with Q4 cache storage. `flash_attn_ext_f16` took 37.437 ms in the request. | Q4 cache reads feed `goose_q4_attention_tiles` and `goose_q4_attention_merge` in 2,916.042 ms. One warp loops over keys for a query and head. | The CUDA scalar Q4 query/key path lacks the tiled FP16 flash MMA compute path. The cache format was Q4 in both launch profiles. |
| GDN and SSM | Fused state handling selects `ggml_cuda_gated_delta_net_mma::gdn_single` (55.773 ms) and `ssm_conv_long_token_f32` (25.888 ms). | `fg_cuda_gdn` dispatches `goose_gated_delta_net` (2,484.190 ms) and the serial-window leaf runs in `goose_ssm_conv` (104.193 ms). | The GDN schedule lost its MMA body. Both runtimes keep a persistent recurrent state and both traces show the kernels. |
| Fusion | Executed `topk_moe_cuda` (79 launches), `moe_weighted_reduction_f32` (79 launches) and fused RMS variants. Source also contains SSM-convolution and RoPE fusion rules. | Goose executes separate softmax, sort, gather, row sum and vector ops. Vector kernels account for 1,292 launches and 663.126 ms. | Some fused outputs are materialized as separate tensors in the Goose graph. Fusion rules present in the reference source are only attributed to this workload when their kernel appears in the trace. |
| Transfers and scheduling | The CUDA scheduler splits the graph by backend. The reference issues asynchronous copies, 41 recorded CUDA graph launches across the session, and stream synchronization. | The 64 MiB activation arena and synchronized adapter copy large intermediates through host storage. Output invalidation also materializes an old overlapping record. Direct trace: 2,080 D2H operations in the compute window, including 80 transfers of 268,435,456 bytes. | Device residency for large outputs and eviction of dead records are missing. Matrix time and GPU copy time are distinct measured losses. CPU synchronization mostly reflects waiting for the recorded GPU work. |

The first ablation target is the Q4_K/Q6_K matrix family. Its
60.690 seconds account for 90.7% of Goose GPU kernel time and 58.1%
of Goose prefill wall time. Transfers account for another 27.162
seconds of GPU activity under the stated window. Attention and GDN
also regress. Optimizing only MMQ cannot recover the full gap.

## Reuse from the existing port

[CUDA-MMQ-PLAN.md](CUDA-MMQ-PLAN.md) records the Q4_K execution probe
and the distinction between the unused DP4A probe and the live MMA
route. The current standalone PTX build uses none of those MMQ leaves.
Reuse the Q8_1 direct/scatter quantizer and the dispatch policy from
EXP180 and EXP203, Q4_K and Q6_K tile loaders from the specific
MMQ EXP255/257/258 cards, and the signed-scale differential from
EXP236. The EXP256 arithmetic gate is a numerical reference. It
recorded a performance cost from spilling its accumulators through
memory. Keep accumulators register-resident around the MMA primitive.
The missing composition is the tile loop, device grouping,
`ldmatrix`/`mma.sync` bindings, and write-back for the independent
runtime. Start with the Q4_K/Q6_K matrix route before tuning
smaller kernels. The old libraries remain outside the candidate.

## Full request checkpoint

The current independent build completed the pinned 32,768/512
request with batch/ubatch 4096/4096. The generation/step boundary
now stages token IDs and logits in native-owned memory, because
nested Goose exports start at arena frame zero. Before the fix,
the incoming values were correct and the GET_ROWS operands were
corrupt. The 3/128/512/1024/2048/4096 RTX 4070 fixture verified the
actual GET_ROWS ID arrays and the explicit insufficient-RAM error.

The real first prefill arena used 1,009,991,680 bytes in 14 blocks
with 2,080 reuses. The previous planner requested 92,570,828,800
bytes. The lifetime planner retains aliases through their final
consumer. The reference allocator used explicit child/view counts,
aligned best-fit blocks, splitting and coalescing, and conditional
in-place reuse. The new planner uses final-consumer indices and
whole-block reuse.

The completed real request recorded **32.071946 prefill tokens/s**
and **10.269880 decode steps/s** using the control's 511-step
denominator. It returned 512 output IDs and 512 probability rows.
The output repeats the prompt rather than answering the task. The
first difference is output index 1. The control predicts ID 198
with log probability -0.779868, while Goose picks ID 1716 with
log probability -0.00002718. The probability distributions already
differ before sampling. Correctness and speed acceptance remain open.

All 5,301,139 model pages were resident before and after the request.
Measured process high-water RSS was 23,883,316 KiB. The 494 samples
recorded a process VRAM maximum of 8,944 MiB, whole-device maximum of
11,752 MiB, and zero process swap. These figures belong to the
diagnostic candidate run. The tool captured no same-session control
memory high-water on the full 32K/512 request.

Raw evidence directory:
`/home/end0/AI/LlamaServer/franken-goose-apex-4096.v1C1cI`.
The inference executable SHA-256 is
`26027d635f8255e11e6de9f705da6300884a38bb75874a94c465b112532dfac3`.
The subdirectories `apex-functional.jl8ydH`, `mmq-profile.GQtAIU`
and `mmq-reference.J7xT6p` hold the full response, memory samples,
validation, and both traces with exported SQLite data.
