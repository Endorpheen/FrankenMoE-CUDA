# EXP-2026-11-28-2438 — Overlap Inkling verifier expert compute and DMA

Extend the existing resident/missing expert job split from one
decode token to the 16-token verifier. A projection submits its
resident jobs to the compute stream, then issues pending writes
on the expert stream. The compute stream waits before consuming
the missing weights. The existing slot fence protects earlier
readers before any cache slot is overwritten. Consecutive jobs
for one weight range share its classification. Keep the routing
policy, per-job result offsets and device buffer capacities.
Allow for all three queued planes in the existing host job table.
Use `I2_VERIFY_EXPERT_HIT_OVERLAP=0` to disable overlap for the
verifier while retaining the earlier one-token behavior.

The diagnostic reports 1223 mixed projections before the final
draft group, including 149723 resident and 6821 missing jobs,
on both candidate runs. Both candidate and control commit 32
draft groups without rejection and transfer 27819417600 expert
bytes. The diagnostic counts submissions on the two streams.

## Full first-request comparison

RTX 4070, Inkling 2 Mini AD-IQ4_XS, 32768 prompt and 512 output tokens,
context 36864, batch/ubatch 4096/4096, Q4_0 KV and expert cache 6720 MiB.
Each fresh process used a warmed model file cache and evaluated the
full prompt. The same binary ran with verifier overlap enabled for
both candidate requests and disabled for the control.

| Run | Prefill tok/s | Decode tok/s | Client s | Peak RSS KiB | Peak process VRAM MiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| Candidate 1 | 622.73 | 52.63 | 62.671 | 13542248 | 9822 |
| Control | 620.57 | 49.74 | 63.425 | 13541848 | 9822 |
| Candidate 2 | 624.14 | 52.39 | 62.584 | 13541668 | 9822 |
| Candidate mean | 623.44 | 52.51 | 62.627 | | |

Mean decode improves 5.56%, observed prefill improves 0.46%, and
client time decreases by 0.80 s. All 512 IDs, text and stop type
match EXP-2128. Scored disk reads, process swap and system swap-out
are zero. The 1000 prefill and 60 decode goals remain open. The
driver build used Fried Goose `d682b183` and GCC 13. The candidate
reuses the EXP-2437 CUDA images.

| Artifact | SHA-256 |
| --- | --- |
| Candidate/control executable | `c923c3b3105cae3dc76149b4747cda5bfc27055e2b4871223bd4b1c900023799` |
| Driver compiler | `ff5764c00fe53755fe01bebf13cc94c6094140aa41a2b4bc3dc77ec4fd3dc80d` |
| Frozen request | `a0e3e565f497045383aca3d431252780990c28498041a01de7ce2176c028f99f` |
| Candidate 1 response | `ef8f562084b0b846d7ed45ffb4e9538b74f8b14cb112a7d19fc818eb08069eef` |
| Control response | `89794d75c27bb0f656fa1ad031366208cb6f93d3daff39f642b9ef95ab6adbda` |
| Candidate 2 response | `893d3279f04ae3badbfb80c37f65cdc59ecef05d28e2f8204bdf0b6d55e51de6` |
