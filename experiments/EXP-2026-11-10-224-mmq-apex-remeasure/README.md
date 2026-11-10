# EXP224: MMQ Q8_1 measured at Apex on the current lineage — inside noise, not yet promotable

The shipped `libggml-cuda-pinned` of `exp220` already contains both the Goose MMQ Q8_1 kernels and the
renamed native, behind `GGML_GOOSE_CUDA_MMQ_Q8_1`. That made a proper Apex measurement possible with
**no rebuild**: arm A is the variable unset (native), arm B sets it to `1` (Goose). This is the first
time the component has been measured at Apex on a comparable build — EXP180 measured it on the EXP179
lineage, 40 commits earlier.

## Result

| pair | prefill | decode | tokens | probabilities |
|---|---:|---:|---|---|
| 1 | −0.27% | −2.62% | equal | equal |
| 2 | +0.84% | +3.38% | equal | equal |
| **mean** | **+0.28%** | **+0.37%** | equal | equal |

Raw decode: native `[26.28, 26.04]`, goose `[25.59, 26.92]`.
Raw prefill: native `[2551.7, 2549.0]`, goose `[2544.8, 2570.3]`.

Correctness is exact in all four runs: 128 token ids, text, and all 128 saved probabilities.

## What this does and does not establish

The **mean is inside the 2% rule in both directions being positive** — but the two pairs disagree by
6.0 points in decode, which is far larger than the effect. So the run shows the component is
*consistent with* costing nothing, not that it costs nothing.

Unlike EXP180's data, the swing here goes both ways (−2.62%, +3.38%) rather than being one-sided
(+0.15% forward, −4.97% reverse), which is what an unbiased noisy measurement looks like rather than
a warm-up artifact. That is an improvement in the evidence but not a resolution.

## Why not promoted on this

One pair alone showed −2.62%, above the 2% threshold. Promoting on a mean whose individual pairs
straddle the threshold would be exactly the reading this project already had to retract twice. The
defensible path is:

1. apply the EXP222 `ne2` fix to the in-chain copy — it is worth −3.69% at operator level on the same
   harness and takes the kernel from 400 to 164 SASS instructions against native's 157;
2. re-run this same protocol with more pairs so the spread closes;
3. then flip the switch direction. Today it is `strcmp(flag,"1")==0`, an opt-in that violates rule 10.

## Note on the harness honesty

The arm label in EXP180's own fixture says `arm=goose` when the variable is *unset*, while the
dispatcher runs **native** in that case. This measurement does not use that label — it is derived from
the environment it sets — but anyone reading the fixture's output should know the label is inverted
for the unset case.
