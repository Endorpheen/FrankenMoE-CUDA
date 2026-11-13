# EXP289: KV cell position maintenance and `seq_pos` in Goose

The remainder of P5-01 named in the registry: `pos_add`, `pos_div` and the
`seq_pos` maintenance they depend on (`seq_pos_dec`, `seq_pos_inc`,
`seq_pos_rm`, `seq_pos_add`) plus the observers `seq_pos_min`,
`seq_pos_max` and `seq_pos_tok_le`, all from `llama-kv-cells.h`.

## Verdict

**GATED, NOT INTEGRATED** — the module is exact against a transcription of the
class, but the read-only source tree cannot be modified, so it is not wired
into `llama-kv-cache.cpp`. Nothing is linked into a shipped binary and no
performance claim is made.

## Scope transferred

All decisions live in Goose. The host keeps the containers, as in EXP199:
`std::vector`, `std::bitset` and `std::set` remain storage.

`seq_pos[s]` is `std::set<std::pair<llama_pos, uint32_t>>` in native code. Here
it is an ordered array of the same pairs plus a count, maintaining exactly the
same order: position first, cell index second. Insertion finds the slot with a
linear scan and shifts the tail; removal finds the cell and shifts down.
Because the order is the same, the observers match: `begin()`/`rbegin()` are the
first and last slots, and `upper_bound({p, UINT32_MAX})` is the last slot with
position <= p.

Arena, int64 slots, regions consecutive and sized by the cell count:

    pos[n] | shift[n] | used[n] | seqw[n*W] | ext[n] | spos[S*n] | sidx[S*n] | scnt[S] | hsh

`W = 8` words (512 sequence bits), `S` the sequence count, `hsh` the class's
`has_shift` flag. `W` is written as a literal because a `const` reads as zero
here — the emitter initialises `gg_globals` only inside `gg_program`, which the
bridge never runs. That trap is carried over verbatim from EXP199.

`pos_add` returns 1 when the cell is released (`pos < 0`): sequence words reset,
position -1, shift 0, used cleared. `pos_div` keeps the shift absorbing
`p_old - p`, matching the native integer division (truncation toward zero).

## Gate

The oracle is a transcription of `llama_kv_cells` using the real containers.
Forty trials of sixty random operations each, driven through one dispatcher
entry, compare after every step: positions, shifts, used flags, sequence words,
the full `seq_pos` contents per sequence, the counts, and the three observers
at nineteen probe positions each.

| check | result |
|---|---|
| full-state comparisons, 40 × 60 operations | 957 717 checks / 0 failures |
| observers (`min`, `max`, `tok_le`) | compared every step, 19 probes per sequence |

## Mutations

| mutation | detection |
|---|---|
| `pos_add` shifts by the wrong sign | 7 354 value mismatches |
| `pos_add` keeps the sequence words when a cell is released | invariant abort (stale bits make the next `pos_set` assert) |
| `pos_div` absorbs the wrong difference | 7 241 |
| insertion orders equal positions by lowest index | 202 |
| removal forgets to shrink the count | invariant abort (`c < n` assert on the next insert) |
| `tok_le` ignores a cell exactly at the position | 347 |
| `seq_pos_max` returns the first entry | 4 270 |
| `pos_add` reports a released cell as kept | 20 |

All eight are caught. Two of them are caught by an invariant abort rather than a
value mismatch, so the mutation driver counts a non-zero exit status as a
detection channel — a mutation that trips a validation code has still been
detected, and reporting it as missed would be the EXP281 mistake again.

The transcription needed one correction that is worth recording: the native
`upper_bound({p, UINT32_MAX})` sentinel only works with an unsigned index. With
`std::pair<int,int>` the sentinel becomes -1 and `upper_bound` skips every cell
at exactly `p`. That was a gate defect, not a module defect, and it showed up as
`tok_le` mismatches only.

## Files

- `source/host.goose` — the port plus a one-entry dispatcher.
- `tests/fixture.cpp` — native transcription, random driver, per-step comparison.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
