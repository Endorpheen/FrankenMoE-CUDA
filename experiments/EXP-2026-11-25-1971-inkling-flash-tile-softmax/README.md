# EXP-2026-11-25-1971: Parallelize the Inkling full-prefill Flash softmax

**Verdict: Inkling-only improvement confirmed. Reject promotion
to the common baseline because the warm Apex decode gate failed.**

Start from EXP-1969 packed Q8. The experimental full-attention
Q4_0 kernel skips wholly masked tiles for the ordered native
position arrays and fuses zero-relative-distance correction
when every key is outside the relative table. Where the relative
correction is needed, load four FP32 coefficients at a time
and preserve the ordered FP64 sum. Expand two Q4 values together
with exact FP16 integer reconstruction and `f16x2` multiplication.
Spread the exponentials across the CTA while retaining the
original per-row max and FP32 probability addition order.
The kernel does not select sliding attention, decode or Apex.

The seed fixtures cover six partial shapes including sliding,
large full ranges, and partial query and key tiles. Every
tested CUDA variant matches the original partials and metadata
bit for bit. The chosen combination takes 1.093 vs 1.583 ms
at 4096 keys and 5.691 vs 7.060 ms at 32768 keys, each with
128 queries. The common sliding shape does not improve.
A 1024/16 diagnostic matches all 1055 state fingerprints,
output IDs, text and properties. The short control was affected
by storage reads and is not a speed result.

## Inkling first request, 32K/512

A–B–B–A, fresh process and first request per arm. Preserve the
accepted EXP-1969 profile with context 36864, Q4_0 K/V,
batch/ubatch 4096 and 6720 MiB experts. Toggle only
`I2_FLASH_TILE_SOFTMAX` in the same binary.

| Arm | Prefill tok/s | Decode tok/s | Prefill s | Client s |
| --- | ---: | ---: | ---: | ---: |
| A1 | 294.356 | 22.024 | 111.321 | 134.840 |
| B1 | 298.199 | 21.990 | 109.887 | 133.439 |
| B2 | 298.481 | 21.905 | 109.782 | 133.418 |
| A2 | 291.893 | 21.920 | 112.260 | 135.899 |

Mean prefill rises 293.124 → 298.340 (+1.78%, 1.956 s saved).
Decode is 21.972 → 21.948 (-0.11%). All 512 IDs, text and final
properties including expert counters match. Mean client response
improves from 135.369 to 133.428 s. Peak board memory increases by
1 MiB (11727 → 11728), and maximum process RSS by 32 KiB.
No process swap or system swap-out.

## Apex second request after full 32K/512 warm-up

Separate A–B–B–A against the EXP-1969 binary on the frozen pinned
profile, including context 262144 and the 6290 MiB expert limit.
All arms include the candidate flag. The Inkling Flash dispatch
was absent from all Apex logs. The recorded second requests
match all 512 IDs, text and final properties with no process swap
or request-interval system swap-out.

| Arm | Prefill tok/s | Decode tok/s | Client s |
| --- | ---: | ---: | ---: |
| A1 | 3424.759 | 80.082 | 16.040 |
| B1 | 3423.871 | 79.238 | 16.107 |
| B2 | 3409.650 | 79.849 | 16.102 |
| A2 | 3422.822 | 80.304 | 16.028 |

Mean prefill is 3423.791 → 3416.761 (-0.21%) and decode
80.193 → 79.544 (-0.81%). Decode slows in both orders. No
material peak RAM or VRAM change was observed. The cause of this
linked-binary difference is unknown. Among CUDA driver object
machine-code sections, 176 match and only Inkling `nk_execute`
differs. This is an object-level comparison. It does not rule
out changes in linked offsets or other timing factors.

Keep EXP-1969 as the accepted Inkling/Apex baseline. Reuse the
device result when the Inkling-specific kernel can be isolated
and the unchanged Apex path can be requalified.

## Reproduction

Build with `bash source/build.sh /absolute/new/scratch`; set
`ASSERT_SOURCE_DIR` to the original absolute
`build/inkling-apex-gate/flash-tiles-1971` directory for
byte-identical assertion paths. The normalized rebuild matches
the executable. Run `python3 /absolute/new/scratch/bench-prefill.py`
for the device screen, or use the 32K/512 fixtures and recorded
argv/env from `results.json`. Raw evidence lives under
`build/inkling-apex-gate/flash-tiles-1971`. No production binary
or profile changed.
