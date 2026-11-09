# EXP202: tokenizer decisions in Goose (APX: tokenizer)

First step into the tokenizer, separate from the loader work.

## Ported

| Goose | native | behaviour |
|---|---|---|
| `utf8_len` | `unicode.cpp:16` | the lead byte's high nibble selects 1/2/3/4 |
| `bigram_valid` | `llama-vocab.cpp:147-151` | skip a queued merge when a side was already merged |
| `try_add_allowed` | lines 203-216 | both indices exist, the token exists, the id is in range |
| `resegment_kind` | lines 177-199 | token / merged pair / raw bytes |
| `clamp_symbol_len` | line 126 | the last symbol may be a truncated sequence |

`utf8_len` is routed into `unicode.cpp`; `clamp_symbol_len`, the SPM `bigram_valid` /
`try_add_allowed` / `resegment_kind`, and the GPT-2 BPE predicates are routed into
`llama-vocab.cpp`.

## Second pass: the tokenizer Qwen actually uses

The first integrated run reported `bigram=0`, which was the useful signal: Qwen does not use the
sentencepiece path. The predicates that matter are in `llm_tokenizer_bpe_session`
(`llama-vocab.cpp:596-810`), so a second set was ported and wired:

| Goose | native | behaviour |
|---|---|---|
| `bpe_add_allowed` | lines 784-797 | queue a pair only when both neighbours exist and a rank was found |
| `bpe_merge_valid` | lines 715-724 | drop a merge whose sides were merged away or whose text went stale |
| `bpe_ignore_merges` | line 686 | `ignore_merges && text_to_token(word)` |

`concat_matches` (the `left_token + right_token == bigram.text` test) is computed in C++, because
Goose has no string type; the decision built on it is Goose's. Same for the rank lookup.

## Evidence

    FG202T_TOKENIZER utf8_len=6 bigram=12 native=0
    tokens and all 128 top-5 probabilities equal, 2551.43 / 25.95

Six calls in this request: the prompt is ASCII, so almost every lead byte is a single-byte
sequence. Small but live, and `tokens_equal` proves the tokenization is unchanged.

Gate: 1965 checks / 0 failures, including all 256 byte values for `utf8_len` against the native
table and 800 randomised BPE predicate cases.

## Note on identification

Body deduplication means three two-or-three-argument functions can share signatures. Rather than
guess from arity, each was called with a distinguishing argument set and the outputs compared to
the expected truth table (f8 answered `rank < 0` as false, f9 as true). The probe wrappers were
then removed.

## Note on the link

The first integrated run showed no witness because `unicode.cpp.o` was still the native object: the
link substitution listed the other replaced TUs but not this one. Adding it produced the witness.

## Switch

`GGML_GOOSE_TOKENIZER=0` restores the native table.
