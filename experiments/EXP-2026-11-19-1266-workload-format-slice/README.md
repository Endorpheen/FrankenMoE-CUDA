# The workload decisions join the accumulated unit

This is an integration slice, recorded because `goose/misc/workload-format.goose` is in the
canonical tree and the manifest has to name the experiment that put it there.

Joined at commit `35adbc5a79d6310c142d36155391f6abb82ea7ac`
("EXP-2026-11-19-1266: the workload decisions join the accumulated unit").

## What the module contains

The deterministic-workload format and replay guards, ported from
`runtime/core/src/moe/deterministic_workload.cpp`: reading, writing, the guards and the routing
replay.

## Where the evidence is

The module header names its own gates, and this slice claims nothing beyond them:

- reading: 9 crafted files, every failure mode - EXP-2026-11-19-1261
- writing: byte-exact against `save()`, 4 workloads - EXP-2026-11-19-1263
- guards: token/finish, 1000 cases - EXP-2026-11-19-1264
- routing: `replay_topk` and the cursor, 1000 cases - EXP-2026-11-19-1265

No new measurement was made here. The slice exists so that a module in `goose/` is traceable to
the experiments that proved it, which is what `tools/check-goose-tree.py` enforces.
