# EXP-2026-11-17-930 — Maxwell BPE vocabulary in the GGUF

Status: ACCEPTED (synthetic tokenizer; real file not downloaded yet)

## Hypothesis

`tokenizer.json` can be written into the GGUF container so the runtime's own
vocabulary loads it and tokenises exactly by the ids in the file. Without this
the model cannot be started: a placeholder vocabulary proves the graph but not
the runtime path.

## What is implemented

`tools/maxwell-convert.py` now writes the real vocabulary when
`tokenizer.json` is present:

- `tokenizer.ggml.tokens` from `model.vocab` plus `added_tokens` (special ids
  marked control=3, the rest normal=1);
- `tokenizer.ggml.scores` (zeros for BPE) and `tokenizer.ggml.merges` in rank
  order, accepting both the string and list-pair HF spellings;
- `tokenizer.ggml.pre` auto-detected: byte-level without a split regex → `gpt-2`;
  a Split regex over letter/number classes (Qwen-style) → `qwen2`;
  otherwise `default`;
- bos/eos/pad/unk ids from the model config;
- a refusal when `tokenizer.json` is missing, unless `--stub-vocab` is passed
  for loader-only tests.

`tools/maxwell-build.sh` refuses to convert until every shard named in
`model.safetensors.index.json` exists, so a partial download cannot produce a
half-complete GGUF.

## Gate

`python3 experiments/EXP-2026-11-17-930-maxwell-vocab/tests/test_vocab.py`
→ `checks=8 failures=0`, `MAXWELL-VOCAB OK`.

The test builds a synthetic BPE `tokenizer.json` (50 ids, 8 merges), converts a
synthetic model with `vocab_size=50`, loads it through the runtime API and asks
`llama_tokenize`:

```
text='hello' n=1 ids=14   piece='hello'
text='world' n=1 ids=18   piece='world'
```

The pre-tokenizer is detected as `gpt-2`, the merge table has 8 entries, and the
runtime reports `vocab=50`.

## Not yet true

The real Maxwell `tokenizer.json` and the shard index are not downloaded yet
(23/35 shards at the time of writing, owned by another agent). The Qwen-style
pre-tokenizer mapping is implemented but unverified against the real file; if the
real detection differs, the runtime refuses to load with
`unknown pre-tokenizer type`, which is the intended failure, not a silent one.

## Next

When the download completes: run `tools/maxwell-build.sh`, load the real GGUF,
compare a short greedy generation with the reference expectations, then measure
prefill/decode/VRAM/RAM and the Apex/regression set.
