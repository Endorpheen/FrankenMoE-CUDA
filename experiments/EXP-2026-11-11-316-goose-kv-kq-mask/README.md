# EXP316: KV cache attention mask in Goose

`llama_kv_cache::set_input_kq_mask`, the float path (P5-02).

## Verdict

**GATED, NOT INTEGRATED** — the mask is exact; the source tree is read-only. No
performance claim.

## Scope transferred

For every token the row holds 0 for a cell it may attend and -inf for one it may
not. A cell is dropped when it is empty, when it does not carry the token's
sequence, when causal masking puts it after the token, or when the sliding window
excludes it (`standard`, `chunked`, `symmetric`).

The row-reuse shortcut is transferred too: tokens of the same sequence copy the
previous row, and only the cells recorded for that sequence are revisited. The
recorded set is the cells within `n_swa + 32` of the sequence's first batch
position — revisiting exactly that set is what makes the shortcut equal to a full
pass.

Stated, not implied, as **not transferred here**: ALiBi values (the mask stores
`-|p0-p1|` instead of 0), the 2-D/M-RoPE causal comparison, the half-precision
path, and the non-causal SWA-full span.

## Gate

| check | result |
|---|---|
| 3 000 batches, all four SWA types, causal on and off, 1–3 streams | 132 213 checks / 0 failures |

The oracle is a **full pass per token with no reuse**, so the shortcut is tested
against the thing it is allowed to optimise away, not against a copy of itself.

## Mutations

| mutation | failures |
|---|---:|
| no cell is recorded for reuse | 11 754 |
| row reuse skips the first cell | 4 106 |
| causal mask keeps the token itself out of range | 10 431 |
| sliding window is not applied | 8 733 |
| cells without the sequence still match | 15 717 |
| row reuse copies one element everywhere | 8 691 |
| sequence minimum becomes the maximum | 11 754 |
| the recorded count is not stored | 11 754 |

All eight are caught.

One candidate mutation was rejected after measuring it rather than assuming:
weakening the recorded-window comparison from `>=` to `>` is **not observable**
here, because a cell exactly on that boundary is far behind every token of its
sequence and its verdict (causal keep, SWA drop) is the same for all of them, so a
missed revisit cannot change a value. The replacement — row reuse skipping its first
cell — fails 4 106 checks.

## Files

- `source/host.goose` — the mask plus a one-entry dispatcher.
- `tests/fixture.cpp` — full-pass oracle and comparison.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
