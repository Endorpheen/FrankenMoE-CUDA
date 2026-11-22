# EXP-1720 — Use direct addresses for single-job dense MMVQ

For one dense quantized job, the host already knows the input and
output byte offsets. Pass their final device addresses to a dedicated
packing and matvec entry. The existing Goose math and block geometry
are unchanged. Multi-job shapes, routed jobs, and unsupported modes
continue to use the descriptor upload. The verified CUDA image was
embedded in the candidate before launch and only `cuda-driver.o`
differs in the archive. The server log confirms use on decode.

The saved EXP-1715 trace has 128,258 16-byte H2D operations. This
change removes the descriptors for the selected MMVQ path. It
does not measure the number removed in a fresh trace. The path can
also run on a one-job prefill tail.

## Full warmed Apex pair

RTX 4070, Apex-35B-V1.0 Q4_K_M, 32,768 input and 512 output tokens,
4,096/4,096 batch and microbatch, context 262,144, Q4_0 KV,
6,290 MiB expert bank, 210 MiB temporary workspace, no probability
rows. Unprofiled, one full warm-up and one measured request per arm
in the same process. Zero prompt reuse.

| Metric | EXP-1719 fresh control | EXP-1720 |
|---|---:|---:|
| Prefill, tok/s | 3012.82 | 2986.82 |
| Decode, tok/s | 49.27 | 49.95 |
| Decode, ms | 10371.910 | 10230.348 |
| Client, s | 21.336627 | 21.289921 |
| Peak process VRAM, MiB | 8766 | 8766 |
| Peak RSS, KiB | 21488504 | 21586000 |
| Process swap, KiB | 0 | 0 |
| System swap-out since launch, pages | 0 | 37 |
| System swap-out after readiness, pages | 0 | 0 |

Warm decode saves 141.562 ms, prefill takes 94.692 ms longer,
and the client saves 46.706 ms. Both requests match all 512 IDs
and text, with identical SLRU counts and upload bytes. The candidate
has a 95.21 MiB higher peak RSS in this pair.

Keep EXP-1720 as an experimental decode checkpoint with the prefill
and RSS differences recorded. EXP-1719 remains available for rollback.
Raw results, the checked executable and the exact profile are saved
under `build/exp1720-direct-dense-mmvq`. The candidate SHA-256 is
`d54f3c46b15bc19ff2dacf133fea4c530a47355ad63e198ae4778f9b8759ae19`.
Apply the supplied patches to the corrected EXP-1719 driver and
companion device glue. Rebuild the two images and replace the driver
object in the corrected EXP-1719 archive.
