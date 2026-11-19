# EXP-2026-11-17-933 — Maxwell Block Sparse Attention (MBSA)

Status: IN PROGRESS (contract landed; graph next)

## Target

Full block-sparse attention up to 262144 tokens, matching
`modeling_maxwell_moe.py`, verified at 8K, 32K, 64K, 128K, 256K. No silent
replacement by dense attention and no precision loss.

## Reference semantics (per query chunk of 64)

1. Indexer branch: q = linear_q_proj(x) -> 4 groups x 128, k = linear_k_proj(x)
   -> 1 head x 128, RMS q/k norm, the same RoPE as the main branch; indexer K is
   cached per layer.
2. scores[g, q, k] = q_g . k / sqrt(128); block score = max over the 128 tokens
   of the block, with masked positions at -inf.
3. Remote eligibility: block_start >= prefix_window and
   block_end <= local_start_block*block_size.
4. Remote = top 16 eligible blocks; final route = prefix blocks (0..cur) +
   remote + local blocks (cur-7..cur, >= prefix blocks), in that order.
5. Attention runs over the routed 32 blocks (4096 keys) per query, causal
   masked; the same Q/K/V and causal rules as the dense path.

## Reuse

The runtime already carries `llama_kv_cache_msa` (two KV caches: main K/V and
indexer K), per-ubatch position-to-cell maps, block-max via `ggml_pool_2d`,
`ggml_top_k`, gather by block index and a flash-attention path with a gathered
mask — see `models/minimax-m3.cpp`. MBSA builds on that machinery instead of a
second implementation.

## Landed so far

- `LLM_TENSOR_INDEXER_Q_PROJ/K_PROJ/Q_NORM/K_NORM` loaded as optional layer
  tensors, so a GGUF without an indexer still loads for the <=4096 dense path.
- New keys: `block_sparse_gqa.prefix_window_size` and `num_remote_blocks`;
  the existing indexer keys carry head count, key length, top-k, block size and
  local blocks.
- Converter emits 180 indexer tensors for the real model (4 per layer) and the
  indexer metadata; a GGUF without them still converts for the dense gate.
- The dense builder still refuses contexts above the support window, so nothing
  silently degrades while the sparse graph is under construction.

## Next

1. Create the MSA cache for `maxwell_moe` with indexer head size 128.
2. Graph: indexer q/k + norm + RoPE, indexer-K update, block-max scores,
   prefix/local bias so the top-32 route equals the reference, gathered K/V and
   masked FA.
3. CPU synthetic oracle for the sparse route at 8K, then 32K; GPU runs only
   with Igor's approval.
