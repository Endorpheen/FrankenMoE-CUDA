# EXP-2026-11-25-1916: Reuse Inkling MoE IQ4 tiles across sixteen rows

The prefill path merges pairs of existing eight-row descriptors when they
are contiguous and use the same expert plane. The GPU decodes each weight
fragment once for both groups. Two independent eight-row MMA accumulators
retain the original reduction, activation packing and output order.
The low-level common descriptor planner and the decode dispatch remain
unchanged. The isolated flag is `I2_MOE_TILE16=1`.

The B–A–A–B runs used the same binary and the frozen 1024/64 fixture
from EXP-1915. Context was 2048, batch 1024, ubatch 512, Q4_0 K/V and
arena 384 MiB. The device was RTX 4070. Both arms retain the earlier
worker, sampler, Q8 and decode attention candidates.

| Paired mean | Eight rows | Sixteen rows |
| --- | ---: | ---: |
| Cold prefill, tok/s | 152.89 | 162.86 |
| Warm prefill, tok/s | 165.63 | 180.78 |
| Cold decode, tok/s | 13.86 | 13.85 |
| Warm decode, tok/s | 13.79 | 13.89 |
| Warm request, s | 10.964 | 10.410 |

All 2302 numerical fingerprints per arm, output IDs and text match the
frozen baseline. Expert traffic did not change. Board peak was
9837–9845 MiB, peak RSS varied by less than 1 MiB, and swap was zero.
There was no measurable change in cold decode. Warm decode was within
the previous range. The short results do not qualify 32K/512 or a
combined Apex build. The existing Apex checksum manifest passes.

In the independent warm CUDA traces, the two IQ4 kernel families went
from 1430.5 to 857.8 ms summed. Grid-Y tiles per family fell from 53759
to 29686 without reducing the number of matrix calls. The measured
trace window fell from 6489.2 to 5944.5 ms. These traces are separate
from the timed pairs and are recorded in `kernel-comparison.json`.

Apply `source/grouped-moe.cl.patch` and
`source/grouped-moe-driver.h.patch` with `patch -p1` to the frozen
grouped MoE source and host header. Compile that device image with
Clang 18, `-ffp-contract=off`, and ptxas for SM89. Rebuild
the matching local CUDA driver and link against the EXP-1914 host
archive and numerical object. No published binary or default changed.
Use `I2_MOE_TILE16=0` for the control and the same normalized request.
