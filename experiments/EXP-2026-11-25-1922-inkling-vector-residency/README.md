# EXP-2026-11-25-1922: Re-evaluate resident Inkling vector constants

**Verdict: REJECTED at 2048/256.** The earlier short workload showed
eliminated upload calls. Restoring the mechanism on the cumulative
EXP-1921 checkpoint did not shorten the measured requests.

Restore the old Inkling placement of immutable model-width F32
vectors in the existing dense allocation. The check admits extra
bytes only outside the expert budget and within the remaining
allocation ceiling. The current CUDA vector lookup already supports
this placement. This candidate changes only the Inkling host
object. The control uses `I2_RESIDENT_VECTOR_CONSTANTS=0`; the
candidate enables residency. Use the patch and build script in
`source` in a sibling build directory containing the EXP-1921 inputs.

Run the EXP-1917 2048/256 fixture on RTX 4070 with context 4096, batch
2048, ubatch 1024 and Q4_0 K/V. The same binary was tested A–B–B–A
with full prompt recomputation and identical hashing. Each launch
used a first request and a repeat. Both launches of the reverse
pair used line-buffered stdout to capture the placement counter.
The counter reports 0 bytes for the control and 940032 bytes for
the candidate. The first pair did not retain this line at exit.

| Paired mean | Control | Resident |
| --- | ---: | ---: |
| First prefill, tok/s | 193.79 | 193.38 |
| Repeat prefill, tok/s | 204.04 | 203.91 |
| First decode, tok/s | 13.884 | 13.772 |
| Repeat decode, tok/s | 13.871 | 13.717 |
| Repeat prefill, ms | 10037.8 | 10044.0 |
| Repeat decode, ms | 18383.8 | 18590.7 |
| Repeat client request, s | 28.639 | 28.847 |

Repeat prefill changed by +108.7 ms in the first pair and -96.4 ms
in the reverse. Repeat decode increased by 236.5 ms and 177.2 ms.
Total repeat request time increased in both pairs. The runs do
not attribute the decode loss to a specific CUDA operation.

All 5118 state fingerprints per launch, both sets of 256 output
IDs and text, and expert fills and upload bytes match EXP-1921 and
the local control. Board peak was 9959 versus 9961 MiB. Peak
process RSS was 13341548 versus 13340668 KiB. Process swap and
system swap-out were zero. The frozen Apex manifest passed;
Apex performance and 32K/512 were outside this experiment.

Keep EXP-1921 as the active Inkling checkpoint. See `results.json`
and `inputs.sha256` for launch parameters and artifacts.
