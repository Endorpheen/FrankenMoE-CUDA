# EXP-2026-11-18-954: the vocabulary is built from the Goose reader

Status: functional PASS, 12 checks with a positive witness of 270 Goose
answers. The C++ body is not deleted; this is the first shipping path where
Goose actually decides.

## Hypothesis fixed before changes

Every piece existed after EXP953 - the reader, its tracked adapter, the unit in
`libllama.so.0` - but nothing called any of it. If the loader registers the file
and the vocabulary reads go through the adapter, the vocabulary built from Goose
must be identical to the one built natively, tokenisation included. A linked but
unused path would show no behaviour change and no query count, which is why the
gate also counts answers.

## What was done

- `llama-model-loader.cpp` registers the file with the reader right after the
  gguf context is created, and a new destructor unregisters it. Without the
  unregister a later context at the same address would answer from a stale
  mapping.
- Small inline helpers in the loader and in `llama-vocab.cpp` route each question
  through the adapter when it is ready and fall back to `gguf_*` otherwise, so a
  file the reader refuses keeps the old behaviour.
- Switched reads: token count, token texts, token types, merge count, merge
  texts, and the kv/arr type checks on those two arrays. That is the whole
  vocabulary-construction input, 14 changed lines.
- The adapter gains a counter incremented by every answer it gives, exported as
  a C symbol so a test can tell "Goose decided" from "Goose was linked".

## Result

| check | control (accepted lib) | this build |
| --- | --- | --- |
| build exit code | - | **0** |
| `libllama.so.0` bytes | 5 766 688 | **5 980 360** |
| vocab load, 260 tokens | PASS | **PASS** |
| gate checks | 11 / 0 failures | **12 / 0 failures** |
| Goose answers during load | 0 (no adapter) | **270** |
| `abc` -> ids | 1 (merged token) | **1 (merged token)** |
| `abc` round-trip | yes | **yes** |

The control run matters: the first fixture had a duplicated token text, which
makes `load` assert `id_to_token.size() == token_to_id.size()`. The *accepted*
library failed the same way on it, which proved the assert was the fixture's
fault, not this change. The fixture was fixed and the control now passes, so the
comparison is between two working builds.

## Not claimed

The C++ bodies remain. `llama-vocab.cpp` still builds `token_to_id`,
`id_to_token`, `bpe_ranks` and does the tokenising itself; Goose supplies the
metadata answers, not the tokenisation. Deleting the body needs the piece and
tokenize paths to call `btl_load`/`bt_bpe` directly, which is the next slice.
P7-06/P7-07/P7-12 stay PARTIAL.

No model was loaded: the fixture is 4 168 bytes of metadata and the load runs in
`vocab_only` mode, so no weights, no graph, no GPU.

## Reproduction

    OUT=/home/end0/AI/LlamaServer/exp954-vocab-wire-20261118
    python3 experiments/EXP-2026-11-18-954-goose-vocab-wire/tests/make_vocab.py "$OUT/vocab.gguf"
    python3 runtime/reconstructed/build.py --build-dir "$OUT/build3" \
        --runtime-libs /home/end0/AI/LlamaServer/apex-independent-fast-20261116/run
    g++-13 -O1 -std=c++17 -w experiments/EXP-2026-11-18-954-goose-vocab-wire/tests/gate.cpp \
        -Ivendor/flint/include -Ivendor/flint/ggml/include \
        "$OUT/build3/libllama.so.0" <accepted>/libggml-base.so.0 ... -o "$OUT/gate3"
    "$OUT/gate3" "$OUT/vocab.gguf"

Logs: `$OUT/gate3.log`, `$OUT/build3.log`.
