# EXP338: Recurrent state restore in Goose

`llama_memory_recurrent::{state_read_meta, state_read_data}`.

## Verdict

**GATED, NOT INTEGRATED** — both restore paths, their rejection codes and the transposed-
state refusal are exact; the source tree is read-only.

## Scope transferred

**Metadata, one sequence**: the section must fit, the destination is removed, and a
zero-length section stops before reading any record. Each record must carry **zero** ids —
the single-sequence layout stores one id for the whole section, unlike the attention cache
where every record carries one. The slot search must succeed, and the restored cells then
claim their own source so a later copy cannot overwrite them.

**Metadata, whole cache**: the cache is cleared **before** any record is read, so a failure
part-way leaves a cleared cache rather than the previous one. Ids are range-checked, and a
sequence may have only one tail cell — the tail lives in the cell whose index equals the
sequence id. Head becomes zero, used becomes the section length.

**Data**: layer count, capacity and transposition are checked first; the transposed state is
rejected before any write, which is why `state_clear` does not handle that layout. Then, per
layer, recurrent rows with optional PLE rows, then state rows. A null tensor skips its layer
entirely — no type and no row size is read for it — and a zero-length section reads nothing
while still validating the type and row size.

The `s_trans == true` branch of `state_read_data` is unreachable in native code, since the
header check already rejected it. It is left unreachable here too rather than "restored".

## Gate

| check | result |
|---|---|
| metadata, 4 000 draws over both paths, every rejection code reachable | 43 689 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| a single-sequence record may carry one id | 1 991 |
| an empty single-sequence section still reads records | 75 |
| a failed slot search is ignored | 290 |
| the whole cache is not cleared before reading | 6 341 |
| a second tail is accepted | 1 130 |
| an id above the range is accepted | 1 090 |
| a transposed state is accepted | 462 |
| null layers are read anyway | 1 202 |
| a zero-length section still reads tensors | 148 |

All nine are caught. The narrowest are the two empty-section behaviours (75 and 148): both
need a zero-length section **and** the specific path to be visited.

## Files

- `source/host.goose` — both restore paths plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
