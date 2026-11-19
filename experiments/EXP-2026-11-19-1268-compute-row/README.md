# EXP-2026-11-19-1268: the compute-trace row and the metrics column count

Two metrics decisions compared with real code, both byte-exact:

```
compute row: cases=2000 fails=0 cols=29
```

- **the row encoder** (`mt_compute_row_encode`) against the shipped sink: a `ComputeTraceRow` is written by the
  real `CsvComputeTraceSink` through a `FILE*` and by the port into a buffer, over 2000 rows whose op and name
  are drawn from an alphabet that reaches every escaping rule (comma, quote, newline, CR, tab, space);
- **the column count** (`mt_metrics_cols`) against the native header literal: 29 names, counted from the string
  the sink actually prints. A miscount here corrupts every consumer that reads columns by position, and the
  native code comments say order is not a contract - the count still is.

## Why the column check matters even though it looks trivial

The port stores the number as a constant; the native side has a literal string. Nothing links them, so a column
added to the header without the constant is exactly the kind of drift that only shows up as a shifted CSV
months later. Counting from the shipped literal turns it into a one-line check.

## State of the metrics side

Two of its decisions now have real-reference evidence (this card and EXP-2026-11-19-1267). The remaining
exported functions are the trace column lists, the summary keys and the weight-kind arm of the route sink - the
same shape, and each one is a comparison away.
