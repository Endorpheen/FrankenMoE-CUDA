# EXP290: KV slot search and the SWA predicate in Goose

`llama_kv_cache::find_slot` and the static `llama_hparams::is_masked_swa` it
calls — the last open item of P5-01 after EXP199, EXP204 and EXP289.

## Verdict

**GATED, NOT INTEGRATED** — exact against the oracle, but the read-only source
tree cannot be modified, so the module is not wired into `llama-kv-cache.cpp`.
Nothing is linked into a shipped binary and no performance claim is made.

## Scope transferred

All of `find_slot`: the streaming split (`n_tokens % n_seqs_unq`, the
`n_seq_id`/`seq_id` assertions), the per-sequence stream and `s0`/`s1` bounds,
the head reset when `head_cur > used + 2*n_tokens`, the `n_tokens > size`
empty result, the ring walk with the wrap accounting, the continuity rule
(`cont` tests the whole run, otherwise one cell at a time and a failure clears
the partial run), the usable-cell rule (empty, or held by exactly one sequence
and masked out of its sliding window) and the empty result paths.

`is_masked_swa` is transferred whole, all four types: none, standard
(`p1 - p0 >= n_swa`), chunked (`p0 < (p1/n_swa)*n_swa`) and symmetric
(`|p1 - p0| > n_swa/2`).

The containers stay native: `pos[]`, `used[]` and the sequence words are
storage. One behavioural substitution is stated rather than implied:
`seq_pos_max` is read from the cached `std::set` in native code, and here it is
a scan over the cells that carry the sequence bit. The value is identical by the
invariant that the set holds exactly those cells; the cost is different, and this
module is not on any timed path.

## Gate

Two oracles. The SWA predicate is the native function itself, copied verbatim
from `llama-hparams.h` (`reference/swa-ref.cuh`, path and `sha256` recorded).
`find_slot` is a transcription of the native body that keeps the real
`std::set` for `seq_pos`.

| check | result |
|---|---|
| SWA predicate: 4 types × 17 window sizes × 41 × 14 position pairs | 39 032 checks / 0 failures |
| slot search: 300 random cache states (64 cells), all four SWA types, `cont` both ways, one and two streams | 2 455 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| standard SWA window is one short | 189 |
| chunked SWA drops the chunk alignment | 908 |
| symmetric SWA loses the lower half | 930 |
| slot search tolerates a further head | 69 |
| slot search wraps one cell early | 9 |
| continuous search tests one cell at a time | 250 |
| continuous retry keeps the partial run | 403 |

All seven are caught. One candidate mutation was rejected before it shipped:
subtracting one from the wrap accounting changes only how many cells are
counted, not which cells are found, so no output can observe it. A mutation that
cannot be observed is not a test of the gate, and calling it missed would be
misleading.

Two defects of mine the gate caught, both in the harness rather than the module:
the SWA probe read `p1` at byte offset 1 instead of 8, which made the gate
disagree in a third of the cases, and the transcription never set the model's
`used` flag, so the head-reset comparison diverged from the first occupied cell.

## Files

- `source/host.goose` — the port plus a one-entry dispatcher.
- `tests/fixture.cpp` — native transcription, random driver, comparison.
- `reference/swa-ref.cuh` — verbatim native predicate with provenance.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
