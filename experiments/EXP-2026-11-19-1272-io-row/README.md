# EXP-2026-11-19-1272: the io-trace row, and twelve columns that must match the header

```
io row: cases=2000 fails=0 io_cols=12
```

The third and last trace row encoder, compared with the real `CsvIoTraceSink` byte for byte over 2000 rows, and
the column count `mt_trace_cols(IO)` compared with the native header literal.

## Why the count is checked separately from the row

A row can be internally consistent and still disagree with its header - and the consumer reads by position. The
compute row (EXP-1268) and the route row (EXP-1269) already had byte comparisons, which implicitly pin their
widths; doing the count explicitly here makes the three trace kinds consistent in how they are checked rather
than leaving one on inspection alone.

## State of the metrics side

| part | evidence |
|---|---|
| CSV field escaping | 4000 strings, EXP-1267 |
| compute row + metrics column count | 2000 rows, EXP-1268 |
| route row + NaN weight arm | 3000 rows, EXP-1269 |
| io row + trace column count | 2000 rows, this card |

What remains exported and unverified is `mt_summary_keys` (a count of 62) and the two smaller helpers
(`mt_count_cols`, `mt_unescape_field`). Each is a comparison away, and the summary count needs a decision about
what to compare it against - the native writes a formatted line rather than a key list, so counting from the
source would be the transcription this session has learned to avoid.
