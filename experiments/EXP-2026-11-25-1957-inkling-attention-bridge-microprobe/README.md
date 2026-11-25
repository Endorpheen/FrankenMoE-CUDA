# EXP-2026-11-25-1957: Screen a resident Inkling attention bridge

**Verdict: partial numerical pass. The complete bridge is not
bit-exact.** Retain the exact-order convolution, residual and reduction
code as a building block. Keep the current runtime.

The EXP-1956 request spends 4.709 ms per decode step across 96 idle
activation round trips. EXP-1920 moved the convolution to CUDA but
retained the host depth mix and readback. This probe covers the
attention-to-MoE boundary in isolation. It evaluates one visit's
post-attention convolution and history update, optional residual,
RMS normalization, reversed-history dots and DepthMix on resident
test buffers with 2304 FP32 channels.

The first implementation serialized double reductions. It was slow
and emitted `sqrt.approx.f32`, which caused 83433 mismatched normalized
values in 288 cases. The recorded revision uses `sqrt.rn.f32`,
round-to-nearest division and disabled contraction. Parallel reductions
start from the same rounded FP32 products. For the finite inputs,
bound the difference from the ordered double reduction by more than
2304 rounding units of the absolute sum. Accept the parallel answer
only if the two ends of the interval round to the same FP32 bits. In
other cases sum products in the reference order. The separate
`2^60 + 1 - 2^60` cancellation test returns the ordered result zero.

## Results

The basic check includes 12 samples for both residual modes at every
history length 0–11. All 288 samples match bit-for-bit, including all
three updated convolution history rows. The stress block uses 96 samples
per case with new queries at four scales for 2304 total samples.

| Stress output | Mismatched FP32 values |
| --- | ---: |
| Convolution history | 0 |
| Residual | 0 |
| RMS-normalized output | 0 |
| Mixed output | 1515 |

Only three of the samples have mixed-output differences. Each failing
case includes a score delta where glibc 2.43 `expf` and double
exponential rounded to FP32 differ by one ULP. The largest absolute
error in these three output rows is 1.192093e-7. The mismatching
delta values and both results are in `exp-differences.json`. Matching
the product, sum and square-root order alone is insufficient for
bit-exactness with the production DepthMix softmax.

On RTX 4070 the ordinary-branch GPU chain took 17.2–22.8 µs per
visit across the basic history sweep. The CPU samples without residual
add took 20.6–50.6 µs, including the probe's three FFI calls and excluding
GPU transfers. The GPU numbers are event times for repeated three-launch
device chains with preallocated buffers; they exclude integration,
transfer and graph costs. At 48 visits the measured GPU cost would be
approximately 0.83–1.09 ms per step before those costs. This is not an
end-to-end speed result. The image uses 26–40 registers per kernel and
has no spill or stack frame.

## Reproduction

Build with `bash source/build.sh /absolute/new/scratch` from the
repository. It uses Clang 18, SM89 ptxas and a PIC build of the
existing Goose-generated numeric module. The scratch rebuild reproduced
the cubin and reference library byte for byte. Run the compiled fixture
with `python3 /absolute/new/scratch/check-bridge.py 12` and repeat
with `96` for the multi-scale stress block. No model is loaded.
See `basic.json`, `stress.json`, `inputs.sha256` and `compiler.ptxas`.

Before integrating the bridge, make the DepthMix exponential identical
to the reference on its production score domain and check the real
layer data, retained graph identity, history restoration and resets.
Then run the 8192/256 numerical gate and a performance A/B. No
production runtime, weights, launch profile or Apex checkpoint changed.
