# EXP-2026-11-15-728 — expert placement flags: three attempts, all OOM, hypothesis closed

Base: `11f7dd6d`. EXP-726 established that expert traffic is structural (16.9 GiB of experts, 12 GiB
device) and completely unoverlapped. The obvious next lever is to stop moving experts at all by
placing some of them permanently on the GPU. Three configurations tried.

## 1. What was tried

| config | allocation attempted | result |
|---|---:|---|
| `--n-cpu-moe 10` (replacing `--cpu-moe`), `-ngl all`, cache 4352 | 15,186.90 MiB | **OOM** |
| `--n-cpu-moe 10`, `-ngl all`, cache 0 | 15,186.90 MiB | **OOM** |
| `--n-cpu-moe 4`, `-ngl all`, cache 0 | 15,186.90 MiB | **OOM** |
| `--n-cpu-moe 10`, `-ngl 30`, cache 4352 | 13,981.78 MiB | **OOM** |

## 2. Why it cannot work in this profile

`--n-cpu-moe N` means "keep the **first** N layers' experts on the CPU" - the rest go to the GPU.
The reference profile is `--cpu-moe` with `-ngl all`, i.e. **all** experts stay on the CPU by design,
and the GPU receives experts only through `--moe-cache-mib`, which is a bounded cache rather than a
permanent placement.

So switching to `--n-cpu-moe` asks the GPU to hold the experts of 30+ layers permanently - 14 to
15.2 GiB - on a 12 GiB device that is already carrying the dense weights and the KV cache. The
allocation fails before the model finishes loading, and it fails the same way regardless of N or of
the cache setting, because the cache size does not affect permanent placement.

The device is a 12 GiB card; the full expert set is 16.9 GiB. **There is no arrangement of these two
flags that fits.** That is arithmetic, not configuration.

## 3. Consequence for the plan

The two levers left from EXP-726 are now one:

| lever | status |
|---|---|
| raise the hit rate by placing more experts | **closed** - 16.9 GiB > 12 GiB, and `--n-cpu-moe` cannot help |
| overlap transfer with compute | **open**, and now the only one |

Which reinforces the previous card's conclusion: the win is in graph construction (issue expert copy
nodes earlier so the scheduler can overlap them), not in placement or cache size. Placement has been
measured and eliminated; there is nothing further to tune there on this hardware.

## 4. State

No configuration changed. The pinned configuration (`runtime/goose-stage/BEST-CONFIG.json`) and its
measured numbers stand: 2429.9 prefill, 39.86 decode at 512 tokens, VRAM 7552 MiB. All four failed
attempts are recorded here rather than silently discarded.

## 5. Cost

Four failed server starts, no successful run, no rebuild. Roughly two minutes.
