# EXP-2026-11-24-1877 — Batch width-128 RMSNorm and retain the input vector

**PROVEN and accepted as the research prefill checkpoint.** This
retargets the EXP-1866 one-vector specialization to EXP-1875.
It keeps the existing graph nodes, allocator, tensor lifetimes,
reference fallback and single-row decode dispatch. The generic
one-vector candidate expresses one input load and retains the
FP32 values throughout the reference reduction. The width-128
entry instead runs four independent rows per 128-thread CTA.
Each row still uses its original 32-lane shuffle order, without
a CTA barrier. SM89 only, prefill only, and the reference driver's
existing `vectors == 1` checks still apply.

The standalone probe matched all FP32 words across eight fixtures,
including signed zero, weighted and postscaled paths, padded
rows and a partial four-row group. Keeping the vector explicitly
did not materially change time versus EXP-1866. At width 128 /
65536 weighted rows, the four-row median was 0.18990 ms versus
0.21924 ms for EXP-1866 and 0.24706 ms for the frozen control.
The grouped entry used 38 registers and no spill or CTA barriers.
[Raw GPU probe](probe.log).

## Full Apex 32K/512 on RTX 4070

Context 262144, expert cache 6290 MiB, batch and ubatch 4096,
Q4_0 KV and decode split 4/count 1. A-B-B-A, same executable,
one warm-up and one scored request per fresh process. A sets
`GOOSE_RMS_BATCHED=0`; B leaves it unset. Both arms keep all
360 EXP-1875 MoE epilogues per request. The runtime log confirms
that the new module was loaded in B; per-entry launch counts
were not collected in this unprofiled block.

| Arm | Prefill tok/s | Decode tok/s | Client s | Sampled peak VRAM MiB | Peak RSS KiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| A1 | 3455.31 | 80.76 | 15.901 | 8774 | 21492120 |
| B1 | 3461.76 | 82.48 | 15.748 | 8774 | 21492224 |
| B2 | 3469.58 | 81.70 | 15.791 | 8774 | 21493860 |
| A2 | 3409.55 | 82.98 | 15.856 | 8774 | 21491784 |

Mean prefill changed from 3432.43 to 3465.67 tok/s (+0.97%,
91.99 ms saved). The two pairs gained 0.19% and 1.76% prefill.
The latter lost 1.54% decode, less than its prefill gain.
Mean measured decode was 81.87 and 82.09 tok/s. This is not an
independent decode improvement claim. Mean client time fell by
109 ms and was lower in both pairs. All eight replies matched
all 512 IDs and exact text. The post-request properties including
SLRU counters matched byte-for-byte. System and process swap-out
were zero. Peak VRAM was identical; maximum RSS in the candidate
group was 1740 KiB higher than the maximum among controls.

## Reproduce

Apply the fragment and wrapper patches under [source](source) to
the EXP-1866 candidate's frozen device sources. Re-emit the RMS
fragment using KeelWing and Clang 18 as in EXP-1866, then extract
both entries with CUDA 12.6 ptxas for SM89. Use the exact
EXP-1866 control and onevec cubins in the isolated probe.

```sh
ptxas -arch=sm_89 -v --entry goose_rms_reference_one,goose_rms_reference_one_warp4 cached.ptx -o cached.cubin
python3 probe.py
```

For the full runtime, apply [cuda-driver.c.patch](source/cuda-driver.c.patch)
to EXP-1875. Embed the cubin as `rms_onevec_image` in
`rms-onevec-image.h`, rebuild only the driver with the EXP-1863
GCC 13 flags and relink the EXP-1875 archive. The local
`codex-20261124-rms-batched-accepted` folder contains the
checkpoint binary, launch files, source and exact archive.
Verify [SHA256SUMS](SHA256SUMS). See [results.json](results.json)
for exact run IDs and measurements. At its measured prefill mean,
about 93 ms remain to 3500. The separate 100 decode target remains open.
