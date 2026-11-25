# EXP-2026-11-25-2021 — Inkling on the cumulative Bonsai binary

**Correctness and measured memory gates passed. Performance
equivalence remains inconclusive.** No production binary or
model profile changed. The candidate is the same EXP-2019 shared
HTTP executable tested on Apex in EXP-2020. The reference is
the frozen EXP-1969 executable and the exact accepted Inkling
first-request profile recorded in EXP-1972.

## Setup

RTX 4070, AD-IQ4_XS GGUF, 36864 context, Q4_0 K/V, batch/ubatch
4096/4096, 6720 MiB expert cache, original CPU masks and all
EXP-1969 flags. Four fresh processes are started in A–B–B–A
order. Score one request per process, with identical 32768 input
IDs and 512 outputs. Retain the original sampler settings
(temperature 1.0, seed 123, top-k 20, top-p 0.95 and no prompt cache).

An initial frozen control starts but times out at 240 s with zero
HTTP body bytes. The historical control had all 14,996,735,968 B
resident before its request. The timed-out control reports only
2,989,365,216 B resident. A sequential read of the unchanged local
GGUF restores file-page residency. Every scored arm verifies
full residency via `/props` before the first request. The prepared
frozen control reproduces the historical timing and output. There
was no candidate launch in the timed-out attempt. No settings
were relaxed and no unrelated process was stopped.

## Scored results

| Arm | Prefill, tok/s | Decode, tok/s | Client, s | Peak process VRAM, MiB |
| --- | ---: | ---: | ---: | ---: |
| A1 | 296.238 | 22.074 | 134.030 | 9810 |
| B1 | 296.278 | 22.017 | 134.076 | 9810 |
| B2 | 294.724 | 21.932 | 134.741 | 9810 |
| A2 | 296.137 | 21.972 | 134.185 | 9810 |
| Aggregate phase times A → B | 296.187 → 295.499 | 22.023 → 21.975 | 134.107 → 134.408 | 9810 → 9810 |

All 512 IDs and UTF-8 response text match the historical EXP-1969
packed-Q8 arm across all four runs. Final `/props` matches exactly,
including placement, KV types, limits and expert counters. All
arms actually activate the packed eight-row Q8 input. Prefill
reports 20,760 expert fills and 89,539,706,880 uploaded bytes
each. System swap-out is zero during all scored requests and
process VmSwap is zero after them. A1 startup adds nine swap-out
pages, recorded outside the measured interval.

Peak board VRAM is 11,716 MiB throughout. Peak RSS ranges
14,229,916–14,230,152 KiB for A and
14,227,760–14,229,664 KiB for B. Prefill changes +0.014%
and -0.477% in the individual pairs. The aggregate change is
-0.23% prefill and -0.22% decode. This block does not show
a material performance regression, but has too few samples
to establish strict speed equivalence at the observed scale.
See [raw timing and properties](data/results.json).

## Reproduction

Use the [recorded argv](data/argv.txt) and [env](data/env.txt)
with Bash mapfile. Substitute the corresponding executable and
an available loopback port. If the file is not already resident,
prepare it before launching either arm:

```bash
model=/home/end0/AI/LocalAI/Models/Inkling-2-Mini-AD-IQ4_XS.gguf
fincore -b -o RES,SIZE "$model"
dd if="$model" of=/dev/null bs=8M status=none
fincore -b -o RES,SIZE "$model"
```

Check `/props` for `model_resident_bytes == model_bytes`, then
POST `build/checkpoint-inkling-exp1969-before-bonsai/request.json`
to `/completion` once. Record per-PID GPU memory at 250 ms,
board telemetry at one second, and `VmHWM`, `VmSwap` and
`pswpout` before and after the score. Retain exact output
IDs, text SHA-256, final `/props` and the packed-Q8 log.
[Binary and request hashes](data/SHA256SUMS).
