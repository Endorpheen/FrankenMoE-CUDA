# EXP-2026-11-17-925: the full Goose detokenizer inside the shipped library

Status: ACCEPTED for the piece path; tokenisation itself still C++.
Base: EXP924, d73a1285.

## Hypothesis fixed before changes

The accepted COMPLETE Goose detokenizer (EXP920) can answer `token_to_piece`
inside a real `libllama`, replacing the C++ body of that path, with the same
bytes for every vocabulary entry — and the replacement can be shown to be live
rather than merely linked, by a mutant that must be detected.

## What was done

Source of truth first, because the older `runtime/reconstructed` copy is not
automatically current:

- the accepted bundle's `libllama.so.0` (sha256 `4eba8242…`) was traced to
  `franken-recovery-20261116/link-fixed/`, linked from
  `franken-recovery-20261116/core-clean/*.o`;
- that object set was compiled from
  `runtime-build-tree-v8/our-src/llama-vocab.cpp` (26 `fg202t_*`/`fg213p_*`
  references), not from the vendored `vendor/flint` copy, which carries none;
- the candidate therefore starts from `runtime-build-tree-v8/our-src` (1.8 MiB),
  copied to `exp925-tokenizer-wire-20261117/our-src`. The accepted tree and the
  bundle were only read.

Then, in the candidate only:

1. the EXP920 module set (unicode tables, UTF-8, QWEN35 pre-tokenizer, BPE tables
   and tokeniser, detokeniser) is composed and compiled into a self-contained
   object exactly like the existing `fg202t.o`: `goose --standalone`,
   `gcc -Dgoose_init=fg920_goose_init`;
2. `llama-vocab.cpp` gains `goose_prepare()`, which flattens the vocabulary texts
   into one buffer and builds the Goose token table over it, and the piece path
   calls `bt_token_to_piece` instead of the C++ copy;
3. the library is relinked from the same object set with the new `llama-vocab.o`
   and `fg920.o`.

## Results

| check | result |
| --- | --- |
| object ABI | declarations mirror the generated header (`sl_u8`/`sl_i64`); first attempt used a different prototype and the Goose call silently refused with `goose_init must be called before exported functions`, which was then fixed by calling `fg920_goose_init` once |
| link | `-Wl,-z,defs` passes; `libllama-exp925.so.0` exports `bt_token_to_piece` |
| piece parity, all 248320 ids, `special` both ways | **5 673 158 bytes byte-identical** to the accepted library (`accepted.bin` and `candidate.bin`, sha256 `51eb6c74…`) |
| the path is live | mutant that inverts `special` in the Goose call is **DETECTED**; the same mutant against the previous (inactive) build was not, which is how the inactive path was found in the first place |

C++ replaced and removed: 0 lines yet - the C++ copy is still present as the
fallback for vocabularies the Goose table cannot serve. Goose lines answering the
path: the whole `token_to_piece` decision, ~90 lines of Goose.

## Boundaries

This is the piece path only. `llama_tokenize` (the BPE tokeniser itself) still
runs in C++; that is the next slice. The C++ body of `token_to_piece` is not yet
deleted, because the Goose table is built from the vocabulary and its refusal
path is only exercised on this model. No speed claim; no server or GPU run.

## Artifacts

- `source/llama-vocab-exp925.patch` - the C++ change, 112 lines of diff;
- candidate library `libllama-exp925.so.0` sha256 `4f37566d…`;
- Goose object `obj/fg920.o` sha256 `8f3b208a…`;
- gate tool `pieces.cpp` (dumps length-prefixed pieces for every id).
