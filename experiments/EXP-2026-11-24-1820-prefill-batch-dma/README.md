# EXP-2026-11-23-1820 — Batch DMA for prefill: engaged, no measurable gain

Status: **REJECTED as a gain (noise).** Baseline `37ac0f1d` unchanged
(prefill 3122.4, decode 78.225).

## What was enabled

Decode already reported `batch DMA for pinned decode experts selected`, while the
prefill staging submitted its expert copies one by one: 80 548 asynchronous
copies per request, sorted into runs of about 84 per projection. Two limits kept
prefill out of the batch door:

1. `submit_expert_stage_range` capped the batch at 24 copies (`max_batch = 24`),
   below the prefill wave size;
2. `fg_cuda_expert_batch` called `submit_expert_stage(false)`, disabling the door
   for the projection path outright.

Both were changed: the cap is now `GOOSE_BATCH_DMA_MAX` (default 256) and the
projection path passes "enabled when the batch door exists".

## Engagement, verified

```
Goose CUDA: batch DMA selected, 160 copies in one call (cap 256)
```

160 job descriptors now travel in one `cuMemcpyBatchAsync_v2` call instead of 160
separate submissions.

## Measurement

32768 prompt with 512 predicted tokens, warm-up plus measured request,
alternating A B B A, `GOOSE_DECODE_SPLIT=4` and `GOOSE_DECODE_SPLIT_COUNT=1`:

| arm | prefill tok/s | decode tok/s | prompt_ms | exact | VRAM | swap |
| --- | ---: | ---: | ---: | --- | ---: | ---: |
| bd-A1 | 3127.7 | 77.998 | 10476.8 | yes | 8774 | 0 |
| bd-A2 | 3114.6 | 78.222 | 10520.6 | yes | 8774 | 0 |
| bd-B1 | 3119.4 | 78.472 | 10504.5 | yes | 8774 | 0 |
| bd-B2 | 3131.0 | 78.638 | 10465.5 | yes | 8774 | 0 |

Medians: prefill 3121.2 -> 3125.2 (**+0.13 %**), decode 78.110 -> 78.555
(inside the spread), `prompt_ms` -0.13 %. All four runs reproduce the 512 IDs and
the text `f8664331bd30…`, peak VRAM 8774 MiB in both arms, VmSwap 0.

The diagnostic protocol (32768/1) showed 2699.4 against 2661-2684, that is +0.6
to +1.4 %, which does not survive the acceptance request.

## Why batching does not help

`cuMemcpyBatchAsync_v2` removes *submission* calls on our side, not the driver's
per-copy work: it still issues the same 160 transfers internally, and EXP-1818
already showed the cost sits in the driver's own threads, proportional to the
number of transfers rather than the number of submissions. The only way to cut
that work is fewer transfers, which the scattered miss pattern does not allow
without tripling the bytes.

## Artefacts

`runs/batch-*-256`, `runs/batch-*-base`, `runs/batch2-021131` (engagement and the
diagnostic reading), `runs/bd-A1/A2/B1/B2-20261124-*` (the A/B).
