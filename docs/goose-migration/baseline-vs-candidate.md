# Baseline vs candidate: the conditions, checked flag by flag

Source of the baseline: `/run/media/end0/CAE3-E74B/AI/Models/GGUF/ПРОФИЛИ_ЗАПУСКА.txt`, profile 4
(Apex-35B-V1.0, Flint/Apex build, measured 2026-10-29). Records beat remembered figures.

## What matches exactly

| item | baseline | candidate |
|---|---|---|
| model file | SHA-256 `c3b92d94…` | **same SHA-256** |
| context | `-c 262144`, one slot | same |
| GPU placement | `-ngl all -fit off` | same |
| KV cache | `-ctk q4_0 -ctv q4_0` | same |
| batches | `-b 4096 -ub 4096` | same |
| threads | `-t 8 -tb 16` | same |
| CPU masks | `--cpu-mask 0f0f --cpu-mask-batch ffff --cpu-strict 1 --cpu-strict-batch 1 --poll 0` | same |
| caches | `-np 1 --cache-ram 0 --ctx-checkpoints 32` | same |
| load | `--load-mode mmap --spec-type none` | same |
| sampling request | temp 1, top-p 0.95, top-k 20, min-p 0, seed 123, n_predict 128, n_probs 5 | same |

## The one difference that matters

| item | baseline | candidate |
|---|---|---|
| MoE experts on CPU | `--n-cpu-moe 35` — first 35 expert layers in RAM, the last 5 on the GPU | `--cpu-moe` — **all** expert layers on the CPU |

So the candidate is a different configuration in exactly the way that costs decode speed: five expert
layers that run on the GPU in the baseline run on the CPU here. Any decode comparison between them is a
comparison of two configurations, not a measurement of the port.

## Baseline numbers, with their own conditions

| condition | prefill tok/s | decode tok/s |
|---|---|---|
| cold 4K requests (1555–1663 / 35.70–36.49) | 1555–1663 | 35.70–36.49 |
| almost the whole window, 261632 in + 128 out | 969.85 | 21.25 |

The remembered 2900/29 and 2700/27 do not match either record; they may belong to a different model or
configuration. They are not used as a reference.

## Consequence

Candidate checkpoints (32K in, 128 out: 1640.2 / 23.5, 1621.3 / 22.7, 1647.0 / 23.75) are a **separate
series** under `--cpu-moe`. They are not a regression against the baseline until the same `--n-cpu-moe 35`
placement is used, and the optimisation stage must restore that flag first rather than guess.
