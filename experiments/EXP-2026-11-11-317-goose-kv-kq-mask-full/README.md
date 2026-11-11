# EXP317: KV attention mask, full float path in Goose

`set_input_kq_mask` with the three branches EXP316 left out: ALiBi, the 2-D/M-RoPE
causal comparison, and the non-causal SWA-full span (P5-02).

## Verdict

**GATED, NOT INTEGRATED** — exact against a full-pass oracle; the source tree is
read-only. No performance claim.

## Scope transferred

- **ALiBi**: the mask stores `-|p0-p1|` instead of 0. Row reuse is disabled under
  ALiBi, because the stored value depends on the query position, not only on the
  cells — a copied row would carry the previous token's distances.
- **2-D/M-RoPE**: under causal masking a cell at the same position is additionally
  masked when its `(x, y)` extension is greater than the token's `(p1_x, p1_y)`,
  compared as y first and then x.
- **Non-causal SWA-full**: when attention is non-causal and the cache declares the
  full span, the sliding window is skipped for cells at or after the sequence's
  first batch position.

## Gate

| check | result |
|---|---|
| 3 000 batches, all four SWA types, causal and non-causal, ALiBi/2-D/span drawn independently | 115 277 checks / 0 failures |

The oracle is a full pass per token with no reuse, so the shortcut is again measured
against what it optimises away.

## Mutations

| mutation | failures |
|---|---:|
| ALiBi keeps the signed distance | 7 796 |
| ALiBi still reuses rows | 142 |
| 2-D comparison ignores the y axis | 1 517 |
| 2-D comparison masks an equal y on the token | 108 |
| non-causal span starts one position late | 98 |
| non-causal span is never applied | 309 |
| 2-D comparison applies to every past cell | 2 043 |
| ALiBi distance loses its absolute value | 5 873 |

All eight are caught. The narrowest are the ones that need two independent draws to
coincide — ALiBi together with row reuse (142) and the exact span boundary (98) —
which is why all four flags are drawn independently rather than tied together.

## Files

- `source/host.goose` — the mask plus a one-entry dispatcher.
- `tests/fixture.cpp` — full-pass oracle and comparison.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
