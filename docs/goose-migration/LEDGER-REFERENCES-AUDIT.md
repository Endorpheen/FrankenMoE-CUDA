# Every document the ledger cites exists; 36 of its build outputs do not

Measured 2026-11-19 over the 152 records, by extracting every path-like token from the evidence fields
and asking the filesystem.

| kind of reference | count | missing |
|---|---|---|
| `docs/...` - a document | 130 | **0** |
| other paths - outputs, experiment trees, objects | 430 | 36 |

## The useful half of the result

All 130 document references resolve, and the 41 explicit `coverage_doc` / `evidence_artefact` fields
resolve as well. So nothing in the ledger points at a document that was renamed or deleted: a reader who
follows a `docs/` link lands on the file the record meant.

## The half that is noise, and why it is not a finding

The 36 unresolved paths are build outputs and transient trees - `runtime/reconstructed/build/run-ab-on1/server.log`,
`runtime/exp175/results/census/apex-census.sqlite`, `runtime/experiments/exp160/build/shared-cuda.o` and
similar. Those never persist by design: this project keeps evidence and deletes build artefacts, which is
rule 36's own requirement. Treating their absence as a defect would be the mistake this document exists
to avoid - the check ran, most of what it flagged is expected, and only that is recorded.

What a reader should take from it: a path in an evidence field is either a **document** (checkable, and it
exists) or a **record of an output** (historical, not checkable). The records themselves do not
distinguish the two, which is the only real observation here.

## What did not change

No status, no evidence text, no record was edited on the basis of this audit. The only action it justified
was the sentence above, so that the next person does not read a missing log path as a missing proof.
