# Optimization applicability matrix

The catalog contains 340 static entries and 23 methods (7,820 combinations). [Download the filterable workbook](optimization-matrix.xlsx).

**Confirmed** links to a measured experiment for the specified shape. **Candidates** are name-based prompts for source review and measurement. Combinations absent from both columns remain unassessed.

The catalog observes build and dispatch declarations. A static entry needs a runtime trace to establish actual use, geometry and model coverage. Shared symbols can serve multiple models.

## Attention and KV

| Backend | Model family | Entry | Confirmed | Candidates | Source |
| --- | --- | --- | --- | --- | --- |
| CPU | Inkling 2 Mini | `inkling_row_major_i2_attention_decode` | — | DOUBLE, EXP-2424, EXP-2426, EXP-2431, SHARED | [numeric.goose](../../integration/goose-standalone/universal/inkling/numeric.goose) |
| CPU | Inkling 2 Mini | `inkling_row_major_i2_attention_token` | — | DOUBLE, EXP-2424, EXP-2426, EXP-2431, SHARED | [numeric.goose](../../integration/goose-standalone/universal/inkling/numeric.goose) |
| CPU | Inkling 2 Mini | `inkling_row_major_i2_store_kv` | — | FUSE | [numeric.goose](../../integration/goose-standalone/universal/inkling/numeric.goose) |
| CPU | Shared | `flash_attention` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [cpu.goose](../../integration/goose-standalone/universal/cpu/cpu.goose) |
| CUDA | Inkling 2 Mini | `goose_i2_attention_gate` | — | DOUBLE, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED | [attention-gate-image-entries.json](../../integration/goose-standalone/universal/cuda/images/attention-gate-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_attention_merge` | — | DOUBLE, EXP-2424, EXP-2426, EXP-2431, SHARED | [attention-image-entries.json](../../integration/goose-standalone/universal/cuda/images/attention-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_attention_score` | — | DOUBLE, EXP-2424, EXP-2426, EXP-2431, SHARED | [attention-image-entries.json](../../integration/goose-standalone/universal/cuda/images/attention-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_attention_softmax` | — | DOUBLE, EXP-2424, EXP-2426, EXP-2431, REG, SHARED | [attention-image-entries.json](../../integration/goose-standalone/universal/cuda/images/attention-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_attention_value` | — | DOUBLE, EXP-2424, EXP-2426, EXP-2431, SHARED | [attention-image-entries.json](../../integration/goose-standalone/universal/cuda/images/attention-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_flash_mma` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [flash-native-image-entries.json](../../integration/goose-standalone/universal/cuda/images/flash-native-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_flash_mma_decode` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [flash-native-image-entries.json](../../integration/goose-standalone/universal/cuda/images/flash-native-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_flash_mma_merge` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [flash-native-image-entries.json](../../integration/goose-standalone/universal/cuda/images/flash-native-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_flash_mma_prefill` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [flash-native-image-entries.json](../../integration/goose-standalone/universal/cuda/images/flash-native-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_flash_mma_prefill_group` | [EXP-2426](../../experiments/EXP-2026-11-28-2426-inkling-sliding-prefill-group/README.md), [EXP-2431](../../experiments/EXP-2026-11-28-2431-inkling-sliding-prefill-packed/README.md) | DOUBLE, EXP-1603, EXP-1609, EXP-2424, FUSE, SHARED, TRIPLE | [flash-native-image-entries.json](../../integration/goose-standalone/universal/cuda/images/flash-native-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_flash_mma_verify` | [EXP-2424](../../experiments/EXP-2026-11-28-2424-inkling-batched-verifier-attention/README.md) | DOUBLE, EXP-1603, EXP-1609, EXP-2426, EXP-2430, EXP-2431, EXP-2443, FUSE, SHARED, TRIPLE | [flash-native-image-entries.json](../../integration/goose-standalone/universal/cuda/images/flash-native-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_matrix_q8_attention_projections` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2423, EXP-2424, EXP-2426, EXP-2431, EXP-2437, EXP-2439, EXP-2443, FUSE, REG, SHARED, TRIPLE | [matrix-image-entries.json](../../integration/goose-standalone/universal/cuda/images/matrix-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_prepare_attention` | — | DOUBLE, EXP-2424, EXP-2426, EXP-2431, SHARED | [attention-input-image-entries.json](../../integration/goose-standalone/universal/cuda/images/attention-input-image-entries.json) |
| CUDA | Inkling 2 Mini | `i2_flash_prefill_all_softmax` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, REG, SHARED, TRIPLE | [flash-native-image-entries.json](../../integration/goose-standalone/universal/cuda/images/flash-native-image-entries.json) |
| CUDA | Shared | `goose_flash_causal_mma` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [cuda-flash-mma-entries.json](../../integration/goose-standalone/cuda-flash-mma-entries.json) |
| CUDA | Shared | `goose_flash_fixup_general` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [cuda-flash-mma-entries.json](../../integration/goose-standalone/cuda-flash-mma-entries.json) |
| CUDA | Shared | `goose_flash_fixup_uniform` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [cuda-flash-mma-entries.json](../../integration/goose-standalone/cuda-flash-mma-entries.json) |
| CUDA | Shared | `goose_flash_q4_stage` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [cuda-flash-mma-entries.json](../../integration/goose-standalone/cuda-flash-mma-entries.json) |
| CUDA | Shared | `goose_flash_stream_mma` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [cuda-flash-mma-entries.json](../../integration/goose-standalone/cuda-flash-mma-entries.json) |
| CUDA | Shared | `goose_flash_vec_combine` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [cuda-flash-vec-entries.json](../../integration/goose-standalone/cuda-flash-vec-entries.json) |
| CUDA | Shared | `goose_flash_vec_pack_query` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [cuda-flash-vec-entries.json](../../integration/goose-standalone/cuda-flash-vec-entries.json) |
| CUDA | Shared | `goose_flash_vec_q4` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [cuda-flash-vec-entries.json](../../integration/goose-standalone/cuda-flash-vec-entries.json) |
| CUDA | Shared | `goose_flash_vec_q4_packed` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [cuda-flash-vec-entries.json](../../integration/goose-standalone/cuda-flash-vec-entries.json) |
| CUDA | Shared | `goose_kv_fwht` | — | FUSE | [cuda-fwht-entries.json](../../integration/goose-standalone/cuda-fwht-entries.json) |
| CUDA | Shared | `goose_kv_fwht_256` | — | FUSE | [cuda-fwht-entries.json](../../integration/goose-standalone/cuda-fwht-entries.json) |
| CUDA | Shared | `goose_kv_fwht_64` | — | FUSE | [cuda-fwht-entries.json](../../integration/goose-standalone/cuda-fwht-entries.json) |
| CUDA | Shared | `goose_mla_attention_batch_merge` | — | DOUBLE, EXP-2424, EXP-2426, EXP-2431, SHARED | [cuda-attention-entries.json](../../integration/goose-standalone/cuda-attention-entries.json) |
| CUDA | Shared | `goose_mla_attention_batch_pair` | — | DOUBLE, EXP-2424, EXP-2426, EXP-2431, SHARED | [cuda-attention-entries.json](../../integration/goose-standalone/cuda-attention-entries.json) |
| CUDA | Shared | `goose_mla_attention_merge` | — | DOUBLE, EXP-2424, EXP-2426, EXP-2431, SHARED | [cuda-attention-entries.json](../../integration/goose-standalone/cuda-attention-entries.json) |
| CUDA | Shared | `goose_mla_attention_pair` | — | DOUBLE, EXP-2424, EXP-2426, EXP-2431, SHARED | [cuda-attention-entries.json](../../integration/goose-standalone/cuda-attention-entries.json) |
| CUDA | Shared | `goose_mla_attention_tiles` | — | DOUBLE, EXP-2424, EXP-2426, EXP-2431, SHARED | [cuda-attention-entries.json](../../integration/goose-standalone/cuda-attention-entries.json) |
| CUDA | Shared | `goose_q4_attention_merge` | — | DOUBLE, EXP-2424, EXP-2426, EXP-2431, SHARED | [cuda-attention-entries.json](../../integration/goose-standalone/cuda-attention-entries.json) |
| CUDA | Shared | `goose_q4_attention_tiles` | — | DOUBLE, EXP-2424, EXP-2426, EXP-2431, SHARED | [cuda-attention-entries.json](../../integration/goose-standalone/cuda-attention-entries.json) |
| CUDA | Ternary Bonsai | `bonsai_attention_gate` | — | DOUBLE, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_flash_async_tile` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_flash_compact_merge` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_flash_compact_ordered_merge` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_flash_compact_tile` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_flash_f32_merge` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_flash_f32_tile` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_flash_fp8_qk` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_flash_fp8_qkpv` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_flash_kv_fused_ordered` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_flash_kv_lean2` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_flash_ordered_merge` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_flash_stage_k_fp8` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_flash_stage_kv` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_flash_stage_v_fp8` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_flash_stage_v_only` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_flash_staged_tile` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_flash_tile_group2` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_flash_tile_group4` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `goose_flash_vec_q4_packed_pair` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [cuda-flash-vec-bonsai-entries.json](../../integration/goose-standalone/cuda-flash-vec-bonsai-entries.json) |
| CUDA | Ternary Bonsai | `goose_flash_vec_q4_packed_quad` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2424, EXP-2426, EXP-2431, FUSE, SHARED, TRIPLE | [cuda-flash-vec-bonsai-entries.json](../../integration/goose-standalone/cuda-flash-vec-bonsai-entries.json) |
| CUDA | Ternary Bonsai | `goose_q4_attention_merge` | — | DOUBLE, EXP-2424, EXP-2426, EXP-2431, SHARED | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `goose_q4_attention_tiles` | — | DOUBLE, EXP-2424, EXP-2426, EXP-2431, SHARED | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| Vulkan | Shared | `attention` | — | DOUBLE, EXP-2424, EXP-2426, EXP-2431, SHARED | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `attention_q4` | — | DOUBLE, EXP-2424, EXP-2426, EXP-2431, SHARED | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `inkling_attention` | — | DOUBLE, EXP-2424, EXP-2426, EXP-2431, SHARED | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `kv_fwht` | — | FUSE | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `kv_store` | — | FUSE | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `kv_store_q4` | — | FUSE | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |

## Epilogues and pointwise

| Backend | Model family | Entry | Confirmed | Candidates | Source |
| --- | --- | --- | --- | --- | --- |
| CPU | Inkling 2 Mini | `inkling_row_major_i2_situ` | — | — | [numeric.goose](../../integration/goose-standalone/universal/inkling/numeric.goose) |
| CUDA | Inkling 2 Mini | `goose_tanh_f32` | — | — | [tanh-image-entries.json](../../integration/goose-standalone/universal/cuda/images/tanh-image-entries.json) |
| CUDA | Shared | `goose_gated_delta_net` | — | FUSE | [cuda-activation-entries.json](../../integration/goose-standalone/cuda-activation-entries.json) |
| CUDA | Shared | `goose_vector_flat` | — | EXP-1603 | [cuda-activation-entries.json](../../integration/goose-standalone/cuda-activation-entries.json) |
| CUDA | Shared | `goose_vector_flat4` | — | EXP-1603 | [cuda-activation-entries.json](../../integration/goose-standalone/cuda-activation-entries.json) |
| CUDA | Shared | `goose_vector_ops` | — | EXP-1603 | [cuda-activation-entries.json](../../integration/goose-standalone/cuda-activation-entries.json) |
| CUDA | Ternary Bonsai | `bonsai_residual` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_split_q_gate` | — | FUSE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_swiglu` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `goose_gated_delta_net` | — | FUSE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| Vulkan | Shared | `add` | — | — | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `bias` | — | — | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `gated_rms` | — | FUSE, SHARED | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `inkling_situ` | — | — | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `sigmoid_gate` | — | FUSE | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `sigmoid_gate_scalar` | — | FUSE | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `swiglu` | — | — | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |

## Gated DeltaNet and SSM

| Backend | Model family | Entry | Confirmed | Candidates | Source |
| --- | --- | --- | --- | --- | --- |
| CPU | Inkling 2 Mini | `inkling_row_major_i2_postconv_batch` | — | — | [numeric.goose](../../integration/goose-standalone/universal/inkling/numeric.goose) |
| CPU | Inkling 2 Mini | `inkling_row_major_i2_postconv_token` | — | — | [numeric.goose](../../integration/goose-standalone/universal/inkling/numeric.goose) |
| CPU | Inkling 2 Mini | `inkling_row_major_i2_shortconv_decode` | — | — | [numeric.goose](../../integration/goose-standalone/universal/inkling/numeric.goose) |
| CUDA | Inkling 2 Mini | `i2_postconv_commit` | — | — | [i2-parallel-conv-image-entries.json](../../integration/goose-standalone/universal/cuda/images/i2-parallel-conv-image-entries.json) |
| CUDA | Inkling 2 Mini | `i2_postconv_parallel` | — | — | [i2-parallel-conv-image-entries.json](../../integration/goose-standalone/universal/cuda/images/i2-parallel-conv-image-entries.json) |
| CUDA | Shared | `goose_gdn_decode` | — | DOUBLE, EXP-1609, EXP-1719, FUSE, REG, SHARED | [cuda-activation-entries.json](../../integration/goose-standalone/cuda-activation-entries.json) |
| CUDA | Shared | `goose_gdn_mma` | — | DOUBLE, EXP-1609, EXP-1719, FUSE, REG, SHARED | [cuda-gdn-mma-entries.json](../../integration/goose-standalone/cuda-gdn-mma-entries.json) |
| CUDA | Shared | `goose_gdn_mma_v32` | — | DOUBLE, EXP-1609, EXP-1719, FUSE, REG, SHARED | [cuda-gdn-mma-entries.json](../../integration/goose-standalone/cuda-gdn-mma-entries.json) |
| CUDA | Shared | `goose_ssm_conv` | — | EXP-1719 | [cuda-activation-entries.json](../../integration/goose-standalone/cuda-activation-entries.json) |
| CUDA | Shared | `goose_ssm_conv_decode` | — | EXP-1719 | [cuda-activation-entries.json](../../integration/goose-standalone/cuda-activation-entries.json) |
| CUDA | Shared | `goose_ssm_conv_direct` | — | EXP-1719 | [cuda-activation-entries.json](../../integration/goose-standalone/cuda-activation-entries.json) |
| CUDA | Shared | `goose_ssm_conv_long` | — | EXP-1719 | [cuda-activation-entries.json](../../integration/goose-standalone/cuda-activation-entries.json) |
| CUDA | Shared | `goose_ssm_conv_save_tail` | — | EXP-1719 | [cuda-activation-entries.json](../../integration/goose-standalone/cuda-activation-entries.json) |
| CUDA | Ternary Bonsai | `bonsai_conv_batch` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_conv_step` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_conv_verify_prefix` | — | EXP-2424, EXP-2430, EXP-2443 | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_conv_verify_prefix_five` | — | EXP-2424, EXP-2430, EXP-2443 | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_conv_verify_prefix_four` | — | EXP-2424, EXP-2430, EXP-2443 | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_decay` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_decay_reuse4` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_decay_reuse8` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_decay_stage_256_0` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_decay_warp_pack` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `goose_gdn_decode` | — | DOUBLE, EXP-1609, EXP-1719, FUSE, REG, SHARED | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `goose_gdn_mma_v32` | — | DOUBLE, EXP-1609, EXP-1719, FUSE, REG, SHARED | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| Vulkan | Shared | `causal_conv` | — | — | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `gdn` | — | DOUBLE, EXP-1609, EXP-1719, FUSE, REG, SHARED | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `gdn_apply` | — | DOUBLE, EXP-1609, EXP-1719, FUSE, REG, SHARED | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `gdn_gates` | — | DOUBLE, EXP-1609, EXP-1719, FUSE, REG, SHARED | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |

## Matrix products

| Backend | Model family | Entry | Confirmed | Candidates | Source |
| --- | --- | --- | --- | --- | --- |
| CUDA | Inkling 2 Mini | `goose_i2_dense_split_inputs` | — | EXP-2423, EXP-2437, EXP-2439 | [grouped-moe-image-entries.json](../../integration/goose-standalone/universal/cuda/images/grouped-moe-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_dense_split_mma` | [EXP-2437](../../experiments/EXP-2026-11-28-2437-inkling-q8-split-half-mma/README.md) | EXP-2423, EXP-2439 | [grouped-moe-image-entries.json](../../integration/goose-standalone/universal/cuda/images/grouped-moe-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_dense_split_mma_shared` | [EXP-2439](../../experiments/EXP-2026-11-28-2439-inkling-q8-shared-tiles/README.md) | EXP-2423, EXP-2437 | [grouped-moe-image-entries.json](../../integration/goose-standalone/universal/cuda/images/grouped-moe-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_dense_split_weights` | — | EXP-2423, EXP-2437, EXP-2439 | [grouped-moe-image-entries.json](../../integration/goose-standalone/universal/cuda/images/grouped-moe-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_matrix_iq4_nl` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2423, EXP-2429, EXP-2437, EXP-2439, EXP-2441, EXP-2442, FUSE, REG, SHARED, TRIPLE | [matrix-image-entries.json](../../integration/goose-standalone/universal/cuda/images/matrix-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_matrix_iq4_nl_pair` | [EXP-2441](../../experiments/EXP-2026-11-28-2441-inkling-iq4-nl-verifier-pairs/README.md) | DOUBLE, EXP-1603, EXP-1609, EXP-2423, EXP-2429, EXP-2437, EXP-2439, EXP-2442, FUSE, REG, SHARED, TRIPLE | [matrix-image-entries.json](../../integration/goose-standalone/universal/cuda/images/matrix-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_matrix_iq4_xs` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2423, EXP-2427, EXP-2429, EXP-2437, EXP-2439, EXP-2442, FUSE, REG, SHARED, TRIPLE | [matrix-image-entries.json](../../integration/goose-standalone/universal/cuda/images/matrix-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_matrix_iq4_xs_blocks` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2423, EXP-2427, EXP-2429, EXP-2437, EXP-2439, EXP-2442, FUSE, REG, SHARED, TRIPLE | [matrix-image-entries.json](../../integration/goose-standalone/universal/cuda/images/matrix-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_matrix_iq4_xs_pair` | [EXP-2427](../../experiments/EXP-2026-11-28-2427-inkling-iq4-verifier-pairs/README.md) | DOUBLE, EXP-1603, EXP-1609, EXP-2423, EXP-2429, EXP-2437, EXP-2439, EXP-2442, FUSE, REG, SHARED, TRIPLE | [matrix-image-entries.json](../../integration/goose-standalone/universal/cuda/images/matrix-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_matrix_q8_0` | — | DOUBLE, EXP-1603, EXP-1609, EXP-2423, EXP-2437, EXP-2439, EXP-2443, FUSE, REG, SHARED, TRIPLE | [matrix-image-entries.json](../../integration/goose-standalone/universal/cuda/images/matrix-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_nl_mmq` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2441, EXP-2442, REG, SHARED, TRIPLE | [grouped-moe-image-entries.json](../../integration/goose-standalone/universal/cuda/images/grouped-moe-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_nl_mmq16` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2441, EXP-2442, REG, SHARED, TRIPLE | [grouped-moe-image-entries.json](../../integration/goose-standalone/universal/cuda/images/grouped-moe-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_nl_mmq32` | [EXP-2429](../../experiments/EXP-2026-11-28-2429-inkling-moe-pairload/README.md) | DOUBLE, EXP-1603, EXP-2438, EXP-2439, EXP-2441, EXP-2442, REG, SHARED, TRIPLE | [grouped-moe-image-entries.json](../../integration/goose-standalone/universal/cuda/images/grouped-moe-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_nl_mmq32_shared` | [EXP-2442](../../experiments/EXP-2026-11-28-2442-inkling-iq4-shared-tiles/README.md) | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2441, REG, SHARED, TRIPLE | [grouped-moe-image-entries.json](../../integration/goose-standalone/universal/cuda/images/grouped-moe-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_xs_mmq` | — | DOUBLE, EXP-1603, EXP-2427, EXP-2429, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [grouped-moe-image-entries.json](../../integration/goose-standalone/universal/cuda/images/grouped-moe-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_xs_mmq16` | — | DOUBLE, EXP-1603, EXP-2427, EXP-2429, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [grouped-moe-image-entries.json](../../integration/goose-standalone/universal/cuda/images/grouped-moe-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_xs_mmq32` | [EXP-2429](../../experiments/EXP-2026-11-28-2429-inkling-moe-pairload/README.md) | DOUBLE, EXP-1603, EXP-2427, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [grouped-moe-image-entries.json](../../integration/goose-standalone/universal/cuda/images/grouped-moe-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_xs_mmq32_shared` | [EXP-2442](../../experiments/EXP-2026-11-28-2442-inkling-iq4-shared-tiles/README.md) | DOUBLE, EXP-1603, EXP-2427, EXP-2429, EXP-2438, EXP-2439, REG, SHARED, TRIPLE | [grouped-moe-image-entries.json](../../integration/goose-standalone/universal/cuda/images/grouped-moe-image-entries.json) |
| CUDA | Shared | `goose_dense_mma` | — | EXP-2423, EXP-2437, EXP-2439 | [cuda-dense-mma-entries.json](../../integration/goose-standalone/cuda-dense-mma-entries.json) |
| CUDA | Shared | `goose_dense_mma_reference` | — | EXP-2423, EXP-2437, EXP-2439 | [cuda-dense-mma-entries.json](../../integration/goose-standalone/cuda-dense-mma-entries.json) |
| CUDA | Shared | `goose_mmq_matmul` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_pack_k_major` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_pack_q8` | — | DOUBLE, EXP-1603, EXP-2423, EXP-2429, EXP-2437, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_pack_shared` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_pack_swiglu` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_q4_dense_m128` | — | DOUBLE, EXP-1603, EXP-2423, EXP-2429, EXP-2437, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_q4_dense_m128n256` | — | DOUBLE, EXP-1603, EXP-2423, EXP-2429, EXP-2437, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_q4_dense_m128n256_pipeline` | — | DOUBLE, EXP-1603, EXP-2423, EXP-2429, EXP-2437, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_q4_dense_m128n256_prepared` | — | DOUBLE, EXP-1603, EXP-2423, EXP-2429, EXP-2437, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_q4_dense_m256n128` | — | DOUBLE, EXP-1603, EXP-2423, EXP-2429, EXP-2437, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_q4_dense_m64` | — | DOUBLE, EXP-1603, EXP-2423, EXP-2429, EXP-2437, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_q4_dense_shared` | — | DOUBLE, EXP-1603, EXP-2423, EXP-2429, EXP-2437, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_q4_dense_x128` | — | DOUBLE, EXP-1603, EXP-2423, EXP-2429, EXP-2437, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_q4_dense_x64` | — | DOUBLE, EXP-1603, EXP-2423, EXP-2429, EXP-2437, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_q4_prepare` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_q4_reference` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_q6_dense_m128_half_k` | — | DOUBLE, EXP-1603, EXP-2423, EXP-2429, EXP-2437, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_q6_dense_m128_pipeline` | — | DOUBLE, EXP-1603, EXP-2423, EXP-2429, EXP-2437, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_q6_dense_shared` | — | DOUBLE, EXP-1603, EXP-2423, EXP-2429, EXP-2437, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_q6_dense_x128` | — | DOUBLE, EXP-1603, EXP-2423, EXP-2429, EXP-2437, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_q6_dense_x64` | — | DOUBLE, EXP-1603, EXP-2423, EXP-2429, EXP-2437, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_q6_reference` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_stream` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_stream_fixup` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmvq_matvec` | — | — | [cuda-mmvq-entries.json](../../integration/goose-standalone/cuda-mmvq-entries.json) |
| CUDA | Shared | `goose_mmvq_quantize` | — | — | [cuda-mmvq-entries.json](../../integration/goose-standalone/cuda-mmvq-entries.json) |
| CUDA | Shared | `goose_mmvq_shared_up_gate_glu` | — | FUSE | [cuda-up-gate-entries.json](../../integration/goose-standalone/cuda-up-gate-entries.json) |
| CUDA | Shared | `goose_mmvq_up_gate_glu` | — | FUSE | [cuda-up-gate-entries.json](../../integration/goose-standalone/cuda-up-gate-entries.json) |
| CUDA | Shared | `goose_quantized_matmul` | — | — | [cuda-activation-entries.json](../../integration/goose-standalone/cuda-activation-entries.json) |
| CUDA | Ternary Bonsai | `bonsai_pq2_mmq` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pq2_mmq_64x128_k64` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pq2_mmq_64x64` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pq2_mmq_64x64_k64` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pq2_mmq_async` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pq2_mmq_int8` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pq2_mmq_int8_down_n128_spread` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pq2_mmq_int8_full_n128` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pq2_mmq_int8_full_n128_pairload` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pq2_mmq_int8_full_n128_panel` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pq2_mmq_int8_full_n128_spread` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, REG, SHARED, TRIPLE | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `goose_mmvq_dense_matvec` | — | EXP-2423, EXP-2437, EXP-2439 | [cuda-mmvq-bonsai-entries.json](../../integration/goose-standalone/cuda-mmvq-bonsai-entries.json) |
| CUDA | Ternary Bonsai | `goose_mmvq_dense_quantize` | — | EXP-2423, EXP-2437, EXP-2439 | [cuda-mmvq-bonsai-entries.json](../../integration/goose-standalone/cuda-mmvq-bonsai-entries.json) |
| CUDA | Ternary Bonsai | `goose_mmvq_short_rows` | — | — | [cuda-mmvq-bonsai-entries.json](../../integration/goose-standalone/cuda-mmvq-bonsai-entries.json) |
| Vulkan | Shared | `f32_project` | — | — | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `iq4_nl` | — | EXP-2429, EXP-2441, EXP-2442 | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `iq4_xs` | — | EXP-2427, EXP-2429, EXP-2442 | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `pq2_0` | — | — | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `q4_0` | — | — | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `q4_k` | — | — | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `q6_k` | — | — | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `q8_0` | — | EXP-2423, EXP-2437, EXP-2439 | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |

## MoE and routing

| Backend | Model family | Entry | Confirmed | Candidates | Source |
| --- | --- | --- | --- | --- | --- |
| CUDA | Apex 35B | `goose_moe_shared_residual_prefill` | — | EXP-2438, FUSE | [cuda-apex-epilogue-entries.json](../../integration/goose-standalone/cuda-apex-epilogue-entries.json) |
| CUDA | Apex 35B | `goose_pred_route` | — | FUSE | [apex-prediction-entries.json](../../integration/goose-standalone/apex-prediction-entries.json) |
| CUDA | Apex 35B | `goose_route_compact8` | — | FUSE | [apex-mailbox-entries.json](../../integration/goose-standalone/apex-mailbox-entries.json) · [apex-route-entries.json](../../integration/goose-standalone/apex-route-entries.json) |
| CUDA | Apex 35B | `goose_router_softmax_fixed` | — | FUSE, REG | [apex-softmax-entries.json](../../integration/goose-standalone/apex-softmax-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_moe_epilogue` | — | EXP-2438, FUSE | [i2-probe-moe-image-entries.json](../../integration/goose-standalone/universal/cuda/images/i2-probe-moe-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_moe_pack` | — | EXP-2438, FUSE | [grouped-moe-image-entries.json](../../integration/goose-standalone/universal/cuda/images/grouped-moe-image-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_moe_situ` | — | EXP-2438, FUSE | [situ-image-entries.json](../../integration/goose-standalone/universal/cuda/images/situ-image-entries.json) |
| CUDA | Inkling 2 Mini | `launch_expert_overlap` | [EXP-2438](../../experiments/EXP-2026-11-28-2438-inkling-verifier-expert-overlap/README.md) | EXP-2427, EXP-2441, EXP-2442 | [driver.goose](../../integration/goose-standalone/universal/cuda/driver.goose) |
| CUDA | Shared | `goose_mmq_q4_routed_down_m128` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, FUSE, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_q4_routed_m128` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, FUSE, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_q4_routed_shared` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, FUSE, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_q4_routed_x128` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, FUSE, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_q4_routed_x64` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, FUSE, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_q6_routed_m128_pipeline` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, FUSE, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_q6_routed_shared` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, FUSE, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_q6_routed_x128` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, FUSE, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_mmq_q6_routed_x64` | — | DOUBLE, EXP-1603, EXP-2429, EXP-2438, EXP-2439, EXP-2442, FUSE, REG, SHARED, TRIPLE | [cuda-mmq-entries.json](../../integration/goose-standalone/cuda-mmq-entries.json) |
| CUDA | Shared | `goose_moe_add_shared_residual` | — | EXP-2438, FUSE | [cuda-activation-entries.json](../../integration/goose-standalone/cuda-activation-entries.json) |
| CUDA | Shared | `goose_moe_weighted` | — | EXP-2438, FUSE | [cuda-activation-entries.json](../../integration/goose-standalone/cuda-activation-entries.json) |
| CUDA | Shared | `goose_route_gather` | — | EXP-2425, FUSE | [cuda-activation-entries.json](../../integration/goose-standalone/cuda-activation-entries.json) |
| CUDA | Shared | `goose_route_norm` | — | FUSE, REG | [cuda-activation-entries.json](../../integration/goose-standalone/cuda-activation-entries.json) |
| CUDA | Shared | `goose_route_reduce` | — | FUSE | [cuda-activation-entries.json](../../integration/goose-standalone/cuda-activation-entries.json) |
| CUDA | Shared | `goose_route_sort` | — | Apex sampler, FUSE | [cuda-activation-entries.json](../../integration/goose-standalone/cuda-activation-entries.json) |
| CUDA | Shared | `goose_route_topk` | — | FUSE, SHARED | [cuda-activation-entries.json](../../integration/goose-standalone/cuda-activation-entries.json) |
| Vulkan | Shared | `inkling_route` | — | FUSE | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `moe_route` | — | EXP-2438, FUSE | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `moe_weighted` | — | EXP-2438, FUSE | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |

## Normalization and softmax

| Backend | Model family | Entry | Confirmed | Candidates | Source |
| --- | --- | --- | --- | --- | --- |
| CUDA | Ternary Bonsai | `bonsai_norm` | — | FUSE, REG | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_norm_wide_128_0` | — | FUSE, REG | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_norm_wide_512_1` | — | FUSE, REG | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| Vulkan | Shared | `apex_qk_norm` | — | FUSE, REG | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `qk_norm` | — | FUSE, REG | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `rmsnorm` | — | FUSE, REG, SHARED | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |

## Packing and gather

| Backend | Model family | Entry | Confirmed | Candidates | Source |
| --- | --- | --- | --- | --- | --- |
| CPU | Inkling 2 Mini | `fg_i2_embed_prefill` | [EXP-2425](../../experiments/EXP-2026-11-28-2425-inkling-parallel-embedding/README.md) | — | [serial-host.goose](../../integration/goose-standalone/universal/inkling/serial-host.goose) |
| CPU | Inkling 2 Mini | `i2_embed_row` | — | EXP-2425 | [model.goose](../../integration/goose-standalone/universal/inkling/model.goose) |
| CPU | Shared | `set_quantized_rows` | — | — | [cpu.goose](../../integration/goose-standalone/universal/cpu/cpu.goose) |
| CUDA | GigaChat4 | `goose_dsa_pack_power2` | — | — | [cuda-dsa-entries.json](../../integration/goose-standalone/gigachat4/cuda-dsa-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_pack_f16` | — | — | [flash-native-image-entries.json](../../integration/goose-standalone/universal/cuda/images/flash-native-image-entries.json) |
| CUDA | Inkling 2 Mini | `i2_pack_q8_input8` | — | EXP-2423, EXP-2437, EXP-2439 | [q8-packed-image-entries.json](../../integration/goose-standalone/universal/cuda/images/q8-packed-image-entries.json) |
| CUDA | Inkling 2 Mini | `i2_q8_packed16w2` | — | EXP-2423, EXP-2437, EXP-2439 | [q8-packed-image-entries.json](../../integration/goose-standalone/universal/cuda/images/q8-packed-image-entries.json) |
| CUDA | Inkling 2 Mini | `i2_q8_packed16w4` | — | EXP-2423, EXP-2437, EXP-2439 | [q8-packed-image-entries.json](../../integration/goose-standalone/universal/cuda/images/q8-packed-image-entries.json) |
| CUDA | Inkling 2 Mini | `i2_q8_packed4` | — | EXP-2423, EXP-2437, EXP-2439 | [q8-packed-image-entries.json](../../integration/goose-standalone/universal/cuda/images/q8-packed-image-entries.json) |
| CUDA | Inkling 2 Mini | `i2_q8_packed8` | — | EXP-2423, EXP-2437, EXP-2439 | [q8-packed-image-entries.json](../../integration/goose-standalone/universal/cuda/images/q8-packed-image-entries.json) |
| CUDA | Shared | `goose_set_q4_rows` | — | — | [cuda-activation-entries.json](../../integration/goose-standalone/cuda-activation-entries.json) |
| CUDA | Ternary Bonsai | `bonsai_embed_batch` | — | EXP-2425 | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_mv_pack_q8` | — | EXP-2423, EXP-2437, EXP-2439 | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pack_half` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pack_q8_128` | — | EXP-2423, EXP-2437, EXP-2439 | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pack_q8_panel128` | — | EXP-2423, EXP-2437, EXP-2439 | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pq2_half_unpack_half2_17408` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pq2_matvec_rows2` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `goose_set_q4_rows` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| Vulkan | Shared | `gather_pq2_0` | — | EXP-2425 | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `gather_q4_0` | — | EXP-2425 | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `gather_q4_k` | — | EXP-2425 | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `gather_q8_0` | — | EXP-2423, EXP-2425, EXP-2437, EXP-2439 | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |

## Review source

| Backend | Model family | Entry | Confirmed | Candidates | Source |
| --- | --- | --- | --- | --- | --- |
| CPU | Inkling 2 Mini | `fg_i2_prefill_partition` | [EXP-2430](../../experiments/EXP-2026-11-28-2430-inkling-verifier-cpu-team/README.md) | — | [serial-host.goose](../../integration/goose-standalone/universal/inkling/serial-host.goose) |
| CPU | Inkling 2 Mini | `i2_depth_mix_cached` | — | — | [numeric.goose](../../integration/goose-standalone/universal/inkling/numeric.goose) |
| CPU | Inkling 2 Mini | `inkling_row_major_i2_cache_layout` | — | — | [numeric.goose](../../integration/goose-standalone/universal/inkling/numeric.goose) |
| CPU | Inkling 2 Mini | `inkling_row_major_i2_cache_row` | — | — | [numeric.goose](../../integration/goose-standalone/universal/inkling/numeric.goose) |
| CPU | Inkling 2 Mini | `inkling_row_major_i2_depth_mix` | — | — | [numeric.goose](../../integration/goose-standalone/universal/inkling/numeric.goose) |
| CPU | Inkling 2 Mini | `inkling_row_major_i2_loop_blend` | — | — | [numeric.goose](../../integration/goose-standalone/universal/inkling/numeric.goose) |
| CPU | Inkling 2 Mini | `inkling_row_major_i2_relative_positions` | — | — | [numeric.goose](../../integration/goose-standalone/universal/inkling/numeric.goose) |
| CPU | Inkling 2 Mini | `inkling_row_major_i2_reset_cache_positions` | — | — | [numeric.goose](../../integration/goose-standalone/universal/inkling/numeric.goose) |
| CPU | Inkling 2 Mini | `inkling_row_major_i2_rms_gain` | — | SHARED | [numeric.goose](../../integration/goose-standalone/universal/inkling/numeric.goose) |
| CPU | Inkling 2 Mini | `inkling_row_major_i2_text_logits` | — | EXP-2443 | [numeric.goose](../../integration/goose-standalone/universal/inkling/numeric.goose) |
| CPU | Shared | `dispatch_dsa_score` | — | — | [cpu.goose](../../integration/goose-standalone/universal/cpu/cpu.goose) |
| CPU | Shared | `fg143_node_run` | — | — | [cpu.goose](../../integration/goose-standalone/universal/cpu/cpu.goose) |
| CPU | Shared | `scan_entry` | — | — | [cpu.goose](../../integration/goose-standalone/universal/cpu/cpu.goose) |
| CUDA | GigaChat4 | `goose_dsa_bound_metadata` | — | — | [cuda-dsa-entries.json](../../integration/goose-standalone/gigachat4/cuda-dsa-entries.json) |
| CUDA | GigaChat4 | `goose_dsa_bound_query` | — | — | [cuda-dsa-entries.json](../../integration/goose-standalone/gigachat4/cuda-dsa-entries.json) |
| CUDA | GigaChat4 | `goose_dsa_compact` | — | — | [cuda-dsa-entries.json](../../integration/goose-standalone/gigachat4/cuda-dsa-entries.json) |
| CUDA | GigaChat4 | `goose_dsa_fallback_score` | — | — | [cuda-dsa-entries.json](../../integration/goose-standalone/gigachat4/cuda-dsa-entries.json) |
| CUDA | GigaChat4 | `goose_dsa_fallback_top` | — | — | [cuda-dsa-entries.json](../../integration/goose-standalone/gigachat4/cuda-dsa-entries.json) |
| CUDA | GigaChat4 | `goose_dsa_finalize` | — | — | [cuda-dsa-entries.json](../../integration/goose-standalone/gigachat4/cuda-dsa-entries.json) |
| CUDA | GigaChat4 | `goose_dsa_integer_score` | — | — | [cuda-dsa-entries.json](../../integration/goose-standalone/gigachat4/cuda-dsa-entries.json) |
| CUDA | GigaChat4 | `goose_dsa_replay` | — | — | [cuda-dsa-entries.json](../../integration/goose-standalone/gigachat4/cuda-dsa-entries.json) |
| CUDA | GigaChat4 | `goose_dsa_score_cutoff` | — | — | [cuda-dsa-entries.json](../../integration/goose-standalone/gigachat4/cuda-dsa-entries.json) |
| CUDA | Inkling 2 Mini | `goose_i2_update_history` | — | — | [attention-input-image-entries.json](../../integration/goose-standalone/universal/cuda/images/attention-input-image-entries.json) |
| CUDA | Inkling 2 Mini | `i2_batch_verifier_logits` | [EXP-2443](../../experiments/EXP-2026-11-28-2443-inkling-verifier-batch-logits/README.md) | — | [serial-host.goose](../../integration/goose-standalone/universal/inkling/serial-host.goose) |
| CUDA | Inkling 2 Mini | `i2_bridge_mix` | — | — | [i2-lazy-prelude-image-entries.json](../../integration/goose-standalone/universal/cuda/images/i2-lazy-prelude-image-entries.json) |
| CUDA | Inkling 2 Mini | `i2_bridge_prepare` | — | — | [i2-lazy-prelude-image-entries.json](../../integration/goose-standalone/universal/cuda/images/i2-lazy-prelude-image-entries.json) |
| CUDA | Inkling 2 Mini | `i2_bridge_score` | — | — | [i2-lazy-prelude-image-entries.json](../../integration/goose-standalone/universal/cuda/images/i2-lazy-prelude-image-entries.json) |
| CUDA | Inkling 2 Mini | `i2_lazy_prepare` | — | — | [i2-lazy-prelude-image-entries.json](../../integration/goose-standalone/universal/cuda/images/i2-lazy-prelude-image-entries.json) |
| CUDA | Inkling 2 Mini | `i2_plane_mix` | — | — | [i2-lazy-prelude-image-entries.json](../../integration/goose-standalone/universal/cuda/images/i2-lazy-prelude-image-entries.json) |
| CUDA | Inkling 2 Mini | `i2_plane_score` | — | — | [i2-lazy-prelude-image-entries.json](../../integration/goose-standalone/universal/cuda/images/i2-lazy-prelude-image-entries.json) |
| CUDA | Inkling 2 Mini | `i2_q8_expand_f32` | — | EXP-2423, EXP-2437, EXP-2439 | [q8-packed-image-entries.json](../../integration/goose-standalone/universal/cuda/images/q8-packed-image-entries.json) |
| CUDA | Inkling 2 Mini | `i2_q8_fp32_pair_w2` | [EXP-2423](../../experiments/EXP-2026-11-28-2423-inkling-q8-row-pair/README.md) | EXP-2437, EXP-2439 | [q8-packed-image-entries.json](../../integration/goose-standalone/universal/cuda/images/q8-packed-image-entries.json) |
| CUDA | Inkling 2 Mini | `i2_q8_fp32_pair_w4` | [EXP-2423](../../experiments/EXP-2026-11-28-2423-inkling-q8-row-pair/README.md) | EXP-2437, EXP-2439 | [q8-packed-image-entries.json](../../integration/goose-standalone/universal/cuda/images/q8-packed-image-entries.json) |
| CUDA | Inkling 2 Mini | `i2_q8_fp32w2` | — | EXP-2423, EXP-2437, EXP-2439 | [q8-packed-image-entries.json](../../integration/goose-standalone/universal/cuda/images/q8-packed-image-entries.json) |
| CUDA | Inkling 2 Mini | `i2_q8_fp32w4` | — | EXP-2423, EXP-2437, EXP-2439 | [q8-packed-image-entries.json](../../integration/goose-standalone/universal/cuda/images/q8-packed-image-entries.json) |
| CUDA | Shared | `goose_dsa_score` | — | — | [cuda-attention-entries.json](../../integration/goose-standalone/cuda-attention-entries.json) |
| CUDA | Shared | `goose_giga_dsa_score_shared_q` | — | — | [cuda-attention-entries.json](../../integration/goose-standalone/cuda-attention-entries.json) |
| CUDA | Shared | `goose_giga_dsa_top_wide` | — | — | [cuda-attention-entries.json](../../integration/goose-standalone/cuda-attention-entries.json) |
| CUDA | Shared | `goose_giga_mask_clear` | — | — | [cuda-attention-entries.json](../../integration/goose-standalone/cuda-attention-entries.json) |
| CUDA | Shared | `goose_giga_mask_scatter` | — | — | [cuda-attention-entries.json](../../integration/goose-standalone/cuda-attention-entries.json) |
| CUDA | Shared | `goose_giga_mla_compact_mask` | — | — | [cuda-attention-entries.json](../../integration/goose-standalone/cuda-attention-entries.json) |
| CUDA | Shared | `goose_giga_mla_indexed_merge` | — | — | [cuda-attention-entries.json](../../integration/goose-standalone/cuda-attention-entries.json) |
| CUDA | Shared | `goose_giga_mla_mma_pv_indexed` | — | — | [cuda-attention-entries.json](../../integration/goose-standalone/cuda-attention-entries.json) |
| CUDA | Shared | `goose_giga_mla_mma_pv_tile` | — | — | [cuda-attention-entries.json](../../integration/goose-standalone/cuda-attention-entries.json) |
| CUDA | Shared | `goose_giga_mla_mma_pv_visible_tile` | — | — | [cuda-attention-entries.json](../../integration/goose-standalone/cuda-attention-entries.json) |
| CUDA | Shared | `goose_giga_mla_mma_tile` | — | — | [cuda-attention-entries.json](../../integration/goose-standalone/cuda-attention-entries.json) |
| CUDA | Shared | `goose_giga_mla_visibility` | — | — | [cuda-attention-entries.json](../../integration/goose-standalone/cuda-attention-entries.json) |
| CUDA | Shared | `goose_rms_reference` | — | SHARED | [cuda-rms-reference-entries.json](../../integration/goose-standalone/cuda-rms-reference-entries.json) |
| CUDA | Shared | `goose_rms_reference_one` | — | SHARED | [cuda-rms-reference-entries.json](../../integration/goose-standalone/cuda-rms-reference-entries.json) |
| CUDA | Shared | `goose_rms_reference_one_warp4` | — | SHARED | [cuda-rms-reference-entries.json](../../integration/goose-standalone/cuda-rms-reference-entries.json) |
| CUDA | Shared | `goose_tensor_blit` | — | — | [cuda-activation-entries.json](../../integration/goose-standalone/cuda-activation-entries.json) |
| CUDA | Shared | `goose_tensor_transpose_concat` | — | — | [cuda-activation-entries.json](../../integration/goose-standalone/cuda-activation-entries.json) |
| CUDA | Ternary Bonsai | `bonsai_choice_final` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_choice_tiles` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_decode_direct_q4_mma` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_decode_direct_q4_mma_half2` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_decode_gqa2` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_decode_gqa3` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_ffn_tail` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_ffn_tail_q8_panel` | — | EXP-2423, EXP-2437, EXP-2439 | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_mtp_concat` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_mtp_eh_q4` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_mtp_mat_q4` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_mtp_q4_four` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_mvq_4_permutation_2` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_mvq_4_permutation_4` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_mvq_5_spread_4` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pq2_cached_64x128_k64` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pq2_cached_64x64_k64` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pq2_cached_fp16_acc_k32` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pq2_half_full_17408` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pq2_half_full_5120` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pq2_half_full_6144` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pq2_mvq_q8_1` | — | EXP-2423, EXP-2437, EXP-2439 | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pq2_mvq_q8_2` | — | EXP-2423, EXP-2437, EXP-2439 | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pq2_mvq_q8_five_1` | — | EXP-2423, EXP-2437, EXP-2439 | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pq2_mvq_q8_five_2` | — | EXP-2423, EXP-2437, EXP-2439 | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pq2_mvq_q8_four_1` | — | EXP-2423, EXP-2437, EXP-2439 | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pq2_mvq_q8_four_2` | — | EXP-2423, EXP-2437, EXP-2439 | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pq2_mvq_q8_three_1` | — | EXP-2423, EXP-2437, EXP-2439 | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pq2_mvq_q8_three_2` | — | EXP-2423, EXP-2437, EXP-2439 | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_pq2_row` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_verify_five_32` | — | EXP-2424, EXP-2430, EXP-2443 | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_verify_merge_parallel` | — | EXP-2424, EXP-2430, EXP-2443 | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_verify_pair_tail3` | — | EXP-2424, EXP-2430, EXP-2443 | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_verify_q4_gqa2` | — | EXP-2424, EXP-2430, EXP-2443 | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `bonsai_verify_q4_tail` | — | EXP-2424, EXP-2430, EXP-2443 | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `goose_rms_decode_2048` | — | SHARED | [cuda-rms-reference-bonsai-entries.json](../../integration/goose-standalone/cuda-rms-reference-bonsai-entries.json) |
| Vulkan | Shared | `head_slice` | — | — | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |

## RoPE and transforms

| Backend | Model family | Entry | Confirmed | Candidates | Source |
| --- | --- | --- | --- | --- | --- |
| CUDA | Shared | `goose_fwht_q4` | — | — | [cuda-fwht-entries.json](../../integration/goose-standalone/cuda-fwht-entries.json) |
| CUDA | Shared | `goose_fwht_q4_256` | — | — | [cuda-fwht-entries.json](../../integration/goose-standalone/cuda-fwht-entries.json) |
| CUDA | Shared | `goose_fwht_q4_64` | — | — | [cuda-fwht-entries.json](../../integration/goose-standalone/cuda-fwht-entries.json) |
| CUDA | Shared | `goose_rope_apply` | — | REG, SHARED | [cuda-activation-entries.json](../../integration/goose-standalone/cuda-activation-entries.json) |
| CUDA | Shared | `goose_rope_decode` | — | REG, SHARED | [cuda-activation-entries.json](../../integration/goose-standalone/cuda-activation-entries.json) |
| CUDA | Shared | `goose_rope_phases` | — | REG, SHARED | [cuda-activation-entries.json](../../integration/goose-standalone/cuda-activation-entries.json) |
| CUDA | Ternary Bonsai | `bonsai_fwht1024` | — | — | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `goose_rope_apply` | — | REG, SHARED | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `goose_rope_decode` | — | REG, SHARED | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| CUDA | Ternary Bonsai | `goose_rope_phases` | — | REG, SHARED | [image-host.goose](../../integration/goose-standalone/universal/bonsai/image-host.goose) |
| Vulkan | Shared | `fwht` | — | — | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
| Vulkan | Shared | `rope` | — | REG, SHARED | [build-goose-vulkan.sh](../../tools/build-goose-vulkan.sh) |
