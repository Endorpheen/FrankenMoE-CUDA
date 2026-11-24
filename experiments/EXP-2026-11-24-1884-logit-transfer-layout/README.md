# EXP-2026-11-24-1884 — Compare host logit delivery after the Q6_K head

**REJECTED for integration.** EXP-1880 remains the accepted runtime.
The Apex 32K/512 request uses non-greedy sampling with top-k 20,
top-p 0.95, temperature 1 and seed 123. This probe preserves the
entire 248320-word FP32 logit array and leaves sampling outside
the GPU. It uses the archived EXP-1786 Q6_K dense MMVQ entry
extracted for SM89 and shape 248320 × 2048.

The first candidate points the unchanged matrix-vector kernel
directly at mapped pinned host memory. The reference writes
device memory, then copies the full array into a pinned host
buffer. The second candidate launches two exact halves by
offsetting the weight and output pointers. A non-blocking copy
stream waits for each half and copies it into its pinned host
slice. Both preserve the original lane, warp and row reductions.
The clock stops only after the host buffer can safely be read.

| Candidate | Control median ms | Candidate median ms | Time change |
| --- | ---: | ---: | ---: |
| Direct mapped output | 0.93704 | 2.15590 | +130.08% |
| Two halves with overlapping D2H | 0.93749 | 0.92142 | −1.71% |

Each comparison uses three independent packed activation
fixtures for correctness, and all 248320 FP32 logit words match
for each fixture. Timing is the median of eight alternating
blocks of 32 host-ready iterations, CPU mask 0f0f.
The direct write is substantially slower. The split probe
saves about 16.1 µs per token in isolation. At 511 decode steps
the observed per-step difference would amount to about 8 ms
if fully transferred. It does not justify the added streams
and events in the live graph for this profile. No full model
test, swap gate or model memory claim is made.

## Reproduce

Extract `goose_mmvq_dense_matvec` from the exact archived
MMQ PTX with CUDA 12.6 ptxas:

```sh
ptxas -arch=sm_89 --entry goose_mmvq_dense_matvec "$exp1786/device/goose-cuda-mmq.ptx" -o head.cubin
sha256sum -c SHA256SUMS
taskset 0f0f python3 probe.py
taskset 0f0f python3 probe-split.py
```

[Direct-output log](probe.log), [overlap log](split.log) and
[results.json](results.json) record the exact measured blocks.
