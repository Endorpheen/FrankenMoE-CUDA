# EXP-2026-11-25-2020 — Apex on the cumulative Bonsai binary

**Correctness and measured VRAM gates passed. Small speed differences
remain inconclusive.** No production binary or configuration changed.
The candidate is the EXP-2019 shared HTTP executable. The reference
is the frozen EXP-1969 executable with the exact EXP-1970 Apex profile.
The matched common archive has 22 members, 21 of them byte-identical.
Only the model host differs. See [archive hashes](data/archive.json).

## Setup and recovery

The first standard start succeeds in placing the full 6290 MiB expert
cache and registering 19,504,005,120 bytes of expert pages. It later
exits on the full model RAM residency guard. No request runs.
Separately, `fincore` reports 16,635,940,864 of 21,713,463,136 B
resident. A sequential read of the unchanged local model file
makes all of its file pages resident, confirmed by `fincore`.
The original control then starts without changing software,
context, cache, pinned placement, or numeric options. The specific
reason for absence of resident pages at the first start is unknown.

For each measured arm, repeat the same preparation, start a fresh
server and post the immutable EXP-1880 32K/512 request twice.
Record only the second response for timing and require `cache_n=0`.
Use RTX 4070, context 262144, Q4_0 K/V, batch/ubatch 4096/4096,
6290 MiB SLRU, CPU masks and all EXP-1970 flags. No unrelated
process was stopped.

## Scored results

| Arm | Prefill, tok/s | Decode, tok/s | Client, s | Peak process VRAM, MiB |
| --- | ---: | ---: | ---: | ---: |
| A1 | 3446.46 | 80.441 | 15.952 | 8774 |
| B1 | 3426.31 | 80.476 | 16.004 | 8774 |
| B2 | 3433.54 | 80.461 | 15.984 | 8774 |
| A2 | 3437.38 | 80.864 | 15.940 | 8774 |
| B3 | 3437.05 | 80.489 | 15.972 | 8774 |
| A3 | 3425.62 | 80.627 | 15.995 | 8774 |
| Aggregate phase times A → B | 3436.47 → 3432.30 | 80.644 → 80.475 | 15.962 → 15.987 | 8774 → 8774 |

All six warmup and all six scored generations match the saved EXP-1970
512 IDs and text. All final `/props` values including SLRU counters,
context settings and full residency match that reference. Measured
VmHWM ranges are 21,458,420–21,495,468 KiB for A and
21,478,468–21,494,904 KiB for B. Process swap and system swap-out
are zero in the scored intervals. Some startups incur separate
system swap-out, including 58,127 pages in the first control.
Both startups of the added B–A pair record zero swap-out.

Aggregate prefill and decode are lower by 0.12% and 0.21%.
Prefill and client wall change sign across the pairs. The data
do not establish a reproducible material slowdown or a strict
zero-regression performance guarantee. This does not qualify Inkling
or close the Bonsai speed goal. The numerical and memory evidence
applies to the exact candidate and workload above.

## Reproduction

From the repository root, with the frozen EXP-1969 and EXP-2019
artifacts present, prepare the identical file before each arm:

```bash
model=/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf
dd if="$model" of=/dev/null bs=8M status=none
fincore -b -o RES,SIZE "$model"
```

Load [argv](data/argv.txt) and [env](data/env.txt) as one entry per
line with Bash mapfile. Substitute the corresponding executable
and an available loopback port. Run A–B–B–A and the extra B–A
pair sequentially against the exact
`build/checkpoint-apex-exp1880-before-inkling/evidence/request.json`.
Capture `/props` after the second request, per-PID GPU memory,
`VmHWM`, `VmSwap` and `pswpout` at startup and around the score.
[Exact output IDs, properties and measurements](data/results.json).
[Artifact hashes](data/SHA256SUMS).
