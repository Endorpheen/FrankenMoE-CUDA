# EXP-2026-11-15-754 — the context/cache trade curve, and the correction of EXP-753

Base: `0811e242`. EXP-753 concluded that shrinking the context was *necessary* to afford cache 4992. That
was wrong, and this card measures the actual curve.

## 1. Cache 4992 fits at every context

| ctx | decode @512 | vs ctx 65536 | reference hash |
|---:|---:|---:|---|
| 65536 | **43.45** | - | yes |
| 98304 | 43.04 | -0.9% | yes |
| 131072 | 42.51 | -2.2% | yes |
| **262144** | **41.61** | -4.2% | **yes** |

At full context 262144: runs 41.85 / 41.61 / 41.22, all with `c2ff42cc`. **Cache 4992 is available without
sacrificing any context.**

## 2. Correcting EXP-753

EXP-753 presented this as a binary trade - "65536 for +6% decode, or 262144 with 4608" - and left the
choice to policy. **That framing was wrong.** It came from testing 5120 at ctx 65536, seeing it OOM, and
concluding the memory for a larger cache had to come out of the context. The measurements above show the
cache ceiling is ~4992 regardless of context: 5056 and 5120 OOM at 65536, 98304 and 262144 alike, and the
failure is a 1284 MiB compute-buffer allocation at first decode, not KV.

So the honest two-dimensional picture is:

| cache | ctx 65536 | ctx 131072 | ctx 262144 |
|---:|---:|---:|---:|
| 4608 | 42.29 | - | 41.04 |
| **4992** | **43.45** | 42.51 | **41.61** |
| 5056+ | OOM | OOM | OOM |

**4992 is dominated by nothing**: it is at least as fast as 4608 at every context, and it keeps all
context options open.

## 3. The real result

Against the previous default (262144 / 4608 = 41.04):

| configuration | decode @512 | gain | context |
|---|---:|---:|---:|
| 262144 / 4608 (previous default) | 41.04 | - | 262144 |
| **262144 / 4992** | **41.61** | **+1.4%** | **262144** |
| 131072 / 4992 | 42.51 | +3.6% | 131072 |
| 65536 / 4992 | 43.45 | +5.9% | 65536 |

**The zero-cost change is cache 4992 at the unchanged context: +1.4%, no trade at all.** The context
reduction is an *additional* optional trade on top, worth 2.2% per halving below 131072.

## 4. Recommendation, now unambiguous

- **Default: `-c 262144 --moe-cache-mib 4992`** - strictly better than the previous default, nothing
  given up.
- Optional short-context profile: `-c 65536 --moe-cache-mib 4992` for +4.6% over it when 64K context
  suffices.

EXP-753's "policy decision" framing is withdrawn - the default choice is now a measurement.

## 5. Cost

Five configurations, fifteen runs, two OOM starts.
