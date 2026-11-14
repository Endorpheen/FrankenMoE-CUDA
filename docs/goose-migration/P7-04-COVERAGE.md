# P7-04 coverage — quantization, importance and conversion policy

Updated: 2026-11-14. Native sources: `llama-quant.cpp`,
`imatrix-loader.cpp`, `ggml-quants.c`, `ggml-cpu/quants.c`,
`ggml-cuda/dequantize.cuh` and the `tools/quantize` entry points.

The requirement is split into six independent slices. The policy slice is
accepted in two steps; the rest is open.

| slice | native scope | state |
|---|---|---|
| S1a. Type selection and imatrix flags | `llama_quant_compute_types`, `requires_imatrix`, `tensor_requires_imatrix` | **done (EXP581)** |
| S1b. Allow-cuts and tensor-type overrides | `tensor_allows_quantization`, the `--tensor-type` loop, the decision prefix of `llama_tensor_get_type` | **done (EXP576)** |
| S2. imatrix loader and remap | legacy and GGUF loaders, dataset metadata, `remap_layer`, `remap_imatrix`, prepare | **done (EXP604)** |
| S3a. Basic row quantizers | q4_0/q4_1/q5_0/q5_1/q8_0/q1_0/q2_0 | **done (EXP603)** |
| S3b. K/IQ/MXFP4/NVFP4/TQ quantizers | the remaining unweighted types | **done (EXP611)** |
| S4. Weighted quantizers | imatrix branches of q2_K…q6_K and IQ1/2/3/4 | open |
| S5a. Basic dequantizers | q4_0/q4_1/q5_0/q5_1/q8_0 row dequant | **done (EXP607)** |
| S5b. Dequant closure | K/IQ/MX/TQ dequant, different-type copy, q8_K, f16/bf16 output | open |

## S1b — accepted

Source: `experiments/EXP-2026-11-14-576-p7-04-quant-allow/`; canonical file
`goose/model/quant-allow.goose`.

The port owns every allow-cut (rank, only_copy, weight suffix, norms, the output
gate, expert gating, the i32 routing table, altup/laurel,
per_layer_model_proj, position/token-type tensors, conv1d, MiniMax indexers,
the fourteen GLM5_NEXT cuts, the VLLM_DECISION2 head, RWKV time-mix, T5
rel-bias, multimodal tensors and audio codebooks), the CLI `--tensor-type`
pattern loop with first-match-wins, the decision priority table, and the
supported `std::regex_search` subset. Unsupported regex constructs take the
documented std::regex fallback instead of silently mismatching.

Gate: one generator produces the native oracle (`probe_q4.cpp` including
`llama-quant.cpp`) and the Goose driver; 16 policy cases plus a 57-pair regex
corpus compare as 73 identical lines. Mutations 16/16 in two halves.

## Open boundaries

- `llama_quant_compute_types`, `requires_imatrix` and `tensor_requires_imatrix`
  (S1a) exist as a port but have no gate yet; they must not be counted as done.
- The native `llama_tensor_get_type` standard per-category selection and shape
  fallback are outside S1b; `qa_priority` is compared against the marked native
  prefix.
- The imatrix loader, the quantizers and the dequant closure (S2–S5) remain
  native.
- The `std::regex` fallback is a real boundary: the caller must run the standard
  library for any pattern outside the subset.
