# EXP-2026-11-24-1892 — Keep F32 Inkling routers resident

**Accepted diagnostic candidate.** Place the F32 router weight tensors
identified by the Inkling layout bindings with the existing dense ranges.
Loop visits reuse the same 24 physical router tensors. They consume
27 MiB of the existing 7168 MiB dense-plus-expert budget; the expert
bank loses seven slots. No quantization, route decision, execution
order, or shared runtime implementation changes.

RTX 4070, Inkling AD-IQ4_XS, context 128, prompt 24 IDs in microbatches
16+8, 17 greedy outputs and 16 decode intervals. A-B-B-A on the same
binary and EXP-1891 profile with `I2_RESIDENT_ROUTERS=0/1`.
All 17 output IDs and 57 residual/logit fingerprints match the control.

| Arm | Prefill seconds | Decode tok/s |
| --- | ---: | ---: |
| A1 | 2.963 | 5.450 |
| B1 | 2.979 | 5.625 |
| B2 | 2.999 | 5.647 |
| A2 | 3.008 | 5.462 |

Mean decode 5.456 → 5.636 tok/s (+3.30%); prefill about 8.04 → 8.03
tok/s (-0.12%). The percentage decode gain exceeds the prefill loss.
Total expert fills rise 1977 → 1981, with no meaningful RSS growth,
slightly reduced GPU allocation and zero swap-out. This result is
limited to the diagnostic line.

Separate matched-shape Nsight traces record 768 → 0 transfers of the
1179648-byte F32 router plane in decode. H2D across 16 decode steps
falls from 3,079,211,264 to 2,190,493,952 bytes (-28.9%), despite an
extra 17,252,352 bytes of expert plane fills. The remaining observed
expert H2D is about 2.02 GB and dominates the payload. The first profiling
attempt had misplaced NVTX markers and was discarded. The corrected
profile is the only candidate in [trace-results.json](trace-results.json).

## Reproduce

Apply [the patch](source/resident-routers.patch) to the EXP-1891
diagnostic harness, relink against its existing archive and preserve
all its launch settings. Add the switch above. Use the exact prompt
and artifacts in [results.json](results.json). For a diagnostic
trace, apply [the NVTX patch](source/trace.patch), link nvToolsExt,
collect CUDA and NVTX, and compare transfer sizes within the two
phase windows. Apex EXP-1880 remains the independent full regression
reference. Full-context Inkling and HTTP qualification remain pending.
