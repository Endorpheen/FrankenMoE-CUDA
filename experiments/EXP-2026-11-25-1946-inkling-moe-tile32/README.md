# EXP-2026-11-25-1946: Reuse Inkling MoE weights across 32 rows

**Verdict: ACCEPTED for the isolated Inkling 8192/256 profile.**
The full 32K/512 workload and Apex performance still require
separate validation.

The EXP-1916 grouped IQ4 path uses up to two eight-row MMA
groups per weight load. Add two wider candidates with three and
four groups. Each group accumulates independently, keeping the
same ordered MMA recurrence and original input FP16 packing.
The production-shaped experiment dispatches 32 rows only for
grouped batches of at least 4096 jobs with the existing wide path
enabled. It merges consecutive original eight-row descriptors
for the same expert plane. Smaller groups, tails and decode keep
the established behavior. Use `I2_MOE_TILE32=0` for control.

## Device screen

RTX 4070 and seeded IQ4_XS and IQ4_NL planes. The device timings
alternate arm order over four samples. All output hashes match
for 7, 31, 128 and 257 jobs per expert, including partial tiles.
The selected entry increases register count from 40 to 47–48
without spills. For the 257-job case the IQ4_XS kernel drops
from 6.323 to 3.595 ms and IQ4_NL drops from 2.162 to 1.623 ms.
The 24-row entry is retained in the image for the device screen.
Short shapes do not consistently benefit, which is why dispatch
retains the established smaller-job path.

## Paired model timings

Same-binary A–B–B–A on RTX 4070 using the EXP-1944 8192/256
fixture. Context 36864, batch and ubatch 4096, Q4_0 KV, wide
vectors, 736 MiB activation arena and 6720 MiB expert limit.
Each fresh process handled one first and one repeat request
with full prompt recomputation and equal fingerprinting.
The host path is the accepted EXP-1943 direct readback with
its original state layout.

| Paired mean | 16 rows | 32 rows |
| --- | ---: | ---: |
| First prefill, tok/s | 255.86 | 276.99 |
| Repeat prefill, tok/s | 259.69 | 280.25 |
| First decode, tok/s | 18.299 | 18.347 |
| Repeat decode, tok/s | 18.243 | 18.338 |
| Repeat prefill, ms | 31545.6 | 29230.8 |
| Repeat full request, s | 45.742 | 43.361 |

Repeat prefill gained 7.92%. It saved 2.232 and 2.398 seconds
in the two pairs. Repeat decode remained within about one
percent. All 17406 fingerprints per launch, all 256 output
IDs and text per request, and expert traffic in both phases
matched. Board peaks ranged from 11696 to 11711 MiB. Process
peak RSS stayed within 2 MiB. There was no swap-out or process
swap. The frozen Apex checksum manifest passed. Production
profiles and shared runtime code were not changed.

## Reproduction

Apply the kernel patch to the EXP-1916 grouped source and the
driver-header patch to EXP-1943. Build with the provided
Clang 18, ptxas and GCC 13 script using the EXP-1943 archive
in the sibling layout. `source/bench-mmq.py` measures the
kernel candidates. See `tile-bench.json`, `compiler.ptxas`,
`inputs.sha256` and `results.json` for exact artifacts,
arguments and individual measurements.
