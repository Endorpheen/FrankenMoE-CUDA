# REPORT — EXP-2026-11-15-645 (P10-05, owned infrastructure and generators)

## Result

| item | value |
|---|---|
| slices | **3** (release/package `pkg_`, build preflight `env_`, artifact policy `art_`) |
| gates | **3/3 green**, 0 failures, identical at `-O1` and `-O0` |
| cases / values | 73 / 73 (pkg), 44 / 44 (env), 54 / 54 (art) → 171 |
| mutations | **36/36 caught**: 12 per slice, two halves of 18/18 |
| native oracle | the scripts themselves: bash, GNU tools, `cmake -P`, and the scripts' own python |
| model / GPU / server / benchmark | none started |
| repository changes | none (work under `/tmp/opencode/sub-647/`) |
| base commit | `792293ad`; the seven native inputs unchanged at `c344b309` |

## Method

1. **Inventory.** Read `scripts/**`, `tools/**` (build, packaging, checks,
   generator and hygiene scripts), `runtime/CMakeLists.txt`, `runtime/cli`,
   `tools/experiment-template` and the P10-05 row of
   `docs/goose-migration/tasks.json`. The acceptance criterion is explicit:
   owned generator decisions move to Goose, declarative build declarations and
   launch wrappers may stay. There is no `cmake/` directory in this repository.
   The decision table and the rejected candidates are in `README.md`.
2. **Three independent slices, one Goose module each.** `pkg_` covers
   `scripts/make-release.sh` + `tools/package-main-runtime.sh`; `env_` covers
   `scripts/build.sh` + `scripts/prepare_upstreams.sh` + `runtime/CMakeLists.txt`;
   `art_` covers `tools/check-generated-files.py` + `tools/check-goose-tree.py`.
   Exports take and return scalars and slices only; there is no shared state.
3. **Differential gates with executable oracles.** Native copies live in
   `tests/native/` (callback: every rule is extracted from that text at run time
   and executed by the native interpreter). Bash evaluates the tag ERE, the
   prerelease glob, the asset case pattern, the `-gt` limits, `basename`,
   `grep -cE`, the awk size rule and the retention guards; `cmake -P` evaluates
   the build-type block, the `option()` lines and the `if(BMOE_BUILD_…)`
   conditions; the packaging `<<'PY'` block is executed as the script runs it and
   its `SHA256SUMS`/`manifest.json` bytes are compared byte for byte;
   `check-generated-files.py` and `check-goose-tree.py` are imported and their
   predicates called directly, with the size-limit and gate expressions `eval`ed
   from the copied text.
4. **Mutation run.** 12 mutations per slice, split 6/6, every mutated module
   generated at `-O0`; a surviving mutation is a gate hole. One survivor was
   found and closed on the first pass (the prerelease marker `-rc.` vs `-rc`
   was not distinguished by the case table), and one missing case kind
   (`art_orphan`) was added for the same reason. Both are recorded here rather
   than hidden.

## Gate output

```
FG645_PKG cases=73 values=73 failures=0
FG645_ENV cases=44 values=44 failures=0
FG645_ART cases=54 values=54 failures=0
```

Same lines from the `-O0` run (`results/gate-o0.log`).

Coverage in brief: tag acceptance and refusal (12 spellings), prerelease
detection, POSIX basenames, the two different runtime-asset rules, both size
limits at their exact boundary, the retention guard chain, the dist file list,
`SHA256SUMS` byte format, `manifest.json` byte format with escaping, the module
count with leading/trailing whitespace and `\r`, `tail -1` extraction; the
`:-` default on unset and empty, the 14-word configure vector with and without
overrides, the build-type default in single- and multi-config, the subdirectory
bit mask across six combinations, the parsed `option()` table evaluated by
CMake, SHA equality with case and length differences, the three-way patch state,
the symlink guard, the ten integration markers with a missing one, the pinned
upstream table extracted from the script; the generated-C marker with and
without `Do not edit.`, the 512-byte head truncation, the `.cuh` path rule with
case sensitivity, the MiB limit conversion, the fail/warn branch, required-line
splitting with comments, trailing spaces and multi-tab snippets, the manifest
problem order, the orphan pass, the exit rule and the `export fn` scan.

## Mutation halves

`results/mutate-1.log`: 18/18 caught. `results/mutate-2.log`: 18/18 caught.
Each line names the rewritten decision and the gate line that killed it, e.g.

```
CAUGHT   pkg  rc suffix off by one                         FG645_PKG cases=73 values=73 failures=2
CAUGHT   env  tests subdirectory bit folded into cli       FG645_ENV cases=44 values=44 failures=2
CAUGHT   art  limit converts with 1024                     FG645_ART cases=54 values=54 failures=6
```

## Memory and runtime impact

No runtime path changes: the three modules are pure decision functions with no
allocation, no global state and no I/O, and nothing in the current binary calls
them yet. Generated C and object sizes, for the record:

| slice | generated C | object `-O1` | exported symbols |
|---|---:|---:|---:|
| `pkg_` | 206 622 B | 274 808 B | 14 |
| `env_` | 196 605 B | 268 248 B | 13 |
| `art_` | 185 586 B | 262 752 B | 10 |

Source size: 490 + 391 + 292 = 1 173 lines of Goose.

## Artifacts

```
source/release-plan.goose                 pkg_ module
source/build-env.goose                    env_ module
source/generated-artifact-policy.goose    art_ module
tests/cases/{pkg,env,art}.tsv             the single input of both arms
tests/oracle_{pkg,env,art}.py             executable native oracles
tests/{pkg,env,art}_fixture.cpp           the two-arm comparison
tests/build.sh                            -O1 / -O0 gate build
tests/mutate.py                           12 mutations per slice, halves 1 and 2
tests/native/*                            byte-identical native inputs
results/gate-o1.log, gate-o0.log          gate output
results/mutate-1.log, mutate-2.log        mutation output
results/native-sha256.txt, hashes.txt     SHA-256 of inputs, ports and gate
README.md                                 interfaces, integration, assumptions
```

No generated C, object, gate binary or `__pycache__` is part of the evidence:
`build/` is reproducible from the sources and the build script.

## Integration step

Copy the three modules to `goose/misc/{release-plan,build-env,artifact-policy}.goose`,
append those paths to `MODULES` in `tools/build-main-runtime.sh`, run
`bash tools/build-main-runtime.sh` (expect 0 duplicate global symbols), register
the files in `goose/MANIFEST.json` and run `python3 tools/check-goose-tree.py`.
No `compose-goose.py` `RENAMES` entry is needed: internal prefixes `rlp_`, `ev_`,
`gap_` were renamed after a collision check against the 66 modules of the main
build.

## Assumptions and open questions for the lead

- The MiB limit conversion is exact for the values in the case table; a
  non-representable `--mb` is out of contract (`art_limit` is integer milli-MiB).
- `pkg_manifest_json` assumes ASCII field values; non-ASCII would need
  UTF-8 aware `\uXXXX`, which no current input produces.
- `env_jobs` returns text, because the script performs no numeric validation;
  if the lead wants the numeric decision modelled, it belongs to `cmake`, not to
  the script.
- The native oracles need `cmake` on `PATH` (4.2.3 here) and GNU `awk`
  (`\s` in the grep pattern is a GNU extension, as in the native script).

## Verdict

ACCEPTED candidate, model-free. The lead's acceptance and the single atomic
commit are the remaining gate.
