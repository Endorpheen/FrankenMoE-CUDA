# EXP270: server-queue scheduling decisions in Goose

P8-03's two decision functions in `tools/server/server-queue.cpp`, ported and gated.

## The decisions

| decision | native rule |
|---|---|
| `pop_deferred_task(id_slot)` | take the first deferred task whose `id_slot` matches; if none matches, take the first deferred task. Empty queue: nothing is popped |
| `cleanup_pending_task(id_target)` | a cancel for a task removes every entry whose `task.id` equals the target, from `queue_tasks`, `queue_tasks_deferred` and `queue_tasks_unhandled` |

Both return an index or a mask, so the queues, their locks and their ownership stay in the server.

## Gate

`tests/queue_fixture.c` compares the module with direct transcriptions and asserts both the count and the mask:

    pick empty                    OK
    pick exact slot               OK
    pick earliest match           OK
    pick no match -> first        OK
    cleanup match 7 / mask        OK
    cleanup no match / mask none  OK
    checks: 8, failures: 0

The gate was shown to fail: changing the no-match fallback from the first entry to the last breaks one case (8/1).

## Still to do

Integration into `server-queue.cpp` (two call sites behind a default-on switch), compile, archive member replacement,
relink of `libllama-server-impl.so`, and an A/B that actually defers and cancels: the accepted single-request profile
rarely defers, so the witness may stay zero there -- unlike the checkpoint policy, this needs either concurrent clients or
a client disconnect mid-request, both of which the server already supports.
