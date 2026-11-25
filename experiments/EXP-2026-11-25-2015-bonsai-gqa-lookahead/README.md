# EXP-2026-11-25-2015 — Look ahead across Q4 decode rows

**Experimental. No runtime promotion.** Keep EXP-2014 as the accepted
Bonsai HTTP baseline. This experiment follows the measured Q4 Attention
gap to PrismML. It changes only query grouping and row scheduling for
the existing 256-wide 6:1, one-token Q4 specialization.

The kernels compute QK dots for two or four neighboring rows before
updating softmax. A separate variant also unpacks V ahead of use.
Each QK dot keeps its original F32 accumulation and warp reduction.
Softmax mass and value sums update in the original row order. Tile
partials use the existing layout and the original merge kernel.
Register counts are 64–96 with no spills and no extra global
workspace. The 96-register candidate has four-row look-ahead for
three query heads. The long candidate has two-row look-ahead for
K and V with 80 registers.

## Component evidence

Seven Q4 cache lengths, including incomplete final tiles. All tested
variants reproduce all partials and merged output bytes of the frozen
kernel. Timing includes the same merge. The following times compare the
already accepted EXP-2005 path and the selected look-ahead:

| Visible KV rows | Active control | Best selected look-ahead | One buffer A/B, ms | 16 buffers A/B, ms |
| ---: | --- | --- | ---: | ---: |
| 4097 | Two query heads | Three heads, four QK rows | 0.149 → 0.136 | 0.260 → 0.236 |
| 8192 | Two query heads | Three heads, four QK rows | 0.179 → 0.165 | 0.281 → 0.256 |
| 16384 | Two query heads | Three heads, four QK rows | 0.291 → 0.230 | 0.330 → 0.298 |
| 32768 | Three query heads | Three heads, two K/V rows | 0.526 → 0.454 | 0.551 → 0.461 |
| 33279 | Three query heads | Three heads, two K/V rows | 0.526 → 0.457 | 0.533 → 0.445 |

The rotation probe uses 16 disjoint pairs of equal Q4 payloads. It
tests repeated use of the same cache addresses as an explanation
for the favorable component result. A large slice of the gain
survives this change. The probe still does not interleave other
Bonsai kernels or reproduce the full model's cache and dispatch state.

## HTTP benchmark and verdict

All arms use the cumulative EXP-2014 runtime, except for toggling
look-ahead on the same binary and CUDA image. RTX 4070, PQ2_0,
context 36864, batch/ubatch 4096/1024, Q4_0 K/V, greedy,
EOG masked, MTP and prefix reuse off. The experimental Bonsai path
chooses four QK rows from 256 to 32767 visible keys and two K/V
rows at or above 32768. The other attention paths are unchanged.

| Workload | Prefill A → B, tok/s | Decode A → B, tok/s | Decode |
| --- | ---: | ---: | ---: |
| 4097/64, two balanced pairs | 804.086 → 826.865 | 35.546 → 36.897 | +3.80% |
| 32768/512, B–A | 612.179 → 607.390 | 31.456 → 30.462 | -3.16% |
| 32768/512, A–B | 636.212 → 624.386 | 32.143 → 32.795 | +2.03% |

All 64 short IDs and text agree across short arms. Across all four
long arms all 512 IDs and the same UTF-8 text SHA-256 match PrismML.
Process VRAM is 8932 MiB in all arms. Long VmHWM ranges from
7,456,828 to 7,457,772 KiB and no swap-out occurs.
During the final 14 high-utilization board samples all long arms
are at 2820 MHz median SM and 10251 MHz median memory. These samples
do not isolate the cause of the end-to-end discrepancy. The final
query of prefill can use the changed single-token attention kernel,
but the measured prefill swings are too large to assign to it.
No separate trace was taken and no end-to-end gain is established
at the target 32K/512 workload. The full request loses 0.95 s and
0.66 s in the two long pairs. The archived build keeps look-ahead
off by default. The runtime source changes only the experimental
Bonsai path. No Apex or Inkling portability result is claimed.

## Reproduction

```bash
ex=experiments/EXP-2026-11-25-2015-bonsai-gqa-lookahead
dev=build/bonsai-gqa-lookahead-repro
out=build/bonsai-gqa-lookahead-http-repro
frozen=build/bonsai-mmq-k64-2000-default/frozen-goose.ptx
bash "$ex/source/build.sh" "$dev"
OPENBLAS_NUM_THREADS=1 python3 "$ex/source/check.py" build/bonsai-integration \
    build/bonsai-integration/inventory.json "$frozen" "$dev/lookahead.cubin" 16
bash "$ex/source/build-runtime.sh" "$out" "$dev"
```

Use EXP-2014 server options and the exact EXP-1977 request.
Set `BONSAI_CUDA_IMAGE="$out/lookahead.cubin"`. Use
`BONSAI_GQA_LOOKAHEAD=0` for A and `BONSAI_GQA_LOOKAHEAD=1` for B.
The scored executable enabled B by default. Both binary hashes
are recorded. See [request data](data/results.json), [component data](data/component.json),
[rotating-buffer data](data/component-rotating.json) and [hashes](data/SHA256SUMS).
