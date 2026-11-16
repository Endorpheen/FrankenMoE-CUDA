# EXP-2026-11-15-742 — 43.05 decode at 512 tokens: a new point on the frontier, not a new optimum

Base: `dabd91ad`. EXP-741 showed the batch/cache relation is a frontier, and left 3072 unmeasured
between the 4096 and 2048 points. Filled in.

## 1. Full matrix for ub 3072 / cache 5376

| length | prefill | decode | decode range | reference | vs reference |
|---:|---:|---:|---|---:|---:|
| 128 | 2072.9 | 39.82 | - | 39.6 | +0.6% |
| 256 | 2027.9 | 41.52 | - | 41.0 | +1.3% |
| 512 | 2008.7 | **43.05** | 42.76-43.19 (6 runs) | 41.94 | **+2.6%** |

**43.05 is the first measurement above 43 tok/s this session**, and it is 2.6% above the reference at
this length.

## 2. But it is not better than ub 2048 - it is a different point

| length | ub3072 decode | ub2048 decode | winner |
|---:|---:|---:|---|
| 128 | 39.82 | **41.27** | ub2048 |
| 256 | 41.52 | **42.43** | ub2048 |
| 512 | **43.05** | 42.35 | ub3072 |

ub 2048 wins two lengths out of three, and by a larger margin (1.45 and 0.91 tok/s) than ub 3072 wins
the third (0.70). **ub 3072 is not a better default; it is better at the longest generation only.**

Against ub 2048 at 512: prefill -0.5%, decode +1.7%, VRAM +190 MiB. That exchange is favourable for
long generation and pointless for short - which is exactly what a frontier looks like.

## 3. The frontier, now with four measured points

| ub | max cache | prefill | decode @128 | @256 | @512 | VRAM |
|---:|---:|---:|---:|---:|---:|---:|
| 4096 | 4608 | 2414 | 38.44 | 40.05 | 40.99 | 7810 |
| **3072** | **5376** | **2009** | 39.82 | 41.52 | **43.05** | 8162 |
| 2048 | 5376 | 2019 | **41.27** | **42.43** | 42.35 | 7972 |
| 1024 | 5632 | 1489 | - | - | 42.74 | 7972 |

Two things stand out that were not visible with fewer points:

1. **ub 3072 and 2048 take the same cache size (5376).** The batch size is not what limits the cache
   between them - only at 4096 does the batch force a smaller cache. So between 3072 and 2048 there is
   no memory trade at all, only a speed profile change.
2. **The decode-by-length curves cross.** ub 2048 is better at short and medium lengths, ub 3072 at
   long. That is not a configuration error in either; it is two different amortisation profiles.

## 4. Decision

**No change to the default.** ub 2048 remains it, because it wins two of three lengths and the third
loss is the smallest margin. ub 3072 is recorded as the **long-generation** point and is available with
`--ub 3072`.

Per rule 20, both are kept as candidates on the evidence; neither is declared a loser.

## 5. What 43 tok/s cost, stated plainly

43.05 at 512 tokens against the reference 41.94 is +2.6%, bought with prefill 2009 against 2228.76
(-9.9%) and 8162 MiB of VRAM against 6778 (+20.4%). The decode number is real; the price is real too.

## 6. Cost

Two configurations, fifteen runs, no failed starts.
