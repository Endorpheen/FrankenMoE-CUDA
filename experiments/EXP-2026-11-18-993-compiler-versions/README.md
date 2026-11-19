# EXP-2026-11-18-993: Compiler versions have numbers, a registry and a changelog

Status: the scheme, the release tool, the registry labels and the artifact identity are
in place. New version **0.1.2+18b469220a78** is the first complete release under it.
The compiler that builds the runtime is still **outside** the registry, and that is
now printed rather than hidden.

## The problem, stated as it was

`releases/fried-goose-*` and `.cache/selfhosted-builds/catalog.json` carried build
hashes and nothing else: no number a person can quote, no changelog, no rule for when a
version is issued, no link from a runtime artifact to its compiler, and no `--version`
in the binary. 59 versions existed and none of them had a name.

## The scheme

    0.MINOR.PATCH+<12 hex of the build id>        0.1.2+18b469220a78

MINOR for a new capability, PATCH for a fix, the build id to keep it unambiguous. The
binary's SHA-256 remains the identity of record; the label is for people and is always
derived from the hash, never typed.

## The rule for issuing

A label is issued only when the qualification status is
`PASS_SELFHOSTED_SUBSET_FIXED_POINT_SAFETY_NATIVE` or
`PASS_CONTROL_CLONE_THREE_STAGE_FIXED_C_SANITIZED_OWN_EMISSION`. The tool refuses
otherwise, so an unqualified build cannot acquire a number by accident.

## What a version now contains

`/home/end0/AI/Fried-Goose/releases/fried-goose-18b469220a787097-20261119/`:

| artefact | value |
| --- | --- |
| `VERSION` | `fried-goose 18b469220a787097`, source commit, qualified date, profile, `label 0.1.2+18b469220a78` |
| `RELEASE.json` | label, source commit `de861d9df0f3…`, binary sha256 `18b469220a…`, generated-C sha256 `59f1c90a78c3…`, qualification status, 145 sources, 1 GiB stack, 2048 MiB AST budget, previous version |
| `README.md` | the same in prose |
| `.tar.gz` | 5 369 348 bytes, with its own sha256 |
| `CHANGELOG.md` | one entry: what changed, and the numbers |
| `catalog.json` | `label` written next to the build hash, so an artifact traces back |

Backfill: `0.1.0` = `3f6057754ec4b6b6`, `0.1.1` = `434af4f1e20b67b3`. The other 56 keep
their hash as their only identifier - renumbering history nobody recorded would be
invention.

## The gap this exposed

    $ python3 tools/compiler-identity.py
    compiler   /home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
    sha256     664d4de42463cc10cb9c4a61dac59ce6023faa9c9da2838fddc0f4fb8294b3ab
    declared   goose-2026.3-string (RELEASE_2026_3_STRING)
    verdict    NOT_IN_REGISTRY

**The compiler that builds the FrankenMoE runtime is not in the registry.** The
registry tracks `selfhosted_subset`; the runtime uses `llm_runtime`, whose entry reads
`NO_NEW_QUALIFIED_RUNTIME_BUILD` with `current: null`. So the new numbers do not cover
runtime builds yet, and 0.1.2 also carries `runtime_approved: false` - it is qualified
for the subset channel and **not** approved for the runtime. The runtime must not be
switched to it on the strength of a label.

Both the gates and the standalone build use the **same** compiler
(`664d4de42463cc10`); no version changed between them, which had to be checked rather
than assumed.

## What changed in this repository

- `tools/compiler-identity.py` - names the compiler from
  `runtime/compiler/current.json`, cross-references the registry, and exits non-zero
  under `--strict` when the compiler is unknown.
- `tools/migration-status.sh` prints one line for it in section 4, so no report can
  quote an artifact without its compiler.
- `docs/goose-migration/COMPILER-VERSIONS.md` - the scheme, the rule, the state, and
  the three remaining gaps.

## Two defects in the tool, both found by running it twice

1. **Not idempotent.** Finishing the same build twice issued `0.1.3` for a build that
   was already `0.1.2` and wrote a second changelog entry. A version tool that invents
   a version is worse than no tool; the label is now taken from the registry when one
   exists, and the duplicate entry was removed.
2. **The release lookup matched the archive.** `releases/fried-goose-<id>-<date>.tar.gz`
   shares the directory's prefix, so `startswith` picked the tarball and the tool then
   looked for `bin/goose` inside a file. Directories only.

## Result

| check | result |
| --- | --- |
| release 0.1.2 complete | VERSION, RELEASE.json, README, archive, hashes, changelog |
| re-run for the same build | label unchanged, one changelog entry |
| `compiler-identity.py` on this repository | `NOT_IN_REGISTRY`, stated |
| `migration-status.sh` | prints `goose-2026.3-string \| 664d4de42463 \| NOT_IN_REGISTRY` |
| gates and build use one compiler | yes, `664d4de42463cc10` |

## Not claimed

No runtime compiler switch was made: 0.1.2 is not runtime-approved, and switching
would need a benchmark, which needs approval.

The changes in `/home/end0/AI/Fried-Goose` are written but **not committed** there.
That is a different repository with uncommitted work of its own, and committing it is
Igor's call.

No model was started, no benchmark was run.
