# EXP-2026-11-17-927 — Maxwell safetensors → our GGUF Q4_0

Status: ACCEPTED (converter slice)

## Hypothesis

Maxwell-V1-Flash can be loaded by the existing runtime if its safetensors are
converted into the GGUF container the runtime already reads, with the weights
in the Q4_0 layout the existing quantised kernels already dequantise. No new
dequant path and no llama.cpp/vLLM converter are needed.

## What is implemented

`tools/maxwell-convert.py`:

- reads `config.json`, the safetensors index or the shard headers;
- writes GGUF v3 with `general.architecture = maxwell_moe` and the keys the
  loader expects (`maxwell_moe.*`: block count, head geometry, expert sizes,
  leading dense blocks, sigmoid gating, mHC count/iterations/eps, rope base,
  block-sparse geometry);
- renames tensors to the runtime convention (`token_embd.weight`,
  `output.weight`, `blk.N.attn_q.weight`, `blk.N.hc_attn_fn.weight`, ...);
- aggregates the 256 routed experts of one projection into a single 3-D tensor
  ([n_embd, n_ff, n_expert], [n_ff, n_embd, n_expert] for down), packed Q4_0
  row-wise;
- packs every 2-D linear weight to Q4_0 with ggml's reference formula
  (`quantize_row_q4_0_ref`, `ggml-quants.c:113`), 18 bytes per 32 values;
- keeps every 1-D tensor in F32: expert bias spreads ~0.005 and bf16's step is
  ~0.06, so truncating it collapses routing (modeling_maxwell_moe.py
  `_fix_expert_bias_precision`);
- keeps mHC projections (`hc_*`) in F16;
- keeps `embed_tokens` and `lm_head` in F16; `--quant-io` switches them to Q4_0;
- BF16/F16/F32 input, shard-by-shard streaming (no whole-model load);
- `--stub-vocab` writes a placeholder vocabulary for loader tests only; the real
  BPE vocabulary is a separate slice.

## Gate

`python3 experiments/EXP-2026-11-17-927-maxwell-quant-convert/tests/test_convert.py`
→ `checks=20 failures=0`, `MAXWELL-CONVERT OK`.

The test builds synthetic shards (BF16 2-D expert, F32 1-D norm and bias, F16
embedding), converts, then:
- parses the GGUF with an independent reader (header/metadata/descriptors);
- compares the Q4_0 payload byte-for-byte with a separate transcription of the
  reference packing;
- dequantises with a separate formula and bounds the error (< 0.01 on weights
  with σ = 0.02);
- checks that F32 tensors are bit-exact and F16 within 1e-4.

No real weights were read or downloaded for this gate.

## Explicit non-claims

- The converter has not yet been run on a real Maxwell shard (the download is
  owned by another agent; the index is not present yet).
- The runtime does not know `maxwell_moe` yet: this slice produces the
  container, not the loader or the graph.
- The 4-bit scheme is Q4_0 for the first working build. A later experiment may
  compare Q4_0 against a group-wise scheme on the three axes; Q4_0 is chosen now
  because the kernels already exist and the memory bound is known.

## Next slice

`maxwell_moe` arch registration and the tensor map in the runtime, then the
graph: QK-norm/RoPE, block-sparse attention (route from the Goose module),
mHC streams, sigmoid router/shared expert, MTP head.
