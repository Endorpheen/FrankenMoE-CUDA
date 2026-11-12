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

## Update 2026-11-12: P0-05 accepted

The coverage registry now carries a decision for every one of the 62 modules in the accepted library:

| status | count | basis |
|---|---|---|
| proven live | 43 | 42 witnesses in the accepted request run + fg390s via a special-token fixture |
| OFF_PATH, documented | 19 | gating condition or configuration recorded per module |
| unclassified | 0 | - |

Why silence counts as evidence: the bridges either print on their first call or report at exit, and the runner delivers
SIGINT, so a missing line means zero calls. Examples settled this way: fg244s needs `n_outputs > 1`, fg403s is in the
file-pointer load branch while the server loads by path, fg377s lives in the direct-I/O path while the profile uses mmap,
six KV state modules sit behind io state_write which RAM-only checkpoints never reach.

Acceptance criterion met: every supported mode has a witness, unsupported coverage is explicit, and nothing is claimed
live from symbol presence. Apex regression after the work: prefill 2356.5, decode 24.31, VRAM 3190, content equal.

Counters after this item: 19 DONE, 2 PARTIAL, 50 IN_PROGRESS, 81 TODO (152 total).

## Update (this session, after the verification work)

Counters: **20 DONE, 1 PARTIAL, 50 IN_PROGRESS, 81 TODO** of 152.

What changed since the first audit:
- P0-05 accepted: 62 registered modules resolved into 43 live and 19 documented OFF_PATH, zero unclassified.
- P0-04 accepted: boundary contracts (no structs cross, sizes verified), zero implicit declarations, full primitive inventory.
- P1-09 gained model-level determinism evidence (batch threads 8/16/32 all give e68df05b) but stays IN_PROGRESS on its
  native allocation/placement remainder.
- P2-01 now has a measured 37-case map instead of an estimate; three findings located to lines, all blocked on the same
  permissioned regeneration pass.
- P1-06 has a verified inventory; TQ/FP4/legacy cannot be declared out of scope because types arrive via GGUF.

The remaining work is dominated by two kinds of item: (a) items needing a permissioned Goose regeneration (P2-01's three
fixes), and (b) stage-2-scale porting (P1-06, P1-09, P1-10 and everything behind them). No item is closed without evidence.
