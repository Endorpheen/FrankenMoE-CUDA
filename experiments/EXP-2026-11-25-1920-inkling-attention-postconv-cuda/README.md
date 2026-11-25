# EXP-2026-11-25-1920: Keep Inkling attention convolution history on CUDA

**Verdict: INCONCLUSIVE for throughput.** The CUDA route is exact on
the experiment fixture. Carry the implementation as a building block
for a resident attention-to-MoE path. EXP-1918 remains the timing
baseline.

## Mechanism

The attention output projection writes into a reusable GPU workspace.
The CUDA convolution writes into the existing graph output activation.
It uses the same four additions and scale as the reference and
disables contraction. Each visit owns a device buffer containing its
coefficients and three FP32 history rows. The cache carries history
between prefill microbatches and checks the expected position. On the
first subsequent host convolution, it restores those three rows before
continuing the existing decode path. A reset clears the validity tags
and the existing host reset clears its history. The comparison uses
`I2_ATTENTION_POSTCONV=0` to select the CPU batch from EXP-1918 in the
same executable. The experimental path is limited to the Inkling
attention graph with multiple inputs.

This step keeps both full activation readbacks while depth mixing
continues on the host. The new workspace holds up to 9 MiB for
ubatch 1024, and the 48 visit buffers can hold about 3 MiB. The handoff
copies up to 1.27 MiB of history per request over the 48 visits.
Retaining the next operation on device is needed to remove the large
readback and the subsequent upload.

## A/B measurements

RTX 4070, EXP-1917 2048/256 input and output budget, context 4096,
batch 2048, ubatch 1024 and Q4_0 K/V. The same executable ran A–B–B–A,
with the first and repeat request on each fresh server. Both arms
recomputed the prompt and enabled internal state hashing.

| Paired mean | CPU batch | CUDA batch | Delta |
| --- | ---: | ---: | ---: |
| First prefill, tok/s | 184.33 | 186.00 | +0.91% |
| Repeat prefill, tok/s | 194.03 | 196.01 | +1.02% |
| First decode, tok/s | 13.582 | 13.548 | -0.24% |
| Repeat decode, tok/s | 13.628 | 13.516 | -0.82% |
| Repeat prefill, ms | 10554.9 | 10448.3 | -106.6 |
| Repeat decode, ms | 18711.7 | 18866.7 | +155.0 |
| Repeat client request, s | 29.482 | 29.528 | +0.045 |

All 5118 state fingerprints per launch, both sets of 256 output IDs
and text, and prefill expert fills and payload bytes matched. Repeat
prefill saved 82–131 ms. Repeat request time was higher in both pairs,
with an observed repeat decode increase of 133–177 ms. The history
handoff contributes work in the first decode step. These timings do
not isolate its cost from the rest of decode. Device peak was 9977
versus 9967 MiB. Peak RSS differed by 248 KiB across arm maxima.
Process swap and system swap-out were zero. The kernel uses 30
registers on SM89 and has no stack frame or spills.

Apex's frozen checkpoint checksum manifest passed. No Apex benchmark
or Inkling 32K/512 regression gate ran. The production runtime and
launch profiles were not changed.

## Reproduction

Use an isolated sibling directory under
`build/inkling-exp-baseline/device-attention`. Copy the EXP-1916 CUDA
driver and headers, EXP-1918 Inkling host sources and numeric object,
and `graph-execute.c` from the local `inkling-attention-graph` build.
Apply the four patches with `patch -p1`. The dispatcher must be
recompiled against the extended attention metadata before linking.
Use the build script in `source` with Clang 18, GCC 13 and ptxas for
SM89. It reuses the EXP-1918 host archive as its link baseline.
See `inputs.sha256`, `ptxas.log` and `results.json`.
