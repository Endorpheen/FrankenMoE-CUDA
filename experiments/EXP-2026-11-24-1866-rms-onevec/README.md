# EXP-2026-11-24-1866 — Isolate one-vector RMSNorm on the accepted image

**INCONCLUSIVE for adoption.** Prefill improved in both model pairs.
The mean decode loss marginally exceeded the mean prefill gain, and the
decode direction changed with the pair. Baseline stays at EXP-1863.

This tests the one-vector specialization from EXP-1847 without replacing
the working CUDA image. Removing only that specialization from the
recovered Goose source reproduces both archived RMS headers byte-for-byte
with KeelWing. Extract the specialized entry as a separate SM89 cubin.
The driver uses it only for prefill at the existing `vectors == 1` branch.
The original entry handles every other launch. The specialized code
preserves reduction order and removes the dynamic vector staging array.

On the same CUDA 12.6 ptxas, the stack frame fell from 64 to 0 bytes and
register count from 38 to 34, with no spills. Synthetic tests are exact
across seven fixtures including weighted, postscaled, negative-zero and
padded rows. Median standalone kernel time improved by 10.7–33.7%.
[Probe](probe.py) and [raw results](probe.log).

## Full Apex 32K/512 on RTX 4070

Context 262144, cache 6290 MiB, batch and ubatch 4096, Q4_0 KV.
A-B-B-A on one executable, one warm-up and one measured request per
fresh server. A sets `GOOSE_RMS_ONEVEC=0`. B leaves it unset.

| Pair | Prefill control → candidate | Change | Decode control → candidate | Change |
| --- | ---: | ---: | ---: | ---: |
| A1 → B1 | 3353.92 → 3415.52 | +1.836% | 82.91 → 80.72 | −2.637% |
| B2 → A2 | 3357.55 → 3376.22 | +0.556% | 83.28 → 83.45 | +0.211% |
| Mean | 3355.74 → 3395.87 | +1.196% | 83.09 → 82.09 | −1.210% |

Mean prefill saved 115.07 ms and decode took 77.01 ms longer.
Mean client time went from 16.00662 to 15.96480 seconds.
The decode loss exceeds the prefill gain by 0.0142 percentage points
on these means. No direct decode change was made. Every reply matched
all 512 IDs and text. Post-request SLRU and other properties matched
byte-for-byte. Peak sampled VRAM was 8774 MiB in every arm, peak RSS
was 21490564–21492384 KiB, and process swap and system swap-out were zero.

## Reproduce

Apply the Goose fragment and OpenCL wrapper patches under [source](source)
to the frozen RMS source and glue. Use the historical KeelWing and Clang
18 flags from [EXP-1859](../EXP-2026-11-24-1859-flash-interior/README.md).
Build the experimental cubin with CUDA 12.6 ptxas and
`--entry goose_rms_reference_one -arch=sm_89`. Select
`goose_rms_reference` directly from the archived MMQ PTX for the probe
control. The probe takes `control.cubin` and `onevec.cubin`.

Embed the specialized cubin as `static const unsigned char rms_onevec_image[]`.
Apply the driver patch to EXP-1863 and relink only its driver object
using the same GCC 13 flags. The local candidate checkpoint contains
the image, source, archive, binary, argv and env. Verify its SHA-256
manifest before launch. Run IDs and all timings appear in [results.json](results.json).
