# EXP-2026-11-24-1825 — Pinned plan source: engaged, no gain

Status: **REJECTED as a gain.** Baseline `37ac0f1d` unchanged (prefill 3122.4,
decode 78.225).

## The change

`launch_mmq` built its per-projection plan array with `malloc`, that is pageable
memory, so the synchronising `cuMemcpyHtoD` had to take the driver's pageable
staging path — measured at 2.6 ms per call, 1860 calls, 4.9 s of the 10.57 s
request. The candidate replaces that allocation with one reused pinned buffer
(`cuMemHostAlloc`, grown once, freed only at exit), keeping the bytes, the device
destination and the stream order identical; `free()` is guarded so the pinned
buffer is not released.

Engagement, from the server log:

```
Goose CUDA: plan buffer pinned, 1.0 MiB
```

## Measurement

Acceptance request 32768/512, warm-up plus measured, `GOOSE_DECODE_SPLIT=4` and
`GOOSE_DECODE_SPLIT_COUNT=1`, against the control block measured in the same
conditions (`runs/ctrl-065141`):

| arm | prompt_ms | prefill tok/s | decode tok/s |
| --- | ---: | ---: | ---: |
| control `37ac0f1d`, 32768/512 second request | 10570.0 | 3100.1 | — |
| pinned plan buffer `7a38bdb8…` | 10898.2 | 3006.7 | 75.733 |

The pinned variant is **3.1 % slower**, not faster.

## Why it does not help

The 2.6 ms per copy was not staging of our source buffer: with the source pinned
the copy still costs the same, and the small pinned write even costs a little
more than writing to cached heap memory. Combined with EXP-1810, EXP-1811,
EXP-1813 and EXP-1820 (four earlier attempts on the same segment), the evidence
is now consistent: the cost inside those calls is driver-internal work per
transfer, proportional to the number of transfers, and it is not removable by
changing where our bytes live or how the copy is submitted.

## Consequence

The one segment large enough to pay for the 1.13 s the 3500 target needs has now
been attacked five ways, all measured, all neutral or negative. The reserve is
therefore not demonstrated: on this configuration the measured movable time is
below the requirement, and the unexplained remainder is the driver-internal
processor time and the 1.75 s request-shape effect documented in EXP-1824.

## Artefacts

`runs/pin-065516` (engagement, response, props), `runs/ctrl-065141` (control).
