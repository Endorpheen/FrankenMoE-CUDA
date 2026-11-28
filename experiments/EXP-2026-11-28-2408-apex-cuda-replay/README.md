# EXP-2026-11-28-2408 — Restore Apex CUDA graph replay and expert staging

The Goose CUDA driver initializes the argument packet layout and the active
expert bit mask before reading either. It represents the native graph kernel
parameter descriptor with explicit padding. The generated declarations for
both replay paths match the 72-byte CUDA x86-64 ABI and the checked field
offsets. This checkpoint also includes the driver initialization, alignment,
and routed projection fixes needed to complete the request on this branch.

## Diagnostic result

RTX 4070, Apex Q4_K_M, 32,768 prompt tokens, 512 generated tokens, context
262144, batch/ubatch 4096/4096, Q4_0 KV and a 6290 MiB expert cache. Each
unprofiled observation is the second request after one full warm-up in the
same server. Prompt reuse was disabled. The candidate matches all 512 token
IDs, text, and the stop type. The two CUDA measurements are for a single
response.

| Build | Prefill, tok/s | Decode, tok/s | Request, s |
| --- | ---: | ---: | ---: |
| Prior migrated Goose driver | 3025.28 | 67.13 | 18.537 |
| Initialized, ABI-correct Goose driver | 3353.14 | 74.92 | 17.639 |
| EXP-1880 accepted mean | 3505.42 | 82.10 | 15.659 |

Candidate peak RSS was 21,499,864 KiB, process swap was zero, and maximum
sampled process VRAM was 8862 MiB. System swap-out increased by 100 pages
during scoring. The observations establish a positive diagnostic result.
The absolute speed and matched memory acceptance gates remain open.

Earlier Nsight traces of the frozen control and the prior migrated driver
showed zero versus 41,056 capture starts in first-request decode and 58.07
versus 92.53 GB of host-to-device copies before the first route predictor
on the second request. The prior migrated build failed during second-request
decode with CUDA status 700. The trace counters are observations from before
the fixes. The individual contribution of each repair remains open.

## Artifact hashes

| Artifact | SHA-256 |
| --- | --- |
| EXP-1880 executable | `0ef188ef55f242ae548994ad793204fef6eb16b580aacb5c37093b5f27fda3ff` |
| Prior migrated executable | `7dcebd6b9763e02de57b823c9eb99fd8dc8480a009a8467ca372848506c62173` |
| Candidate executable | `51d5aad111b45d07f4d54b02cef586c8ecf3880900ca5972c2dded25bb7c1632` |
| Candidate Fried Goose compiler, build from d682b183 | `ff5764c00fe53755fe01bebf13cc94c6094140aa41a2b4bc3dc77ec4fd3dc80d` |
| Apex GGUF | `c3b92d94cd03a7730ad45cc45de8c753e369e0228b2f5d83d819ce2ea6ab3355` |
| Frozen request | `d81f501a24aa29e1933662fbb8b05d3a1eec5bf7709c622fe7ec63371d8d2d9f` |
| Candidate warm response | `d4844d4f01653b828f5273cfa2747f6a847c226590a2645528c08b1154e5e3ab` |
