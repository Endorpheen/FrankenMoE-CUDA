# MBSA handoff — Maxwell block-sparse attention

State after EXP-2026-11-17-933 (commit 4f86f2f5). Dense path is complete and
proven; the sparse path is the remaining architectural gap.

## What already exists in the runtime

- `llama_kv_cache_msa` (`src/llama-kv-cache-msa.h/.cpp`): two KV caches (main
  K/V + indexer K), per-ubatch position→cell maps (`set_input_pos_slot`,
  `set_input_pos_mask` with causal masking), same sequence operations for both.
- `llm_graph_input_msa` is defined **inside** `src/models/minimax-m3.cpp` (not
  in llama-graph.h). To reuse it for Maxwell, lift it into a shared header/source
  unchanged, or copy it and record the duplication.
- `models/minimax-m3.cpp` contains the full block-sparse graph: indexer q/k +
  RMS norm + RoPE, indexer-K `cpy_k`, block scores via `ggml_pool_2d` MAX,
  `ggml_top_k`, position→cell gather of K/V, and `build_attn_msa_fa` with a
  gathered F16 mask. Lines ~240-470 are the reference.
- MSA cache creation for an arch is in `src/llama-model.cpp` around line 2808
  (`case LLM_ARCH_MINIMAX_M3`); Maxwell needs the same call with
  `filter_idx = all layers` (every Maxwell layer has an indexer).

## What Maxwell needs beyond minimax

1. Hard route, not free top-k. Reference (`modeling_maxwell_moe.py`):
   - prefix blocks 0..cur (prefix_window 1024 / block 128 = 8),
   - remote = top 16 of blocks with `bstart >= prefix_window` and
     `bend <= local_start*block`,
   - local blocks `cur-7..cur` with `>= prefix_blocks`,
   - final route = prefix + remote + local (32 blocks).
   Using MSA top-k with K=32 needs prefix and local blocks forced. Either add a
   `prefix`/`local` bias in the MSA input fill, or compute the route bias in the
   Maxwell graph from `pos_slot` (do not change minimax behaviour silently).
2. Query chunk 64 and 4 indexer groups with 1 KV group: the minimax graph
   already assumes one indexer head per GQA group (`HKV == Hd`).
3. Indexer K is post-RoPE with the main cos/sin; both use NEOX for Maxwell.
4. All 45 layers are sparse (`mlp_only_layers` affects FFN only).

## Verification plan (no GPU run without Igor's approval)

- CPU synthetic oracle: extend `experiments/EXP-2026-11-17-929-.../numeric.py`
  with block-sparse routing; compare 1-layer logits at 8K, then the full trunk
  at 8K/32K. Must match in F32, not just greedily.
- Sequence: 8K -> 32K -> 64K -> 128K -> 256K, each with the route verified
  against the reference selection, then a generation check.
- Keep the dense <=4096 refusal until the sparse path passes 8K.

## Current guarantees

- Dense <=4096 is exact (exhaustive route proof, EXP-928).
- Contexts above the support window are refused, never silently dense.
- Real GGUF conversion (running now) has no indexer tensors; a second conversion
  is required to include them (converter already emits the 180 tensors).
