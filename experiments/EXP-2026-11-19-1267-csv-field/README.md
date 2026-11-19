# EXP-2026-11-19-1267: the metrics CSV encoder against the real writer

`write_csv_field` (decode_trace_sink.cpp) is small and has three rules that a hand port gets wrong silently:

- a field needs quotes only for `,`, `"` and `\n` - not for `\r`, tab or space;
- inside the quotes a `"` is doubled (RFC4180);
- **a newline inside a quoted field becomes a space**, so a field never spans lines.

The port already existed (`goose/metrics/metrics-traces.goose`); what it lacked was a comparison with the real
writer, which writes through a `FILE*`.

```
csv field: cases=4000 fails=0
```

4000 random strings over an alphabet chosen to reach every rule (`a b , " \n \r \t space 0 Z`), written by the
native function through a temp file and read back, compared byte for byte with the ported
`mt_field_encode` - including the escaped-byte rendering on failure, because a difference of one control
character is otherwise invisible.

## Why the third rule is the one worth having a test for

Quoting and doubling are conventional and easy to remember. Replacing a newline with a space is a **choice** this
code makes so that the row stays one row, and it is the kind of rule that is lost when someone rewrites the
encoder "properly". With `\n` in the alphabet, every case containing one exercises it.

## What this closes in P9-07

The metrics side is ported (`goose/metrics/metrics-traces.goose`, 27 functions) and now has its first verified
decision against compiled native code. The remaining metrics functions (column lists, weight formatting) are the
same kind of check and can follow the same shape.
