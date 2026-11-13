# Regression sweep after the defect fixes

Every stage whose object was rebuilt while fixing the five defects found by
`test-backend-ops` was re-run through its own local gate afterwards. This is the check that the
fixes did not damage the stages they were layered on.

| stage | gate | result |
|---|---|---|
| EXP166 activations | `check.py` | pass |
| EXP171 rope | `check.py` (preflight build/link/run) | pass |
| EXP176 get_rows | `test_local.py` | 27 checks / 0 failures, GATES_PASS |
| EXP177 cpy | `test_local.py` | 18 / 0, GATES_PASS |
| EXP184 mmvf | `test_local.py` | 42 / 0, GATES_PASS (both arms) |
| EXP185 mmvq_q4k | `test_local.py` | 8 / 0, GATES_PASS (both arms) |
| EXP188 dequant q4_0 | `test_local.py` | 15 / 0, GATES_PASS (both arms) |

`test-backend-ops test -b CUDA0`: rc=0, 0 failures, 2/2 backends passed.

Apex after the last rebuild: all 40 witness lines present, output sha256 `e68df05b`, and the
last paired run read Goose 2533.23/24.96 against native 2532.72/25.00 (+0.02% / −0.16%, inside
noise).

So the fixes are additive: no stage's own guarantee regressed, the cross-module harness is
green, and the deployed profile is unchanged.

One caveat recorded rather than buried: the `test-backend-ops grad` mode still reports four
`SUM` failures, and those reproduce with every Goose switch off, so they belong to the baseline
and not to this work — see `grad-mode-open.md`.
