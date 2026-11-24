# EXP-2026-11-24-1869 — Full Apex test of aligned GDN V32 blocks

**REJECTED for the complete Apex profile.** This integrates the
no-spill variant from [EXP-1868](../EXP-2026-11-24-1868-gdn-exp-reuse/README.md).
It keeps the accepted CUDA images and embeds only the isolated SM89
cubin in a separate module. The driver uses it in prefill when tokens
are divisible by 16, the staged Q and K pointers are both 16-byte
aligned and the Q strides preserve that alignment. All other calls
use the original entries. Shared memory increases by 128 bytes per
selected CTA. The kernel log confirmed entry selection in both B arms.

## RTX 4070, Apex 32K/512

Context 262144, expert cache 6290 MiB, batch and ubatch 4096, Q4_0
KV, decode split 4/count 1. One fresh server, warm-up and scored
request per arm, A-B-B-A on the same executable. Controls set
`GOOSE_GDN_V32_FULL=0`. B leaves it unset.

| Pair | Prefill control → candidate | Change | Decode control → candidate | Change |
| --- | ---: | ---: | ---: | ---: |
| A1 → B1 | 3329.67 → 3379.02 | +1.48% | 83.25 → 81.78 | −1.77% |
| B2 → A2 | 3359.59 → 3401.73 | +1.25% | 83.64 → 81.44 | −2.62% |
| Mean | 3344.63 → 3390.37 | +1.37% | 83.45 → 81.61 | −2.20% |

Mean prefill saved 132.28 ms while decode took 137.50 ms longer.
Mean client time was 16.00935 versus 16.01732 seconds. The decode
loss exceeds the prefill gain by 0.83 percentage points. The new
entry is prefill-only. The paired measurements establish a profile
regression on decode, but they do not identify the cause of it.

All eight replies matched all 512 output IDs and text. SLRU
counters and the rest of the post-request properties were exact.
Sampled peak VRAM was 8774 MiB across all arms. Peak RSS ranged
from 21491448 to 21492304 KiB. Process swap and system swap-out were zero.

## Reproduction

Build EXP-1868's local-load SM89 cubin. Embed it in
`gdn-full-image.h` as `static const unsigned char gdn_full_image[]`.
Apply the [driver patch](source/cuda-driver.patch) to EXP-1863.
Recompile its driver object and relink with the frozen archive,
headers and GCC 13 flags from EXP-1863. The rejected local checkpoint
contains binary, source, image and exact argv and env.
See [SHA256SUMS](SHA256SUMS) and [raw run IDs and timings](results.json).

EXP-1864 and EXP-1869 show a similar full-profile decode decrease
after a change restricted to prefill. Before attributing effects,
compare per-phase clocks and traces as well as actual launch and
wait times under the same A/B protocol. Keep EXP-1863 as the
research prefill baseline.
