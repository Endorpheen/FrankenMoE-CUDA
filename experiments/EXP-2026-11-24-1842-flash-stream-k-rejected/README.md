# EXP-2026-11-24-1842 — Flash Stream-K in prefill: slower and not bit-exact

Status: **REJECTED.** Baseline `37ac0f1d` unchanged (prefill 3122.4, decode 78.225).

## What was tried

The largest kernel in the prefill window is `goose_flash_causal_mma` (482 ms, 12.9 %
of kernel time). Its Goose entry has a Stream-K path that partitions the flattened KV
tiles among workers (`fg_flash_stream_mma`), shipped off by default
(`GOOSE_FLASH_STREAM_K=0`). The kernel itself is a precompiled image, so the only
lever available without rebuilding it is that partition, which is the "fewer passes"
idea in its shipped form.

## Measurement

Acceptance request, warm-up plus two measured, one flag apart:

| arm | prefill tok/s | decode tok/s | text |
| --- | ---: | ---: | --- |
| `GOOSE_FLASH_STREAM_K=1` (`runs/sk-083357`) | 3054.9 / 3048.9 | 75.328 / 75.219 | `47042c67ae58` |
| baseline | 3122.4 | 78.225 | `f8664331bd30` |

VmHWM 21 494 376 kB, swap 0, VRAM peak 10 802 MiB.

## Why it is rejected twice over

1. **Slower**: −2.2 % prefill and −3.8 % decode.
2. **Not bit-exact**: the text differs (`47042c67ae58` against `f8664331bd30`),
   because splitting the KV range changes the accumulation order. Even if it were
   faster it could not be adopted without changing the accepted numerics.

The cause is the same dependency noted in EXP-1839: at 4096-token ubatches the causal
tiles already saturate the multiprocessors, so partitioning adds fixup passes instead
of filling idle SMs.

## Consequence

The remaining way to speed that kernel is inside the kernel image — a rebuild — which
is the subject of the next experiment (local-frame removal), not a launch parameter.

## Artefacts

- `runs/sk-083357` — request, response, VRAM before/after, VmHWM, props.
