# EXP-1697 — Match the hybrid's one-warp Vector Attention geometry

This variant changes the long packed Q4 Vector Attention entry from
128 threads and 64 partitions to 32 threads and 11 partitions. It
uses the existing Q8 query pack, Q4 dot products and FP32 online
softmax. A single warp writes each partial vector and metadata to
the existing combiner. Partition order changes, so numerical
identity is not assumed. The short path retains its prior entry.

The ordinary 32K/512 pair uses the working API mode without
probability rows, the same weights, 6,290 MiB SLRU, and full host pin.

| Measure | Control | One warp |
|---|---:|---:|
| Prefill, tok/s | 3,011.82 | 3,020.39 |
| Decode, tok/s | 39.86 | 17.86 |
| Client, s | 23.791 | 39.559 |
| Peak process VRAM, MiB | 8,766 | 8,766 |
| Peak RSS, KiB | 21,488,448 | 21,582,888 |
| Process swap, KiB | 0 | 0 |
| System swap-out from launch, pages | 50 | 0 |

Both arms evaluate 32,768 prompts and generate 512 output tokens
with zero prompt reuse. The first differing ID is output token 4.
The control remains the working checkpoint. The candidate is
retained only as a failed geometry experiment.

## Identical Q/K/V fixture

The existing `goose-flash-vec-parts-test.c` fixture runs the same
deterministic tensors through the control and candidate archives.
Both pass the existing CPU comparison. At 32,768 keys the max
absolute errors are 4.6134e-5 and 4.6223e-5 respectively.
Nsight sums eight calls at that shape to 2.833 ms for the
control and 20.008 ms for one warp. At 4,097 keys both retain
the same short geometry. The slowdown is therefore present
with identical operands, independently of the changed output
tokens and resulting expert choices.

The candidate long entry uses 40 registers and no spills.
The traced hybrid one-warp entry uses 255 registers. Matching
geometry alone does not match load scheduling or instruction
parallelism. The vendor source explicitly unrolls hot row
loops. A separate arithmetic-order-preserving unroll test is
needed to measure that mechanism without changing partitions.

Artifacts and raw Nsight fixtures are preserved under the
FrankenMoE worktree's `build/exp1697-single-warp-attention`.
The patches target the EXP-1665 extracted Goose device fragment,
the standard OpenCL glue, and the cumulative EXP-1694 driver.
The decode working target is 71.17 tok/s on the ordinary API mode,
using the same output fields as the measured fast hybrid profile.
