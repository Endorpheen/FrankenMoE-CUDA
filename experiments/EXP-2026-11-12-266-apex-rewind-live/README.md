# EXP266: live rewind on the accepted runtime -- completion, erase, completion

P5-07 asks for exact saved state **and final outputs** on reuse, rewind, early branch and near capacity. Save/restore
already had a witness; this covers rewind with the output comparison the requirement demands.

## The path

`POST /slots/0?action=erase` (requires `--slot-save-path`) posts a `SERVER_TASK_TYPE_SLOT_ERASE` task, which calls
`server_slot::prompt_clear()`, and that calls `mem.seq_rm(id, -1, -1)` on the slot's sequence. So the request exercises
sequence removal through the memory interface -- the same decision family as P5-07's rewind clause.

## Result

    first_sha  = e68df05b4829865dcf8cd2424febfaded15b055f56fec36fefc002650ef8e7c9
    second_sha = e68df05b4829865dcf8cd2424febfaded15b055f56fec36fefc002650ef8e7c9
    tokens_equal = true, probs_equal = true, content_equal = true
    erase_response = { "id_slot": 0, "n_erased": 32895 }

32 895 tokens removed (32 768 prompt + 127 generated), and the repeated completion is identical in content, token ids
and all saved probabilities. Rewind therefore leaves no residue that changes the answer, which is exactly what the
criterion asks for and what a KV-only comparison could not establish.

The run was the accepted runtime (`runtime/reconstructed/build/stage-run`): its witnesses are in the log
(`FG398S_BATCH_CONTAINER_LAYOUT` 527, `FG401S_DETOK_DECISIONS` 389, `FG405S_SCHED_DECISIONS` 29, `FG147_BACKEND` 21,
`FG150_REGISTRY` 10, `FG149_POOL` 7, `FG375S`/`FG376S` hybrid memory and context, `FG384S`/`FG385S`/`FG387S` sampler).

## What this does and does not close

Closes: the rewind **behaviour** requirement, with the output comparison the criterion names.

Does not close: `seq_rm` **inside Goose** on this path. The accepted runtime integrates the KV state I/O routes
(FG392S/FG393S/FG394S/FG395S, FG345W/FG367S/FG373S/FG381S); it does not integrate the sequence-removal modules
(EXP-291/295/341), which remain gated only. Erase exercised the native `seq_rm`, so the decision is verified in
behaviour but not owned. That distinction is recorded rather than folded into a "P5-07 rewind done" claim.

Still open for P5-07: an early-branch run (the server exposes no sequence-copy action) and a near-capacity run (shares
P5-09's defrag work).
