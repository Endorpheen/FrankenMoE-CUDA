# EXP-2026-11-25-1918: Batch Inkling post-attention convolution

## Host measurements

A phase-only build using the EXP-1916 candidate ran the EXP-1917
2048/256 fixture at ubatch 1024. It matched all output IDs and text from
the original candidate. The repeat prefill took 11065 ms.

| Scope | Repeat time, ms |
| --- | ---: |
| Attention graph | 4533.4 |
| MoE graph and host output copies | 4402.5 |
| Sequential post-attention convolution | 639.5 |
| Input preparation | 546.4 |
| Preparation after convolution | 219.6 |

The graph scopes include device work, host work, transfers and waits.
They cannot be added to the CUDA timings in EXP-1917. This probe used
an unprofiled CUDA execution with host timing enabled. The patch in
`source/prefill-phase.patch` reproduces the host scopes on the
EXP-1912 worker-team source linked against the EXP-1916 archive.

## Candidate

The new Goose function applies the four-tap post-attention convolution
to contiguous channel groups on the existing prefill team. Each
channel keeps the original tap order, intermediate FP32 operations
and final scale. The function reads earlier activations directly from
the microbatch and commits the last three values to the slot history
once. The first three rows read any missing samples from the prior
history. Single-token decode still uses the original implementation.

The candidate changes the Inkling host wrapper and its Goose numerical
object. It reuses the EXP-1916 CUDA objects and the existing worker
affinity, keeps both activation readbacks, and leaves the Apex paths
alone. The experimental binary defaults to the new routine when the
prefill team is available. Set `I2_PREFILL_POSTCONV_BATCH=0` to use the
original loop for a same-binary control.

## A/B result

A–B–B–A runs used the same 2048/256 fixture, context 4096, batch 2048,
ubatch 1024, Q4_0 K/V and RTX 4070. Each launch served a first request
and a repeat with prompt recomputation. Each pair enabled internal
state hashing equally.

| Paired mean | Original | Batched | Delta |
| --- | ---: | ---: | ---: |
| First prefill, tok/s | 175.94 | 184.12 | +4.65% |
| Repeat prefill, tok/s | 184.45 | 193.48 | +4.89% |
| First decode, tok/s | 13.490 | 13.463 | -0.20% |
| Repeat decode, tok/s | 13.515 | 13.288 | -1.68% |
| Repeat prefill, ms | 11103.3 | 10585.2 | -518.1 |
| Repeat decode, ms | 18868.5 | 19193.3 | +324.8 |
| Repeat client request, s | 30.188 | 29.991 | -0.197 |

All 5118 numerical fingerprints per launch, both sets of 256 output
IDs and their text, and expert fills and upload bytes match. First
prefill saved 478–557 ms, and repeat prefill saved 500–536 ms across
pairs. The highest observed board VRAM peak was 9925 MiB with the
candidate and 9917 MiB with the control. Peak process RSS differed by
456 KiB. All runs had zero process swap and zero system swap-out.

Retain this as an isolated Inkling research candidate under the
percentage-tradeoff rule. The matched runs both show slower repeat
decode, so this is evidence of a prefill improvement and a small
observed decode loss. Additional uninstrumented cumulative tests and
the separate Apex performance gate are needed before runtime
promotion. The unchanged frozen Apex checksum manifest passed. This
experiment did not run Apex or qualify Inkling at 32K/512.

## Reproduction

Copy the EXP-1912 host source into a sibling build directory under
`build/inkling-exp-baseline/device-attention`. Apply
`source/numeric.goose.patch` and `source/prefill.h.patch` with
`patch -p1`. Use `source/build-candidate.sh` with KeelWing and GCC 13
to recompile the isolated host and numerical objects and link the
EXP-1916 archive. See `inputs.sha256` and the launch arguments and raw
metrics in `results.json`.
