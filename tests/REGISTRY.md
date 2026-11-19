# Permanent test registry

One row per permanent test. The row is added in the same commit as the test, and is removed only
when the test is replaced by something that covers at least as much - never silently.

Tiers: `fast` (no model, no GPU), `integration` (two or more subsystems, still no model),
`performance` (controlled load, run only on request), `model` (needs the model and the Apex lock,
run only with explicit permission).

| area | test | path | EXP | requirement | tier | status |
|------|------|------|-----|-------------|------|--------|
| grammar | reject-alternates | `tests/grammar/reject-alternates` | EXP-2026-11-19-1491 | P6-10 | fast | migrated 2026-11-19 |

## Notes on the first migration

`tests/grammar/reject-alternates` is the former
`experiments/EXP-2026-11-19-1491-grammar-reject-alternates/tests/fixture.cpp`, moved to the
permanent tree unchanged in substance: the same oracle compiled into the fixture, the same seven
cases, the same `-O1` gate. The difference is that it now builds against the live
`goose/sampling/sampler-chain-full.goose` rather than a pinned copy, which is what makes it a
regression test instead of a record.

The experiment directory keeps its card, its pinned source and its recorded numbers. The card
points here for the repeatable check.
