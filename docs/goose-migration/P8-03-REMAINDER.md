# P8-03 remainder: what "timeout and backpressure" means here

Status: three increments of the item are live (queue, slot selection, batch compatibility). This note records what is
left of the criterion and why two of its words do not describe mechanisms in this tree.

## Backpressure: absent

- The deferred queue is unbounded: `server_queue::defer()` is a single `queue_tasks_deferred.push_back(...)`
  (`tools/server/server-queue.cpp:79`) with no size check, and there is no `max_tasks`, `queue_size` or equivalent
  parameter anywhere in `tools/server` or `common`.
- There is no admission cap and no 429 path: grep over the server sources finds no `429`, `too_many`, `queue full` or
  capacity rejection.
- So the server absorbs as many requests as memory allows. "Backpressure owned by Goose" has nothing to own: adding one
  would be a new mechanism and an owner decision, like the pinned-ring and defrag clauses.

## Timeouts: two different things, neither a policy layer

| timeout | where | character |
|---|---|---|
| HTTP read/write timeout (`--timeout`, default 3600 s) | cpp-httplib parameters | library primitive: the server passes the number, the transport enforces it |
| SSE keepalive ping | `server-context.cpp:5383-5400` | a small live decision: if `sse_ping_interval` seconds pass without data, emit `":\n\n"` and continue; if the client is gone, stop |
| `--stop-timeout` | shutdown path | shutdown sequencing, not per-request scheduling |

The only portable piece is the SSE keepalive test -- one comparison and a branch. It is live for streaming requests but
so small that a module plus gate would be ceremony; recorded as a candidate rather than silently claimed.

## Generation budget: present, tiny, live

`server_slot::has_budget()` / `n_remaining()` (`server-context.cpp:535-541`) decide whether a slot may keep generating
under `n_predict`, and the speculative path bounds the draft with `n_draft_max = min(n_draft_max, n_remaining()-1)`
(line 577). Speculation is off in the accepted profile (`--spec-type none`), so the second rule is dormant here; the
first is two comparisons.

## What would close the remainder

1. A recorded decision that "backpressure" is out of scope for this substrate (it has no such mechanism), or a design
   change to introduce one.
2. Either port the SSE keepalive and the budget predicates for completeness, or record them as too small to be a
   decision layer; the item's criterion lists them, so silence is not an option.

Nothing here reclassifies the three live increments: those are ported, gated and witnessed.
