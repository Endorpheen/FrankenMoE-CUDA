# EXP-2212 — Reuse tensor layout for contiguity checks

The graph executor checks contiguity in Goose using the destination layout
already collected for regular nodes. It only requests the layout early for
operations eligible for overwrite. The CPU object has no unresolved call
to the old GGML contiguity entry point.

The baseline is EXP-2211 at `37a374fc`. Both microbenchmark arms use the
same layout bridge and Goose span function. The reference makes a separate
native contiguity call. The candidate reads the existing packet.
On six alternating paired trials, median time was 29.42 ns/node for the
reference and 28.54 ns/node for the candidate, a 3.0% reduction.
All 8960 supported-type layouts and all 256 packets matched. A full
model-throughput measurement is deferred until several changes accumulate.

Use KeelWing v0.3.0 preview and GCC 13.4.0. Supply the v0.15.0 Linux build's
`common` object directory for the native reference.

```bash
GOOSE_BIN=/path/to/keelwing bash source/measure.sh /path/to/empty-output /path/to/baseline/common
/path/to/empty-output/parity
/path/to/empty-output/bench
```
