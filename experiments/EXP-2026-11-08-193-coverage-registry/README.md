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

## The README is now the primary record, and the registry reads it

The ledger is a summary; the stage README is where a verdict is actually argued. Several
stages said "not promoted to the working runtime", which is true and means only "not
installed into production" -- installation is Igor's and Codex's decision. Read quickly it
looks like "not accepted", and two stages (EXP177, EXP178) contained that phrase while
being accepted and executing.

Two things were done about it:

1. every stage now carries an explicit `## Verdict` section or an inline
   `Verdict: **X**` sentence, and the ambiguous state lines are explained in place;
2. the registry parses the README verdict and reports any disagreement with the ledger.

Verified by flipping EXP185's README verdict to REJECTED: the registry exits 1 with
`EXP185: ledger says ACCEPTED, README says REJECTED`, then the file was restored.

The parser accepts both verdict shapes because the tree had both, and an earlier version
that only understood the section form silently skipped EXP183/184/185 -- which is why the
first negative test passed when it should have failed.

## A third defect in the builder: it read the wrong log

After an EXP175 measurement the registry reported every late-stage module as missing. The
cause was `newest_log()` globbing `runtime/*/results/*-goose/server.log` and taking the most
recent file: a CONCAT run carries EXP175 but not EXP183-185, so "newest in the tree" was a
run of a different stage's module set.

Fixed by scoping the glob to the active stage's `results/` directory. This is the same
class of mistake as trusting a stale manifest -- the evidence has to come from a run of the
runtime being described, not from whatever happened to be written last.
