# P10-09: test runners, fixtures and diagnostics - what exists and what is accounted for

Recorded 2026-11-19. The requirement asks the final build to be functionally testable without
linking replaced C++ into the production artefact, and every absent diagnostic tool to be accounted
for rather than wired to nothing.

## Present

| item | state |
|---|---|
| `goose/metrics/bench-diagnostics.goose` | exists, three exported doors (`dg_percentile`, `dg_median`, `dg_analyze`), **not** in the composed unit list |
| permanent tests | `tests/`, ten tests, each with `run.sh`, `tier`, `README.md` |
| one runner | `bash tests/run.sh list\|fast\|<area>\|integration\|full\|performance` |
| registry | `tests/REGISTRY.md`, a row per test with its EXP and requirement, added in the same commit |
| fixtures | `tests/fixtures/` (empty until a second test needs the same input) |
| oracles | `tests/oracles/` (empty; the grammar oracles are compiled into their fixtures today) |

## Absent, and therefore not wired

| item | finding |
|---|---|
| `scripts/bench-analyze.py` | not in the tree |
| `scripts/trace_io.py` | not in the tree |
| callers of `bench-diagnostics.goose` | none: the module is not in `tools/build-main-runtime.sh` and no host calls `dg_*` |

The requirement's own wording is "when their caller programs are present". They are not, so the
module stays uncomposed and unwired, and no fake hook is created to make the ledger look better.
Composing it without a caller would add a module to the unit and prove nothing (rule 33).

## The criterion, and how it is met

"the final build can be functionally tested without including the replaced C++ in the production
artefact" is demonstrated by the permanent tests, not by the diagnostics tools:

- the native oracle is compiled **into the test fixture** (`tests/grammar/*/fixture.cpp` includes
  `vendor/flint/src/llama-grammar.cpp`), never into the library that ships;
- the production artefacts stay untouched by a test run: each test writes only under its own
  gitignored `build/`;
- a result always names the artefact and the source revision: the library-path tests record the
  library path, its SHA-256 and the commit;
- a check that needs a model or a GPU says so in its `tier` and refuses to run without its
  precondition, so the reproducible set is well defined.

## Consequence

P10-09 closes on this accounting: the test surface exists and is reproducible, the oracles are
outside the production link, and the three absent items are named with the reason they are absent.
No item is claimed as wired.
