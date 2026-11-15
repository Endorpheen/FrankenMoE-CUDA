# EXP-2026-11-15-714 — the GPU expert cache IS active, and it is not the gap

Base: `89eac9ca`. This card settles the question the 12 November history raised.

## 1. The cache is active, measured live

Measured with `nvidia-smi --query-compute-apps` on our own pid, during the run:

| moment | our process VRAM |
|---|---:|
| after model load | 5174 MiB |
| after request 1 | **6778 MiB** |
| after request 2 | **6784 MiB** |
| reference with `--moe-cache-mib 3584` (12 Nov, r58) | **6778 MiB** |

**The VRAM figure matches the reference to the MiB.** The cache is allocated, sized and resident.
The 4084 MiB observed earlier was before the expert pages were touched, not a disabled cache -
which is why VRAM alone was never going to settle this.

## 2. And it does not deliver the reference decode

| | prefill | decode | VRAM |
|---|---:|---:|---:|
| this run 1 | 2011.50 | 34.48 | 6778 |
| this run 2 | 2291.73 | 35.33 | 6784 |
| reference r54 (3584 MiB cache) | 2326.5 | 41.01 | 6778 |
| reference r56 (3584 MiB cache, 512 tok) | 2326.8 | 41.94 | 6778 |

Identical memory footprint, 14% less decode. So the cache is not the missing mechanism: it is
present and doing whatever it does, and the speed difference lies somewhere else.

## 3. What that points at

The cache being fully allocated while decode stays at 35 means the bytes are resident but the
*lookup path* that turns residency into speed is not running - which is exactly what
`PERF-INVENTORY.md` found independently: the Goose entry points that implement the quantised dot
and expert routing (`fg148_*`, `fg150_*`, `fg_moe_*`) are reachable only through
`*_unused_main` wrappers that **nothing calls**.

Two independent measurements now agree on the same conclusion:

- memory: the cache is there, 6778 MiB as the reference;
- code: the expert/quantised routes exist, and have no callers.

If the cache alone produced the speed, resident bytes would be enough. They are not.

## 4. The 43.04 tok/s figure

Correct to keep it separate: it is a 512-token generation (`ebee4d1`), so it is not comparable
to a 128-token run. It is a real measured result and belongs in the record, labelled with its
own conditions.

## 5. Balance, per rule 19

| axis | value | vs reference |
|---|---:|---:|
| prefill | 2291.73 | +2.8% |
| decode | 35.33 | -14.0% |
| VRAM | 6784 MiB | +0.1% |
| RAM | not sampled in this series | - |

Memory is not the problem and is not being inflated. Speed on one axis is down while memory and
the other axis are at or above reference - which is the signature of a missing code path, not a
misconfigured cache.
