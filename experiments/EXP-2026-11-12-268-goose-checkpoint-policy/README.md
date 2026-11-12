# EXP268: context-checkpoint eviction policy in Goose

`--ctx-checkpoints 32` is in the accepted argv, so this policy runs on every long prompt. The three decisions live in
`tools/server/server-context.cpp`, `create_checkpoint()`, and are pure functions of the checkpoint list of one slot.

## The decisions

| decision | native rule |
|---|---|
| near-eviction | only while the list is full (`size + 1 >= n_ctx_checkpoints`): walk from the oldest and erase an entry when a previous surviving entry exists and the entry is within `checkpoint_min_step` tokens of it, unless the entry belongs to the task creating the new checkpoint |
| make-room | while the list holds at least `n_ctx_checkpoints` entries, drop the oldest |
| supersede | drop entries whose `n_tokens` equals the token count of the checkpoint about to be stored |

The near-eviction guard uses the *live* list size, which shrinks with every erase -- that is why the module reproduces the
loop rather than computing a single threshold.

## Module and gate

`source/checkpoint_policy.goose` takes parallel `n_tokens`/`id_task` arrays and returns a mask of entries to erase, so the
caller keeps ownership of the container. The emitter derives the three wrappers from the emission.

`tests/checkpoint_fixture.c` compares each entry with a direct transcription of the native loop and asserts the mask as
well as the count:

    near: not full                 OK
    near: full, tight spacing      OK
    near: exact step bound         OK
    near: current task protected   OK
    near: gaps keep entries        OK
    make_room 33/32 32/32 31/32 0/32  OK
    supersede duplicates / none    OK
    checks: 11, failures: 0

The gate was shown to fail: changing the bound from `<=` to `<` in the near-eviction step test breaks three cases
(11/3). Restored, 11/0.

## Still to do

The integration into `create_checkpoint()` (three call sites behind a default-on switch), a relink of the
server implementation, and an A/B on the accepted profile -- which creates many checkpoints for a 32K prompt, so the
witness would be non-zero without changing the profile.
