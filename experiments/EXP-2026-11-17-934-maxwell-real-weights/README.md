# EXP-2026-11-17-934 — Maxwell-V1-Flash Q4_0 on real weights

Status: ACCEPTED (load + generation); long-context MBSA not implemented

## Artefact

`maxwell-v1-flash-q4_0.gguf`

- size: 21,418,677,984 bytes (21.4 GB)
- sha256: `8d53f1e39d9b1940f2639b364a01c420aec99cbf26320f1fdeb8a662945c11f3`
- tensors: 981, metadata keys: 44, `pre=qwen35`, merges 199,740
- source: `maxwell-model/Maxwell-V1-Flash` (35 BF16 shards), converted with
  `tools/maxwell-convert.py`; Q4_0 for 2-D weights, F16 for mHC projections and
  embeddings/LM head, F32 for every 1-D tensor.

## Checks on real weights

- Load: `n_vocab=200704 n_embd=2048 n_layer=45`; `llama_decode` returns 0;
  logits finite.
- Raw completion via `POST /completion` (bypasses the chat parser), prompt
  "The capital of France is", 16 tokens, temperature 0:
  ` Paris.\n\n**Constraints:**\n- The user asks: "What is`
- CPU timings at 16 threads: prompt 23.39 tok/s, decode 9.47 tok/s
  (5 prompt tokens, 16 predicted; not a benchmark profile).

## Known limitations (stated, not hidden)

- Verified context is 4096 tokens. Above it the builder refuses until MBSA
  lands; there is no silent dense fallback.
- MBSA graph is not implemented yet; the indexer contract (tensors, keys,
  converter) is in EXP-933 and `docs/goose-migration/MBSA-HANDOFF.md`.
- Chat path: the model's Jinja template is embedded, but the CLI's PEG parser
  rejects the generated output; the raw completion endpoint works.
- No GPU benchmark was run (requires Igor's approval); no Apex regression run
  yet.
- Tokenizer pre-type maps to `qwen35`; Han/Thai splits and `\p{N}{1,3}` from the
  original file are a known difference.
