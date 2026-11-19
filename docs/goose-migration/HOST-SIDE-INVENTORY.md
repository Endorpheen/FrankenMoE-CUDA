# Which remaining items have a host side in this tree

A requirement can be blocked by something other than work: the call site it names may not exist here. This inventory
checks that, one marker per item, and it is the answer to "why is this still open" that does not involve another run.

| item | what it must wire into | present in this tree | consequence |
|---|---|---|---|
| P8-06 | `model_router.cpp`, `request_stop`, reload sets | **no** - `model_router` appears nowhere under `vendor/flint`, `integration`, `runtime` | the wiring has no destination here |
| P8-09 | `web_serve`, API-key middleware in `server-http.cpp` | **no** - `web_serve` appears nowhere | same |
| P9-07 | CSV, decode-trace, route-trace sinks | **partial** - `decode_trace`/`route_trace` exist only in `integration/m1/gates/s14/*` (a smoke gate), not in the runtime | the runtime has no sink to connect |
| P9-08 | `session.cpp`, `cli/main.cpp` | **no** - the names match only unrelated files (`ggml-openvino`, `bmoe/config.h`) | same |
| P9-03, P9-04, P9-05 | expert-cache and row-source call sites | partially: the cache is `llama-moe-cache.cpp`, which exists | wiring is possible and is real work |
| P10-09 | `scripts/bench-analyze.py`, `scripts/trace_io.py` | **no** - neither file exists; `bench-analyze.py` matches only a string inside `csv_metrics_sink.cpp` | the module is attached when they do |
| P10-06 | CUDA arch/flags/install decisions | partially: the build scripts exist | wiring is possible |

## What this changes

Three items (P8-06, P8-09, P9-08) and two prerequisites (P10-09's scripts) refer to host code that is **not in this
repository**. Their `remaining` text is accurate but their blocker is not measurement or effort - it is that the
destination does not exist here. Reporting them as "almost done" would be wrong, and so would reporting them as waiting on
this agent's work.

Two items (P9-07 and P10-06) have a real host side and are ordinary wiring work. P9-03/04/05 have the expert cache in
place, so they are the largest genuine migration blocks left in P9.

## How this was checked

`grep -rl <marker> vendor/flint integration runtime` for each marker, with the first two hits inspected by name. A marker
that matches an unrelated file is recorded as absent, not as present - the failure mode this document exists to avoid.
