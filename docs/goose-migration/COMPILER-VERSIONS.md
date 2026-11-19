# Compiler versions

The compiler had no `--version`, and a runtime artifact carried no trace of the build
that produced it. Versions existed only as build hashes in directory names and in
`.cache/selfhosted-builds/catalog.json`: no number a person can quote, no changelog,
no rule for when a version is issued, and no link from an artifact to its compiler.

This document defines the scheme and records the state. It lives here, but the
registry and the changelog live with the compiler they describe:

    /home/end0/AI/Fried-Goose/.cache/selfhosted-builds/catalog.json   the registry
    /home/end0/AI/Fried-Goose/CHANGELOG.md                            human-readable
    /home/end0/AI/Fried-Goose/releases/fried-goose-<id>-<date>/       one release
    /home/end0/AI/Fried-Goose/tools/compiler-release.py               the release tool

## The scheme

    0.MINOR.PATCH+<12 hex of the build id>          e.g. 0.1.2+18b469220a78

- **MINOR** grows on a new capability;
- **PATCH** grows on a fix;
- the **build id** keeps the label unambiguous, so two builds can never share one.

The binary's own SHA-256 remains the identity of record. The label is for people; the
hash is for verification. A label that disagreed with the hash would be worse than no
label, which is why the tool derives it from the hash and refuses to invent one.

## The rule for issuing a version

A build gets a label **only** if its qualification status is one of

    PASS_SELFHOSTED_SUBSET_FIXED_POINT_SAFETY_NATIVE
    PASS_CONTROL_CLONE_THREE_STAGE_FIXED_C_SANITIZED_OWN_EMISSION

Anything else is a build, not a release. `tools/compiler-release.py` enforces this: it
exits with an error rather than labelling an unqualified build.

Issuing is mechanical, not a decision taken per case:

    python3 tools/compiler-release.py --build subset-<id> \
        --summary "one line" --change "..." [--change "..."] [--minor]

The tool then writes every artefact a version must have: `VERSION` (with the label
line), `RELEASE.json`, `README.md`, the `.tar.gz`, the binary and generated-C
SHA-256s, the source commit, the source count, the profile (stack reserve, AST
budget), the qualification status, and a `CHANGELOG.md` entry with the gate results.
It records the label in the catalog next to the build hash, so any artifact can be
traced back.

Re-running it for the same build is a no-op on the label: a build that already has one
keeps it.

## State as of 2026-11-18

| label | build id | note |
| --- | --- | --- |
| 0.1.0 | `3f6057754ec4b6b6` | last release that was already fully formed; labelled retroactively |
| 0.1.1 | `434af4f1e20b67b3` | caller-argument ordering fix |
| **0.1.2** | `18b469220a787097` | **current for the subset channel**: default-expression ordering and view liveness |

0.1.2 is the newest qualified build. Its `RELEASE.json` records source commit
`de861d9df0f3e2b7c2ea10153810e85a5a145b55`, binary SHA-256
`18b469220a7870974e55a9e67504623c00143ecf9d2eb8bf9537fe0064f1c40d`, generated-C
SHA-256 `59f1c90a78c3df0225f1921397b4f8178ea91eec06345a4ae39d2d2a2647cc06`, 145
sources, 1 GiB stack reserve, 2048 MiB AST budget.

Earlier versions (59 entries in the catalog) keep their build hash as their only
identifier and are **not** renumbered. Renumbering history nobody recorded would be
invention.

## Which compiler builds the runtime - and the gap

`bash tools/compiler-identity.py` answers this from `runtime/compiler/current.json`
and the registry:

    compiler   /home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
    sha256     664d4de42463cc10cb9c4a61dac59ce6023faa9c9da2838fddc0f4fb8294b3ab
    declared   goose-2026.3-string (RELEASE_2026_3_STRING)
    source     e0501dc7dee4522428a46f813cebbce0ad62736e
    verdict    NOT_IN_REGISTRY

**The compiler that builds the FrankenMoE runtime is not in the version registry at
all.** The registry tracks the `selfhosted_subset` channel; the runtime uses the
`llm_runtime` channel, whose catalog entry says `NO_NEW_QUALIFIED_RUNTIME_BUILD` with
`current: null`. The new numbers therefore do **not** cover runtime builds yet.

This is stated rather than papered over for one specific reason: 0.1.2 also carries
`runtime_approved: false` in the registry. It is qualified for the subset channel and
not approved for the runtime, so the runtime must **not** be switched to it on the
strength of its label. The switch needs a runtime qualification, which is a benchmark
and therefore needs approval.

`tools/migration-status.sh` prints one line for this, so no report can claim an
artifact without saying which compiler produced it:

    compiler of this artifact     goose-2026.3-string | 664d4de42463 | NOT_IN_REGISTRY

## What is still missing

1. **`--version` in the compiler.** Recorded in every `RELEASE.json` as
   `"version_flag": "not supported; the compiler prints its usage for --version"`. It
   stays a plan item, not a note: until the binary can answer, a user still has to
   hash the file to know what they have.
2. **A labelled `llm_runtime` build.** The channel has no qualified entry, so runtime
   artifacts are traceable by SHA-256 only. The rule and the tool already accept a
   second channel; what is missing is a qualification run for it.
3. **Automatic issuing at qualification.** Today the tool is invoked explicitly. The
   qualification run should call it as its last step; that needs the qualifying
   script to be the only caller, which is a change in the compiler repository, not
   here.

## A defect this work produced, and what it cost

The first version of `compiler-release.py` was not idempotent: finishing the same
build twice issued `0.1.3` for a build that was already `0.1.2` and wrote a second
changelog entry. A version tool that invents a version is worse than no tool. The
label is now taken from the catalog when one already exists, and the duplicate was
removed rather than left as history nobody can explain.

A second defect, found in the same pass: the release-directory lookup matched the
`.tar.gz` next to the directory, because it shares the prefix, and then looked for
`bin/goose` inside the archive. Directories only.

Both were caught by running the tool twice, which is the only way to see either.
