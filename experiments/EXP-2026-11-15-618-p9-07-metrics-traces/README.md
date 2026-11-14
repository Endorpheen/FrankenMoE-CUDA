# EXP-2026-11-15-617 — metrics/traces formats and deterministic workload validation (P9-07)

Native sources (carrier, unchanged):

| file | lines | what is ported |
|---|---|---|
| `runtime/core/src/metrics/decode_trace_sink.cpp` | 19-30 | `write_csv_field`: the shared CSV field encoder |
| `runtime/core/src/metrics/decode_trace_sink.cpp` | 45-55, 74-83 | compute and io row encoders |
| `runtime/core/src/metrics/route_trace_sink.cpp` | 33-48 | route row encoder, including the NaN arm |
| `runtime/core/src/metrics/csv_metrics_sink.cpp` | 128-131, 73-110 | per-token column list, summary key list |
| `runtime/core/src/moe/deterministic_workload.cpp` | 13-79, 131-172 | workload load, replay, token and finish validation |

Base commit: `df3ce73a`. The tree has since moved to `77ef4c72`, but
`git diff df3ce73a 77ef4c72 -- runtime/` is empty, so the ported sources and their line
numbers are the ones named here. The experiment was performed outside the repository, in
`/tmp/opencode/sub-624/`, so the production runtime was never touched.

## Hypothesis

The metrics/trace text formats and the deterministic-workload validation decisions are
self-contained arithmetic and byte handling: they can be expressed as a Goose module with
no model, no graph and no file I/O policy, and they can be gated against the carrier's own
sink classes and `DeterministicWorkload` rather than against a re-typing of them. If a
mutation to any one decision makes the gate fail, the format contract is pinned rather
than merely documented.

## Slices

Three slices, one module, prefix `mt_` (21 exports):

| slice | exports | native decision |
|---|---|---|
| S1 CSV/progress fields | `mt_field_encode`, `mt_field_needs_quote`, `mt_weight_kind`, `mt_metrics_cols`, `mt_summary_keys` | quote iff `,` `"` `\n` (`\r` not quoted); `""` doubling; `\n`→space; NaN weight is the literal `nan`; 29 metrics columns, 62 summary keys |
| S2 trace records | `mt_compute_row_encode`, `mt_io_row_encode`, `mt_route_row_encode`, `mt_trace_cols`, `mt_count_cols`, `mt_unescape_field` | 9/12/10 column rows; integer field order and signs; RFC4180 decode of a raw field |
| S3 workload validation | `mt_wl_header_ok`, `mt_wl_next_route_ok`, `mt_wl_route_nids_ok`, `mt_wl_route_words`, `mt_wl_route_shape_ok`, `mt_wl_route_take_ok`, `mt_wl_prompt_ok`, `mt_wl_token_index_ok`, `mt_wl_finish_code`, `mt_wl_static_ok` | magic+version 1; unsigned `n_ids == top_k*token_n`; six-field shape; cursor bound; replay-only prompt check; sequential capture index; routes-before-tokens finish; three-way static match |

## Owned decisions

- The field predicate is exactly `,` `"` `\n`. A carriage return travels raw. The Goose
  stdlib CSV writer quotes `\r`; the carrier's writer does not, and the difference is
  observable in a CRLF node name, so the port follows the carrier.
- The newline-to-space substitution happens inside the quoted arm only, after the quote
  decision, and before quote doubling for that byte. A field of only `\n` becomes `" "`.
- `nan` is printed only for a NaN weight; `0.5` prints `0.5` and never `0`, so "unknown"
  cannot be read as "zero mass".
- `n_ids == (size_t) top_k * (size_t) token_n` keeps unsigned width: `top_k` is cast to
  `u64` exactly as the carrier casts to `size_t`, so a negative top_k cannot match a small
  count. `2^32 * 3` is pinned, not just small products.
- `finish()` reports the route shortfall before the token shortfall; a run short on both is
  a route failure in the carrier and is a route failure here.
- A capture token index must equal the current length (the file stays a sequential list); a
  replay index must be strictly below the captured count. The two arms are one function
  because the carrier has one `token()`, and the mode selects the arm.
- Numbers, not texts: the `error_` strings, the sink classes, `FILE*` ownership, the
  preamble lines and the `fflush` policy stay in the carrier.

## Gate

`tests/build.sh` generates the module at `-O1`, compiles it, then links
`tests/metrics_traces_fixture.cpp` with the carrier's four real source files. The oracle is
the carrier: the fixture drives `make_csv_compute_trace_sink`, `make_csv_io_trace_sink`,
`make_csv_route_trace_sink`, `make_csv_metrics_sink` and `DeterministicWorkload`, and
compares their bytes and decisions with the port.

