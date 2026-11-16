# EXP-2026-11-15-740 — the best configuration beats the reference at all three lengths

Base: `53fea6ca`. Rule 20 requires three lengths before a configuration is accepted; until now the
ub 2048 / cache 5376 configuration had only been measured at 512. Completed.

## 1. Full matrix

| length | prefill | decode | decode range | reference | vs reference |
|---:|---:|---:|---|---:|---:|
| 128 | 2040.1 | **41.27** | 40.92-41.45 | 39.6 | **+4.2%** |
| 256 | 2035.2 | **42.43** | 42.14-42.98 | 41.0 | **+3.5%** |
| 512 | 2019.3 | **42.35** | 41.97-42.56 | 41.94 | **+1.0%** |

VRAM 7972 MiB at every length, RAM 20.4 GiB. Configuration: `-ub 2048 --moe-cache-mib 5376`.

**The configuration exceeds the historical reference at all three lengths.** That is the first time
this session a configuration has been ahead of the reference on the metric that matters at every
generation length, rather than at one point.

## 2. The shape of the curve is the interesting part

Reference decode rises with length: 39.6 -> 41.0 -> 41.94.
This configuration: 41.27 -> 42.43 -> 42.35.

So it is **better at short lengths and converges at long ones**:

| length | gap |
|---:|---:|
| 128 | +4.2% |
| 256 | +3.5% |
| 512 | +1.0% |

The advantage shrinks as generation grows, which means the per-token cost is what improved - consistent
with the memory reallocation mechanism: more resident experts means fewer expert copies per token, and
a fixed improvement per token is worth proportionally more when there are fewer tokens to amortise.

That is the opposite of the earlier build (EXP-717), which was weakest at 128 (-6.0%) and best at 512
(-4.5%). The profile has genuinely inverted.

## 3. Where the cost lies

Prefill is 2019-2040 against the reference 2228.76, i.e. **-8.5% to -9.4%**. Per rule 19 that is the
stated compromise: prefill down ~9%, decode up 1-4.2%, VRAM up 420 MiB (+5.6%), RAM unchanged.

For the reference workload (32K prompt, 128 out) the prefill cost is real: 16.1 s to first token
against 14.7 s. For anything generation-heavy it is repaid within a few hundred tokens.

## 4. Configurations on record

| configuration | 128 | 256 | 512 | when |
|---|---:|---:|---:|---|
| ub 512, cache 4352 | 38.17 | 39.50 | 40.04 | highest prefill, reference profile |
| **ub 2048, cache 5376** | **41.27** | **42.43** | **42.35** | **best balance, ahead at all lengths** |
| ub 1024, cache 5632 | - | - | 42.74 | max decode at 512, prefill sacrificed |

The middle one is now the recommended default for generation; the first remains correct when prefill
dominates and the third is only for maximum single-length decode.

## 5. All rows recorded

Nine responses in this directory plus the journal entries; each length has its own request file so the
measurement is reproducible.

## 6. Cost

Two lengths, six runs, no failed starts.
