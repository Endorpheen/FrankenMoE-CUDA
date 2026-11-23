# EXP-2026-11-23-1815 — Prefill switch sweep: the accepted MMQ set is the fastest of its variants

Status: **INVESTIGATION.** Baseline `37ac0f1d` unchanged.

## Method

`tools/prefill-sweep.sh`: fresh server per configuration, one 32768-token prompt,
one predicted token, `prompt_ms` and the staging counters printed. All
configurations carry `GOOSE_DECODE_SPLIT=4` and `GOOSE_DECODE_SPLIT_COUNT=1`.
Measured on the baseline binary `checkpoints/cand-prefill-jobpack-split`.

## MMQ switch variants (turning an accepted switch off)

| configuration | prompt_ms | tok/s | fills | upload |
| --- | ---: | ---: | ---: | ---: |
| accepted set (base) | 12342.1 | 2655.0 | 49 809 | 94.8 GB |
| `GOOSE_MMQ_Q4_ROUTED_M128=0` | 13068.8 | 2507.3 | 49 809 | 94.8 GB |
| `GOOSE_MMQ_Q4_ROUTED_DOWN_M128=0` | 13015.0 | 2517.7 | 49 809 | 94.8 GB |
| `GOOSE_MMQ_Q6_HALF_K=0` | 12773.2 | 2565.4 | 49 809 | 94.8 GB |

Every variant is slower: the accepted configuration is already the best of these
four, so the MMQ switch axis is closed. The staging counters are identical in
all arms, which also confirms the switch only changes the kernel geometry, not
the miss pattern.

For scale: the staging counters of a single request (49 809 fills, 94.8 GB
uploaded, one warm and one cold phase in one server) are the volume an eviction
policy would have to reduce.

## Policy knobs found in the driver, queued for measurement

```
GOOSE_CUDA_PREFILL_TEMP_MIB     temporary prefill workspace (default 210 MiB, two buffers)
GOOSE_CUDA_DISABLE_CACHE_BALANCE  the "expert cache 1904/1531 rebalance selected" split
GOOSE_CUDA_DISABLE_ONE_TOUCH_SLRU one-touch SLRU against its alternative
```

The first measurement attempts of the last two failed to start: the card was
short of VRAM (`requested 6290 MiB, available 6362324992 bytes`), a 222 MiB gap
created by the desktop's GPU applications, not by the variant. A queue waits for
a VRAM window and then measures them (`/tmp/opencode/apex-pfqueue.sh`).

## Artefacts

`runs/sw-base-*`, `runs/sw-Q4_ROUTED_M128=0-*` (the tag carries the variant),
`runs/sw-Q4_ROUTED_DOWN_M128=0-*`, `runs/sw-Q6_HALF_K=0-*`; tool
`tools/prefill-sweep.sh`.
