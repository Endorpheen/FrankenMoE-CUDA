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

## Second slice: the BPE tokeniser itself

`llama_tokenize` now calls the Goose BPE (`bt_bpe`) for every raw text fragment,
after the C++ special-token partition. The tables are built once from the
vocabulary: 248320 token texts and 247587 merges flattened into one buffer, with
the merge texts appended after the token texts because `bt_build` addresses both
in a single span space.

Two defects were found by the gate itself, both mine:

- the first attempt passed the merge spans against the token buffer, and
  `bt_build` refused with rc -2 (offset past the end). The buffers are now one,
  with the merge offsets shifted by the token length;
- a first mutation control (a corrupted merge rank, then half the text) reported
  "not detected" - because the Goose branch was inactive and the C++ fallback was
  answering. The log line `BPE branch ready=` showed `0`; the cause was the rc -2
  above. With `ready=1` the same class of mutation is caught.

Results, after the fix:

| check | result |
| --- | --- |
| corpus of 31 cases x parse_special on/off | **687 ids byte-identical** to the accepted library (`tok_accepted.bin` = `tok_candidate.bin`, sha256 `86193663…`) |
| tables | 248320 tokens, 247587 merges, `bt_build` rc 0 |
| the path is live | mutant adding 1 to every emitted id is **DETECTED** |
| fallback | `bt_bpe` returning <0 still falls back to the C++ session, so no vocabulary is lost |

## Third slice: the draw itself

`llama_sampler_dist_apply` now hands the whole draw to the Goose chain
(`sm_select`): the generator is the Goose MT19937 seeded from `seed_cur`, the
softmax and the cumulative selection are Goose, and the C++ side keeps only the
candidate array and the interface.

| check | result |
| --- | --- |
| 3 configurations (k20/p0.95/t1, k40/p0.9/t0.8, k100/p0.5/t1.2) x 4 seeds x 32 draws | **384 draws identical** to the accepted library (`samp_accepted.bin` = `samp_candidate.bin`, sha256 `4bae8efe…`) |
| the path is live | mutant that forces `top_k = 1` in the Goose call is **DETECTED** |
| fallback | a non-zero `sm_select` return falls through to the existing C++ branches |

The C++ `std::mt19937` for this sampler is now unused for the draw; it is still
kept for the other branches until they are ported, so nothing was deleted yet.

## Fourth slice: the whole chain, not just the draw

Every step of the sampler chain now runs on Goose: `top_k` (partial sort and the
re-ordering of the candidate array), `top_p`, `min_p`, `temp` and the draw, with
C++ keeping only the array and the interface. The Goose module gained
`sm_apply_top_k`, `sm_apply_top_p`, `sm_apply_min_p` and `sm_apply_temp` for the
host that applies steps one by one; the unit-level chain still exists and is
covered by its own gate.

| check | result |
| --- | --- |
| 384 draws, 3 configurations x 4 seeds x 32 | **identical** to the accepted library (`samp_candidate2.bin` = `samp_accepted.bin`, sha256 `4bae8efe…`) |
| tokenizer gate re-run after the sampler change | still identical (`tok_candidate2.bin` = `tok_accepted.bin`) |
| the new steps are live | mutant scaling `top_p` by 0.9 is **DETECTED** |
| fallback | each step falls back to the previous C++ body when the Goose call refuses |

## End to end through the shipped API

The candidate library, loaded by the same generation harness used for EXP922,
reproduces the accepted run exactly:

    PROMPT ids=5: 760 6511 314 9338 369          (both)
    GENERATED ids=8: 11751 11 264 3177 8807 303 3712 321   (both)
    text: " Paris, a city rich in history and"  (34 bytes, both)

So the integration is not only correct step by step: a real greedy generation
through libllama's own tokenizer, decoder and sampler loop produces the same ids
and the same text when those three pieces are answered by Goose.

## Fifth slice: the Goose reader answers the loader's tensor lookup

The earlier attempt failed because the loader object was compiled with
`-I vendor/flint/src`, so the patched header in the working copy never took part.
Compiling `llama-model-loader.cpp` with the working tree's headers first makes the
wiring real, and the proof is a counter, not a claim:

| build | `gguf_find_tensor` calls during a load of the two-block model |
| --- | ---: |
| Goose lookup enabled | **0** |
| Goose lookup refused (`EXP925_MUTATE_LOOKUP=1`, falls back to gguf) | **38** |

Both loads succeed with the same parameter count (2696067776). So every tensor
lookup on the model-load path is answered by the Goose reader (EXP919) from a
64 MiB header mapping registered next to the metadata context, and the C gguf
lookup is not reached at all in the normal case.

The index itself was checked against `gguf` over the full Apex file first: 753 of
753 tensors resolve with 0 offset/type differences and the same 52 metadata
entries (`ggufcmp`).

Gates with the reader in place: pieces OK, tokenize OK, sampler OK - all
byte-identical to the accepted library - and greedy generation on the two-block
model is unchanged (107194 169222 16544 2280 on both).

## Sixth slice: metadata keys as well as tensors

`gguf_find_key` is now answered by the Goose reader too, in the loader, the
vocabulary loader and the model builder, with the C gguf lookup used only when no
Goose table exists for that context. The indices are interchangeable: over the
real Apex file the Goose index equals `gguf`'s for **52 of 52 keys** (`ggufcmp`).

| build | `gguf_find_tensor` | `gguf_find_key` |
| --- | ---: | ---: |
| before this slice | 0 | 94 |
| Goose keys enabled | 0 | **1** |
| Goose keys refused (`EXP925_MUTATE_KV=1`) | 0 | 94 |

The single remaining call is inside `gguf_init_from_file` itself (`general.alignment`),
i.e. in the C parser that is not yet replaced. With Goose keys enabled but the
Goose key lookup refused, the load fails with "unknown model architecture: ''" -
which is the direct proof that the architecture and every hparam now come from the
Goose reader.

Gates unchanged: pieces OK, tokenize OK, sampler OK (byte-identical to the
accepted library) and greedy ids on the two-block model identical.

## Boundaries

`llama_tokenize` now runs on Goose for BPE vocabularies whose merges fit the
table; the C++ bodies are still compiled and remain the fallback, so no C++ has
been removed yet. Sampler, session/CLI, backend and server paths are untouched. The C++ body of `token_to_piece` is not yet
deleted, because the Goose table is built from the vocabulary and its refusal
path is only exercised on this model. No speed claim; no server or GPU run.

## Artifacts

- `source/llama-vocab-exp925.patch` - the C++ change, 112 lines of diff;
- candidate library `libllama-exp925.so.0` sha256 `4f37566d…`;
- Goose object `obj/fg920.o` sha256 `8f3b208a…`;
- gate tool `pieces.cpp` (dumps length-prefixed pieces for every id).
