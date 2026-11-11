# EXP297: Recurrent state payload plan in Goose

`llama_memory_recurrent::{state_write_data, state_read_data}`, without the bytes.

## Verdict

**GATED, NOT INTEGRATED** — the plan is exact; the source tree is read-only. No
performance claim.

## Scope transferred

Both native functions do two things: validate the stream header and each
tensor's type and row size, and translate cell ranges into transfer records
`(tensor, offset, size)`. The bytes themselves are ggml tensor rows, so the
module produces the records and the host keeps the I/O — the arithmetic that
decides which bytes move is what is transferred and tested.

Write order matches native: `s_trans`, `n_layer`, then per layer the R tensor
(type, row size, one record per range) and the PLE tensor when present, then the
S tensor — contiguous with row-size records, or, when transposed, element size
and embedding size followed by one record per embedding row per range.

Read order mirrors it: header check (`n_layer`, capacity, `s_trans`), per-layer
type and row-size checks, R and PLE read records for `[head, head+cell_count)`,
then S in either layout, with the transposed form walking
`(head + j*size) * element_size` per embedding row.

The stream is abstracted as int64 slots per field rather than the native
mixed 32/64-bit bytes; the module walks it in bytes so the abstraction stays a
buffer, not a special case.

## Gate

The oracle is a transcription producing the same records, compared field by
field. The read side is fed the exact field sequence the write transcription
produced, with a deliberate corruption in one trial out of five (layer count,
`s_trans`, or the first tensor type).

| check | result |
|---|---|
| 600 trials: write records + read records + cursor | 16 692 checks / 0 failures |
| rejected streams | 120, compared by result |

## Mutations

| mutation | failures |
|---|---:|
| write skips the PLE tensor | 3 347 |
| write offsets the S tensor without its row stride | 1 431 |
| write transposed element offsets are doubled | 1 605 |
| write miscounts the layers | 600 |
| read accepts a higher tensor type | 39 |
| read rejects an exactly full cache | 3 |
| read offsets the R tensor by one byte | 915 |
| read transposed rows lose the cell stride | 460 |

All eight are caught.

Two defects the gate found, both mine. The range helper multiplied the index by
eight twice, so cells after the first were read from beyond the array — and it
showed up only in `size`, because the offset used the first cell and stayed
right; that is why the mismatch looked like arithmetic rather than addressing.
And the stream cursor was counted in fields in one arm and in bytes in the other,
which reported a cursor eight times too large for every successful read.

One candidate mutation was rejected rather than forced: accepting a larger layer
count from the header cannot be observed, because the reader consumes fields per
layer according to the model, not the header, so a stream with a wrong header
fails later anyway. A mutation that no output can separate is not a test.

## Files

- `source/host.goose` — the plan plus a one-entry dispatcher.
- `tests/fixture.cpp` — transcription, record comparison, corruption driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
