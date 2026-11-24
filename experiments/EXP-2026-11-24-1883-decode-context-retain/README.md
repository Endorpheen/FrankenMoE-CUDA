# EXP-2026-11-24-1883 — Retain the CUDA context through a decode graph

**REJECTED for the full decode profile.** EXP-1880 remains the
accepted checkpoint. On planned decode only, push the CUDA context
once on the current thread at graph entry and pop it at graph exit.
The existing mutex still serializes each adapter operation. Graph
capture, kernels, queue order, copies, SLRU and prefill are unchanged.
If initial retention fails the driver uses the original per-call path.

Each candidate request logged 511 retained graph contexts and
564655 avoided push/pop pairs. A separate primary-context probe pinned
with CPU mask 0f0f measured six blocks of 200000 push/pop pairs.
The median was about 24 ns per pair, equivalent to about 13.5 ms
of CPU call time if that isolated cost transfers. This is not a
bound on the live graph or an estimate of critical-path savings.

## Full Apex 32K/512

RTX 4070, context 262144, expert cache 6290 MiB, batch and ubatch
4096, Q4_0 KV. One warm-up and one measured request per fresh server.
Same binary throughout. A sets `GOOSE_DECODE_CONTEXT_RETAIN=0`;
B leaves it unset.

| Arm | Decode tok/s | Prefill tok/s | Sampled peak VRAM MiB | System swap-out pages |
| --- | ---: | ---: | ---: | ---: |
| A1 | 82.34 | 3516.52 | 8774 | 0 |
| B1 | 81.62 | 3533.72 | 8774 | 0 |
| B2 | 82.36 | 3502.04 | 8774 | 0 |
| A2 excluded | 83.52 | 3473.43 | 8774 | 3281 |
| A2 retry | 83.22 | 3475.53 | 8774 | 0 |

Both clean pairs lost decode. The mean was 82.78 → 81.99 tok/s,
a decrease of about 0.95%. The prefill mean changed by about +0.63%,
less than the decode loss. Peak RSS remained in the range
21492664–21494264 KiB. All replies and SLRU properties matched
byte-for-byte with the accepted reference. Process swap was zero.
The A2 retry replaces only the arm with system swap-out.

## Reproduce

Apply [the driver patch](source/cuda-driver.patch) to the frozen
EXP-1880 driver. Rebuild only its object and relink its archive
with the same GCC 13 flags as EXP-1863, using all original image
headers and the exact argv and env. Verify [SHA256SUMS](SHA256SUMS).
Run the control/candidate toggle as above. For the isolated API probe:

```sh
gcc-13 -O2 -Wall -Wextra -Werror -I"$cuda/include" probe-context.c -Wl,-l:libcuda.so.1 -o probe-context
taskset 0f0f ./probe-context
```

[Full measurements](results.json) and [API samples](context-probe.log)
include the raw run identifiers and diagnostic counters.
