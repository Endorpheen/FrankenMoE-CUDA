# P9-07: metrics, traces and deterministic workloads - what is composed and who calls it

Checked 2026-11-19 on `main`, text level, no build.

## Metrics and traces

`goose/metrics/metrics-traces.goose` exports **21** doors (`mt_field_needs_quote`,
`mt_field_encode`, `mt_weight_kind`, `mt_trace_cols`, `mt_metrics_cols`, `mt_summary_keys`, ...).

**Four** have a call site, and it is the m1 harness, not the shipped runtime: `mt_field_encode`,
`mt_compute_row_encode`, `mt_io_row_encode` and `mt_route_row_encode` are called from
`integration/m1/common/fg-goose-metrics-bridge.cpp`, exercised by the gate
`integration/m1/gates/s14/fg_goose_metrics_smoke.cpp`. The other seventeen have none.

Correction of the first version of this note, which said "none has a call site": the search had
covered `vendor/flint/src/` and `runtime/` only. The conclusion about the shipped path is unchanged -
the bridge is a harness - but the count was wrong and is now stated with its scope.

The records are still printed natively - `runtime/cli/main.cpp:108-111,340` writes
`"compute_ms":%.1f,"mgmt_ms":%.1f,"stall_ms":%.1f,"read_mb":%.2f` and
`"read_mib":%.1f,"stall_s_tok":%.4f,...` as literal formats. So the CSV/JSON sink formatting that
P9-07 names is native on the served path.

## Deterministic workloads

`goose/misc/workload-format.goose` has **no exported door at all**: every function is private
(`wl_skip`, `wl_word`, `wl_num`, `wl_is`, `wl_count`, `wl_parse`, `wl_put`, ...). A module that
exports nothing cannot be reached from a host, whatever else is true about it.

The CLI does have the call sites the requirement talks about - `--workload-capture F` and
`--workload-replay F` at `runtime/cli/main.cpp:522-523,680` - and those paths are served by the
native `DeterministicWorkload` class, not by this module.

## What P9-07 needs

1. export the workload decisions (`wl_parse` at least, plus what capture and replay need) and wire
   them into the two CLI paths that already exist;
2. switch the metric formatting to `mt_field_encode` / `mt_metrics_cols` and compare the emitted
   bytes with the native output on the same numbers;
3. then a capture/replay check: capture a short run, replay it, and require identical token ids.

All three are verifiable without a GPU when the numbers are supplied directly, which is why they
are named as steps rather than as a measurement.

## What is not claimed

That any of this is wired, measured or removed. Twenty-one metric doors have no caller, the
workload module cannot be called at all, and the CLI keeps using the native class.
