# EXP-2026-11-25-1958: Match the Inkling DepthMix exponential on SM89

**Verdict: exhaustive numerical pass on the observed x86 FMA libm.**
The experimental bridge remains outside the serving graph.

EXP-1957 matched the convolution, residual, history and normalization,
but its double GPU exponential rounded to FP32 disagreed with the
CPU's `expf`. DepthMix subtracts the maximum score before exponentiation,
so the ordinary input domain is nonpositive. A finite score difference
can also underflow to negative infinity.

The candidate uses Arm optimized-routines' 32-entry exponential range
reduction and polynomial, with the explicit FP64 FMA placement observed
in the active glibc 2.43 x86 implementation. This matches the CPU
approximation including its rounding behavior. The original Arm
license and attribution accompany the CUDA helper. CPU source and
the other bridge operations were preserved.

## Checks

- RTX 4070, SM89, round-to-nearest. A CUDA kernel enumerated every
  nonpositive non-NaN FP32 encoding from negative zero to negative
  infinity. The native C audit compared the returned bits to the
  local `libm.so.6` `expf` for **2139095041 inputs**, with zero
  differences. Positive zero was checked against the same device
  result as negative zero. This run took 8.801 s.
- Repeating EXP-1957's 2304-case DepthMix stress block produced zero
  differing FP32 values for the full bridge, against 1515 before the
  change. Both optional-residual modes and all 12 history lengths
  were included. The cancellation fallback check also passed.
- Resident three-kernel GPU chain medians were 17.08–20.07 µs over
  the stress shapes. The CUDA executable uses 26–40 registers per
  bridge kernel and no spill or stack frame. This measurement excludes
  any transfers and graph handoff.

The exhaustive claim is limited to the recorded host libm, the
GPU image and inputs within the tested domain. Other host math
implementations and NaN propagation are outside this measurement.
The model integration must keep an appropriate reference fallback
for unsupported hosts and check actual generated IDs, state
fingerprints, loop transitions, history restoration and resets.
No model, server, Apex path or production build changed.

## Reproduction

From the repository root, build with
`bash source/build.sh /absolute/new/scratch`, using the saved
EXP-1957 CPU reference, Clang 18 and ptxas for SM89. A new scratch
build reproduced the reference library and cubin byte for byte.
Then run:

```bash
/absolute/new/scratch/audit-expf /absolute/new/scratch/attention-bridge.cubin
python3 /absolute/new/scratch/check-bridge.py 96
```

See `exp-audit.json`, `stress.json`, `inputs.sha256` and
`compiler.ptxas`. The runtime proof still needs a fused graph
boundary in which Attention output stays device-resident until MoE.

Method provenance: [Arm expf](https://github.com/ARM-software/optimized-routines/blob/master/math/expf.c),
[Arm table](https://github.com/ARM-software/optimized-routines/blob/master/math/exp2f_data.c),
[glibc expf](https://github.com/bminor/glibc/blob/master/sysdeps/ieee754/flt-32/e_expf.c).
