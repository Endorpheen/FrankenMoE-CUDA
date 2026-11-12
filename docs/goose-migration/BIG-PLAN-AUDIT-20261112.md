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

## Stale clauses, with proofs (do not re-search these)

Two requirements describe mechanisms that do not exist in the substrate that actually runs today (the flint tree). They are
recorded so the next agent does not spend a session looking for them.

| requirement | clause | proof | consequence |
|---|---|---|---|
| P1-10 | "expert staging ... upload grouping ... bank/slot ownership" as a pinned-ring mechanism | flint `ggml/src/ggml-backend.cpp` has no "group consecutive experts" path; `GGML_EXPERT_PINNED_RING` is absent from the build; the text exists only in old FrankenMoE patches | the ring clause cannot be completed as written; the live residency paths are `pin_mmap_experts`, the device cache (bank/slot planning already Goose), scratch policy (EXP-145) and KV lazy mapping |
| P1-09 | "expert chunk claims ... barrier state" | `fg140_claim` / `fg141_claim` are defined in the Goose modules and used only by their own fixtures; there is **no call site anywhere in the flint tree** | the claim/barrier half is dormant by construction; the live half is FG143/147/149 pool/executor/partition, all witnessed |

Two options exist for each, and neither is a porting task: restate the clause for the current substrate, or deliberately
reintroduce the old mechanism. Choosing is an owner decision; the audit only proves the clauses are stale.

What is **not** stale and must not be confused with the above:
- `pin_mmap_experts` is live and is what gives +41% prefill;
- the device cache is live and is the only measured lever past 30 tok/s decode;
- EXP-087/EXP-101 are live C++ in our tree and are what removed 3218 MiB of VRAM.

Nothing in this section changes a status; it changes what the remaining work means.

## Sweep for further stale clauses: one more found, and a caution about the search

Scanned all open requirements for FrankenMoE mechanism markers (`ring`, `pinned ring`, `tier`, `bank`, `EHS`, `staging`,
`claim`). 15 open items matched. Reading them rather than trusting the match:

- **P3-17 "Port GPU/pinned/VMM pools and transfer/residency policy"** - genuinely partly stale: it names the "pinned ring",
  which does not exist here. The VMM/residency half is live and partly ours already (FG195V VMM arithmetic, EXP-101 lazy KV
  mapping). Same treatment as P1-09/P1-10: restate or reintroduce.
- **P5-01, P5-04, P1-11, P3-05, P3-07, P3-08, P3-19, P4-07, P4-08, P1-08, P10-10, C1-03** - the marker came from wording
  such as "during", "staging buffer" in a generic sense, or "bank"/"claim" in a context unrelated to the old mechanism.
  No stale clause established for these by inspection; they are ordinary open work.

Caution recorded for the next pass: a keyword match is a hint, not a finding. Of 15 hits only one survived reading, and
reporting the other 14 as stale would have been exactly the kind of inflation this audit is supposed to prevent.

## Pattern across P1-09, P1-10, P3-17: planning is Goose, lifecycle is native

Three items were inventoried individually and produced the same shape, so it is worth stating once instead of three times:

| | Goose-owned and witnessed | still native | stale clause |
|---|---|---|---|
| P1-09 | FG143 control/cpu/executor, FG147 backend, FG149 pool | physical allocation, OS placement, non-OpenMP path | `fg140/141_claim` has no call site in this substrate |
| P1-10 | device-cache bank/slot planning (EXP-150), graph scratch policy (EXP-145) | cache constructor, tensor allocation, full upload-copy, resource lifecycle | pinned-ring staging does not exist here |
| P3-17 | `fg195v_plan_alloc`, `fg196p_plan_host_alloc` (both witnessed) | pool growth/reuse beyond planning, OOM, reset, async lifetimes | "pinned ring" does not exist here |

The common conclusion: **the decision layer is largely ported, the lifetime layer is not**, and each item also carries one
clause that refers to a mechanism removed with the old tree.

That has two practical consequences:

1. The remainder of all three is **one class of work**, not three: move lifetime decisions (create/destroy/reuse/fail/reset)
   into Goose with the failure-injection checks the criteria name. Doing it once with a shared fixture serves all three -
   which is exactly the "reusable component needed by several targets" priority.
2. The stale clause in each needs an owner decision (restate for this substrate, or reintroduce the mechanism). Until then
   those clauses cannot be completed no matter how much porting is done, and counting them as work in progress would be
   misleading.

No status changes here; this is a reading of three inventories together.
