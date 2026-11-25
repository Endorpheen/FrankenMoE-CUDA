# EXP-2026-11-25-1947: Screen wider Inkling grouped MoE tiles

**Verdict: INCONCLUSIVE for 48 rows at request level.** The 64-row
device candidate was rejected. EXP-1946 remains the runtime baseline
for the 8192/256 test.

Add six- and eight-group variants of the EXP-1946 independent
eight-row MMA recurrence. Each variant reuses the decoded expert
fragment for additional rows and preserves the existing FP16 input
packing and FP32 MMA order. The 48-row model probe only extends the
existing 32-row eligibility gate. It merges consecutive descriptors
for the same expert weight plane. Disable it with
`I2_MOE_TILE48=0`. The 64-row entry is used only by the device test.

## Device screen

On RTX 4070 the 32-row entries use 47–48 registers, the 48-row
entries use 61–64, and the 64-row entries use 77–78. ptxas reports
no spills. The seeded tests compare outputs byte-for-byte at 7,
31, 64, 128 and 257 jobs per expert, with four samples in alternating
order. All shapes match. The kernels were timed using CUDA events
with otherwise resident inputs.

| Quant | Jobs per expert | 32 rows, ms | 48 rows, ms | 64 rows, ms |
| --- | ---: | ---: | ---: | ---: |
| IQ4_XS | 128 | 1.763 | 1.721 | 2.147 |
| IQ4_XS | 257 | 3.515 | 3.260 | 3.924 |
| IQ4_NL | 128 | 0.792 | 0.788 | 1.085 |
| IQ4_NL | 257 | 1.668 | 1.511 | 1.946 |

The largest synthetic shape improves with 48 rows. Smaller groups
vary. The 64-row layout loses in the main tested geometries. These
numbers measure the grouped kernel rather than the total request.

## Model screen

One control and one candidate process on RTX 4070, each handling a
first and repeat 8192/256 request. Use the same executable, context
36864, batch and ubatch 4096, Q4_0 K/V, 736 MiB arena and 6720 MiB
expert limit. The prompt is recomputed and diagnostic hashing is
enabled equally.

| Timing | 32 rows | 48 rows |
| --- | ---: | ---: |
| First prefill, tok/s | 276.53 | 279.50 |
| Repeat prefill, tok/s | 281.12 | 282.25 |
| First decode, tok/s | 18.327 | 18.060 |
| Repeat decode, tok/s | 18.246 | 18.295 |
| Repeat client, s | 43.344 | 43.182 |

The repeat saves 117 ms of prefill (0.40%) and 162 ms end to end.
This small result from one pair does not justify switching the
working checkpoint or running the reverse order. Both launches
match all 17406 fingerprints, both sets of 256 token IDs and text,
and both expert traffic counters. Peak board VRAM is 11717 and
11714 MiB, peak RSS differs by less than 1 MiB, and there is no
process swap or system swap-out.

The production profile and the frozen Apex binary were preserved.
The Apex checksum manifest passed. No Apex performance or 32K/512
cohort ran. The next optimization should target a different cost
than simply extending the register tile.

## Reproduction

Apply both patches to local EXP-1946 and build with the supplied
Clang 18, ptxas and GCC 13 helper against the same EXP-1943 archive.
Use `bench-mmq.py` for the seeded device comparison. The complete
run arguments and timings are in `results.json` and the hashes in
`inputs.sha256`. See `tile-bench.json` and `compiler.ptxas` for
the device measurements and register counts.
