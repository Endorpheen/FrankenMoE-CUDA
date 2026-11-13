# EXP306: DSV4 state tensor I/O plan in Goose

`dsv4_state_write_tensor_streams` / `dsv4_state_read_tensor_streams` — the
per-stream transfer records of the compressed state, without the bytes. P5-03.

## Verdict

**GATED, NOT INTEGRATED** — exact against a transcription; the source tree is
read-only. No performance claim.

## Scope transferred

Both helpers write four metadata fields (type, `ne0`, rows, row size) and then one
record per stream at `stream * tensor_rows * row_size`, size `n_rows * row_size`,
and both reject a row count above the tensor's storage. The write side also
rejects a stream map whose length differs from the stream count and a stream
index outside the tensor's plane count. The read side re-checks the metadata
before emitting records for `s0 + s`.

Every rejection is a contract: native throws, the FFI form is `-1`, and the gate
pins it in both directions.

## Gate

| check | result |
|---|---|
| write records, 4 000 draws with and without a stream map | 5 200 checks |
| read metadata checks and records, 4 000 draws | 4 724 checks |
| total | 9 924 checks / 0 failures |

The draws include a map shorter and longer than the stream count, a row count
above storage, a zero row size, and each metadata mismatch in both directions
(higher *and* lower than the reference).

## Mutations

| mutation | failures |
|---|---:|
| stream stride uses the live row count | 632 |
| record size uses the storage row count | 1 021 |
| row count check is off by one | 407 |
| stream map accepts fewer ids than streams | 63 |
| stream bound check is off by one | 65 |
| write offset drops the stride | 314 |
| read offset drops the stride | 549 |
| read ignores the tensor type | 348 |

All eight are caught.

A representation defect the gate exposed, worth carrying forward: the FFI passed
the stream map as pointer-plus-length, so a null map and an empty map were the
same input. Native distinguishes them (`stream_ids && size != ns`), and the port
was silently taking the null branch for an empty map. There is now an explicit
presence flag. **Length zero does not encode presence** — the same class of
mistake as handing a byte offset to an index helper.

## Files

- `source/host.goose` — the plan plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
