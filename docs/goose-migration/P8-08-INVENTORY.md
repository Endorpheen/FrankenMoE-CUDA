# P8-08 inventory: metrics, watchdog and diagnostics

Read from the guards rather than assumed, and cross-checked against the accepted argv.

## What the accepted profile serves

| surface | served | why |
|---|---|---|
| `GET /health` | yes | used by every run in this project to detect readiness |
| `GET /props` | yes | reports `n_ctx = 262144` and the chat template; pure serialisation of server state |
| `GET /metrics` | **501** "start it with `--metrics`" | the accepted argv does not set the flag (`server-context.cpp:5598`) |
| `GET /slots` | **501** "start it with `--slots`" | same, `server-context.cpp:5660` |
| watchdog | **no such subsystem** | grep over `tools/server` and `common` for `watchdog`/`watch_dog` finds nothing |

## What the metrics endpoint would do if enabled

`server_task.cpp:1557-1651` renders Prometheus text from a `server_metrics` structure: a list of `metric_item`s with a
type, a name, a description and value buckets, plus the speculative-decoding per-position counters. The decisions there
are which items exist and their bucket boundaries; the rendering is string assembly.

## What this means for the item

- Every observable diagnostic on this profile is serialisation of state that other Goosed layers already own (context
  parameters, slot state, tokenizer template).
- The two richer surfaces are flag-gated and off, so they cannot be exercised without changing the accepted argv.
- There is no watchdog to own: request cancellation exists and is measured (EXP267, EXP379), but it is triggered by client
  disconnect or shutdown, not by an inactivity monitor.

## What would close it

1. A decision on whether to enable `--metrics` and `--slots` in an agreed configuration; with them on, the metric item list
   and the slot snapshot become checkable contracts.
2. Otherwise, record the two endpoints as flag-gated serialisation and the watchdog clause as absent -- the same
   treatment the pinned-ring, defrag and backpressure clauses received.
