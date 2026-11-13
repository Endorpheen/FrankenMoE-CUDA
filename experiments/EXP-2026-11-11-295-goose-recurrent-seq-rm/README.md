# EXP295: Recurrent `seq_rm`, `set_rs_idx`, `clear` and `state_clear` in Goose

The removal half of the recurrent memory subsystem (P5-04), after EXP293's
`find_slot` and EXP294's per-sequence operations.

## Verdict

**GATED, NOT INTEGRATED** — exact against a transcription; the source tree is
read-only. No performance claim. The tensor-data clearing inside `state_clear`
and the buffer clearing inside `clear(true)` are reported, not executed: they
are `llama_clear_tensor_data` / `ggml_backend_buffer_clear` calls, not cell
metadata.

## Scope transferred

`set_rs_idx`: a negative sequence resets the whole table, otherwise the id and
index are bounds-checked and stored.

`clear`: every cell loses position, sequence set, source and tail; head and used
reset; the snapshot table zeroes.

`seq_rm` — the interesting one, three behaviours in one body:

- a full-range removal also resets the sequence's snapshot index;
- a *partial rollback* (`0 < p0 <= pos < p1`) is served from the per-token
  snapshot: a cell shared by several sequences cannot roll back for one of them,
  an empty memory only moves the position, and the rollback must fit in
  `n_rs_seq` and not be already pending — pending rollback is single-use;
- otherwise the sequence's tail is invalidated when its cell falls in the range,
  and every cell in the range loses the sequence (all sequences when the id is
  negative); cells that empty release their position, source and a `used` count,
  and the first one moves the head back.

`state_clear`: `seq_id == -1` clears everything; otherwise it removes the
sequence over the full range and then clears the tensor rows for
`[cell_head, cell_head + cell_count)` — the row clearing is external.

The unsigned comparison matters: native tests `(uint32_t) seq_id >= n_seq_max`,
so a negative id is rejected before the `seq_id < 0` branch is reached, which
makes that branch dead in the native code. The port rejects negatives at the same
place for the same reason rather than reviving the dead branch.

## Gate

Oracle: a transcription over a model with the native `std::set` and a real
`rs_idx` vector. 300 states × 25 operations, with `n_seq_max = 12` and 32 cells
so ids above the cache but below the table are exercised, negative ids are drawn,
and half of the removals aim at the sequence's own cell so the rollback branch is
actually reached — including draws that put the rollback exactly on the
`n_rs_seq` bound.

| check | result |
|---|---|
| 300 × 25 operations, cells + snapshot table + head/used after each | 951 126 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| rollback ignores the upper position bound | 1 |
| rollback accepts one snapshot too far | 44 |
| pending rollback is treated as free | 165 |
| shared cell may roll back for one sequence | 13 |
| full-range removal keeps the snapshot index | 718 |
| tail is not invalidated for a removed cell | 552 |
| emptying a cell leaves the used count alone | 370 |
| head moves forward instead of back | 368 |

All eight are caught.

Three of them were invisible until the generator aimed at the branch: a uniform
position range almost never satisfies `0 < p0 <= pos < p1`, so the rollback
mutations reported zero failures while the rest were caught. The gate now draws
half of the removals around the sequence's own position and probes the exact
snapshot bound. The narrowest surviving margin is one value — the upper position
bound only differs when `p1 == pos + 1`, which the aimed draw now produces.

## Files

- `source/host.goose` — the port plus a one-entry dispatcher.
- `tests/fixture.cpp` — native transcription, random driver, comparison.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
