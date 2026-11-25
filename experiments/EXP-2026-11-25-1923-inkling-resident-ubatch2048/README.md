# EXP-2026-11-25-1923: Keep the Inkling 2048-token MoE microbatch resident

**Verdict: ACCEPTED for the isolated 2048/256 profile.** Keep the
EXP-1921 executable. For this workload use ubatch 2048,
`GOOSE_CUDA_WIDE_VECTOR=1` and `GOOSE_CUDA_ACTIVATION_MIB=576`.
The reference uses ubatch 1024 and the 384 MiB arena.

## Why the larger microbatch initially lost

On RTX 4070, the larger microbatch saved 13.62 GB of warm expert
uploads, but repeat prefill fell from 204.16 to 139.22 tok/s.
The corresponding CUDA traces showed 3789.6 ms of extra exclusive
D2H activity. The MoE products are 72 MiB each at ubatch 2048,
above the 32 MiB threshold in `cv_try` in
`goose/cpu/goose-cpu-backend-api/cuda-vector.goose`. The windowed
fallback publishes pieces of up to 16 MiB to host memory.

Enabling `GOOSE_CUDA_WIDE_VECTOR` at the original 384 MiB arena
still gave 139.11 tok/s. Driver diagnostics established that the
first attempted wide product had both device operands, but no
contiguous 72 MiB output slot. The next attempted product lacked
a resident operand after fallback. The diagnostic checked
`fg_cuda_wide_vector_ready` in a separately built driver. See
`arena-diagnostic.patch` and `arena-diagnostics.txt`.

With 576 MiB, a separate trace recorded zero fallback writes of
16773120 or 8404992 bytes, down from 384 and 96 per repeat prefill.
D2H volume returned to 1.816 GB from 9.064 GB. The full Attention
and MoE boundary copies remain. The trace window was 9836.2 ms
versus 15130.0 ms with the failing microbatch. Trace values are
diagnostic and separate from the timed comparison.

## Paired gate

EXP-1917 input and output budget, context 4096, batch 2048, Q4_0 K/V.
Both arms used the same EXP-1921 binary and exact state hashing.
Each launch was fresh and handled a first request and a repeat
with prompt recomputation. The two screened profiles preceded the
accepted candidate pair. The measured order for the accepted
profile was A–D–D–A.

| Paired mean | ubatch 1024 | ubatch 2048, resident |
| --- | ---: | ---: |
| First prefill, tok/s | 193.26 | 202.39 |
| Repeat prefill, tok/s | 203.92 | 214.34 |
| First decode, tok/s | 13.824 | 13.892 |
| Repeat decode, tok/s | 13.850 | 13.853 |
| Repeat prefill, ms | 10043.3 | 9555.1 |
| Repeat client request, s | 28.668 | 28.183 |
| Repeat expert payload, GB | 20.24 | 6.62 |
| Peak board VRAM, MiB | 9996 | 10241 |

Repeat prefill improved by 5.11% on average and won both pairs.
Decode was flat on average across the two repeat pairs. Peak
process RSS rose by about 197 MiB. There was no process swap or
system swap-out. All 5118 state fingerprints, all 256 output
IDs and text matched in the timed arms. The frozen Apex checksum
manifest passed. The code and deployment defaults are unchanged.
End-to-end Apex and the Inkling 32K/512 gate remain open.

## Reproduction

Use the binary and the request identified by `inputs.sha256`.
`results.json` includes the full timed launch arguments. The
local artifacts are under
`build/inkling-exp-baseline/device-attention/inkling-ubatch2048`.
For each CUDA trace, set only the request's `n_predict` to 1 and
send the first request and one repeat. Use Nsight Systems with a
collection duration sufficient to finish both requests. Export
SQLite and run `compare-prefill-traces.py` with the three profile
directories and their response receipts. The early forcibly
terminated captures did not flush CUDA events. The retained
reports used Nsight's duration timer to collect complete prefill
windows.
