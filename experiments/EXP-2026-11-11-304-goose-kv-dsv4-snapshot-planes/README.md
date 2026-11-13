# EXP304: DSV4 restore/snapshot planes in Goose

The rollback tail of `dsv4_build_comp_plan` (`n_rs_seq > 0`), which EXP302 and
EXP303 left out. P5-03.

## Verdict

**GATED, NOT INTEGRATED** — exact against a transcription; the source tree is
read-only. No performance claim.

## Scope transferred

For every layout stream — whether or not the ubatch touches it, so the graph
tensor widths never depend on the batch:

- **restore**: copy the live row from the pending rollback plane, or from plane 0
  when no rollback is pending. A rollback is pending when the sequence's
  `rs_idx` entry is in `(0, n_rs_seq]`, and that entry is looked up by sequence
  id against the table length, not against the stream count.
- **snapshot**: for every depth `d`, copy the live row into plane `d`, except
  that the trailing `d` tokens of the sequence come from the scratch rows, so a
  rollback lands on the state that existed before those tokens. With no tokens
  for the stream the copy is a no-op onto itself.

## Gate

| check | result |
|---|---|
| 3 000 random batches, streams 1–4, depths 0–3 | 162 768 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| rollback lookup bounds by the stream count | 203 |
| rollback plane is not scaled by the row count | 2 818 |
| restore destination ignores the stream offset | 15 711 |
| snapshot prefix uses every token of the sequence | 2 011 |
| snapshot prefix takes one token too many | 1 565 |
| scratch override loses the token index | 1 009 |
| empty sequence never self-copies | 41 355 |
| unified stream ignores its single sequence | 1 009 |

All eight are caught.

The one that found a real defect: I bounded the `rs_idx` lookup by `n_stream`
instead of the table length, which is invisible while the two happen to be equal
and wrong the moment a sequence id is legal but outside the stream range — the
gate reported it on the very first batch.

One candidate mutation was rejected as unobservable: making the prefix condition
`d < n_seq_tokens` instead of `<=` changes nothing, because at equality the
prefix is zero either way. The replacement takes one token too many and fails
1 565 values.

## Files

- `source/host.goose` — the tail plus a one-entry dispatcher.
- `tests/fixture.cpp` — native transcription and comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
