# P10-09 S1 — test-runner policy (selection, fixtures, verdicts, summary)

File: `source/tr-testplan.goose` (667 lines, 11 exports, `tr_` prefix).
Base commit: `70de805c`. Every native file below is byte-identical at the
current HEAD `9ac3eab0` (checked with `git diff`), so the analysis is not stale.

## Hypothesis

A native-free build cannot run the carrier's CTest registry, because CTest
drives CMake targets that link the owned implementation. What a native-free
build actually needs from a runner is a small set of decisions: which tests
are selected, in which order fixture setup/cleanup tests run, what a return
code means, how the counts are summarised and which exit code the process
returns. Those decisions are separable from process execution, and CTest itself
can serve as the oracle.

## Native sources

| native | what is owned here |
|---|---|
| `runtime/tests/CMakeLists.txt` | the registry shape: per-architecture fixtures via `FIXTURES_SETUP`/`FIXTURES_REQUIRED`, the split-shard case, the python3-present skip |
| `upstream/llama.cpp/tools/server/tests/tests.sh` | the `SLOW_TESTS` switch and the `-m "not slow"` deselect |
| `upstream/llama.cpp/tools/server/tests/pytest.ini` | the marker declaration |

The oracle is CTest 4.2.3 itself: the same definitions are handed to `ctest`
and to this module, and every decision is compared.

## Interfaces

| export | decision |
|---|---|
| `tr_match(pat, s) -> bool` | POSIX-ERE-subset match: literals, `.`, `^`, `$`, classes with ranges and negation, `\d \D \w \W \s \S`, escapes, `* + ?`, top-level `\|`; unanchored search |
| `tr_selected(name, filter, exclude) -> bool` | the `-R`/`-E` rule: empty include selects everything, empty exclude drops nothing; `DISABLED` is not a filter |
| `tr_plan(offs, lens, blob, n, setup, req, clean, disabled, filter, exclude, order, n_out) -> i64` | CTest's `UpdateForFixtures` + serial cost list: selected tests keep definition order, a required fixture pulls in its setup tests and appends its cleanup tests, cleanups depend on every dependent and on the setups, and the order is a depth-first walk over the roots |
| `tr_dep_blocked(req_mask, fx_state) -> i64` | a failed or never-run setup blocks dependents; a skipped setup does not |
| `tr_fx_state(status) -> i64` | the fixture state a setup test's verdict leaves behind |
| `tr_classify(rc, has_rc, skip_rc, has_skip, dep_blocked) -> i64` | pass / fail / skip / not-run from the return code |
| `tr_status(..., disabled) -> i64` | the same, with `DISABLED` first |
| `tr_percent(reported, disabled, failed) -> i64` | `100*(T-F)/T` rounded half up, `T = reported - disabled` |
| `tr_summary(reported, disabled, failed, notrun, out, out_code) -> i64` | the summary line text and the process exit code (8 on any failure) |
| `tr_row(seq, total, id, name, status, out) -> i64` | the progress row, including `***Not Run (Disabled)` |
| `tr_fail_row(id, name, status, out) -> i64` | one row of the failure list |

## Gate

`tests/gen_tr.py` writes 23 synthetic CMake projects, configures each, runs the
real `ctest --output-junit`, then generates a Goose driver that embeds the same
definitions and the same process return codes and compares:

* the execution order of the reported tests,
* every per-test status word taken from ctest's own progress lines,
* the disabled rows,
* the summary line, character for character,
* the process exit code,

at `-O1` and `-O0` (the two levels must produce identical bytes).

```
cd tests && python3 gen_tr.py          # 23 scenarios, cached native truth
cd tests && python3 gen_tr.py --rebuild  # re-run cmake + ctest for every scenario
```

Result: **23/23 scenarios identical, 0 disagreements, `-O1` == `-O0`.**
The 23 scenarios cover: plain pass/fail, all-skip, skip mixed with pass, a
disabled test among failures, a working fixture, a failing setup, a skipping
setup, two fixtures at once, a nested fixture (a setup test that itself needs a
fixture), a three-way not-run chain, `-R` include, `-E` exclude, a filter that
pulls a setup in, a filter over a character class, the 13% rounding case, two
dependents on one fixture, a not-run plus skip mix, cleanups defined first, one
cleanup test for two fixtures, `-R` selecting only a setup, three regex shapes,
and a filter that pulls a cleanup in.

