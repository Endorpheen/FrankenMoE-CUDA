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

`utf8_len` is routed into `unicode.cpp`; the BPE functions are gated but not yet wired, because
their call sites are inside the tokenizer class and need the same copy treatment as the loader.

## Evidence

    FG202T_TOKENIZER utf8_len=6 bigram=0 native=0
    tokens and all 128 top-5 probabilities equal, 2553.44 / 26.13

Six calls in this request: the prompt is ASCII, so almost every lead byte is a single-byte
sequence. Small but live, and `tokens_equal` proves the tokenization is unchanged.

Gate: 1161 checks / 0 failures, including all 256 byte values for `utf8_len` against the native
table.

## Note on the link

The first integrated run showed no witness because `unicode.cpp.o` was still the native object: the
link substitution listed the other replaced TUs but not this one. Adding it produced the witness.

## Switch

`GGML_GOOSE_TOKENIZER=0` restores the native table.
