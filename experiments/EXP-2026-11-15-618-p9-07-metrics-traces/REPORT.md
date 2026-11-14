# REPORT — P9-07 metrics/traces and deterministic workload infrastructure

Experiment: `EXP-2026-11-15-617`
Base commit: `df3ce73a` (carrier tree at `77ef4c72`; `runtime/` identical between them)
Work directory: `/tmp/opencode/sub-624/EXP-2026-11-15-617-p9-07-metrics-traces`
Verdict: **ACCEPTED** (differential gate and mutations, no performance claim)

## Outcome

The metrics/trace text formats and the deterministic-workload validation decisions are now
a single Goose module of 430 lines with a `mt_` prefix and 21 exports, split into three
slices:

1. **CSV/progress field formatting** — quote predicate (`,` `"` `\n`; `\r` raw), RFC4180
   quote doubling, newline-to-space, `nan` versus `%.6g` weight, 29 metrics columns, 62
   summary keys.
2. **Trace record encode/decode** — compute/io/route rows of 9/12/10 columns, integer
   field order and sign, reader column count, field unescape.
3. **Deterministic workload validation** — header, unsigned `n_ids` product, six-field
   route shape, cursor bound, replay-only prompt check, sequential capture index,
   routes-before-tokens finish, three-way static match.

The oracle is the carrier itself: the gate links `decode_trace_sink.cpp`,
`route_trace_sink.cpp`, `csv_metrics_sink.cpp` and `deterministic_workload.cpp` and drives
the real sinks and the real `DeterministicWorkload`.

```
FG617_S1 cases=25 values=42  failures=0
FG617_S2 cases=32 values=106 failures=0
FG617_S3 cases=49 values=55  failures=0
FG617 total=PASS
```

106 cases, 203 values, 0 failures. The same fixture passes with the module built at `-O0`.

## Mutations

12 mutations, two halves of 6, each rebuilding the module at `-O0` and rerunning the same
fixture:

```
HALF 1 total=6 caught=6 survived=0 broken=0
HALF 2 total=6 caught=6 survived=0 broken=0
```

Half 1 (field encoder): comma dropped from the predicate; `\r` added (the stdlib rule); newline
raw; quote not doubled; opening quote skipped; NaN weight on the numeric arm.

Half 2 (rows and workload): io read size replaced by request size; route separator before
`dropped` removed; reader counting commas inside quotes; unescaper not collapsing `""`;
shape comparison ignoring the phase; `finish()` checking tokens before routes.

Nothing survived, so each of the twelve decisions is pinned by an observable difference.

## Interfaces used

`bmoe/decode_trace.h` — `IComputeTraceSink`, `IIoTraceSink`, `ComputeTraceRow`,
`IoTraceRow`, `DecodeTraceStatic`, `make_csv_compute_trace_sink`,
`make_csv_io_trace_sink`. `bmoe/route_trace.h` — `IRouteTraceSink`, `RouteTraceRow`,
`RouteTraceStatic`, `RouteResidency`, `make_csv_route_trace_sink`. `bmoe/metrics.h` —
`IMetricsSink`, `RunInfo`, `RunSummary`, `make_csv_metrics_sink`.
`runtime/core/src/moe/deterministic_workload.h` — `DeterministicWorkload` with
`capture/load/check_static/prompt/begin_batch/capture_topk/replay_topk/token/finish/save`
and `route_count/token_count/error`. C ABI: `sl_u8 {uint8_t*, int64_t}` and `goose_init`.

## Integration step

Copy the module to `runtime/core/src/metrics/metrics-traces.goose`, generate at `-O1` into
the `bmoe_core` target, then replace the row emission in the three sinks with the matching
`mt_*_row_encode` call in native print order, `write_csv_field` with `mt_field_encode`, and
the five `DeterministicWorkload` checks with `mt_wl_*` in the order
header → nids → cursor → shape → prompt → token index → finish. Parameter lists and the
order of checks are given in `README.md`; the carrier keeps the first failure's message, so
moving a check changes `error()`.

## Compatibility

`goose check` clean (90 specializations); gate `-O1` and module `-O0` both 0 failures;
12/12 mutations caught; 21 unique `mt_` exports with no duplicates and no carrier clash;
`git status --porcelain -- runtime/` empty, so the production runtime is untouched.

## Assumptions and conflicts

`%.6g` stays libc through the `mt_fmt_g6` shim (one definition, `snprintf`), so the route
weight column is byte-exact only against the same libc. `mt_field_encode` is truthful about
its length and never truncates; the caller sizes the buffer (4096 bytes covers every node
name observed). Exports are leaves because the toolchain resolves one concrete signature per
export; new Goose code must call the non-exported helpers. `mt_metrics_cols` and
`mt_summary_keys` are constants that must move if the metrics CSV is ported later. The
`mt_` prefix is reserved by this experiment.

## Artifacts

```
13850b53f856344b115ff333b937f5e1564c74f0f98b9268900c82cb06382eda  source/metrics-traces.goose
6058436b1247a7d59632143e9d858fd9b561c4bd01ef91b6cd56446134ce5541  tests/metrics_traces_fixture.cpp
f6a286e0daf80a4a41c9b541d333915da5a882499d7c53d3d4e61c7bae142225  tests/mutate.py
101ae9dd454ca6f595bd66291c3d712e682ffe5141f59684e1b89f8079169418  tests/build.sh
ee2ecc7525c0f135336df33a34eba7d29f64a87317d71147b6e070c9cf1500f3  build/metrics_traces_gate
```

Recorded runs: `results/gate-o1.txt`, `results/mutations-half1.txt`,
`results/mutations-half2.txt`. Generated `build/g.c`, `build/g.h` and objects are not for
commit.
