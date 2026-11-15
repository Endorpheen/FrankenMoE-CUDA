# P10-05 — owned executable infrastructure and generators

Status: **PARTIAL** — 3 of 3 slices accepted; the script wiring is open.

## Slices

| slice | content | exports |
|---|---|---|
| S1 release plan | tag validation (bash ERE), prerelease, two runtime-asset rules, the 2 GiB and 100 MB limits, retention chain, dist list, byte-exact `SHA256SUMS`, byte-exact `manifest.json`, module count | `pkg_*` |
| S2 build environment | `${VAR:-default}` on unset and empty, the 14-word cmake vector in script order, build type for single/multi-config, subdir bitmask, `option()` table, pin check, three-state patch, symlink guard, integration markers, upstream pins | `env_*` |
| S3 generated-artifact policy | generated-C marker and 512-byte head trim, `.cuh` rule, MiB conversion, fail/warn branch, required-line parsing with comments and tabs, manifest problem order, orphan, verdict, `export fn` scan | `art_*` |

## Evidence

- Experiment: `experiments/EXP-2026-11-15-645-p10-05-owned-infra`.
- Canon: `goose/misc/{release-plan,build-env,generated-artifact-policy}.goose`.
- Gates: 73 + 44 + 54 values, 0 failures, identical at -O0.
- Mutations: 36/36 (12 per slice, halves 18/18).
- The oracles execute the native rule text instead of transcribing it: bash/ERE,
  `cmake -P`, the `package-main-runtime.sh` python block, and the imported
  `check-generated-files.py` / `check-goose-tree.py` functions. `SHA256SUMS` and
  `manifest.json` are compared byte for byte.

## Remaining

Wire the release plan, build-environment resolution and artifact policy into
`scripts/` and `tools/`.

## Boundaries

The generated C and binaries stay out of the repository. The slices own the
decisions, not the file system: writing artefacts, running bash/cmake and
invoking the tools stay with the host.