## Mutations

`tests/mutate_tr.py <lo> <hi>` edits one decision, regenerates the drivers and
compares against the cached native truth.

| # | mutation | result |
|---|---|---|
| 0 | the filter search becomes anchored | caught (`filter_include`) |
| 1 | class negation is ignored | caught (`filter_neg_class`) |
| 2 | dot stops matching | caught (`filter_dot`) |
| 3 | plus allows zero repetitions | caught (`filter_classes`) |
| 4 | alternation is not split | caught (`filter_include`) |
| 5 | an empty exclude filter excludes everything | caught (`basic`) |
| 6 | a failed setup stops blocking its dependents | caught (`setup_fail`) |
| 7 | a skipped setup blocks its dependents | caught (`setup_skip`) |
| 8 | cleanup tests are never appended | caught (`filter_pull_cleanup`) |
| 9 | setup tests are never appended | caught (`filter_pull_setup`) |
| 10 | dependencies are emitted after their dependents | caught (`fixtures_ok`) |
| 11 | the disabled test is classified from its command | caught (`disabled_mixed`) |

The table carries three further entries (percentage truncation, denominator
including disabled tests, a skip classified as a failure) for a third block:

```
cd tests && python3 mutate_tr.py 0 6    # 6/6
cd tests && python3 mutate_tr.py 6 12   # 6/6
cd tests && python3 mutate_tr.py 12 15  # 3/3
```

All twelve scheduled mutations are caught, and the three extra ones too.

## Recorded limits

* The regex is a documented subset. Groups, back references and `{m,n}` are not
  implemented: `tr_atom_len` returns 0 for `(`, `)`, `{`, `}` and the match then
  fails, so an unsupported filter selects nothing rather than silently
  selecting the wrong tests. Every filter in this tree stays inside the subset.
* Costs and `--schedule-random` are not modelled: the oracle is the serial
  scheduler. `-j` would change only the order, not the statuses.
* `EXCLUDE_FROM_ALL`, `PROCESSORS`, `RESOURCE_LOCK`, labels and `DEPENDS`
  properties are not owned; only fixture dependencies and `DISABLED` are.
* The oracle is CTest 4.2.3. A newer CTest that changes `UpdateForFixtures`
  semantics would move the golden rows, not the port.

## Assumptions and conflicts

* One fixture id per fixture name, packed into an `i64` bitmask (30 ids). The
  carrier's registry uses fewer.
* `i64[64]` bounds the test table; a larger registry is refused with `-1`
  rather than truncated.
* No conflicting module: nothing in `goose/` implements runner policy. The
  nearest neighbour, `misc/generated-artifact-policy.goose` (P10-05 S3), owns
  artifact and tree checks and does not overlap.
* The module needs no externs and imports only `std`.

## Integration step

1. Copy `source/tr-testplan.goose` to `goose/misc/test-runner.goose`.
2. Add to `tools/sync-goose-tree.py` `CANONICAL_SOURCES`:
   `("misc/test-runner.goose", "EXP-<date>-<nnn>-p10-09-test-runner", "source/tr-testplan.goose", "runtime/tests/CMakeLists.txt, tools/server/tests/tests.sh", "23 cases / 0 failures, 12/12 mutations")`.
3. Add the matching `goose/MANIFEST.json` entry with the file's SHA-256.
4. Update `docs/goose-migration/tasks.json` for `P10-09` (`slices_done` 1 of 3,
   `remaining` listing the fixture and diagnostics slices, `coverage_doc`).
5. Do **not** add this module to `tools/check-composition.sh`: its exports take
   slices, and a module with slice-parameter exports only specialises when a
   driver calls it (the same is true of the accepted
   `misc/generated-artifact-policy.goose` set). Each gate driver *is* the
   composed unit; `gen_tr.py` builds one per scenario.

## SHA-256

```
2f42df0fe0d69ea70913667d5f22b2695070e6c96b3f8f3c659f87d4d53a7465  source/tr-testplan.goose
18138cec09ce52c2d5981cc23454729dde053714ad32278d6c99bc762f48a334  tests/gen_tr.py
46319743ab1a1f2496dac398d2ac0bbaecf4090420d78dc6f20e4301dc2d79f3  tests/mutate_tr.py
b51686e681763078de5a785cd26a435eee75b2a432cc70a3c5ba527b456e5b68  build/native.json
```
