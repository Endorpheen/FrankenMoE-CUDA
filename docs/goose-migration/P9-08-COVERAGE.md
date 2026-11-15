# P9-08 — standalone engine native implementation

Status: **PARTIAL** — 3 of 3 slices accepted; the wiring and two characterised
defects are open.

## Slices

| slice | content | exports |
|---|---|---|
| S1 load admission | `Session::open` admission, mparams/cparams projection, 20-stage init order with first-error reporting | 10 `lga_*` |
| S2 perplexity plan | request gates, prefix, scoring-position sets in both modes, the choices row, aggregates | 12 `ppl_*` |
| S3 KV and turn state | `clear_kv`, KV-prefix reuse, draft rewind, `rollback_turn`, token addition | 9 `kvp_*` |

## Evidence

- Experiment: `experiments/EXP-2026-11-15-636-p9-08-engine-closure`.
- Canon: `goose/misc/{load-admission,perplexity-plan,kv-turn-state}.goose`.
- Gates: 118/512 + 287/2455 + 62/281 checks (467 cases / 3248 checks), 0
  failures, identical at -O0.
- Mutations: 36/36 (12 per slice, halves 6/6).

## Characterised but unchanged

- Perplexity step mode never scores position 0 (floor = 1).
- `rollback_turn` resizes `kv_tokens` even after a `seq_rm` refusal and memory
  clear.

Both are recorded as candidates for a decision, not silently changed.

## Additional slices (EXP650/EXP655)

G1/G2/G3/G6 (tallies, prefill plan, trace order, moe-predict) and G4/G5/G7 (MTP
capture warm-up, sampler chain, CLI driver) are accepted; P9-08 is 10/10 slices.

## Remaining

Wire the engine closure points into `session.cpp` and `cli/main.cpp`, and decide
the two characterised behaviours above.

## Boundaries

Three modules in one binary need `objcopy --redefine-sym` for `goose_init`
(link proven). Mutexes, CVs, threads, the graph and the sampler stay native.