```
FG617_S1 cases=25 values=42  failures=0
FG617_S2 cases=32 values=106 failures=0
FG617_S3 cases=49 values=55  failures=0
FG617 total=PASS
```

106 cases, 203 values. S2 compares every row byte for byte against the real sink output and
then parses it back: column count from the port's reader and the port's unescaper must
return the original `op`/`name` string (with `\n` as the space the encoder wrote). S3 drives
the real class and a port driver through the same ten scenarios and compares the step that
rejects each one, then pins 37 boundary values by hand from the native source.

The same fixture passes against a module built at `-O0` (`build/o0`), which is the
mutation build.

## Mutations

`tests/mutate.py 1|2` holds 6 + 6 mutations; each rewrites one decision, rebuilds the module
at `-O0` and reruns the same fixture. Builds are serial on purpose.

```
python3 tests/mutate.py 1   # 6/6 caught
python3 tests/mutate.py 2   # 6/6 caught
```

Half 1, the field encoder: comma dropped from the predicate; `\r` added to it (the stdlib
rule); a newline written raw; a quote not doubled; the opening quote skipped; a NaN weight
taking the numeric arm.

Half 2, rows and workload: the io row writing the requested size as the read size; the route
row losing the separator before `dropped`; the reader counting commas inside quotes; the
unescaper never collapsing `""`; the shape comparison ignoring the phase; `finish()`
checking tokens before routes.

## Interfaces the port relies on (canon)

Everything below exists in `df3ce73a` and is unchanged at `77ef4c72`:

- `bmoe::IComputeTraceSink`, `bmoe::IIoTraceSink`, `bmoe::IRouteTraceSink`, and the
  factories `make_csv_compute_trace_sink`, `make_csv_io_trace_sink`,
  `make_csv_route_trace_sink`, `make_csv_metrics_sink` (`bmoe/decode_trace.h`,
  `bmoe/route_trace.h`, `bmoe/metrics.h`).
- Structs `bmoe::ComputeTraceRow`, `bmoe::IoTraceRow`, `bmoe::RouteTraceRow`,
  `bmoe::DecodeTraceStatic`, `bmoe::RouteTraceStatic`, `bmoe::RunInfo`, `bmoe::RunSummary`
  and `bmoe::IMetricsSink`. Field names and integer widths are the contract; the port's
  parameter lists are those fields in print order.
- `bmoe::RouteResidency` values (`route_miss=0`, `route_hit=1`, `route_hit_spec=2`) — the
  port only passes the number through.
- `bmoe::DeterministicWorkload` (`runtime/core/src/moe/deterministic_workload.h`):
  `capture()`, `load(path, error)`, `check_static(arch, n_layer, n_expert_used)`,
  `prompt(tokens)`, `begin_batch(base_pos, n_tokens, phase)`,
  `capture_topk(layer, n_expert_used, n_tokens, ids)`,
  `replay_topk(layer, n_expert_used, n_tokens, ids&)`, `token(index, natural, selected&)`,
  `finish(emitted_tokens)`, `save(path)`, `route_count()`, `token_count()`, `error()`.
- C ABI of a generated module: `sl_u8 { uint8_t *data; int64_t len; }` and `goose_init`,
  from the Goose 2026.3 toolchain used by every other P9 port.

## Integration step (one commit, not performed here)

1. Copy `source/metrics-traces.goose` to
   `runtime/core/src/metrics/metrics-traces.goose` and add a CMake custom command that runs
   `goose --standalone -o metrics-traces-gen.c --header metrics-traces-gen.h` plus a
   `gcc -O1 -std=c11` object, linked into the `bmoe_core` target the same way the other P9
   modules are wired. No absolute paths.
2. `runtime/core/src/metrics/decode_trace_sink.cpp`, `write_csv_field(std::FILE*,
   const std::string&)` (19-30): keep the signature and replace the body with a 4096-byte
   stack buffer, `mt_field_encode(sl_u8{data,size}, sl_u8{buf,sizeof buf})` and
   `fwrite(buf, 1, n, f)`. Call sites at 49 and 51 do not change.
3. `CsvComputeTraceSink::on_rows` (45-55): replace the per-row `fprintf` with
   `mt_compute_row_encode(turn, phase, step, seq, layer, op, name, wall_ns, majflt, out)`,
   then `fwrite`. Argument order is the native print order; `op`/`name` are passed as
   `sl_u8` views of the existing `std::string`.
4. `CsvIoTraceSink::on_rows` (74-83): `mt_io_row_encode(turn, phase, step, layer, expert,
   proj, lane, spec, offset, req_bytes, read_bytes, latency_ns, out)`. `expert` is
   sign-extended from `int32_t`, `proj`/`lane` from `int8_t`, `spec` from `uint8_t`.
