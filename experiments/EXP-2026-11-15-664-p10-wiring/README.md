# EXP-2026-11-15-664 — P10-05/P10-09 wiring into scripts and tools

Base: `0da127f5`; the target files were unchanged at `6659beb1`, so the gate
results remain valid. The working tree carried unrelated changes during
acceptance and they were not touched.

## What was wired

| decision | module | host |
|---|---|---|
| W1 generated-artifact policy | `misc/generated-artifact-policy.goose` (`art_`) | `tools/check-generated-files.py` |
| W2 build environment | `misc/build-env.goose` (`env_`) | `scripts/build.sh` |
| W3 release plan | `misc/release-plan.goose` (`pkg_`) | `scripts/make-release.sh`, `tools/package-main-runtime.sh` |
| W4 fixture plan | `misc/fixture-plan.goose` (`fx_`) | `scripts/make-tiny-moe.py` (and the fixtures it feeds) |

Transport: `tools/goose-hosts/` — four C hosts, each linked with the generated
unit built from the accepted Goose module (module sources unchanged), one line
per request. All four are ON by default; when the host binary is absent the
previous inline path runs with a warning.

## Evidence

- Host build: `results/hosts-build.log`, four hosts at -O1, rc=0.
- `w1_art`: corpus 23/23, E2E 14 scenarios / 28 comparisons, 0 failures.
- `w2_env`: corpus 27/27, E2E 12 environments / 36 comparisons, 0 failures.
- `w3_pkg`: corpus 73/73 (the full P10-05 S1 corpus, `SHA256SUMS` and
  `manifest.json` byte-for-byte); the heavy E2E arm was skipped at acceptance per
  the build rule and its earlier log is kept.
- `w4_fx`: 12 runs / 131 comparisons, byte-identical gguf files, banners and
  refusals.
- `w3_release` was skipped because the working tree was not clean (another
  session's edits); it is re-run at the next clean control point.
- Mutations: 48/48 (12 per decision, halves 24/24), logs `results/mutate-1.log`
  and `results/mutate-2.log`.
- An E2E run caught a real defect: the OFF branch of
  `tools/package-main-runtime.sh` dropped the four artefact copies; fixed and
  re-run 6/6.

## Patch application

`results/apply-check.log`: all six patches apply cleanly to `6659beb1`.
Patches 01-05 were applied to the repository; patch 06 only carried the gate
directory, which is committed here as the experiment.

## Remaining

- `test-runner.goose` cannot be generated as standalone C: `tr_match` and
  `tr_selected` export `bool`/slice signatures and the compiler cannot resolve a
  concrete function; the host wrapper does not help. This needs a small decision
  on the accepted module (return `i64` or drop `export`), then a gate re-run.
- `bench-diagnostics.goose` (`dg_`) has no host yet: `scripts/bench-analyze.py`
  and `trace_io.py` do not exist in the tree at this base; only the EXP-652
  oracle copies exist.
