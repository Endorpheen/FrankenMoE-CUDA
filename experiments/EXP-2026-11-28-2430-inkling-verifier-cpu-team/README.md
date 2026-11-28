# EXP-2026-11-28-2430 — Reuse the Inkling prefill CPU team for verification

Apply the [EXP-2425 host row partitioning](../EXP-2026-11-28-2425-inkling-parallel-embedding/README.md)
to embedding and host activation preparation during exact short
verification. The normal 64-row threshold leaves the 16-row
verifier serial. Reuse the existing team, CPU affinity and barriers
for exact verifier batches of 8–16 rows. Use at most the configured
thread count and four rows per task. Each partition executes the
original per-row operations and writes disjoint locations. The
normal prefill threshold, CUDA code, allocations and H2D policy
remain unchanged. The existing team and parallel preparation
settings also control this path.

The previous CUDA trace records 1.44 s of idle intervals between
the 16-row activation readbacks and their following uploads. This
motivates the CPU change. The idle time is diagnostic and includes
work beyond the newly parallel portions.

## Full first-request comparison

RTX 4070, Inkling 2 Mini AD-IQ4_XS, 32768 prompt and 512 output tokens,
context 36864, batch/ubatch 4096/4096, Q4_0 KV and expert cache 6720 MiB.
Each fresh process used a warmed model file cache and evaluated
the full prompt. The candidate ran before and after the EXP-2429 control.

| Run | Prefill tok/s | Decode tok/s | Client s | Peak RSS KiB | Peak process VRAM MiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| Candidate 1 | 563.40 | 49.11 | 68.903 | 13571716 | 9822 |
| EXP-2429 control | 564.28 | 46.19 | 69.504 | 13571168 | 9822 |
| Candidate 2 | 564.25 | 49.37 | 68.769 | 13571988 | 9822 |
| Candidate mean | 563.82 | 49.24 | 68.836 | | |

Mean decode improves 6.61% and observed prefill declines 0.08%.
All runs match frozen EXP-2128 in all 512 token IDs, text and stop
type. All use 32 draft groups, no draft rejects and 29363503104
expert H2D bytes. Scored disk reads, process swap and system
swap-out are zero. The 800 prefill and 50 decode tok/s goals remain
open. At the candidate mean, the remaining decode reduction is
approximately 0.16 s for the 512-token workload.

The Inkling host backend was built with Fried Goose `d682b183`
and GCC 13. The other link inputs were reused from EXP-2429.

| Artifact | SHA-256 |
| --- | --- |
| EXP-2429 executable | `c70b696221c7ac240c5e805610189e083e77d1526fc5c723a8db5460095b9501` |
| Candidate executable | `6b36cf501f0fc6ab144c4cb1747d6c6ff2791c0d17dfdc5bb1e47c63b2d576af` |
| Candidate host backend object | `b4c7248beca134c1218b80d7cd07c914f70dea80d9d05bfe1af7871b43ad7a11` |
| Frozen request | `a0e3e565f497045383aca3d431252780990c28498041a01de7ce2176c028f99f` |
| Candidate 1 response | `3846314856fdadee7caea5eb860d4966abca2e20fa9bcdac8c3964ecc599dd26` |
| EXP-2429 response | `cfa23fb4a3f7c9ab6eaed354cdef11a0568c6e6cf4bd8eaa7ec8ec50926ed3d1` |
| Candidate 2 response | `553be743b24f0b1646eea2c304e5c80a2383d49bcfe53aad0c878af768255339` |
