# Gate sweep after the closed items (2026-11-11)

All adaptors in `experiments/EXP-2026-11-11-3*` and `EXP-2026-11-11-40*` were rebuilt and re-run after
the last integration. Result: **38 gates, 0 failures.**

Largest sets:

| gate | checks |
|---|---|
| FG349S ubatch_add | 136 183 |
| FG357S ubatch_add (working line) | 81 787 |
| FG370/FG352S classify | 6 981 |
| FG355/FG348S keep-tail | 67 619 |
| FG392S slot walk | 53 703 over 18 000 caches |
| FG367S state clear | 51 077 |
| FG389S GGUF inventory | 31 117 |
| FG386S selection | 24 086 comparisons |
| FG360/FG350S selection | 18 453 |
| FG347/FG345W write bridge | 15 342 |
| FG365S read data | 11 595 |

Everything else: 373 41, 375 76, 376 35, 377 2 161, 380 92, 381 22, 382 34, 383 37, 384 3 769,
385 73, 387 51, 388 76, 390 33, 391 43, 393 43, 394 875, 395 103, 396 875, 397 33, 398 251, 401 42,
402 525, 403 809, 404 31, 405 47, 406 18 — all 0 failures.

The sweep was run because the remaining small-path items are blocked by named boundaries, and weakening or
drifting tests while waiting would be the wrong reaction. It also covers gates that had not been re-run
since their commits (355, 356, 357, 360, 365, 366, 370).
