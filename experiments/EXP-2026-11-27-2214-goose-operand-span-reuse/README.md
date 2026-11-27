# EXP-2214 — Reuse operand spans for activation alias checks

On the overwrite-eligible path, the first four operands already have byte
spans in the Goose packet. Reuse the spans during alias validation. Operand
slots beyond the packet use the existing size lookup. No allocation changes.

The control is EXP-2213 at `dec5fe1a`. The fixture covers 256 nodes with
varying operand counts and overlapping or disjoint addresses. All packet
bytes and alias decisions matched. Six alternating trials measured median
80.69 ns/node for the control and 73.05 ns/node for the candidate, a 9.5%
reduction. The result is a CPU microbenchmark of this path.

Use KeelWing v0.3.0 preview and GCC 13.4.0, with the v0.15.0 Linux build's
`common` output directory for the native reference.

```bash
GOOSE_BIN=/path/to/keelwing bash source/measure.sh /path/to/empty-output /path/to/baseline/common
/path/to/empty-output/operand-bench
```

The checked-in harness reproduced all 256 matches and median times of
79.70 ns/node for the control and 71.79 ns/node for the candidate.
