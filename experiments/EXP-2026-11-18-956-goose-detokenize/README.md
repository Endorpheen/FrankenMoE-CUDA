# EXP-2026-11-18-956: detokenisation runs in Goose

Status: byte-identical to the native path over the whole vocabulary, with a
positive witness. The C++ body is untouched and still serves other vocabularies.

## Hypothesis fixed before changes

After EXP955 the tokenisation goes through Goose but `token_to_piece` still
decodes in C++. If the engine's attribute pass, GPT-2 byte decode and lstrip rule
are exact, every id must produce the same bytes as the native body - and the
witness counter must move, so "Goose decided" is distinguishable from "the native
body ran".

The cache matters here: `cache_token_to_piece` is filled at load by calling
`token_to_piece` itself, so a hook placed after the cache would be bypassed for
every id the cache holds. It is placed before the cache on purpose, which means
the cache is also built from the Goose path.

## What was done

- The adapter gains `llama_goose_token_to_piece(ctx, token, buf, length, lstrip,
  special, escape_ws)`, which calls `bt_token_to_piece` with the borrowed token
  offsets, lengths and attributes it already holds.
- `llama_vocab::impl::token_to_piece` calls it when the vocabulary is BPE, the
  pre-tokenizer is QWEN35 and `escape_whitespaces` is off - the same condition as
  the tokenize hook. A refusal falls through to the native body.
- One duplicate `btl_load` declaration from the previous cycle was removed.

## Result

| check | control (accepted lib) | this build |
| --- | --- | --- |
| build exit code | - | **0** |
| `libllama.so.0` bytes | 5 766 688 | **5 988 960** |
| vocabulary size | 260 | **260** |
| Goose answers before any dump | 0 | **531** (cache build) |
| Goose answers after special dump | 0 | **791** |
| Goose answers after plain dump | 0 | **1050** |
| piece dump, special=true | 1311 bytes | **1311 bytes, identical** |
| piece dump, special=false | 1304 bytes | **1304 bytes, identical** |
| `abc` (257) decodes to | `abc` | **`abc`** |

`cmp goose.bin native.bin` reports no difference: every id's piece, under both
`special` settings, is the same byte sequence from both libraries. The dump is
length-prefixed per piece, so an empty piece cannot hide behind a neighbour.

The counter proves the path is live: 531 answers during the cache build (two per
id, the load pass and the cache pass) and 519 during the two dumps, which is not
a plausible count for a path that was only linked.

## Not claimed

The C++ body remains and still runs for SPM, WPM, UGM, RWKV and every BPE
pre-tokenizer other than QWEN35. Nothing was deleted, so P7-06/P7-07/P7-12 stay
PARTIAL. The hook covers Apex, not all models.

No model was loaded: the fixture is 4 KB of metadata in `vocab_only` mode.

## Reproduction

    OUT=/home/end0/AI/LlamaServer/exp956-goose-detok-20261118
    python3 runtime/reconstructed/build.py --build-dir "$OUT/build" \
        --runtime-libs /home/end0/AI/LlamaServer/apex-independent-fast-20261116/run
    "$OUT/gate"         "$OUT/vocab.gguf" "$OUT/goose.bin"
    "$OUT/gate-control" "$OUT/vocab.gguf" "$OUT/native.bin"
    cmp "$OUT/goose.bin" "$OUT/native.bin"

The control needs a stub for the witness symbol, since the accepted library has
no adapter: `extern "C" int llama_goose_queries = 0;` in a small C++ object.
