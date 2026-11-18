# EXP-2026-11-18-955: the BPE tokeniser itself runs in Goose

Status: functional PASS with a positive witness. The C++ session still exists as
the fallback and is still the code for every other pre-tokenizer.

## Hypothesis fixed before changes

EXP954 moved the vocabulary *build* to Goose but left tokenisation in C++. If the
engine's QWEN35 pre-tokenizer and merge queue are exact, routing a raw fragment
through `bt_bpe` should produce the same ids as the native session - and the
witness counter should move, which is what separates "Goose decided" from "the
path was linked and the native code ran".

## What was done

- The adapter builds the BPE tables once per file with `btl_load` during register:
  it sizes the token and merge arrays from the reader, picks power-of-two hash
  capacities above twice the entry counts, and holds the offsets, lengths, slots,
  pairs, token types, attributes, special order and eog list for the model's life.
- New adapter entry `llama_goose_tokenize(ctx, text, len, out, cap)` runs one raw
  fragment through `bt_bpe` with scratch sized from the fragment length, exactly
  as the gated EXP920 corpus did.
- New accessor `llama_goose_tokenizer_ctx()` returns the one registered file that
  has usable tables. The vocab object does not carry the gguf pointer, and one
  model means one answer, so nothing is threaded through C++ state.
- `llama_vocab::impl::tokenize` gains the hook in the BPE branch only. It is used
  when the pre-tokenizer is `QWEN35`, `escape_whitespaces` is off (the engine
  expects raw text) and the model is `gpt2`; on any engine refusal the native
  session runs, so behaviour cannot get worse.

## Result

| check | control (accepted lib) | this build |
| --- | --- | --- |
| build exit code | - | **0** |
| `libllama.so.0` bytes | 5 766 688 | **5 988 824** |
| vocab load | PASS | **PASS** |
| Goose answers during load | 0 | **271** |
| Goose answers during tokenize | 0 | **1** |
| `abc` -> ids | 1 (`abc`, id 257) | **1 (`abc`, id 257)** |
| gate checks | 13 with 2 expected failures | **13 / 0 failures** |

The control run is the useful half: the same fixture on the accepted library
produces the same ids with zero Goose answers, and its two failures are exactly
the witness checks ("Goose answered", "tokenisation went through Goose"). That
proves the ids do not depend on who computes them, and that the witness can tell
the two paths apart.

## Not claimed

The C++ `llm_tokenizer_bpe_session` body is untouched and still runs for every
other pre-tokenizer and for any file the engine refuses. Nothing was deleted, so
P7-06/P7-07/P7-12 stay PARTIAL. The hook covers QWEN35 only; Apex uses it, other
models keep the native path until their pre-tokenizers are ported.

No model was loaded: the fixture is 4 167 bytes of metadata in `vocab_only` mode.

## Reproduction

    OUT=/home/end0/AI/LlamaServer/exp955-goose-bpe-20261118
    python3 experiments/EXP-2026-11-18-954-goose-vocab-wire/tests/make_vocab.py "$OUT/vocab.gguf"
    python3 runtime/reconstructed/build.py --build-dir "$OUT/build" \
        --runtime-libs /home/end0/AI/LlamaServer/apex-independent-fast-20261116/run
    g++-13 -O1 -std=c++17 -w experiments/EXP-2026-11-18-954-goose-vocab-wire/tests/gate.cpp \
        -Ivendor/flint/include -Ivendor/flint/ggml/include \
        "$OUT/build/libllama.so.0" <accepted>/libggml-{base,cpu}.so.0 ... -o "$OUT/gate"
    "$OUT/gate" "$OUT/vocab.gguf"

Logs: `$OUT/gate.log`, `$OUT/control.log`, `$OUT/build.log`.
