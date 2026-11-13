# EXP340: Recurrent accessors in Goose

The remaining members of `llama_memory_recurrent`: `seq_pos_min`, `seq_pos_max`,
`set_rs_idx`, `memory_breakdown`, `get_can_shift`, `is_empty` and the r/s/p size sums.

## Verdict

**GATED, NOT INTEGRATED** — each rule is exact, including the two asymmetries below; the
source tree is read-only.

## Scope transferred

- `seq_pos_min` starts at the largest position and **falls back to -1** when nothing matched.
- `seq_pos_max` starts at -1 and **never falls back** — an empty sequence and a sequence at
  position -1 are indistinguishable, which is native behaviour.
- `set_rs_idx`: a negative id zeroes the whole array, both bounds are preconditions, and the
  index bound is **inclusive** (`idx <= n_rs_seq`).
- `memory_breakdown`: sums per backend type, reporting each type once at its first
  appearance; the same type appearing twice must add up.
- `get_can_shift`: always true — nothing to consult.
- `is_empty`: the buffer **list** decides, and the assert requires the total to agree. Zero-
  sized buffers make the distinction observable: a non-empty list of zero-sized buffers is
  not empty.
- size sums: only non-null tensors, per family, independently.

## Gate

| check | result |
|---|---|
| extrema and rs_idx, 3 000 draws | 36 000 checks |
| breakdown / is_empty / can_shift, 2 000 draws | 10 886 checks |
| size sums, 2 000 draws | 8 000 checks |
| total | 53 886 checks / 0 failures |

The first version of the `is_empty` case never produced a zero-sized buffer, so "decided by
the list" and "decided by the total" agreed on every draw and the mutation passed at 0. Zero
sizes now appear at 75%, and the mutation is caught at 737. Two mutations in this run were
also unusable on the first try — one did not compile and one was caught by the shape of the
generator instead of the rule.

## Mutations

| mutation | failures |
|---|---:|
| an empty sequence reports the largest position | 3 844 |
| the minimum is computed as a maximum | 11 204 |
| the maximum starts at zero | 3 844 |
| a negative id is taken as a sequence index | abort (the bound check fires) |
| the index bound is exclusive | abort (the legal boundary then fails) |
| the breakdown keeps only the last buffer size | 386 |
| the breakdown repeats a key per buffer | 1 042 |
| null tensors count towards the family size | 2 660 |
| is_empty is decided by the total, not the list | 737 |

All nine are caught.

## Files

- `source/host.goose` — the accessors.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
