# P10-09 S3 — benchmark diagnostics (CSV → numbers and rows)

File: `source/dg-diagnostics.goose` (692 lines, 7 exports, `dg_` prefix).
Base commit: `70de805c`; both native files are byte-identical at HEAD `9ac3eab0`.

## Hypothesis

A native-free build still has to read the CSV its own runs produce and print the
same diagnostic tables, otherwise a ported engine cannot be measured at all. The
analysis is a pure text-to-numbers function over two artifacts (the run CSV and
its `.metrics` sidecar), so it can be ported and gated against the real analyzer
without a model, a GPU or a build of the engine.

## Native sources

| native | what is owned here |
|---|---|
| `upstream/BigMoeOnEdge/scripts/bench-analyze.py` | `analyze()` (45-105): the header-driven column map, the row skip rules, the `# summary` trailer, the aggregate mean, the median, p5/p95, MiB/token, TTFT, the "-1.0 means unmeasured" cache-hit rule, the spec-useful percentage, and the `.metrics` conversions |
| `upstream/BigMoeOnEdge/scripts/trace_io.py` | `percentile()` (46-53), linear interpolation on a sorted list; `read_kv_file()` (34-43), the flat key=value sidecar |
| `upstream/BigMoeOnEdge/scripts/bench-analyze.py` (tables) | the throughput row and the cache-sizing row layouts and their dash fallbacks |

## Interfaces

| export | decision |
|---|---|
| `dg_analyze(csv, metrics, out) -> i64` | the whole analysis into 22 integer thousandths; 0 when no row parsed (the native `None`) |
| `dg_percentile(values, n, q_milli) -> f64` | `trace_io.percentile` with q in thousandths so 5 and 95 are exact |
| `dg_median(values, n) -> f64` | `statistics.median`: middle value, or the mean of the two middle values |
| `dg_fmt(out, at, milli, dec) -> i64` | a thousandth-scaled value with 0, 1 or 2 decimals, truncated |
| `dg_row(label, fields, hit_raw, out) -> i64` | the throughput markdown row |
| `dg_sized(fields) -> i64` | whether a run belongs in the cache-sizing table |
| `dg_sized_row(label, fields, out) -> i64` | one cache-sizing row |

`out[]` indices are exported as `DG_N`, `DG_MEAN`, … `DG_BATT_MAX` so a caller
never depends on the numeric order.

## Why integers

