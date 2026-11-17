# P7-06 / P7-07: tokenizer and detokenizer coverage

This note records what the vocabulary requirement actually contains after
EXP-2026-11-17-920, and what is still open. It replaces the earlier reading in
which the requirements were closed by a C++ driver that called Goose decisions.

## P7-06 slices (vocabulary loader, tokenizer)

| # | slice | state | evidence |
| --- | --- | --- | --- |
| 1 | vocab type and pre-tokenizer dispatch | done | EXP622, `goose/model/vocab-type.goose`, 2556 cases / 224908 record bytes |
| 2 | special-token partition | done | EXP622, `goose/model/special-partition.goose`, 443 cases / 1469 fragments |
| 3 | codepoint flags, lowercase map, byte maps | done | EXP920, generated from `src/unicode-data.cpp` sha256 `5c80b44db0958e67`, 3512 ranges + 1433 entries |
| 4 | UTF-8 decode with native recovery, encode | done | EXP920, invalid 2/3/4-byte corpus |
| 5 | QWEN35 pre-tokenizer and byte encoding | done | EXP920, per-branch port of `unicode_regex_split_custom_qwen35` |
| 6 | token and merge tables over borrowed spans | done | EXP920, last-wins / first-wins, 248320 / 247587 entries, 0 duplicates |
| 7 | BPE merge loop | done | EXP920, native rank/left queue order, stale-pair rejection |
| 8 | attribute passes and special-token order | done | EXP920, token_type, FIM, unused, EOG, gpt-oss |
| 9 | end-of-generation set | done | EXP920, 5 ids, all 248320 candidates agree |
| 10 | wire the Goose tokenizer into the shipping path and delete the C++ body | **open** | not started; the shipping server still tokenizes in C++ |

Additional open item outside the slice list: the model-name attribute overrides
(phi-3, modern-bert, `<mask>`) are not ported, because they are keyed on names
this model does not carry. They are needed before the requirement can claim
general vocabulary support.

## P7-07 slices (detokenization)

| # | slice | state | evidence |
| --- | --- | --- | --- |
| 1 | token_to_piece, all attribute paths | done | EXP920, 496640 pieces (every id, `special` both ways) exact |
| 2 | detokenize: lstrip, capacity protocol, clean_spaces input | done | EXP920, 140 calls over the corpus, `remove_special`/`unparse_special` both ways |
| 3 | wire into the shipping path and delete the C++ body | **open** | not started |

## What the gate proves

`libc`-only Goose library against the native tokenizer on the same GGUF: 70
tokenize cases / 11017 ids, 496640 pieces, 140 detokenize calls and the whole
end-of-generation set agree exactly. Mutants that break the merge validity, the
byte decode, the contraction rule, the end-of-generation text list or the token
id lookup are all rejected, so the gate is sensitive to the decisions it covers.

## What it does not prove

- The shipping binary is unchanged: Apex still tokenizes through the native
  llama-vocab.cpp body. "Ported" here means the Goose module holds the decision
  and answers correctly; "replaced" still requires the wire-up and the removal.
- Only the BPE family with the QWEN35 pre-tokenizer is exercised end to end.
  The other pre-tokenizer matchers and the SPM/WPM/UGM/RWKV families are still
  native code; the dispatch for them lives in `vocab-type.goose`.