5. `CsvRouteTraceSink::on_rows` (33-48): `mt_route_row_encode(turn, phase, step, layer,
   slot, expert, (double) weight, residency, expert_bytes, dropped, out)`. Provide the
   `extern "C" int64_t mt_fmt_g6(sl_u8, int64_t, double)` shim next to the sink (one
   definition, `snprintf("%.6g")`).
6. `DeterministicWorkload`: in `load()` use `mt_wl_header_ok` before the metadata keys and
   `mt_wl_route_nids_ok` for the per-record count check (13-79); in `replay_topk()` call
   `mt_wl_next_route_ok` first, then `mt_wl_route_shape_ok` (131-146); in `token()` use
   `mt_wl_token_index_ok` (148-159); in `finish()` use `mt_wl_finish_code` (161-172); in
   `check_static()` use `mt_wl_static_ok` (92-99). Keep `fail()` and the message text; the
   port only decides.

The order in steps 3-6 is the contract: the carrier keeps the first failure's message, so a
decision moved ahead of another changes `error()` for a run that fails twice.

## Compatibility checks

- `goose check source/metrics-traces.goose`: parsed and typechecked, 90 specializations,
  zero errors (`results/gate-o1.txt`).
- Gate at module `-O1`: `FG617_S1/S2/S3` 0 failures, 106 cases / 203 values.
- Gate at module `-O0`: same result, so no decision depends on the optimiser.
- Mutations: 6/6 + 6/6 caught, no setup, build or fixture failures.
- No duplicate exports: the generated header declares 21 distinct `mt_` symbols, no
  duplicates, and none collides with a carrier symbol (`bmoe::` namespace only).
- Production untouched: `git status --porcelain -- runtime/` is empty in the carrier.

## Assumptions and possible conflicts

- The `%.6g` of a float weight is the only conversion left to libc. On glibc it matches the
  carrier byte for byte (the gate proves it on this machine); a different libc must be the
  same one the sink was compiled with, or the route weight column can differ in the last
  digit.
- `mt_field_encode` returns the truthful length even when the buffer is short, because the
  carrier's writer cannot truncate. An integrator must size the buffer for the longest
  possible `op`/`name`; 4096 bytes covers every ggml node name observed, and truncation is
  a caller error, not a port behaviour.
- Exported functions may not be called from Goose code: the toolchain resolves only one
  concrete signature per export. All shared logic therefore lives in non-exported helpers
  and each export is a leaf. `mt_field_needs_quote` is a leaf wrapper over
  `mt_quote_needed` for this reason — do not call the export from new Goose code.
- `mt_wl_route_words` multiplies in `i64`; it exists to check a saved line's width and is
  not a bound check for absurd counts.
- `mt_metrics_cols` / `mt_summary_keys` are constants today. If the metrics CSV itself is
  ported later, those two constants must move with it or they will drift silently.
- The prefix `mt_` is reserved by this experiment. A second P9 module must not export a
  symbol starting with `mt_`.
- No model, sampler, graph, tokenizer or KV is involved; there is no tok/s number for this
  slice by design. The gate checks formats and decisions, never a generation.

## Artifacts

| path | SHA-256 |
|---|---|
| `source/metrics-traces.goose` (430 lines) | `13850b53f856344b115ff333b937f5e1564c74f0f98b9268900c82cb06382eda` |
| `tests/metrics_traces_fixture.cpp` (688 lines) | `6058436b1247a7d59632143e9d858fd9b561c4bd01ef91b6cd56446134ce5541` |
| `tests/mutate.py` (126 lines) | `f6a286e0daf80a4a41c9b541d333915da5a882499d7c53d3d4e61c7bae142225` |
| `tests/build.sh` | `101ae9dd454ca6f595bd66291c3d712e682ffe5141f59684e1b89f8079169418` |
| `build/metrics_traces_gate` (binary) | `ee2ecc7525c0f135336df33a34eba7d29f64a87317d71147b6e070c9cf1500f3` |
| `build/g.c` (generated, not committed) | `3085325cb7f88453bd3f9c2ce10f8ce3e101157e5f735ba4d94879fa8ba7246c` |
| `build/g.h` (generated, not committed) | `0acd73e68f45ac1ce5edfe01107fef19a3b0da11252dafac59878895321edbbc` |

Recorded results: `results/gate-o1.txt`, `results/mutations-half1.txt`,
`results/mutations-half2.txt`. Generated C, objects and the gate binary stay out of the
commit; they are reproducible from the Goose file plus the mutation table.