The toolchain's `print` does not implement float formatting, and printf's
rounding of a binary double is not reproducible with a scale-and-round in the
same way. The port therefore emits **thousandths** (rounded half-to-even, the
same rule as Python's `round` on the same double) and renders text from those
integers. The native side renders its rows from its own thousandths with the
same integer formatter, so a row comparison checks the layout and the fallbacks
without either side's float formatting involved. This is a recorded design
choice, not an approximation: the gate compares exact bytes.

## Gate

`tests/make_cases.py` writes 16 synthetic CSV + `.metrics` pairs;
`tests/native_rows.py` executes the **real** `analyze()` (the native source is
exec'd up to the table section) and prints normalised rows; `tests/gen_dg.py`
embeds the same texts in one Goose driver and compares at `-O1` and `-O0`:

* `A` — the 22 analysed fields per case,
* `T` — the throughput row, character for character,
* `H` — the cache-hit cell,
* `S` — the cache-sizing membership decision,
* `R` — the cache-sizing row.

```
cd tests && python3 make_cases.py
cd tests && python3 native_rows.py
cd tests && python3 gen_dg.py             # 66 rows compared
cd tests && python3 gen_dg.py --rebuild   # rebuild cases + native rows
```

Result: **66/66 rows identical, 16/16 cases, `-O1` == `-O0`.**

The 16 cases: a full row set with a `.metrics` sidecar; no stall column; an
absent cache hit; `-1.0` cache hit; a zero wall time; an unparsable wall in the
middle of the file; an unparsable optional field; one row; two rows (even
median); extra trailing columns; no summary trailer; prefill-only summary; a
speculative-prefetch run; an explicit cache budget without prefetch; CRLF line
endings; an empty file.

## Mutations

`tests/mutate_dg.py <lo> <hi>`.

| # | mutation | result |
|---|---|---|
| 0 | the wall time is read from the wrong column | caught (`full`) |
| 1 | a row with an unparsable wall is kept | caught (row count) |
| 2 | the aggregate mean divides by the positive walls | caught (`zero_wall`) |
| 3 | an even count takes the lower middle value | caught (`two_rows`) |
| 4 | p5 and p95 swap places | caught (`full`) |
| 5 | the per-token read divides by the positive walls | caught (`zero_wall`) |
| 6 | -1.0 cache hit reads as a measurement | caught (`hit_minus_one`) |
| 7 | an absent cache hit prints as zero | caught (`hit_absent`) |
| 8 | an absent stall prints as zero | caught (`no_stall_column`) |
| 9 | the sizing table requires both budget and prefetch | caught (row count) |
| 10 | the sizing table hides the prefetch usefulness | caught (`spec`) |
| 11 | two-decimal values lose their zero padding | caught (`no_stall_column`) |

```
cd tests && python3 mutate_dg.py 0 6    # 6/6
cd tests && python3 mutate_dg.py 6 12   # 6/6
```

## Recorded limits

* Decimal text is parsed as integer part plus fraction divided by a power of
  ten. For the gated corpus this is bit-identical to `float(text)`; a decimal
  long enough for the two parses to differ in the last ulp is outside the
  port's contract and would be caught as a field mismatch, not silently
  absorbed.
* The column layout keeps the native quirk: `wall_ms` is read from **column 2**
  by position while `stall_ms` and `mgmt_ms` are read by name. Reproducing that
  is deliberate — a file with the columns in another order is native-broken too.
* The model/ORDER table, the labels, the file walk, `summary.md` writing and the
  console tables around the rows stay with the caller.
* `mgmt_ms` is carried through the same path as `stall_ms` but no gated case
  exercises a present `mgmt_ms` column; it is reported as absent (`-1`).

## Assumptions and conflicts

* Out buffers: the CSV is scanned line by line; a case may hold at most 64 rows
  and 32 columns. Larger files are truncated rather than mis-parsed, which the
  caller must check if it feeds a very long capture.
* `import std` and `import math` (for `floor`); no externs.
* No conflicting module. `metrics/metrics-traces.goose` (P9-07) *writes* the CSV
  trailer; this module *reads* it. They share the key names and nothing else,
  and the writer's `%.6g` extern stays its own.

## Integration step

1. Copy `source/dg-diagnostics.goose` to
   `goose/metrics/bench-diagnostics.goose` (the metrics group, next to
   `metrics-traces.goose`, which produces the input).
2. Add to `tools/sync-goose-tree.py` `CANONICAL_SOURCES`:
   `("metrics/bench-diagnostics.goose", "EXP-<date>-<nnn>-p10-09-diagnostics", "source/dg-diagnostics.goose", "scripts/bench-analyze.py, scripts/trace_io.py", "16 cases / 66 rows / 0 differences, 12/12 mutations")`.
3. Add the `goose/MANIFEST.json` entry with the file's SHA-256.
4. Update `tasks.json` `P10-09` (`slices_done` 3 of 3, `remaining` empty,
   `status` DONE once the acceptance note is recorded).
5. Keep `tests/native/bench_analyze.py` and `tests/native/trace_io.py` as the
   pinned oracle copies (SHA-256 below).

## SHA-256

```
c79f6718fff6d671b80603716f6052bb3b3cfc6712dc04f77bb160da0f07d00f  source/dg-diagnostics.goose
d78414ae4c4eb84c3bde384b96f49cfbcf6d07504e84bed531946c3873c62e64  tests/native/bench_analyze.py
01b341758b96c31c73dc33582dbc55b832acee2af8f53237bd26d19a25d9f057  tests/native/trace_io.py
91eeadbcb47b283ce96119f65300f87b8bef435db07a239626ace5bd89d5903b  tests/make_cases.py
3c859f45e8d72e580e05336651cc63215169861009b8bf5c6c3250fbcab5016c  tests/native_rows.py
59430a8d64810b6ceb052cc05022bf46480cd9107f508f19b71e0a4213136fea  tests/gen_dg.py
df0b735cd260de4c314e293fe33b08a22b8c1f06e7a6918489c8730e31e413fe  tests/mutate_dg.py
ca72c0f2b37318a18e86ac59020eedc5d5f29f87446296a6332c7ff5838b5e1f  build/native_dg.tsv
5cababd48168a970964f905b5265632145c0e77c9803e46617104175ecf7ff76  build/goose_dg.tsv
```
