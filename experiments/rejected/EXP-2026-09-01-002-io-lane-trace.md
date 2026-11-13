# EXP-2026-09-01-002-io-lane-trace

## Decision

`REJECTED` hypothesis: four I/O lanes are already balanced, so a systematically slow lane does not explain the drain wait.

## Configuration

The unchanged P0 runtime ran 64 generated tokens with the baseline prompt and parameters plus `--io-trace`. Trace locking makes the absolute 5.759 tok/s diagnostic only; it is not compared with the untraced performance baseline.

## Trace summary

- 23,247 demand reads.
- 14,734.5 MiB of aligned physical traffic.
- 15,050 requests of 524,800 bytes, 448 of 704,000 bytes, and 7,749 of 921,600 bytes.
- Overall latency: 1.919 ms p50, 2.859 ms p95, 3.288 ms p99, 5.913 ms maximum.
- Prefill latency: 0.900 ms p50 and 2.233 ms p95.
- Decode latency: 2.122 ms p50 and 2.993 ms p95.

| Lane | Reads | Read MiB | Latency sum s | p50 ms | p95 ms | Effective MiB/s |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | 5,832 | 3,690.2 | 10.364 | 1.924 | 2.867 | 356.1 |
| 1 | 5,839 | 3,717.9 | 10.375 | 1.919 | 2.866 | 358.4 |
| 2 | 5,779 | 3,656.7 | 10.215 | 1.923 | 2.859 | 358.0 |
| 3 | 5,797 | 3,669.8 | 10.187 | 1.911 | 2.847 | 360.2 |

Lane counts differ by only 1.0%, transferred bytes by 1.7%, and latency sums by 1.8%.

## Coalescing opportunity

Within each step/layer/projection, 9.02% of adjacent request pairs are exactly contiguous. Merging all of them would remove about 6.6% of read calls without over-read. Allowing gaps up to 1 MiB makes 16.56% of pairs mergeable but increases traffic by 5.54%. A 2 MiB gap adds 20.74% traffic, and a 4 MiB gap adds 64.18%, so broad coalescing is not justified.

## Next action

Test eight I/O lanes as a configuration-only experiment. If it fails, exact-adjacency coalescing remains the smallest code experiment supported by the trace.
