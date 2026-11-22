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
| json | nlohmann-shadow | `tests/json/nlohmann-shadow` | P10-07 state, 2026-11-19 | P10-07 | fast | verified 2026-11-19 |
| json | adapter | `tests/json/adapter` | P10-07 adapter, 2026-11-19 | P10-07 | fast | verified 2026-11-19 |
| json | swap | `tests/json/swap` | P10-07 swap, 2026-11-19 | P10-07 | fast | verified 2026-11-19 |
| integration | goose-determinism | `tests/integration/goose-determinism` | P10-06 state, 2026-11-19 | P10-06 | integration | verified 2026-11-19 |
| integration | link-manifest | `tests/integration/link-manifest` | P10-06 audit, 2026-11-19 | P10-06 | fast | verified 2026-11-19 |
| integration | clean-checkout | `tests/integration/clean-checkout` | P10-06 state, 2026-11-19 | P10-06 | fast | verified 2026-11-19 |
| integration | path-b-abi | `tests/integration/path-b-abi` | Path B audit, 2026-11-20 | P6-10, P7-12, P11-01 | integration | compares public sampler logits and Goose GGUF against a native fixture |
| kv | slot-state | `tests/kv/slot-state` | Path B slot compatibility, 2026-11-20 | P5-01, P5-08 | model | compares warm, restored and imported slot continuations, including prefix reuse, tokens and probabilities |
| json | door | `tests/json/door` | door work of P10-07 | P10-07 | fast | migrated 2026-11-19 |
| server | router-status | `tests/server/router-status` | door work of P8-06 | P8-06 | model | migrated 2026-11-19 |
| server | api-key-policy | `tests/server/api-key-policy` | P8-09 state, 2026-11-19 | P8-09 | fast | added 2026-11-19, skips without the door |
| server | http-compile | `tests/server/http-compile` | P8-09 wiring, 2026-11-19 | P8-09, P8-12 | fast | verified 2026-11-19 |
| integration | goose-composition | `tests/integration/goose-composition` | set list of tools/check-composition.sh | P6-10, P7-06, P8-09 | integration | created 2026-11-19, run pending |
| regression | accepted-components | `tests/regression/accepted-components` | release pre-flight of tooling | rule 31 | fast | verified 2026-11-19 |
| regression | cumulative-baseline | `tests/regression/cumulative-baseline` | release pre-flight of tooling | rule 31 | fast | verified 2026-11-19 |
| integration | unwired-modules | `tests/integration/unwired-modules` | module accounting of tooling | P11-01, P11-04 | integration | verified 2026-11-19 |
| integration | fit-params-line | `tests/integration/fit-params-line` | EXP-2026-11-19-1407 | P10-02 | fast | migrated 2026-11-19 |
| integration | carrier-symbols | `tests/integration/carrier-symbols` | carrier divergence, 2026-11-19 | rule 33 | integration | verified 2026-11-19 |
| models | qwen4exp-slices | `tests/models/qwen4exp-slices` | EXP-2026-11-15-660 | C1-02 | integration | migrated 2026-11-19 |
| models | architecture-regressions | `tests/models/architecture-regressions` | Model architecture registry | P11-07, F1-02 | model | mandatory for a new architecture; records one A–B–B–A per registered reference |
| regression | symbol-loss | `tests/regression/symbol-loss` | rule 33 guard of tooling | rule 33 | fast | verified 2026-11-19 |
| regression | library-provenance | `tests/regression/library-provenance` | library drift, 2026-11-19 | rule 22 | fast | added 2026-11-19 |

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

## Fourth batch

`tests/json/door` is a move of `tools/json-door-check.sh`: same probe, same five valid and four
invalid texts, same verdict.

`tests/server/router-status` is a move of `tools/router-status-check.sh` and is the first
`model`-tier test: it starts a server, so it requires `FG_ALLOW_MODEL_RUN=1` - the explicit
permission rule 1 demands - and otherwise skips. Its host handling was hardened after the first
run of this script left a server holding 9101: the server is now a direct child, so the recorded
pid is the server's own, and cleanup verifies the port was released instead of assuming it.

## Fifth batch

`tests/integration/goose-composition` wraps `tools/check-composition.sh`: the nine canonical module
sets still compose and typecheck together. It is not a duplicate of `tools/check.sh fast` - that
level composes the single cumulative unit and compiles it, while this one names the individual set
that broke.
| server | origin-policy | `tests/server/origin-policy` | native origin_is_localhost | P8-12 | fast | 29/29 agree 2026-11-19 |
| regression | extra-objects-provenance | `tests/regression/extra-objects-provenance` | 18 hand-built objects | P10-06 | fast | 18/18 2026-11-19 |
| integration | goose-modules | `tests/integration/goose-modules` | m1 stage build doors | P10-06 | integration | FG_MODULES 8/8 against runtime/goose-stage 2026-11-19 |
| regression | set-self-contained | `tests/regression/set-self-contained` | a hash-matching set that cannot load | rule 22 | fast | release ok, checkpoint FAILS 2026-11-19 |
| windows | alpha-transport | `tests/windows/alpha-transport` | Windows Alpha | WinSock, Cyrillic GGUF path, Ember assets, OpenAI JSON and SSE | integration | opt-in fixture, no real GGUF inference |
