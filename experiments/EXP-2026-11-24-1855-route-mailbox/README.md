# EXP-2026-11-24-1855 — Route mailbox research checkpoint

Accepted for cumulative Apex research. The deployed runtime and the previous
`37ac0f1d` release checkpoint remain available.

The compact Top-K producer preserves the original GPU ID buffer and publishes
the same eight IDs to aligned mapped host memory with a system-scope release.
The CPU reads the matching sequence with acquire ordering after submitting
the queued CUDA graph region. An ineligible activation or a timeout takes
the original synchronous path. Expert routing, SLRU ordering and copy policy
are unchanged. `GOOSE_ROUTE_MAILBOX=0` disables the feature.

## Apex 32K/512 on RTX 4070

Context 262144, batch/ubatch 4096, Q4_0 KV, 6290 MiB expert cache,
decode split 4/count 1. One warm-up plus one measured request per arm.

| Arm | Prefill tok/s | Decode tok/s | Decode ms | Full seconds |
| --- | ---: | ---: | ---: | ---: |
| A1 | 3232.748 | 81.432 | 6275.183 | 16.502089 |
| B1 | 3251.700 | 83.070 | 6151.433 | 16.314580 |
| A2 | 3251.501 | 81.742 | 6251.415 | See results.json |
| B2 | 3246.624 | 82.927 | 6162.014 | 16.345479 |
| Final enabled by default | 3247.200 | 83.075 | 6151.045 | 16.329755 |

The same A/B binary was used in A1–B1–B2–A2 order. Mean decode increased
from 81.587 to 82.999 tok/s, or 1.73%. Mean decode duration fell by
106.6 ms. Both pairs retained exact results and the same 8774 MiB peak
sampled VRAM. VmHWM did not increase in either pair. Swap-out was zero.

All twelve saved responses matched the archived 512 IDs and text, and
the final properties including SLRU counters matched byte for byte.
The separately instrumented executable checked at least 39845 mailbox
payloads against the device buffer without mismatch and forced at least
155 fallbacks. Its runtime is diagnostic only.

## Reproducing the mailbox image

The compact ranking in `source/route.goose` is unchanged from EXP-1782.
The pinned emitted fragment is `source/device-goose.h`. The modification
publishes the original eight IDs in `source/device.cl`. Build the image from
this directory with Clang 18 and CUDA 12.6 ptxas.

```bash
clang-18 -target nvptx64-nvidia-cuda -nocudalib -O2 -x cl -cl-std=CL1.2 \
  -Xclang -target-cpu -Xclang sm_80 -S source/device.cl -o mail.ptx
ptxas -arch=sm_89 mail.ptx -o mail.cubin
```

The cubin SHA-256 is
`560e986ac3937ffd7086cd2adef09102d180c27461527eee3a681ce32346db6a`.
Its byte array is pinned in the Linux Apex adapter. The default-on
research executable hash is
`4280695005072ce2791717ccde434a7401913766c39b0cbf775fd18538d73b74`.
These benchmarks cover the standalone research host on Apex only.
