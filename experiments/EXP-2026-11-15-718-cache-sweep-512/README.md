# EXP-2026-11-15-718 — cache sweep at 512 tokens: 4352 MiB is the optimum

Base: `1cdf003e`. Rule 20 applied: candidates filtered at 128 are re-judged at 512, where this
build's measurement is most stable (0.2 tok/s spread).

## 1. Result — the sweep, three runs each, one warm-up

| cache MiB | prefill | decode median | decode range | VRAM |
|---:|---:|---:|---|---:|
| 3584 | 2261.4 | 33.81 | 33.79-34.82 | 6784 |
| 3840 | 2399.5 | 38.43 | 38.42-38.72 | 7042 |
| 4096 | 2409.4 | 39.40 | 38.34-39.50 | 7298 |
| **4352** | **2429.9** | **39.86** | 39.83-40.22 | 7552 |
| 4608 | 2313.3 | 38.65 | 38.64-38.84 | 7810 |

4352 MiB wins on **both** speed axes and uses **258 MiB less** than 4608. It was re-measured to
confirm: 39.86 and 40.12 on two separate servers, ranges 39.83-40.22 and 39.85-40.25.

## 2. Balance, per rule 19

Against 4608 (the previous best), at 512 tokens:

| axis | 4608 | 4352 | change |
|---|---:|---:|---:|
| prefill | 2313.3 | 2429.9 | **+5.0%** |
| decode | 38.65 | 39.86 | **+3.1%** |
| VRAM MiB | 7810 | 7552 | **-258 (-3.3%)** |
| RAM GiB | 20.8 | 20.8 | 0% |

Every axis improves. This is not a compromise - it is a straight win, and it means the 4608
recommendation in EXP-715 was measuring a local dip rather than a peak.

Against the 3584 reference configuration, which is what the project shipped:

| axis | 3584 | 4352 | change |
|---|---:|---:|---:|
| prefill | 2261.4 | 2429.9 | **+7.5%** |
| decode | 33.81 | 39.86 | **+17.9%** |
| VRAM MiB | 6784 | 7552 | +768 (+11.3%) |
| RAM GiB | 20.8 | 20.8 | 0% |

+768 MiB buys +17.9% decode and +7.5% prefill. Favourable.

## 3. Against the reference at this length

4352 MiB gives 39.86 decode at 512 tokens. The recorded reference at 512 is 41.94, so this is
**95.0%** - and at 3584 MiB the reference was 41.94 too, so the remaining 5% is not memory.

## 4. Why the curve is not monotone, and why that matters

3584 -> 3840 jumps +13.7% decode; 3840 -> 4096 adds 2.5%; 4096 -> 4352 adds 1.2%; 4352 -> 4608
**drops** 3.0%. A clean memory-to-speed curve would not behave like that.

The likely explanation is expert residency: the cache holds whole experts, and whether a given
token's top-8 land in the cache depends on how many fit. Between 4352 and 4608 another expert
group may become resident-but-thrashing rather than resident-and-used. That is a hypothesis, not
a measurement - it is recorded as one.

What the sweep does establish is that the optimum is a specific point (4352), not "as much as
possible", which is why sweeping mattered and why the earlier assumption that more cache is
better was wrong twice.

## 5. Recommendation

**Cache 4352 MiB is the throughput configuration**, superseding 4608:
- best prefill (2429.9) and best decode (39.86) of every configuration measured;
- 258 MiB less VRAM than 4608;
- reproducible across two servers.

**Cache 3584 MiB stays the economical configuration**: 33.81 decode at 6784 MiB, 768 MiB cheaper
for 17.9% less decode.

Both are recorded in the journal. Neither is declared a loser; they serve different points on the
memory-speed trade.

## 6. Not claimed

That 4352 is the global optimum. It is the best of five points sampled at 512 tokens. The dip at
4608 suggests structure in the curve, and 4480 was not measured.

All twelve rows are in `benchmarks/experiment-history.jsonl`.
