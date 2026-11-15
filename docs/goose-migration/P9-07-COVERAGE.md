# P9-07 — metrics, traces and deterministic workload

Status: **PARTIAL** — 3 of 3 slices accepted; the sinks are still native.

## Slices

| slice | content | exports |
|---|---|---|
| S1 CSV/progress formatting | quote predicate (comma/quote/newline, raw carriage return), RFC4180 doubling, newline to space, nan vs `%.6g`, 29 metric columns, 62 summary keys | `mt_*` |
| S2 trace rows | compute/io/route row encode and decode (9/12/10 columns), reader column count, unescape | `mt_*_row_encode` |
| S3 deterministic workload | header/version, unsigned n_ids check, six-field route match, cursor bound, replay prompt, capture index, finish order, static triple | `mt_wl_*` |

## Evidence

- Experiment: `experiments/EXP-2026-11-15-618-p9-07-metrics-traces`.
- Canon: `goose/metrics/metrics-traces.goose` (21 exports).
- Gates: 25/42 + 32/106 + 49/55 values, 0 failures against the real sinks and
  the real `DeterministicWorkload`; also green at -O0.
- Mutations: 12/12 in two halves.
- Staged switch: `EXP-2026-11-15-628-s1-4-metrics-staged` wires the field/row
  encoders behind `FG_GOOSE_METRICS` with 53/53 parity on both paths.

## Remaining

Wire the CSV, decode-trace and route-trace sinks and `DeterministicWorkload` to
the Goose rows (the 29-column CSV and `mt_wl_*` are not switched in the staged
slice yet).

## Boundaries

`%.6g` stays libc through `mt_fmt_g6`, byte-exact only with the same libc. The
exports are leaves; the native sinks keep `FILE*`, the `#` preamble, headers and
flush policy until the wiring step.
