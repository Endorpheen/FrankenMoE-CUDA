# Permanent test registry

One row per permanent test. The row is added in the same commit as the test, and is removed only
when the test is replaced by something that covers at least as much - never silently.

Tiers: `fast` (no model, no GPU), `integration` (two or more subsystems, still no model),
`performance` (controlled load, run only on request), `model` (needs the model and the Apex lock,
run only with explicit permission).

| area | test | path | EXP | requirement | tier | status |
|------|------|------|-----|-------------|------|--------|
| grammar | reject-alternates | `tests/grammar/reject-alternates` | EXP-2026-11-19-1491 | P6-10 | fast | migrated 2026-11-19 |
| grammar | first-set | `tests/grammar/first-set` | EXP-2026-11-18-1110, EXP-2026-11-19-1491 | P6-10 | fast | added 2026-11-19 |
| models | model-manager-doors | `tests/models/model-manager-doors` | door work of P8-06 | P8-06 | fast | migrated 2026-11-19 |
| json | roundtrip | `tests/json/roundtrip` | door work of P10-07 | P10-07 | fast | migrated 2026-11-19 |

## Notes on the first migration

`tests/grammar/reject-alternates` is the former
`experiments/EXP-2026-11-19-1491-grammar-reject-alternates/tests/fixture.cpp`, moved to the
permanent tree unchanged in substance: the same oracle compiled into the fixture, the same seven
cases, the same `-O1` gate. The difference is that it now builds against the live
`goose/sampling/sampler-chain-full.goose` rather than a pinned copy, which is what makes it a
regression test instead of a record.

The experiment directory keeps its card, its pinned source and its recorded numbers. The card
points here for the repeatable check.

## Second batch

`tests/grammar/first-set` covers the other exported grammar decision,
`fg_grammar_first_set`. It is not a move: EXP-2026-11-18-1110 gated `g_advance`,
`g_match_char`, `g_match_partial` and `g_first_set` through generated shims, and only the last two
are exported. Exporting internals so a test can reach them would change the artefact under test,
so the permanent tree gates the exported decision and the experiment keeps its direct gates as the
record of that cycle. `tests/grammar/first-set/README.md` states which path covers which
function.

## Third batch

`tests/models/model-manager-doors` and `tests/json/roundtrip` are moves of
`tools/model-manager-doors-check.sh` and `tools/json-roundtrip-check.sh`: the probe source is the
same text, now a committed `.c` file instead of a heredoc, and the checks are the same tables. Both
ask the shipped library through `dlopen`, so both record the library path, its SHA-256 and the
source revision in their log - the doors live in that artefact and a verdict without it is not
evidence about anything.

Both need the built library and skip with exit 77 when it is absent. A skip is counted and printed
separately by `tests/run.sh`, never folded into a pass. The original `tools/` scripts stay until
their permanent copies have run in a second context; that removal is its own step.
