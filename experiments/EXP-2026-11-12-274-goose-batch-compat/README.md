# EXP274: batch-compatibility predicate in Goose

The rule that decides whether two processing slots may share one batch: `server_slot::can_batch_with()` plus
`are_lora_equal()` from `tools/server/server-common.cpp`.

## The rule

- a joint decision head reads the whole batch, so a task carrying a decision order on either side disables batching;
- otherwise the task types must match, the embedding-input sizes must match, and the LoRA lists must match in length
  and, at every index, in adapter pointer and scale.

Two details are reproduced deliberately: the path is *not* compared in the native code, and the scale comparison is a
float one -- `-0.0` equals `+0.0`, while a NaN compares unequal to itself. The scales therefore travel as f32 bits and
are compared as floats.

## Gate

    identical / type differs / embd differs                       OK
    decision on one side / on the other                           OK
    lora ptr differs / scale differs / length differs             OK
    signed zero equal / NaN not equal                             OK
    checks: 11, failures: 0

The gate was shown to fail: changing the decision-head test from `||` to `&&` breaks the two one-sided cases (11/2).

## Still to do

The call site in the batch-assembly loop of `update_slots()`, archive replacement, relink, and an A/B under the
concurrency harness -- with two requests that differ in type or LoRA the predicate returns false and the second request
must be deferred exactly as before.
