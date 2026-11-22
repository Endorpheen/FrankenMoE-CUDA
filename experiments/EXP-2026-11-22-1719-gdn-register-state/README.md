# EXP-1719 — Keep the one-token GDN recurrence in registers

Decode with 128 values and four 32-lane warps assigns 16 recurrent
elements to each lane. The general kernel used three dynamic-index
arrays for state, K, and Q. The dedicated one-token function holds
state and K as scalars, loads Q at the point of use, and stores
each new state directly. State elements do not alias across lanes.
Native per-lane accumulation order, shuffle reduction, and FP32
recurrence are preserved. The common prefill implementation stays
in the device image.

| sm_89 compilation | EXP-1717 | EXP-1719 |
|---|---:|---:|
| Registers per thread | 47 | 56 |
| Stack frame, bytes per thread | 384 | 0 |
| PTX local load sites | 9 | 0 |
| PTX local store sites | 101 | 0 |
| Spill loads and stores | 0 | 0 |

The original frame is an addressable array allocation. The ptxas
report does not classify it as register spilling.

## Full warmed Apex pair

RTX 4070, Apex-35B-V1.0 Q4_K_M, 32,768 input and 512 output tokens,
batch and microbatch 4,096, context 262,144, Q4_0 KV, 6,290 MiB expert
bank, 210 MiB temporary workspace, default API without probability
rows. One full warm-up and one measured request per process. Zero
prefix reuse. Unprofiled runs.

| Metric | Fresh EXP-1717 | EXP-1719 |
|---|---:|---:|
| Prefill, tok/s | 3011.90 | 3010.86 |
| Decode, tok/s | 47.02 | 49.26 |
| Decode, ms | 10866.951 | 10372.503 |
| Client, s | 21.837580 | 21.348337 |
| Peak process VRAM, MiB | 8766 | 8766 |
| Peak RSS, KiB | 21488392 | 21585420 |
| Process swap, KiB | 0 | 0 |
| System swap-out, pages | 0 | 0 |

Decode saves 494.448 ms and client time saves 489.243 ms. Both
requests matched all 512 IDs, full text and SLRU counters. RSS rises
by 94.75 MiB in the pair and the difference is present at readiness,
before inference. No attribution of the entire speedup to the GDN
kernel or the RSS difference to the source change is made.

The initial diagnostic had byte-identical binaries because quoted
header lookup took the original image beside the driver source.
That pair is invalid for evaluating GDN. The corrected build
compiles a local driver copy and verifies before launch that the new
PTX is embedded only in the candidate. The initial artifacts remain
under the parent build directory.

Retain the measured candidate as an experimental decode checkpoint
and EXP-1717 for rollback. The corrected executable SHA-256 is
`a62be0d0d0c43c54ea6f7ac02cfc8752f50c8a6c9d26648a4cce4d1b420d4362`.
The corrected raw pair and exact profile are under
`build/exp1719-gdn-register-state/corrected`. Apply `gdn.patch` to
the EXP-1665 extracted `gdn.goose`. Rebuild the device images with
the EXP-1717 companion fragments and replace only `cuda-driver.o`
in the EXP-1717 archive.
