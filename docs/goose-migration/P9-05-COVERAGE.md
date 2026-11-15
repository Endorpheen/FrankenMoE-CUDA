# P9-05 — standalone sessions, runtime and ngram draft

Status: **PARTIAL** — 3 of 3 slices accepted; the runtime wiring is open.

## Slices

| slice | content | exports |
|---|---|---|
| S1 session lifecycle | VRAM hot-store sizing from GGUF shape, preflight clamp against free VRAM, post-context probe (keep/resize/disable), teardown order | 11 `ss_*` |
| S2 runtime facade | `session_config_from` projection, derived `n_batch = n_ctx`, effective `n_ubatch`, the five-call `run()` order with two early exits, four `GenerateRequest` fields, the `RunResult` invariant | 6 `rts_*` |
| S3 draft source | `load_mtp`, MTP head admission by `n_layer_nextn`, stream bound, `n_rs_seq`, draft and verify widths, four `RunInfo` fields | 11 `nd_*` |

## Evidence

- Experiment: `experiments/EXP-2026-11-15-632-p9-05-sessions-runtime`.
- Canon: `goose/misc/{session-lifecycle,runtime-facade,draft-source}.goose`.
- Gates: 17/119 + 11/135 + 36/78 checks, 0 failures, identical at -O0.
- Mutations: 36/36 (12 per slice, halves 6/6), each mutant passes `goose check`
  and fails its slice gate.

## Remaining

Extract `expert_bytes_per_slot(meta)` from `hot_slots_from_budget`, keep
`rts_config` as a drift pin (it is not a replacement for
`session_config_from`), and wire only `rts_n_batch` / `rts_ubatch`. The MTP head
admission must stay strictly before the hook/stream bound.

## Boundaries

The tensor-name regex stays an input. Teardown is a rank table, not destructor
semantics. `n_rs_seq` comes from the context when speculation is off. Three
modules in one binary need `objcopy --redefine-sym` for `goose_init` and an
explicit initialiser call.
