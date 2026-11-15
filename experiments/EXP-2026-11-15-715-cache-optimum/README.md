# EXP-2026-11-15-715 — the GPU cache optimum: 4608 MiB at 37.23 decode

Base: `4a78cd29`. Four measured runs after one unrecorded warm-up, one server.

## 1. Result

| run | prefill tok/s | decode tok/s |
|---|---:|---:|
| 1 | 2294.5 | 37.26 |
| 2 | 2290.9 | 36.63 |
| 3 | 2175.3 | 37.20 |
| 4 | 2269.2 | **39.84** |
| **median** | **2280.0** | **37.23** |

VRAM during generation 7804 MiB, RSS 21.8 GiB. Content sha256 identical in all four.

## 2. Why four runs, and why the earlier sweep misled

The first sweep took two runs per configuration and the spread inside one configuration was as
large as the difference between configurations (3584 gave 25.63 and 36.23 on two consecutive
runs). A two-run comparison of a 5%-noisy measurement is not a comparison.

With a warm-up plus four measured runs the picture is stable and the earlier anomaly resolves:

| cache MiB | best decode (2-run sweep) | median decode (4-run) |
|---:|---:|---:|
| 3584 | 36.23 | same build, same noise band |
| 4608 | 37.72 | **37.23** |
| 5120 | OOM | **OOM (twice)** |

## 3. The ceiling is real

5120 MiB failed with `cudaMalloc failed: out of memory` on both attempts, the second with
8568 MiB showing free on the GPU - the allocation is contiguous and the device cannot provide
it. 4608 MiB is the highest cache that runs, and it leaves ~570 MiB of headroom.

## 4. Balance, per rule 19

Compared with the 3584 MiB configuration the project used as its reference:

| axis | 3584 | 4608 | change |
|---|---:|---:|---:|
| decode median | 36.23 | 37.23 | **+2.8%** |
| prefill median | 2189.5 | 2280.0 | **+4.1%** |
| VRAM | 6784 | 7804 | +1020 MiB (+15.0%) |
| RAM | 21.8 GiB | 21.8 GiB | 0% |

Both speed axes improve and memory grows 15%. The exchange is favourable: 1020 MiB of VRAM buys
+2.8% decode and +4.1% prefill. It is a compromise and it is stated as one - the memory cost is
real, and it is the reason 4608 is offered as the throughput configuration rather than the only
configuration.

The 3584 configuration remains the economical one and is preserved in the journal.

## 5. Honest position against the historical reference

Historical reference: 41.01-41.94 decode at 6778 MiB.
This build: 37.23 median at 7804 MiB.

So this build needs **more** memory for **less** decode. That is not an optimisation victory; it
is the state of the tree. The journal now records both, separately, so the gap cannot be
mistaken for noise.

The decode gap is the same one the inventory names: the quantised-dot and expert routes have no
live call sites, so resident experts are not used the way the reference used them. More cache
therefore buys less than it did historically - which is exactly what the non-monotone curve above
shows.

## 6. Reproducibility

```
/tmp/ab4.sh <port> 4608 <tag> 4     # warm-up + 4 measured runs, frees the port after
python3 tools/bench-log.py --compare
```

Both the 4608 rows and both 5120 OOM rows are in `benchmarks/experiment-history.jsonl`.
