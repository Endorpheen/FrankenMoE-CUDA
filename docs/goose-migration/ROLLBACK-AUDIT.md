# Rollback: a field on four records, a sentence on the rest

Measured 2026-11-19 over all 152 records.

| property | count |
|---|---|
| records DONE | 127 |
| with **any** evidence text | 127 (all of them) |
| with an explicit `rollback` field | 4 |
| without the field, but stating a revert path in prose | 40 |
| without the field and without a revert path in prose | 83 |

The four with the field are the ones closed today - F1-03, P10-10, C2-04, C1-03 - because the field was
added to the schema during this session. Everything else was written before it existed, and
`update_rule` in those records already demanded rollback evidence, which for them lives in the evidence
text ("revert via http://...", "the scalar baseline object is preserved as ...", "approved revert
list...") rather than in a key a tool can check.

## Why this matters for the handover

`F1-04` is the requirement that counts sources, artefacts and releases at the end. A rollback that
exists only as a sentence is a rollback nobody can execute without reading 127 evidence strings and
deciding which sentence is the instruction. Before final acceptance, each DONE record needs one of two
things:

1. a `rollback` field naming the artefact, commit or command that undoes it; or
2. an explicit statement that the change is documentation-only or that the artefact is preserved
   immutably, which is itself a rollback answer.

This was not backfilled here on purpose: writing "revert the commit" into 123 records without reading
each one is the kind of bulk edit that produces confident nonsense - the same failure mode as the
eleven wrong records corrected today.

## What is not at risk

No accepted artefact depends on this field. The checkpoints, the published release and its SHA256SUMS
verify on disk today (`sha256sum -c`: exit 0), so the ability to go back to a working runtime does not
depend on the ledger being complete - only the ability to know, per requirement, what "back" means.
