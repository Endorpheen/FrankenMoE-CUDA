# EXP-2213 — Reuse the destination span in graph execution

The overwrite-eligible path already collects the destination byte span in
its Goose graph packet. Use that span instead of calling the native size
function again. The other execution paths retain their existing size lookup.

The control is EXP-2212 at `59f03021`. On six alternating paired trials,
median time was 34.62 ns/node with the separate native lookup and 27.44
ns/node with packet reuse, a 20.7% reduction on the measured path. All
256 fixture packets and outputs matched. The change adds no allocation.
Model speed and memory are deferred to the next accumulated inference check.

Use KeelWing v0.3.0 preview and GCC 13.4.0. Supply the v0.15.0 Linux build's
`common` object directory for the native reference.

```bash
GOOSE_BIN=/path/to/keelwing bash source/measure.sh /path/to/empty-output /path/to/baseline/common
/path/to/empty-output/span-bench
```

The checked-in harness reproduced parity for all 256 packets and median
time of 31.84 ns/node for the reference and 27.56 ns/node for reuse.
