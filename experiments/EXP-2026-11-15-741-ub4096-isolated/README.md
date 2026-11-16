# EXP-2026-11-15-741 — isolating ubatch 4096: prefill champion, decode loser at every length

Base: `6f388175`. The label correction exposed a real gap in the sweep: ubatch 4096 - the reference
value - was only ever tested with cache 4352. The cache effect and the ubatch effect had been measured
together and never separated at 4096.

## 1. Two complete configurations, all three lengths

| length | ub4096 / cache4608 | ub2048 / cache5376 | reference |
|---:|---|---|---:|
| 128 | 2434 prefill / **38.44** decode | 2040 prefill / **41.27** decode | 39.6 |
| 256 | 2426 prefill / **40.05** decode | 2035 prefill / **42.43** decode | 41.0 |
| 512 | 2423 prefill / **40.99** decode | 2019 prefill / **42.35** decode | 41.94 |

**ub2048 wins decode at every length** by 2.8 / 2.4 / 1.4 tok/s. **ub4096 wins prefill by ~19%** at
every length.

## 2. What the isolation actually shows

At ub 4096, raising the cache from 4352 to 4608 moved decode 39.86 -> 40.99 at 512 (+2.8%) and cost
258 MiB. That is the same mechanism seen at ub 2048, and it confirms the cache effect is **independent
of the batch size** - cache size buys decode, ubatch size trades prefill against memory headroom.

The reason 4096 cannot take a large cache is arithmetic: a bigger batch needs bigger compute buffers,
so at 4096 the cache OOMs above 4608 while at 2048 it reaches 5376. **The two levers compete for the
same memory**, which is why the ubatch sweep and the cache sweep produced interleaved results.

## 3. Neither configuration dominates

| axis | ub4096/cache4608 | ub2048/cache5376 |
|---|---:|---:|
| prefill | **2414** (vs reference 2228: +8.3%) | 2019 (-9.4%) |
| decode @512 | 40.99 | **42.35** |
| decode @128 | 38.44 (below reference 39.6) | **41.27** |
| VRAM | **7810** | 7972 |

So ub4096 is the first configuration this session to **beat the reference on prefill** (+8.3%) while
still gaining 2.8% decode at 512 - and it uses 162 MiB less. But its decode is below the reference at
128 tokens, so it is not ahead at every length the way ub2048 is.

**Both stay on record.** For a workload where time-to-first-token dominates - long prompt, short answer
- ub4096/cache4608 is now the best measured. For generation, ub2048/cache5376 remains ahead.

## 4. Correcting the framing of the sweep

Earlier cards presented ubatch as monotone: "2048 optimal, 1536 and 1024 cost prefill with no decode
gain". That was measured against cache 5376 in every arm, which only ub 2048 can hold. **The correct
statement is a frontier, not an optimum:**

| ub | max cache that fits | prefill | decode @512 |
|---:|---:|---:|---:|
| 4096 | 4608 | 2414 | 40.99 |
| 2048 | 5376 | 2019 | 42.35 |
| 1024 | 5632 | 1489 | 42.74 |

Every row is on the memory frontier. Choosing between them is choosing a point on a trade curve, not
finding a better setting.

## 5. Cost

Four configurations, twelve runs, three failed starts (cache 5120 and 5632 at ub 4096 both OOM).
