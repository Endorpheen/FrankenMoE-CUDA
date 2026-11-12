# Route audit of the active runtime, 2026-11-10

The EXP219/EXP220 losses came from auditing files instead of builds. This applies the rule that
would have caught both: enumerate every route prefix any experiment declares, then check which of
them the active runtime actually contains and actually executes.

## Method

1. Scan every `experiments/*/integration/*.cpp` and `*/source/bridge.c` for `fgNNNx_` prefixes —
   the route symbols a bridge exposes.
2. Compare against `nm -D --defined-only` on the active `libllama-exp220`.
3. For anything not in that library, check whether a witness line proves it executed, and if not,
   read that experiment's own verdict.

Prefixes are compared, not names, because one prefix covers a whole module's symbols.

## Result

23 route prefixes declared, 14 in the active `libllama`. The remaining 9 split cleanly:

| prefix | status | evidence |
|---|---|---|
| `fg195v` | live, own DSO | `FG195V_CUDA_VMM_POOL` in the run |
| `fg196p` | live, own DSO | `FG196P_CUDA_PINNED` |
| `fg203d` | live, own DSO | `FG203D_MMQ_DISPATCH` |
| `fg204s` | live, own DSO | `FG204X_KV_SET` |
| `fg205f` | live, own DSO | `FG205G_KV_FIND_SLOTS` |
| `fg206c` | live, own DSO | `FG206D_KV_CP` |
| `fg190q` | not part of the migration | README: "This directory is kept as a record … it is not part of the migration" |
| `fg207p` | REJECTED | STATUS.md: linear scan costs 59% of decode, 26.46 → 10.76 |
| `fg212s` | REJECTED | README: correct but 3.4-3.6% slower on decode |

Six of the nine are CUDA-side modules linked into their own DSOs, which is why they are absent from
`libllama` while still executing. The three that neither appear nor execute all have a written
reason to be absent.

So: **no unexplained gap.** Every route is either live, deliberately absent, or rejected with a
number.

## Why this is worth keeping as a file

The EXP219 and EXP220 losses were both invisible to speed and token gates. A prefix audit is the
only check that compares *what the chain should run* against *what it does run*, and it is cheap
enough to run before every stage. It would have caught both losses in one pass, before any Apex run
was spent.

Limitation to keep in mind: a prefix present in the library does not prove it is called, and a
witness line does not prove it computed the right thing. This audit establishes presence and
execution only; correctness is the module gates' job.
