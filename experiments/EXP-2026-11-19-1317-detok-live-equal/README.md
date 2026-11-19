# EXP-2026-11-19-1317: the ported detokenizer agrees with the native one on all 248 320 tokens

## The measurement

The cross-check now sits in `token_to_piece_for_cache` - the function that **produces** the native piece - and
calls only the door. Compared on the byte-encoded token text, which is what the door reads, and only for the
`special = true` form:

```
FG_TK_DETOK_PROGRESS calls=247500 disagreements=0
FG_TK_DETOK_PROGRESS calls=248000 disagreements=0
FG_TK_DETOK_TOTAL    calls=248320 disagreements=0
```

248 320 is the vocabulary size: the cache is built by asking for every token, so this compares **every** token in the
model, byte for byte - all scripts, all special tokens, all byte-encoded pieces - not a sample.

## Why the two earlier attempts failed and this one did not

1. first attempt built the token table from the **decoded** piece cache; the door reads **byte-encoded** text (a
   space is U+0120, not 0x20), so it reported 566 "disagreements" that were entirely the table
   (EXP-2026-11-19-1315);
2. second attempt hooked `token_to_piece` and asked `token_to_piece_for_cache` for the reference, which calls the
   hook again: the server died with a native call stack overflow (EXP-2026-11-19-1316);
3. the third hooks the producer itself and calls only the door. No re-entry, and the reference is the very value the
   function was about to return.

## Acceptance with the check compiled in and its flag off

```
pre-flight   11 of 11 components
decode       62.78 / 62.97 / 63.30     median 62.97
prefill      2700.8 / 2962.1 / 2961.6  median 2961.6
VRAM         9648 MiB
hash         c2ff42cc1219
```

The diagnostic costs a `getenv` read cached in a static when off, which is why the numbers match the baseline rather
than trail it.

## What this does and does not establish

Establishes: the ported `bt_token_to_piece` reproduces the native piece for every token of this model, measured by
the shipped library on its own vocabulary.

Does not establish: that the runtime **uses** the ported detokenizer. It still calls the native one, and the door is
exercised only under the diagnostic flag. Replacing the native path is a separate decision with its own acceptance,
and it is not taken here.
