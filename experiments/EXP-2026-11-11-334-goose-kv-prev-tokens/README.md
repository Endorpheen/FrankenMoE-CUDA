# EXP334: KV previous-token lookup in Goose

`llama_kv_cache::get_prev_tokens`.

## Verdict

**GATED, NOT INTEGRATED** — both position rules and the equal-position tie-break are
exact; the source tree is read-only.

## Scope transferred

For every token and every depth d in 1..n the answer is the token in the nearest cell at or
before a target position, for the token's first sequence.

Two rules set the target. **By position** (a token batch): `pos[i] - d`, and a negative
target stays null. **By order** (an embedding or mixed batch, where one image can repeat a
position): the predecessor comes from the ubatch order of the same sequence — a depth
inside the ubatch uses that token's own position, a depth before the chunk extends
backwards from the sequence's first token.

The nearest cell is the **last** cell at the target position, not the first: the
per-sequence index is ordered by (position, cell index), so at equal positions the higher
cell index wins. The gate generates repeated positions for exactly this reason.

## Gate

| check | result |
|---|---|
| 4 000 draws, 1–6 tokens, depth 0–3, repeated positions, 1–10 cells | 46 566 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| the first cell at a position wins instead of the last | 1 209 |
| the target position itself is excluded | 4 002 |
| cells of other sequences count | 5 891 |
| only a token-less batch goes by order | 833 |
| the order depth is added instead of subtracted | abort (the k-th occurrence then does not exist) |
| the pre-chunk extension forgets the depth | 3 831 |
| the answers are laid out by token plus depth | 12 743 |
| the order counts every token, not just the sequence | abort (same) |

All eight are caught; two are caught by the run aborting, which is still detection.

Two mutations were provably unobservable and were replaced:

- `k >= 0` → `k > 0`: at k == 0 the pre-chunk branch computes `pos[first] + 0`, which is
  exactly what the in-batch branch returns. Measured 0 failures.
- "look up a negative target anyway": an empty cell carries an empty sequence mask, so the
  lookup rejects it and the answer is null either way. Measured 0 failures. The
  replacement — dropping the mask check — is caught... but that was already the third
  mutation, so the slot went to the layout mutation instead.

## Files

- `source/host.goose` — the lookup plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver (per-sequence index rebuilt as std::map).
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
