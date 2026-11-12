# Big-plan audit, 2026-11-12

Basis: `tasks.json` (152 requirements, updated 2026-11-11), `coverage-registry.json`, the accepted library
`libllama-exp371` (62 `fg` modules), `APEX-FINAL-20261112.md`, and the experiment tree. The small Apex path and the big
runtime plan are kept separate here, as required.

## Four counters (requirement count, not project percentage)

| counter | count | basis |
|---|---|---|
| fully closed | 18 | `tasks.json` DONE, each with its own acceptance evidence |
| partially implemented | 3 | P0-04, P0-05, P2-01 |
| awaiting acceptance | 1 | P0-05 registry rebuilt today (14 -> 62 modules); needs liveness for the remaining witnesses before it can be called done |
| without confirmed implementation | 130 | the rest of IN_PROGRESS/TODO, i.e. requirements with no source+producer+active-path proof |

The Apex small plan is separate and closed: 15/15 rows DONE, final run `e68df05b`, in `APEX-FINAL-20261112.md`.

## What the audit found that the registry does not reflect

1. **The coverage registry was stale by 48 modules.** It listed 14 modules while the accepted library contains 62, all
   traceable to experiment sources. Rebuilt from the library symbols plus `fgNNNx_enabled` provenance; 42 of the 62 also
   have their witness string in the newest accepted run log. Symbol presence is not execution proof, and this is stated
   in the file itself.
2. **The Apex port closes parts of six big-plan areas but not the requirements as written.** Areas CORE, LOAD, TOKENIZER,
   KV, RECURRENT_MEMORY, BATCH, OUTPUT, CPU_CONTROL, CPU_MATH, CUDA_MATVEC, CUDA_CONTROL, CUDA_MMQ and MODEL_GRAPH have
   ported decision modules on the executed path. Their requirement text is broader ("port KV cell allocation, sequence
   ownership and position maps", "port complete shared graph-builder DSL"), so they stay IN_PROGRESS rather than being
   relabelled done. That is a deliberate refusal to inflate the count.
3. **EXP087 and EXP101 do not close P5-06.** They are C++ patches in our tree, not Goose implementations. The requirement
   asks for Goose ownership of lazy KV/VMM reservation, so it remains open while the patches are recorded as optimisation,
   not migration.
4. **No duplicate requirements found** among the 152; the scope narrowing of 2026-11-08 already removed 8 legacy items.

## Next step chosen from the partial list

P0-05 (coverage registry) was the cheapest to advance with real value, because it is the ledger every later acceptance
depends on. Today's increment: 14 -> 62 modules with provenance and a witness column, generated from the built library
rather than hand-maintained. Remaining for full closure: liveness for the 20 modules whose witness is not in the current
log, which needs either their profile or their teardown path.
