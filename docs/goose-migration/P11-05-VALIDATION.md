# P11-05 — clean build, rollback and failure recovery

Status: **PARTIAL** — the clean build and the rollback path are proven by M1; the
failure-recovery matrix is open.

## Proven by M1 (EXP658)

| check | result |
|---|---|
| clean configure + build, OFF | ok, two upstream warnings only |
| clean configure + build, ON-all | ok, two upstream warnings only |
| `llama-server` linked, OFF | 17 794 640 B |
| `llama-server` linked, ON-all | 20 316 528 B |
| rollback: flags OFF | the native path builds and links; `goose_init` count 0 in all 12 OFF binaries |
| Apex reference binary | unchanged (`badbfc5cf63895a9…19f10d26`, mtime 12.11) |
| determinism | the unit reproduces byte-identically from the same module list |

Rollback is a rebuild with the flags off, not an in-place revert: the OFF and ON
trees are separate build directories, and the OFF library is byte-identical to
the base build in the S1.3 staging experiment.

## Still open

The failure-recovery matrix:

1. A missing generated C or adapter object must fail the link, not fall back
   silently.
2. A wrong flag value must be rejected at configure time.
3. A broken IQ table registration must abort loudly (the current guard widens to
   `FG_GOOSE_RUNTIME`).
4. A partially switched build must still link and run the native path.
5. The rollback must be exercised after a failed ON build, not only from a clean
   tree.

Each item needs a scripted check with a recorded exit code; none is executed yet.

## Not claimed

No model, server or GPU run is part of this validation. Functional recovery
after a runtime failure (crash, OOM, interrupted load) belongs to the Apex
milestone and needs Igor's approval.
