# EXP-2026-11-17-937: GGUF string arrays read by Goose, with the terminator

Status: ACCEPTED for the array path.

Base: EXP925 (candidate `exp925-tokenizer-wire-20261117`), which had the arm
wired but broken.

## Hypothesis

The earlier array slice reduced the GGUF calls from 495921 to 14 and broke the
vocabulary because GGUF strings are **length-prefixed and not NUL-terminated**:
the C++ consumer receives a pointer into the header mapping and treats it as a
C string, so it reads the next entry's length word. Producing NUL-terminated
text where the length is known must restore the vocabulary without giving up the
Goose path.

## Rule compliance

No new C++ logic. The layout walk, the bounds checks, the copy and the
terminator live in Goose (`source/reader.goose`, `gr_arr_str_table`). The C++
side only supplies storage and returns the entry pointer; it makes no decision
about the file format.

## What changed

1. `gr_arr_str_table(data, state, kv, index, text, offs)` — one pass over the
   array: validates the entry is a string array, lays every element out back to
   back in `text` with a terminator after each, and writes the element offset
   into `offs`. Answers `-(bytes + 1)` when `text` is too small and `-2` when
   `offs` is too small, so the caller only sizes buffers.
2. `llama_goose_arr_str` calls it once per array and caches the table for the
   model's life. The per-element walk that existed before was O(n^2) across a
   248320-entry vocabulary and hung the load; a single pass removed it.

## Results

Counters against the accepted `libllama.so.0` (`4eba8242`), same model:

| call | accepted | candidate |
| --- | ---: | ---: |
| `gguf_find_tensor` | 753 | **0** |
| `gguf_find_key` | 94 | **1** |

The single remaining `gguf_find_key` is `general.alignment` inside
`gguf_init_from_file`, which is the next slice.

Gates, candidate vs accepted, byte-identical:

| gate | result |
| --- | --- |
| `pieces` (both special/non-special for every id) | identical, 5673158 bytes |
| `tokdump` (corpus, both modes) | identical, 2996 bytes |
| `sampdump` (3 configurations x 4 seeds) | identical, 1536 bytes |

Live-path counters on the candidate: `FG213P_PIECE_COPY calls=255585`,
`FG217C_UNICODE_CPTS calls=512404`, `FG212S_SAMPLER top_p=384 top_k=384` — the
Goose tokenizer, detokenizer and sampler are the ones answering.

## Reproduce

    bash build/compile-candidate.sh
    python3 build/link-candidate.py <candidate-dir> <out.so>
    LD_LIBRARY_PATH=<candidate>/run-cand ./pieces <model.gguf> out.bin

## Boundaries

The C++ bodies of the tokenizer, detokenizer and sampler are still compiled and
remain the fallback for vocabularies the Goose table refuses; they are not
deleted here. No speed claim. The shipping library is unchanged; this is a
candidate.

## Remaining in this area

- `general.alignment` lookup inside `gguf_init_from_file`;
- `gguf_kv_to_str` (metadata logging during load);
- the remaining `gguf_get_val_*` accessors;
- replacing `gguf_init_from_file` with the Goose parser.

## Tokenizer engine: vocabulary laid out in Goose

The standalone runtime lists "no tokenizer engine" as a gap: the 8 `tok_*`
decisions exist, but the load-and-tokenise path around them does not. The piece
that was missing was the vocabulary layout, which lived in C++ (`goose_prepare`
in `llama-vocab.cpp` built the blob, the offsets, the lengths, the slot sizes and
the merge block).

`goose/model/tokenizer-engine.goose` now does that from the parsed header:

- `bt_prepare_need` — reads both string arrays through the Goose reader and
  answers the blob bytes, the entry counts and the slot sizes;
- `bt_prepare_from_gguf` — writes the token texts followed by the merge texts
  into one buffer, records every offset and length, and zeroes both hash tables
  in the shape `bt_build` expects.

The host only allocates buffers and calls `bt_build`/`bt_bpe`. Nothing in the
engine consults the C++ vocabulary.

### Compiler constraint found

An exported Goose function cannot also be called from inside the same unit: the
C ABI specialization and the internal one compete and the compiler answers
"could not resolve one concrete function signature". Both `gr_arr_str_span` and
`bt_prepare_need` are therefore split into a non-exported body (`gr_span_impl`,
`te_need_impl`) plus a one-line export wrapper. The reader keeps its previous
behaviour: the candidate still passes pieces and tokenize byte-identically.

## Standalone engine gate

`integration/goose-standalone/tests/tokenizer_engine.c` runs the path with no
llama.cpp vocabulary at all: header -> Goose reader -> `bt_prepare_from_gguf`
-> `bt_build` -> `bt_token_to_piece` for every id in both modes, written as
length-prefixed records for byte comparison against a dump produced through the
accepted libllama.

    FG_TOKENIZER_ENGINE tokens=248320 merges=247587 blob=5972470 bytes=3686598 refused=0 build_rc=0
    ENGINE PIECES IDENTICAL 5673158 bytes

md5 `ce130048d0dfef254bc72fa0042fc4c5` on both sides. The vocabulary layout, the
hash tables and every piece are therefore answered without libllama.

Reproduce: `bash build/engine-gate.sh <model.gguf> <reference.bin> [outdir]`.

### Second O(n^2) removed

The first engine walk asked the reader for the span of element i for every i,
which re-walks the array each time and hung on 248320 entries. `te_array_bytes`
and `te_fill` now walk each array once, reading the length prefix and advancing,
and the unused span helper is gone.

## Tokenisation through the engine

The same gate also compares ids with libllama on a corpus, one line at a time:
the line goes into `bt_bpe`, which runs the Qwen3.5 pre-tokenizer itself and
then the merges.

    FG_TOKENIZER_ENGINE tokenize cases=28 ref_ids=355 goose_ids=355 mismatches=0

Two defects were fixed to get there:

- `bt_bpe` pre-tokenizes internally, so it takes the whole line; slicing the line
  by hand and passing encoded bytes made it encode twice and lose ids;
- the hash tables must be handed over at the power-of-two size the layout
  computed, not at `2 * n`, or the probing misses entries.

Only the non-special path is compared: special-token matching is still C++.

## Special tokens, and with them the whole tokenize path

`bt_tokenize` now matches special tokens first — longest text first, using the
order `bt_special_order` already produced — then sends each gap through
`bt_bpe`. The comparison covers both `parse_special` modes:

    FG_TOKENIZER_ENGINE tokenize cases=56 ref_ids=685 goose_ids=685 mismatches=0

The corpus carries `<|im_start|>`, `<|im_end|>` and the negative case
`<|not_a_token|>`, so both the recognised and the unrecognised text are
exercised. Pieces stay byte-identical.

`bt_bpe` needed the same impl/export split as the reader functions: an exported
function that is also called inside the unit cannot resolve one concrete
signature, so the body is `bt_bpe_body` and the export is a one-line forward.
