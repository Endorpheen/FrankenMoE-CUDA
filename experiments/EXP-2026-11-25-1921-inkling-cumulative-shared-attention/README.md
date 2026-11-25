# EXP-2026-11-25-1921: Restore shared Inkling attention inputs in the cumulative build

**Verdict: ACCEPTED for the isolated Inkling research checkpoint.**

The EXP-1918 launch still set `I2_SHARED_ATTENTION_INPUTS=1`, but its
host source did not read that option after the worker-team path was
rebuilt. The earlier shared-input implementation is preserved under
the local `inkling-shared-attention-inputs` build. Restore that graph
construction on top of EXP-1918. This retains EXP-1918's worker team
and batched CPU post-convolution, EXP-1916's grouped MoE, and the
eight-row Q8 kernels.

Sliding attention and layers without per-stream depth queries share
the input tensor and its RMS normalization. Each controller keeps its
own message, weights, blend and subsequent normalization. Sliding
attention without controllers also shares the final scaled tensor.
The prefill worker fills one input row instead of three when the
graph shares it. Controller message preparation remains separate.
There are no CUDA, numerical object, or common scheduler changes.

## A/B measurements

RTX 4070, the EXP-1917 Inkling AD-IQ4_XS 2048/256 fixture, context
4096, batch 2048, ubatch 1024 and Q4_0 K/V. The same executable ran
A–B–B–A. Each server handled a first and repeated request with a
fully recomputed prompt and equal internal state hashing. The
control used `I2_SHARED_ATTENTION_INPUTS=0`. Sharing is enabled by
default in the experiment binary.

| Paired mean | Control | Shared | Delta |
| --- | ---: | ---: | ---: |
| First prefill, tok/s | 183.85 | 191.37 | +4.09% |
| Repeat prefill, tok/s | 194.27 | 203.38 | +4.69% |
| First decode, tok/s | 13.568 | 13.753 | +1.37% |
| Repeat decode, tok/s | 13.548 | 13.828 | +2.07% |
| Repeat prefill, ms | 10541.9 | 10069.7 | -472.2 |
| Repeat decode, ms | 18822.7 | 18440.5 | -382.2 |
| Repeat client request, s | 29.579 | 28.724 | -0.855 |

Both pairs were faster in all four throughput measures. All 5118
fingerprints per launch, all 256 output IDs and text in both requests,
and prefill expert traffic matched. The fingerprints and outputs
also match the original EXP-1918 binary. Peak board VRAM was 9995 MiB
in each arm, and peak RSS rose by 9384 KiB. Process swap and system
swap-out were zero.

The Apex checkpoint checksum manifest passed. A combined-runtime
Apex benchmark and the Inkling 32K/512 gate remain open. Restoring
shared-input construction reduces repeated work in the graph. The
full attention and MoE activation handoffs still exist.

## Reproduction

Copy the EXP-1918 host inputs and numeric object into a sibling
directory under `build/inkling-exp-baseline/device-attention`.
Apply the two patches in `source` with `patch -p1` and use its
build script. GCC 13 rebuilds only the Inkling host object and links
it with the EXP-1918 archive and numeric object. See `inputs.sha256`
and the exact arguments and individual results in `results.json`.

In this build the old `I2_RESIDENT_VECTOR_CONSTANTS` setting still
has no host consumer. Its separate weight placement change requires
a new isolated trial before returning it to the cumulative checkpoint.
