# P7-06, P7-07, P7-12: the tokenizer and GGUF doors - what the shipped copies call

Checked 2026-11-19 on `main`, text level, no build. The three requirements are one family: the
vocabulary that reads a GGUF, encodes text and turns tokens back into pieces.

## The carrier copies are the ones that ship

`runtime/reconstructed/build.py` compiles thirteen carrier sources from `runtime/reconstructed/src`
and takes the rest from `vendor/flint/src`. `llama-vocab.cpp` and `llama-model-loader.cpp` are both
carriers, so the **staged** copies are what ships - and reading the vendor copies instead gives the
opposite answer, which is why this note records the staged ones.

## What the staged vocabulary reaches

`runtime/reconstructed/src/llama-vocab.cpp` calls, through the reader owner:

| door | uses | serves |
|---|---|---|
| `llama_goose_tokenize` | 2 | **P7-06**, encode: the owner runs `bt_bpe` underneath |
| `llama_goose_token_to_piece` | 2 | **P7-07**, detokenise: the owner runs `bt_token_to_piece` |
| `llama_goose_tokenizer_ctx` | 3 | the engine's context, so the tokenizer tables are loaded once |
| `llama_goose_kv_type`, `_arr_type`, `_arr_n`, `_arr_data`, `_arr_str` | 2 each | vocabulary layout taken from the GGUF arrays |
| `llama_goose_ready` | 6 | whether the reader answered at all |

The owner (`integration/goose-standalone/gguf-reader-owner.cpp`) provides all of these and reaches
the Goose doors `bt_bpe`, `bt_token_to_piece`, `btl_load`, `gr_parse`, `gr_find_kv` and the array
accessors underneath.

## What that means per requirement

- **P7-06** (vocabulary loader and Apex tokenizer): wired on the encode path. Open: the acceptance
  run that compares token ids, pre-tokenisation and special tokens against the reference over the
  whole corpus, and then the removal of the replaced native body.
- **P7-07** (detokenisation and incremental decoding): wired on the token-to-piece path. Open: the
  same acceptance, plus the streaming and short-buffer cases the requirement lists.
- **P7-12** (GGUF, loader, tokenizer, common dependencies): wired, because every metadata question
  above goes through the reader when it has the file, and falls back to the native container when it
  does not. Open: multi-part GGUF naming and shard offsets, v1 and byte-order limits, float arrays,
  and the HF download/cache calls - each named in the requirement and each still native.

## The one thing this note cannot say

Whether the answers are equal. That is a differential run on the real vocabulary, and no run of
this kind has been made since the last change to these doors. Nothing here is claimed as accepted.
