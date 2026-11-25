# Bonsai CUDA comparison with PrismML and ik_llama.cpp

## Evidence and scope

- **Goose:** the accepted EXP-2006 experimental HTTP executable
  `build/bonsai-row-reuse-2006-http/frankenmoe-infer`, with matching
  `backend.c`, `rows.cubin` and EXP-2000 frozen PTX. The executed model
  graph is the EXP-1999 Goose tail composition
  `build/bonsai-mmq-k64-2000-default/ffn-tail.goose`
  (`bonsai_graph`, `b_attention`, `b_gdn`, `b_ffn_finish`). The
  operation dispatch is `bonsai_launch` in the generated host, with
  original sources and rebuild recipes in EXP-1976 through EXP-2006.
- **PrismML:** local checkout `910442b0ce1128ef85d7730cbcae835cce60cdd4`.
  EXP-1977 measured the exact 32K/512 request, Q4_0 K/V, batch/ubatch
  4096/1024, context 36864 and MTP off. The saved environment has no
  optional `GGML_CUDA_*` override. Source dispatch is a conditional
  inference, since EXP-1977 has no kernel trace.
  [PrismML tree](https://github.com/PrismML-Eng/llama.cpp/tree/910442b0ce1128ef85d7730cbcae835cce60cdd4).
- **ik_llama.cpp:** source-only audit at
  `d6854c826c50b5a22ccf5e1e1aa5b2ceb4755576`, saved manifest in
  `data/source.sha256`. CUDA PQ2_0 support and dense Qwen35 build paths
  exist in source. No same-file load, dispatch trace, output parity,
  speed or memory measurement on this machine is established.
  [Pinned ik_llama.cpp tree](https://github.com/ikawrakow/ik_llama.cpp/tree/d6854c826c50b5a22ccf5e1e1aa5b2ceb4755576).
- **Measured comparison:** Goose EXP-2006 B 612.45 prefill and
  16.65 decode tok/s vs historical PrismML EXP-1977 1093.21 and
  47.97 tok/s. Same 512 IDs and UTF-8 text. Goose takes 53,502.89 ms
  prefill and 30,689.30 ms decode. PrismML takes 29,974.15 ms and
  10,653.27 ms. These are distinct benchmark blocks with different
  model placement for inactive MTP weights.
- **Budget:** EXP-2006's separate 32K/16 CUDA capture was on its
  EXP-2005 control, before the latest decode matvec change.
  The captured prefill uses the same K64 and Flash code subsequently
  retained by EXP-2006, except for the final one-token matvec.
  The decode bracket is 15 continuation evaluations. Percentages
  below are sensitivity calculations on this capture or end-to-end
  gaps. No PrismML or ik kernel time is inferred from a gap.

## Source and execution comparison

Paths under `ggml/src/ggml-cuda/` are given relative to each fork
unless stated otherwise.

| Priority / mechanism | PrismML source and eligibility | ik_llama.cpp source and eligibility | Active Goose path and actual difference | Measured time and savings budget |
| --- | --- | --- | --- | --- |
| P0 PQ2 prefill MMQ | `ggml-cuda.cu::ggml_cuda_mul_mat` → `mmq.cu::ggml_cuda_mul_mat_q` and `ggml_cuda_mul_mat_q_switch_type` → `mul_mat_q_case<PQ2_0,GGML_PREC_Q8>`. `quantize.cu::quantize_mmq_q8_1_cuda` packs D4 (four scale values per 128 activation entries). `mmq-load-tiles.cuh::ggml_cuda_mmq_load_tiles_pq2_0` expands codes to integer lanes; `mmq.cuh::mul_mat_q_case` selects shape and K scheduling. The distinct wgmma/J128 implementation has a Hopper-or-newer guard. | `mmq.cu::ggml_cuda_mul_mat_q_switch_type` → PQ2 case. `mmq.cuh::load_tiles_pq2_0` and `mmq_type_traits<...,GGML_TYPE_PQ2_0>` use D4 Q8_1 MMA or the DP4A dot implementation. CPU R8 repacking is a different layout and is outside this resident-GPU test. | `bonsai_graph` → `b_mat` → `bonsai_launch/MATRIX` → `bonsai_pack_half` / `bonsai_pq2_mmq_64x128_k64` for 1024-token batches. K64, async copies and ldmatrix are active. Existing experimental INT8 has **one** scale per 128, unlike D4; it is not enabled. Match packing, signed codes and scale epilogue before adapting the schedule. | **31,981 ms** of Goose prefill GPU time. Every 10% improvement on the entire bracket saves **3,198 ms** before added costs. Total measured cross-runtime prefill gap is **23,529 ms**, with unknown per-kernel attribution. |
| P0 PQ2 decode MMVQ | `mmvq.cu::ggml_cuda_mul_mat_vec_q` quantizes Q8_1 or reuses a prepacked activation. `vecdotq.cuh::vec_dot_pq2_0_q8_1` selects one of four 32-element chunks per PQ2 scale and performs integer dots. `mmvq-pq2_0.cuh::mul_mat_vec_pq2_0_mc` is a multi-column implementation. Ada single-column must be traced separately; the similarly named `ggml-cuda.cu::mul_mat_vec_pq2_0_ordered` specialization is Pascal-gated. | `mmvq.cu::ggml_cuda_op_mul_mat_vec_q_impl` → `iqk_mmvq.cu::iqk_mul_mat_vec_q` → `mul_mat_vec_pq2_0_q8_1_cuda`. A distinct backend uses the same PQ2/Q8_1 idea. | `bonsai_launch/MATRIX` on one token → `bonsai_pq2_matvec_rows2` when rows >1024, else ordered. EXP-2006 already reuses the activation across two output rows. Current decode uses FP32 input and ordered FP32 FMA, with no Q8_1 pack. | EXP-2005: **622.9 ms / 15 steps = 41.53 ms/token**. On that bracket 10% is **4.15 ms/token**, or **2,122 ms / 511** if sustained at the measured shape. This extrapolation predates EXP-2006; neither fork's kernel time is known. |
| P1 Flash prefill, native Q4 tile | `fattn.cu::ggml_cuda_get_best_fattn_kernel` and `ggml_cuda_flash_attn_ext_mma_f16` select the 256-wide Q4_0 instantiation when eligible. `fattn-mma-f16.cuh::flash_attn_ext_f16_load_tile_q` expands aligned pairs of blocks in the required shared layout. `ggml_cuda_fattn_prefill_f16` is an optional whole-cache conversion override, absent in EXP-1977. | `fattn.cu::ggml_cuda_flash_attn_ext` dispatches equal head shapes to MMA where eligible. The explicit head-dim-256 GQA 6/12 new-MMA branch has one query and does not explain batched prefill. | `b_attention` → `flash_f32` → `bonsai_flash_stage_kv` then `bonsai_flash_async_tile` / `bonsai_flash_compact_ordered_merge`. Compact 6:1, half operand staging, FP32 accumulation and async loads are **active**. Missing direct in-CTA Q4 load with the same current half bits, fragment layout and merge order. | Stage **131.5 ms**; tiles **12,566.5 ms**; merge **2,796.3 ms**. Removing only the stage caps its gross direct saving at **131.5 ms**. A 10% reduction of tiles would save **1,257 ms**, conditional on avoiding repeated dequant overhead. EXP-2007 rejects larger scratch as a substitute. |
| P1 Flash decode, 6:1 | `fattn.cu::ggml_cuda_get_best_fattn_kernel`: Ada with Q4 and ratio >4 can reach in-place MMA if cache strides, mask, and shape gates pass. Otherwise vector path. | `fattn.cu::ggml_cuda_flash_attn_ext` → `fattn-new-mma.cu::ggml_cuda_flash_attn_ext_mma_new` for the 256-wide 6:1/12:1 single-query branch when supported. | `bonsai_decode_gqa2/3` and `goose_q4_attention_merge` are **active**. Shared Q4 loads are already present. Direct tensor-core decode would change the current ordered QK/PV and softmax arithmetic unless adapted or separately qualified. | **120.5 ms / 15 = 8.04 ms/token** after grouped GQA. 10% would save **0.80 ms/token**, about **411 ms / 511** at the traced shape. Prior GQA improvement is already included. |
| P2 GDN and convolution | `gated_delta_net.cu::launch_gated_delta_net` selects recurrent path with optional L8/L8P. L8 opt-in; the prefill opt default in `common.cuh::ggml_cuda_prefill_opt` targets Hopper, so do not attribute EXP-1977 to this variant. `ggml-cuda.cu::ggml_cuda_try_gdn_conv_fused` → `ssm-conv.cu::ssm_conv_gdn_fused_f32` for 1–16 tokens, plus separate gather/state-copy matches. | `src/graphs/build_qwen35.cpp::llm_build_context::build_qwen35` → `src/llama-delta-net.cpp::delta_net::build_fused_delta_net` → `delta-net.cu::delta_net_f32_cuda`; recurrent at ≤8 and tiled F32 at >8. `ggml/src/ggml-cuda.cu` handles `GGML_OP_DELTA_NET` and can write into the state-copy destination. | `b_gdn` → `bonsai_conv_batch`, `bonsai_decay_reuse8`, `goose_gdn_mma_v32` on large prefill. Decode uses `bonsai_conv_step` and `goose_gdn_decode`; recurrent buffers exchange ownership directly, so skip a port of state-copy removal. Conv→SiLU and alpha/beta preparation already fused in their own kernels. Test a Bonsai-scoped conv/gate consumer only after state order and chunk fixtures pass. | Prefill GDN **1,396.4 ms**, conv **569.2 ms**, decay **625.3 ms**. These are separate groups, not additive savings. Decode conv **1.28 ms / 15** and GDN **7.05 ms / 15**, so the Prism short fusion cannot explain tens of seconds. |
| P2 Fusion, residual/norm/rotation and activation packing | `ggml-cuda.cu::ggml_cuda_try_fwht_q8`, `ggml_cuda_norm_fwht_q8_prepare`, `norm.cu::ggml_cuda_op_add_rms_norm_fwht_fused` can fuse residual, norm, Hadamard and optional pack when aliases and consumers permit. `ggml_cuda_try_glu_fwht_mmq` targets the tail with D4 pack; default prefill feature policy is Hopper. The `ggml_cuda_mul_mat_q_fused_two` call site for post-norm gate/up is DGX Spark guarded. | The audited delta-net copy fusion is confirmed. The fetched dispatch does not establish the corresponding fused Bonsai Hadamard/pack path on RTX 4070. Do not infer a fast path from the format support. | `bonsai_graph` currently launches `bonsai_residual`, `bonsai_norm`, `bonsai_fwht1024` separately for FFN input, then shares `packed_source` across gate/up. `b_ffn_finish` already uses fused `bonsai_ffn_tail` for count ≥32. EXP-1999 rejected joint gate/up MMQ. Transfer the **prefix** fusion with exact FP32 norm reduction/FWHT order and the same half pack, preserving the residual buffer for its later consumer. | Prefill totals are residual **496.0 ms**, norm **853.7 ms**, FWHT **567.4 ms**, half pack **224.2 ms**, across all relevant and unrelated callers. Sum **2,141.3 ms** is an upper bound on those entire categories, not a prefix fusion forecast. |

## Three candidates for paired trials

1. **PQ2 MMQ, D4 Q8_1 activation contract and Ada schedule.** First
   recreate PrismML's four scales, signed code mapping, and scale
   application with a Bonsai-scoped Goose dispatch plus CUDA/PTX
   kernels. Start with layer 0 and 42 gate, up and down on the
   common packed operands, at 257/1023/1024 tokens. Compare prepacked
   bytes and output to the fork numeric oracle, and compare timing
   including pack with EXP-2006. Preserve FP16 in all unqualified
   projections. EXP-2009's selective-Q8 pilot preserves observed
   choices and task scores but changes logits and has one unpaired
   timing per mode. The exact fallback stays active until the
   candidate passes the full numerical and quality gate.
2. **PQ2 MMVQ, per-32 packed activation and DP4A.** Start with one
   column on the RTX 4070. Port the operand layout and inner dot
   to the existing Bonsai matvec ABI. Time gate/up, down, QKV and
   vocabulary, including pack and reuse across shared consumers.
   Keep the EXP-2006 rows2 path as control. Validate the same-history
   logits and downstream state before testing the full request.
   Small multi-column variants are a separate experiment. Fusion
   with FWHT becomes relevant after the unfused pack has a qualified
   arithmetic contract.
3. **Native Q4-into-shared Flash prefill.** Reuse the compact 6:1
   layout, Q conversion, MMA sequence and ordered merge from
   EXP-2006. Replace only the staged source loads with a tile-local
   aligned Q4 decoder preserving the same half bits. Compare
   decoded fragments, tile partials and final outputs bitwise at
   4K/8K/32K, plus causal and row tails. Score the entire stage,
   tiles and merge together. Preserve the existing staged variant
   when direct unpack increases time.

For each candidate, run two order-balanced pairs on the same RTX 4070,
fixed binary/images, GGUF, input IDs, Q4_0, batch/ubatch, CPU settings and
model state. Record full prefill, decode, client time, peak process VRAM,
RSS and swap. Exact paths must retain byte identities at the component
boundary and 512 IDs/text on the full request. For an arithmetic change,
also use fixed-history full logits, top-token decisions, multi-prompt
free generation including long responses, and independently graded
complex tasks. A failed correctness gate blocks promotion regardless
of speed. Scope dispatch to the Bonsai model contract, preserve existing
Apex/Inkling CUDA paths and verify their own complete regression gates
before promoting a shared binary. Build native Goose/C host plus
CUDA images with the existing toolchain. The fork C++ code is a reference.

### Source anchors

- PrismML [MMQ dispatch](https://github.com/PrismML-Eng/llama.cpp/blob/910442b0ce1128ef85d7730cbcae835cce60cdd4/ggml/src/ggml-cuda/mmq.cu), [MMQ schedule](https://github.com/PrismML-Eng/llama.cpp/blob/910442b0ce1128ef85d7730cbcae835cce60cdd4/ggml/src/ggml-cuda/mmq.cuh), [MMVQ](https://github.com/PrismML-Eng/llama.cpp/blob/910442b0ce1128ef85d7730cbcae835cce60cdd4/ggml/src/ggml-cuda/mmvq.cu), [Flash loader](https://github.com/PrismML-Eng/llama.cpp/blob/910442b0ce1128ef85d7730cbcae835cce60cdd4/ggml/src/ggml-cuda/fattn-mma-f16.cuh), [GDN](https://github.com/PrismML-Eng/llama.cpp/blob/910442b0ce1128ef85d7730cbcae835cce60cdd4/ggml/src/ggml-cuda/gated_delta_net.cu), [fusion dispatch](https://github.com/PrismML-Eng/llama.cpp/blob/910442b0ce1128ef85d7730cbcae835cce60cdd4/ggml/src/ggml-cuda/ggml-cuda.cu).
- ik_llama.cpp [MMQ](https://github.com/ikawrakow/ik_llama.cpp/blob/d6854c826c50b5a22ccf5e1e1aa5b2ceb4755576/ggml/src/ggml-cuda/mmq.cuh#L2248), [MMVQ](https://github.com/ikawrakow/ik_llama.cpp/blob/d6854c826c50b5a22ccf5e1e1aa5b2ceb4755576/ggml/src/ggml-cuda/iqk_mmvq.cu#L10), [Flash dispatch](https://github.com/ikawrakow/ik_llama.cpp/blob/d6854c826c50b5a22ccf5e1e1aa5b2ceb4755576/ggml/src/ggml-cuda/fattn.cu#L46), [DeltaNet](https://github.com/ikawrakow/ik_llama.cpp/blob/d6854c826c50b5a22ccf5e1e1aa5b2ceb4755576/ggml/src/ggml-cuda/delta-net.cu#L413).
- Goose [EXP-1977 reference](../EXP-2026-11-25-1977-bonsai-prism-32k-reference/README.md), [EXP-2006 GPU attribution](../EXP-2026-11-25-2006-bonsai-matvec-row-reuse/data/profile-exp2005.json), [EXP-2007 wave experiment](../EXP-2026-11-25-2007-bonsai-flash-warp-merge/README.md), [EXP-2009 quality evidence](README.md).
