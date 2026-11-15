# P10-09 — test runners, fixtures and diagnostics

Status: **PARTIAL** — 3 of 3 slices accepted; host wiring is open.

## Slices

| slice | content | exports |
|---|---|---|
| S1 test runner | registry shape (fixtures setup/required), filters, split-shard case, disabled/skip classification, statuses, summary line, exit code | 11 `tr_*` |
| S2 fixture plan | tiny-MoE fixture: tensor shapes, seeds, metadata, writer constructors and banners | 9 `fx_*` |
| S3 bench diagnostics | 22 analysed fields, throughput row, cache-hit cell, cache-sizing membership and row | 7 `dg_*` |

## Evidence

- Experiment: `experiments/EXP-2026-11-15-652-p10-09-test-runners`.
- Canon: `goose/misc/test-runner.goose`, `goose/misc/fixture-plan.goose`,
  `goose/metrics/bench-diagnostics.goose`.
- Gates: 23 CTest scenarios, 195/195 fixture rows and 66/66 diagnostic rows,
  0 failures, identical at -O0.
- Mutations: 36/36 (tr 15 including a third block, fx 6+6, dg 6+6).
- The oracles are the real CTest 4.2.3, the real `make-tiny-moe.py` and the real
  `analyze()` from `bench-analyze.py`, not transcriptions.

## Remaining

Wire the decisions into `runtime/tests` and `tools`.

## Boundaries

These modules cannot join the single-unit build without their drivers: their
exports take slice parameters and specialize only with a caller. The unit of
typechecking is module plus driver, as in every gate in the tree.
