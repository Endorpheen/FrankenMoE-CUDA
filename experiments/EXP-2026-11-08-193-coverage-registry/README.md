# EXP193: differential and liveness coverage registry (P0-05)

Builds `docs/goose-migration/coverage-registry.json` from artifacts that already exist,
and fails when the recorded verdict for a module disagrees with what the runtime does.

## What it records per module

| field | source |
|---|---|
| `tier` | `tasks.json` history: ACCEPTED / REJECTED / OFF_PATH |
| `in_active_chain` | whether the module's object appears in the link line of the active stage |
| `witness_seen_in_live_log` | whether the module's witness line appears in the newest Apex `server.log` |
| `byte_compare` | case count and status from the module's differential fixture |
| `sanitizer` | memcheck / racecheck / initcheck from the milestone artifact |

Nothing here is a new measurement; the point is that all four facts are assembled
mechanically from files and cross-checked against each other, so a stale claim cannot
survive.

## The cross-check that matters

The registry exists because of a specific failure that already happened: EXP180 was
rejected for a measured regression, yet it stayed enabled by default and every Apex run
paid for it for several iterations. A document recorded one thing and the running system
did another, and nothing compared them.

The rule now enforced:

- a module recorded `REJECTED` or `OFF_PATH` must **not** have its witness in the live log;
- a module recorded `ACCEPTED` must have it.

Run with `--check` to make a conflict exit non-zero, which is what a gate needs.

## Current state

Fourteen modules: eleven accepted and executing, EXP180 rejected and not running, EXP186
rejected and not in the chain, EXP182 and EXP188 byte-exact but `OFF_PATH` because the
Apex profile never reaches them. Registry status `CONSISTENT`.

## Two defects found in the builder itself

Worth recording, because both would have made the registry quietly useless:

1. `in_active_chain` was computed from object basenames, which collide across stages
   (`cuda-runtime.o` exists in several), so it reported `False` for everything. Fixed by
   matching the full path.
2. The `ACCEPTED`-must-execute check was written with a `(182, 188)` exemption that was
   too broad: flipping EXP188 to `ACCEPTED` in the ledger still passed. The exemption was
   wrong in principle — those two are `OFF_PATH` precisely because they do not execute —
   so it was removed, and a deliberate status flip now fails with exit 1.
