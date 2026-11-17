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
