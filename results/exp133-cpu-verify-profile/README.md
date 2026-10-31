# EXP133: CPU MoE attribution and cross-token reuse

Status: DIAGNOSTIC_COMPLETE; no optimization/default promotion.
Reference: immutable accepted EXP107. A separate libggml-cpu adds thread0
per-node elapsed timers around normal compute/fusion and existing barriers,
plus counters over the existing matrix_row_counts. No additional barrier,
route download, arithmetic, quantization, placement or kernel change.

Four manually issued serial requests: ordinary and MTP1 processes, each the
same frozen32K/128 and code839/256 greedy fixtures as EXP132. All four have
full monitor coverage, system swap-out0, process VmSwap0, major faults0.
Selected IDs, ordered top5 and probabilities match the corresponding
uninstrumented EXP132 profile exactly (including MTP's own numerical path).
No performance acceptance claim: timings include diagnostic overhead.

## Observed CPU work

Whole-process counters include initialization, warmup and both requests.
They are not isolated vec-dot timings or request-only critical-path shares.
Node time includes existing CPU barriers and scheduling waits; CPU/GPU
intervals must not be added as independent removable savings.

| Profile/type/token bucket | Calls | Total CPU-node us | Mean us/call |
| --- | ---: | ---: | ---: |
| ordinary Q4_K /1 | 38226 | 6758227 | 176.79 |
| ordinary Q6_K /1 | 7653 | 1957705 | 255.81 |
| MTP Q4_K /2 | 20602 | 6849067 | 332.45 |
| MTP Q6_K /2 | 4241 | 1854524 | 437.28 |

The different token/route distributions and extra MTP layer prevent treating
these averages as a controlled per-kernel batching speed ratio. They establish
substantial real CPU MUL_MAT_ID work worth a targeted exact microbenchmark.

MTP's two-token bucket records397488 routed expert/projection rows,
350922 distinct used expert/projection entries and46566 same-expert row pairs.
Paired rows are23.4301% of all rows; at most11.7151% of weight-block processing
instances can disappear if every pair shares one read/unpack. These counts
repeat per projection. They are an upper bound, not achieved time savings.

The existing one_chunk path tiles16 output rows and iterates input columns
inside that tile. Thus shared expert weight lines can already remain in L1;
fusing two columns does not automatically save a second DRAM read. Unpack/scale
reuse may still help, and must be measured separately from memory traffic.

## Real-model observations (diagnostic only)

Ordinary32K prompt12805.482ms, decode28.6188tok/s; code prompt997.012ms,
decode30.5460tok/s. MTP32K prompt15671.399ms, decode16.9020tok/s; code
prompt1006.882ms, decode19.6353tok/s with127/127 accepted drafts. These are not
an A/B speedup claim or new baseline measurements. MTP adds hidden-state/draft
and rollback costs outside the CPU expert node sums; their shares are unknown.

## Next bounded direction

Use exact real Q4_K/Q6_K shapes/weights and observed same-expert pairs to compare
shared unpack against the existing optimized AVX2 implementation. A Goose
candidate must prove bounds elimination/SIMD actually happens, preserve every
lane's reduction/FMA semantics, and respect C export/runtime thread ownership.
Compare also an optimized C/C++ implementation of the same algorithm so a
changed algorithm is not incorrectly credited to the source language.

Working EXP107 restored on8081, healthy/idle, all nine mapped runtime hashes
match the reference. Diagnostic code/library remains isolated under
/home/end0/AI/LlamaServer/exp133-cpu-verify-profile. No installed settings changed.
Raw traces/SSE/logs/monitors remain local under
results/archive/EXP-2026-10-31-133/raw. Hashes, source patch, build commands and
structured results are stored here. CPU sampling permissions were not changed.
