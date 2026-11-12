# P5-01 inventory: KV cell allocation, sequence ownership and position maps

Criterion: "Every cell find/copy/remove/keep/shift/div/sequence operation checked for interleaving, multiple sequences
and full-capacity failures."

Evidence: one accepted-profile run (EXP-293 stage log) carries all fifteen KV witnesses at once -- `FG199K_KV_CELLS`,
`_NKV`, `_SEQCP`, `_SEQRM`, `_SHIFT`, `_SLOT`, `_UPDATE`, plus `FG205G_KV_SLOT`, `FG206D_KV_CP`, `FG209I_KV_SEQPOS`,
`FG227N_KV_N_KV`, `FG228E_KV_CELL_EXT`, `FG229I_KV_INPUT_IDXS`, `FG233C_KV_SCAN` and `FG392S_KV_FIND_SLOT`.

## Operation by operation

| operation | live | gate |
|---|---|---|
| find / slot placement | `FG392S_KV_FIND_SLOT`, `FG205G_KV_SLOT` | EXP390: 53 703 checks, 8 mutations, full-capacity case returns -1 |
| copy (same stream) | `FG206D_KV_CP` | EXP341/291 |
| copy (cross stream) | -- | EXP292: 130 370 checks, 9/9 mutations |
| remove | `FG199K_KV_SEQRM` | EXP291/295/341 |
| keep | available in the module, **not wired at the cell level** | EXP291 gate |
| shift | `FG199K_KV_SHIFT` | EXP327: 17 656 checks, 8/8 |
| div | available in the module, **not wired at the cell level** | EXP328 gate |
| sequence count / used_max_p1 / pos_set | `FG199K_KV_CELLS` | EXP199/289 |
| position map maintenance | `FG209I_KV_SEQPOS` (live, and just repaired in EXP293) | EXP289: 957 717 checks |

## The gap this inventory exposes

`llama_kv_cells` keeps several operations as inline methods in the header. Three of them are still native while their
`fg199k` counterparts exist in the module:

- `pos_in(i, p0, p1)` -- `llama-kv-cells.h:576`, plain comparison;
- `pos_div(i, d)` -- `:632`, divides the position and maintains the per-sequence index;
- `seq_keep(i, seq_id)` -- `:414`, keeps only one sequence, rewrites the cell and maintains the index.

The live `FG199K_*` lines come from call sites in `llama-kv-cache.cpp` (range normalisation, plan_cell, head selection,
update), not from these inline predicates. So the operations the criterion names are correct and gated, but their
cell-level form executes native code.

## What closing P5-01 needs

1. Wire the three inline methods to the module under `fg199k_enabled()`, with the index maintenance moved into the
   module's journal (the same shape used for `seq_add`): `pos_in` is a pure predicate, `pos_div` and `seq_keep` mutate
   positions and the per-sequence index.
2. Recompile the three TUs that include the header, relink, and require the accepted profile to stay at `e68df05b` plus
   the state round trip from EXP-293 to stay exact.
3. State the full-capacity behaviour explicitly for keep/div, which the existing gates cover only indirectly.
