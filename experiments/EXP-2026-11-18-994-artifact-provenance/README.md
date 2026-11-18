# EXP-2026-11-18-994: The artifact records the compiler that built it

Status: the link between a runtime artifact and its compiler is now a measured record
inside the artifact, not a value read from configuration. 3 files, one rebuild.

## The gap this closes

EXP993 made the compiler identifiable, but the report read
`runtime/compiler/current.json` - which says what the **next** build would use. Nothing
recorded what produced the bytes that exist. After a compiler change those two answers
differ, and the report would confidently name the wrong one.

    before:  compiler of this artifact   goose-2026.3-string | 664d4de42463 | configured only
    after:   compiler of this artifact   goose-2026.3-string | 664d4de42463 | built this artifact

The word before the pipe is the same; the claim is not. That difference is the whole
point, and it is now visible instead of implied.

## What was done

1. **One compiler identity.** `tools/build-standalone-runtime.sh` had a hardcoded
   `GOOSE` path that duplicated `runtime/compiler/current.json`. It now reads that
   record, so the build and the gates cannot silently name different compilers.
2. **Provenance written by the build.** The script's new step measures the compiler's
   SHA-256 and writes `builds/goose-standalone/COMPILER.json`: path, sha256, registry
   label and version when the registry knows it, the unit's source hash, and the
   timestamp. Measured during the build, not reconstructed afterwards.
3. **The report reads the artifact.** `tools/migration-status.sh` prefers
   `COMPILER.json` and falls back to configuration only when there is none, saying so.
4. **Configuration and artifact are compared.** `compiler-identity.py --artifact DIR`
   prints both and states whether the artifact was built by the compiler that is
   configured now.

## Evidence

The provenance record produced by the build:

    compiler   /home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
    sha256     664d4de42463cc10cb9c4a61dac59ce6023faa9c9da2838fddc0f4fb8294b3ab
    verdict    NOT_IN_REGISTRY
    unit       28390c01c122967bd7f5f7884909c67cbbf84d76ed5a3bcb9afd43f7504c120c

The registry path proved itself in the other direction, against the compiler the
registry does know:

    $ python3 tools/compiler-identity.py --compiler-json <0.1.2>
    label      0.1.2+18b469220a78  (subset-18b469220a787097, channel selfhosted_subset)
    source     de861d9df0f3e2b7c2ea10153810e85a5a145b55
    verdict    VERSIONED

And the mismatch is detected rather than assumed away:

    $ python3 tools/compiler-identity.py --compiler-json <0.1.2> --artifact builds/goose-standalone
    built by   goose-2026.3-string
    configured subset-18b469220a787097
    agreement  MISMATCH - the artifact was built by a different compiler

## A defect in the registry, found by reading its own output

`compiler-identity.py` printed `label None` for the current version. The label had been
written into the catalog's `versions` list but not into the channel's `current` entry -
and the channel entry is the record a reader asks "what is the current version" about.
The most-read record was the empty one.

Both records are now written together, and the existing catalog was repaired.

## Result

| check | result |
| --- | --- |
| artifact provenance | `COMPILER.json`, written by the build |
| report wording | `built this artifact` vs `configured only` |
| registry lookup of 0.1.2 | `VERSIONED`, label and source commit |
| configuration vs artifact | `MISMATCH` detected |
| unit | unchanged: `28390c01c122…`, 1161 globals |

The gate suite is not re-run: no file under `goose/` changed in this cycle, and
`verify-unit-gates.sh` does not call the build script. The last measurement stands -
38 274 checks, 0 failures - because its inputs are unchanged.

## Not claimed

No switch to 0.1.2. It remains `runtime_approved: false`, and a switch needs a
benchmark and approval.

`COMPILER.json` is git-ignored with the rest of `builds/`, so the record lives with the
artifact it describes rather than in the history. That is deliberate: a provenance file
checked into git would describe a build nobody has.

The changes in `/home/end0/AI/Fried-Goose` are still uncommitted.

No model was started, no benchmark was run.
