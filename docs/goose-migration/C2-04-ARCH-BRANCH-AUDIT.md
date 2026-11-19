# C2-04 audit: one common implementation, every architecture branch with a recorded reason

Measured 2026-11-19 from `goose/MANIFEST.json` and the release set. The requirement is a standing
invariant, not a deliverable, so it closes on evidence that the invariant holds - not on a count of work.

## 1. One common implementation

There is exactly one composed unit: `tools/build-main-runtime.sh` composes **156 modules** into one
translation unit, and `check.sh fast` typechecks and compiles it (0 duplicate global symbols). The
architecture modules join that unit; none of them is built as a separate runtime, and no architecture
gets its own library.

## 2. Every architecture branch records its reason

`MANIFEST.json` carries **33 entries** whose `native_targets` point at architecture model sources
(`src/models/*.cpp`):

| property | count |
|---|---|
| entries pointing at architecture sources | 33 |
| with an experiment recorded | **33 of 33** |
| with a task id recorded | 4 of 33 |

The experiment record is the semantic reason: each card states what the port reproduces and what differs
from the native path (for example `EXP-2026-11-14-589-p6-05-qwen3next` records the gate and the 12
semantic mutations that must be rejected). The missing task id on 29 entries is a bookkeeping gap, not a
missing reason - it is recorded here rather than papered over.

## 3. The Apex history is unchanged

`frankenmoe-release-20261120-exp1476` verifies against its own `SHA256SUMS`: **exit 0**, every file
byte-identical to the published record. The accepted numbers (decode 62.86 median, prefill 2940.9,
response hash `c2ff42cc1219`) therefore still describe the artefact on disk, and no architecture work in
this session modified it.

## What this audit does not claim

It does not claim the 33 ports are exercised: most are compiled into the unit and not called from
production, which is the separate finding in `SERVER-DOORS-OUTSIDE-UNIT.md` and the per-requirement
`remaining` fields. C2-04 is about protecting the common runtime while architectures are added, not about
each architecture being reachable - and the audit says which of those two questions it answers.
