# EXP314: Hybrid indexed host pool inputs in Goose

`llama_memory_hybrid_idx_context::set_input_kpool` (P5-05): the pool tables and the
masks the graph reads, without touching ggml tensors.

## Verdict

**GATED, NOT INTEGRATED** — the plan is exact; the source tree is read-only. No
performance claim. Tensor writes stay native.

## Scope transferred

**Tables.** For every pool, the representative cell (its last member's row) and the
member cells; pools of sequences absent from the batch fall on the sentinel row.
Fresh pools also fill the scatter lists, the representative list, and the M-RoPE
positions rotated to the first member. Padded entries take distinct uncached cells,
so the scatter never writes one cell from two lanes, and unused slots address the K
storage through a live cell plus a sentinel member index.

**Masks.** A pool is visible for a token when it belongs to the token's sequence and
ends at or before it; in order mode the visible count comes from the token's rank.
The selection mask keeps the first `min(nv, n_top)` ranked pools and marks the
remaining and tail slots by whether they hold a real cell. The tail list carries the
cells of the sequence's trailing window, by rank in order mode and by position
otherwise.

One substitution is stated in the code: the native position block strides by
`n_new_g`, which belongs to EXP312's state and is not part of this transfer, so the
gate uses the real pool count as the stride on both sides.

## Gate

| check | result |
|---|---|
| 2 000 batches over both modes, fresh and padded pools | 212 205 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| absent sequences are treated as present | 4 780 |
| the representative is the first member, not the last | 23 738 |
| padded scatter entries may reuse a representative cell | 1 987 |
| pooled position comes from the last member | 4 095 |
| order mode visible count skips the partial pool | 1 271 |
| order mode tail keeps one cell too many | 2 873 |
| selection keeps the tail slots as visible | 3 154 |
| position mode tail drops the inclusive one | 2 544 |

All eight are caught.

## What the first attempt got wrong

The first version of this gate diverged on the pool-cell table at the second
sequence and produced `1*12+23 = 35` against `20`, with the module seemingly reading
`cells_off[1] = 0`. The module's formula matched native; the harness was describing
the same facts three times — a stream array, two cell arrays, two pool arrays — and
one of them disagreed. **The rewrite carries a single `seqdesc` table (stream, cell
range, pool range) and both arms read it**, which removed the disagreement
immediately. Three later defects were all harness-side: an empty sequence read past
its cells, tokens could name sequences with no cells (native asserts a rank), and
the selection mask was compared when the graph had not asked for it.

## Files

- `source/host.goose` — the plan plus a one-entry dispatcher.
- `tests/fixture.cpp` — arena description, transcription and comparison.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
