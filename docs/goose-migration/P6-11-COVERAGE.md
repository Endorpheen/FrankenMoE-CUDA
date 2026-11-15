# P6-11 — MTP, speculative decoding and draft state

Status: **PARTIAL** — 3 of 3 slices accepted; the engine wiring is open.

## Slices

| slice | content | exports |
|---|---|---|
| S1 draft source and accept | most-recent equal-length ngram match, run's own last-token prediction, accept loop and emit bound | `sdp_ngram_draft`, `sdp_accept`, `sdp_emit_count` |
| S2 MTP draft state | width, chained-head clamp, p_min boundary, n_max bound, rewind and catch-up seed | `sdp_mtp_width`, `sdp_mtp_head_clamp`, `sdp_mtp_rewind`, `sdp_mtp_catchup` |
| S3 budget and trim | step plan, rollback keep, step advance, tail trim, confirmed-token room | `sdp_step_plan`, `sdp_rollback`, `sdp_step_advance`, `sdp_tail_trim` |

## Evidence

- Experiment: `experiments/EXP-2026-11-15-619-p6-11-spec-decode`.
- Canon: `goose/misc/spec-decode-decisions.goose` (`sdp_` prefix, 11 exports).
- Gate: 5 sections, 9828 comparisons, 0 failures, byte-identical across two
  runs; the end-to-end loop matches plain greedy and a no-draft step costs
  exactly one plain decode.
- Mutations: 12/12 in two halves.

## Remaining

Wire `ngram_draft.cpp` and `session.cpp` to the Goose decisions:
`sdp_step_plan` -> `sdp_accept` -> `sdp_mtp_rewind`/`sdp_mtp_catchup` ->
`sdp_rollback` -> `sdp_step_advance` -> `sdp_tail_trim`.

## Boundaries

Batched verify is not bit-identical; MTP head probabilities, the draft KV,
tokenizer and any speed metric stay native. `llama_get_logits_ith`/argmax, the
EOG test, `llama_memory_seq_rm` and the speculative helpers stay native.
