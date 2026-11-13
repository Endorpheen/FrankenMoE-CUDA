# EXP458: bounded reader and file I/O decisions in Goose (P9-02 second slice)

Every metadata and tensor read goes through `gguf_reader`, which decides whether
a request fits, how much one chunk transfers, and how EOF changes the remaining
count. This experiment moves those decisions into Goose.

## What was ported

`source/io.goose`, from `ggml/src/gguf.cpp` (`gguf_reader::read`,
`read(string)`, `read(vector)`, `seek`, `read_raw`, `file_remain`):

| decision | rule |
|---|---|
| scalar read | accepted when `size <= remaining` |
| array read | reject above `1 << 30` elements, reject when `n * elem` overflows, reject when it exceeds the remaining bytes |
| string read | reject above `1 << 30` bytes, then reject when longer than the remaining bytes |
| seek | `absolute <= data_offset + remaining`, otherwise rejected; the new remaining count is the distance to the window end |
| chunked transfer | copy `min(max_chunk, want)` until done, clamp at the file end, and set the remaining count to zero once EOF is reached |
| file_remain | `end - current`, zero when either position is unknown |

The byte copy, the `FILE*`/callback and the path handling stay native.

## Gate

`tests/io_fixture.cpp` transcribes `gguf_reader` over an in-memory buffer with
the same callback semantics and drives the same operation sequence through both
arms, comparing success, `data_offset`, `nbytes_remain` and the delivered length.

| corpus | cases |
|---|---|
| fit table, array/string limit tables (including element sizes that make the overflow branch reachable) | 1 000+ |
| seek boundaries at `end-1`, `end`, `end+1` for three window shapes | 27 |
| 200 randomised 40-operation sequences (read / string read / seek / short read), half of them over-claiming the remaining count so EOF leaves bytes behind | 8 000 |
| `file_remain` table | 18 |
| total | **8 275 cases, 19 629 values, 0 failures** |
| ASan + UBSan (module and fixture) | 0 failures, rc 0 |

Mutations 7/7 caught: scalar fit becomes exclusive (8 failures), array element
cap dropped (50), array overflow guard dropped (18), byte check becomes exclusive
(7), string length cap dropped (6), seek rejects the exact end (15), EOF leaves
the remaining count untouched (833).

## Corrections made while gating

- **`const` declarations become module globals.** The first build returned
  nonsense because `MAX_ARRAY`/`MAX_STRING` live in `gg_init_globals`, which only
  `main` calls. The unit wrapper now runs it from a constructor. Any future
  module using `const` needs the same boot step.
- **Three candidate mutations were unobservable, not "caught".** The element cap
  is checked before the overflow guard, so the guard needs element sizes above
  `1 << 33` to be reachable; the corpus now includes `1 << 40` and `1 << 60`.
  Random reads never reached EOF with bytes left over, so half the sequences now
  over-claim the remaining count. Two mutations were dropped instead of counted:
  the transfer chunk size and the empty-tail `file_remain` case cannot change the
  returned state, so they are covered by construction, not by a mutation.
- **`seek` takes `uint64_t` natively**, so a negative target is rejected; the
  transcription had used a signed comparison and the gate caught the difference
  between the two arms.

## Scope

P9-02 still needs mmap/pinned allocation, shard offsets and endian handling. It
stays IN_PROGRESS.

## Reproduce

    bash experiments/EXP-2026-11-13-458-p9-02-file-io/build_fixture.sh
    python3 experiments/EXP-2026-11-13-458-p9-02-file-io/mutate.py
