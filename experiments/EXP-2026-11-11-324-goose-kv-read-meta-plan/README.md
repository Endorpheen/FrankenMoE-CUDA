# EXP324: KV metadata restore plan in Goose

`llama_kv_cache::state_read_meta`, both branches (P5-08).

## Verdict

**GATED, NOT INTEGRATED** — the decision sequence and the rejection codes are exact;
the source tree is read-only. No performance claim. The I/O stays native.

## Scope transferred

Single sequence: the section must fit; the destination is removed first; every record
must carry exactly one id (the id is read and discarded in favour of the destination);
the layout either mirrors another cache — one stream, the right cell count, every cell
free — or is searched for; and the saved extension is restored after the batch is
applied, because the batch only carries the token and the 2-D position.

Whole cache: cells go in from zero, the mirror check is only about the count, and every
id must be inside the sequence range.

Rejection codes are part of the contract: 1 section too large, 2 record with the wrong
id count, 3 mirrored layout mismatch, 4 mirrored cell not free, 5 no cells found, 6 id
out of range.

## Gate

| check | result |
|---|---|
| 4 000 draws over both branches, every rejection path reachable | 6 715 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| an exactly full cache is rejected | 466 |
| a record with no ids is accepted | 1 133 |
| a short mirrored layout is accepted | 57 |
| a mirrored cell past the cache is accepted | 16 |
| a failed search is accepted | 232 |
| an id past the range is accepted | 69 |
| the destination sequence is not removed | 1 922 |
| the extension is never restored | 91 |
| the head is not reset on a whole-cache restore | 136 |

All nine are caught. The narrowest are the boundary rejections — a cell exactly at the
cache size (16) and a mirrored layout one cell short (57) — which is why both counts are
drawn just around the boundary rather than uniformly.

## Files

- `source/host.goose` — the plan plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
