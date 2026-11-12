# EXP453: stream session buffer policy in Goose (P8-02 sixth slice)

Resumable streaming keeps a bounded byte window per conversation. This
experiment moves the retention arithmetic and the read decision into Goose.

## What was ported

`source/stream.goose`, from `server-stream.cpp`
(`stream_session::append`, `stream_session::read_from`):

1. **Retention**: a session keeps at most `cap` bytes of the newest data.
   - an empty chunk is accepted even after finalize (native returns true before
     it looks at `done`);
   - a finalized session rejects non-empty chunks;
   - a chunk at least as large as the cap keeps only its tail and counts the
     rest, including the previous buffer, as the dropped prefix;
   - an overflow drops the oldest bytes and counts them in the dropped prefix.
2. **Read decision**: stop request wins, then an offset below the dropped prefix
   is OFFSET_LOST, then the whole available tail is one chunk, then a finalized
   session ends the read, otherwise the reader would block.

The returned values are `(accept, new_buf_len, dropped_delta, src_skip)` for the
append and `(status, bytes)` for the read. Mutexes, condition variables, the ring
buffer storage and the sink callbacks stay native; they are the threading
boundary of this slice.

## Gate

`tests/stream_fixture.cpp` transcribes the native code with `std::vector` and
runs six capacities (1, 7, 64, 1000, 4096, 4 MiB) through 200 chunk steps each,
with chunk sizes `0, 1, cap-1, cap, cap+1, 2*cap` and random lengths, finalizing
at a fixed step. After every step it compares the retained bytes, the dropped
prefix, the total size and the accept flag; then it compares read decisions for
eleven offsets around the boundaries with and without a stop request.

| check | result |
|---|---|
| append steps and read decisions | **1 464 cases, 5 064 values, 0 failures** |
| ASan + UBSan (C and C++ objects) | 0 failures, rc 0 |

Mutations 8/8 caught: oversized chunk keeps its head (2 352 failures), dropped
prefix forgets the skip (2 352), overflow keeps the old bytes (1 928), empty
chunk rejected after finalize (182), finalized session still accepts (1 546),
lost offset treated as a chunk (48), chunk length measured from the dropped
prefix (20), stop request loses priority (132).

The empty-chunk-after-finalize case was found by the gate on the first run: my
first draft checked `done` before `len == 0`, while native returns true first.
Fixed in the module, not worked around in the fixture.

## What stays native

- the mutex/condition-variable wait loop and the wake interval;
- the `uploaded_file` container, the ring buffer storage and the sink closure;
- session creation/replacement, GC and the HTTP handler that writes SSE frames.

P8-02 stays IN_PROGRESS on those items and on the transport, GCP AIP and
`/props`.

## Reproduce

    bash experiments/EXP-2026-11-13-453-p8-02-stream-policy/build_fixture.sh
    python3 experiments/EXP-2026-11-13-453-p8-02-stream-policy/mutate.py
