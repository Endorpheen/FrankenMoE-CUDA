# EXP-2211 — Describe tensor layouts in Goose

The candidate computes graph packet spans in Goose. The C boundary copies the
contiguous shape and stride fields and reports which source operands exist.
It leaves absent operand descriptors untouched, matching the release behavior.

## Microbenchmark

The baseline is GooseInfer v0.15.0, clean source revision
`c03c2ee9b93ee0709b9bde74bded6070bda8251f`. The direct span calculation
matched 8960 native layouts across supported GGML types. The complete node
description matched all 256 fixture packets. Six alternating paired trials
measured median 28.63 ns/node for the release C path and 21.29 ns/node for
the candidate, a 25.7% reduction. This includes the metadata copy and
presence mask. The test does not establish an inference throughput gain.

Use KeelWing v0.3.0 preview and GCC 13.4.0. Supply the baseline release
build's `common` output directory. The harness builds the Goose descriptor
with GCC `-O1` and the C bridge with GCC `-O2`, as in the universal runtime.

```bash
GOOSE_BIN=/path/to/keelwing bash source/measure.sh /path/to/empty-output /path/to/baseline/common
/path/to/empty-output/parity
/path/to/empty-output/describe-bench
```

## GigaChat4 output gate

On RTX 4070, the candidate completed 32768/512 at 292.70 prefill and
12.64 decode tok/s versus 294.48 and 12.63 for v0.15.0. All 512 output
IDs, full text and stop type matched. RSS high-water mark was 24365116 KiB
with zero process swap. System swap-out increased by 1948 pages during
the candidate request, so the run qualifies output parity only.

A reproduction using the checked-in harness matched the same 8960 layouts
and 256 packets. Its alternating paired trials measured median 31.12
ns/node for C and 22.61 ns/node for Goose.
