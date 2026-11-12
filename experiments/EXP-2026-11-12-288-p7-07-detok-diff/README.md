# EXP288: detokenisation and token-piece differential -- P7-07 closure evidence

Criterion: "All token-to-piece/special/byte fallback/stream boundary behavior exact; arbitrary byte and Unicode
roundtrips; token IDs unchanged."

## Byte-level piece differential (the strong arm)

`native_detok.c` is one driver compiled twice: once against the untouched flint `libllama` + `libllama-common`, once
against the accepted runtime's `stage-run` libraries. Both load the model vocab-only and, in `pieces_bin` mode, write
length-prefixed raw piece bytes per token id, so no JSON or Unicode conversion sits between the vocabulary and the
comparison.

    ids compared: 248320 (the whole vocabulary)
    native-pieces.bin   2 836 764 bytes
    goose-pieces.bin    2 836 764 bytes
    cmp -> identical

This covers ordinary tokens, special tokens, and every byte-fallback token, because the sample is the full range rather
than a selection.

## Text-concatenation differential

The 2216 id arrays produced by EXP287's corpus were detokenised through the accepted runtime's `/detokenize` and
compared with the native driver's concatenation of `common_token_to_piece`:

    text cases: 2216, mismatches: 0

## Why the byte case needs the C-level arm

The first attempt read pieces through `/detokenize` and failed on the first byte token: a piece such as `0xa1` is not
valid UTF-8, so it cannot travel through a JSON string. That is a limitation of the HTTP representation, not of the
vocabulary -- the C-level comparison above is the correct instrument for it. Recorded because a JSON-only gate would
silently exclude exactly the tokens the criterion calls out.

## The other clauses

- **token IDs unchanged**: EXP287, 2216 cases, 0 mismatches against the same native library.
- **stream boundary**: EXP279 showed concatenated SSE deltas equal to the non-stream answer for chat, and EXP281 for
  responses; `FG401S_DETOK_DECISIONS` is live in the accepted run.
