# EXP-2026-11-14-576 — quantization allow-cuts and type overrides (P7-04 S2)

Native counterpart: `tensor_allows_quantization`, the `--tensor-type` pattern
loop and the decision prefix of `llama_tensor_get_type` in
`llama-quant.cpp`. Base commit: `19e10cdd`.

## What this slice covers

`source/quant-allow.goose` (711 lines, 4 exports):

| export | native | scope |
|---|---|---|
| `qa_allows(only_copy, quantize_output, n_dims, arch_flags, pos_embd, token_types, name)` | `tensor_allows_quantization` | every cut: rank, only_copy, the weight suffix, norms, the output gate, expert gating, the i32 routing table, altup/laurel, per_layer_model_proj, position/token-type tensors, conv1d, MiniMax indexers, the fourteen GLM5_NEXT cuts, the VLLM_DECISION2 head, RWKV time-mix, T5 rel-bias, multimodal tensors and audio codebooks |
| `qa_override(name, blob, off, len, ty, n, out_type)` | the pattern loop in `llama_tensor_get_type` | CLI order, regex_search semantics, first match wins; -1 none, -2 an unsupported pattern, so the caller falls back to std::regex for the whole decision |
| `qa_priority(...)` | the prefix of `llama_tensor_get_type` | 0 denied, 1 token-embedding type (unless per_layer_token_embd.weight), 2 output type, 3 manual override, 4 standard logic; -2 unsupported override |
| `qr_search(pattern, subject)` | `std::regex_search` | the supported subset, exported so the corpus gates it directly |

The pattern subset is literals, `.` (not a line terminator), `^`, `$`, classes
with ranges and negation, `\d \D \w \W \s \S` and escaped punctuation, and the
quantifiers `* + ? {m} {m,} {m,n}`. Groups, alternation, word boundaries and
back references are reported as unsupported and the caller falls back to
`std::regex`; that is a documented boundary, not a silent mismatch.

Architectures share one flag word (bit 0 GLM5_NEXT, bit 1 VLLM_DECISION2); the
arch name table stays with the caller and the port compares the two tensor
names it receives.

## Gate

`tests/gen_q4.py` generates both sides from one table: the native oracle
(`tests/probe_q4.cpp`, which includes `llama-quant.cpp` and links the prebuilt
`libllama`) and the Goose driver (`build/gate_q4.goose`). 16 policy cases and a
57-pair regex corpus are compared line by line:

```
diff build/native_q4.out build/goose_q4.out   # empty, 73 lines
```

## Mutations

`tests/mutate_q4.py <lo> <hi>` edits the port, regenerates the driver, runs it
with `goose run` and compares with the native output.

```
python3 tests/mutate_q4.py 0 8    # 8/8 caught
python3 tests/mutate_q4.py 8 16   # 8/8 caught
```

## Recorded limits

- `qa_priority` is compared against a transcription of the native prefix, not
  the whole `llama_tensor_get_type`, because the standard per-category
  selection and shape fallback are a later slice.
- The regex engine is a subset; unsupported constructs take the documented
  std::regex fallback.
- The gate runs `goose run` (TinyCC) rather than a full C build; the driver is
  regenerated from the port on every run.
