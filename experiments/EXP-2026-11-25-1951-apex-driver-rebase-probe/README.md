# EXP-2026-11-25-1951: Rebase the Inkling CUDA driver on accepted Apex

**Verdict: diagnostic only.** The candidate retains the accepted
Apex host and graph objects in the Apex link. The two probe
executables share one CUDA translation unit, but retain their
respective model hosts and Goose objects. No checkpoint is promoted.

The 4190-line main-tree CUDA driver gave eight overlapping hunks
when merging frozen EXP-1880 with the EXP-1946 driver. Preserve the
Apex mailbox, split transfers, stage fences, dense MMVQ and CUDA
images. Select the Inkling matrix, partial-upload and routing paths
only in Inkling mode. The shared driver also requires the frozen
compact planner and the Inkling prior and bucket implementations.

Inkling first failed at model startup. The placement trace showed
10283974656 bytes free, 1463353344 reserved, 1236879360 resident
dense bytes and a 7046430720-byte expert limit. These pass the
model-level capacity checks. The Apex temporary prefill workspace
validator rejects a zero plane size, while Inkling's layout is
`{up, 0, down}`. Select that temporary workspace only for the
non-Inkling path. The edited probe completed the same 8192/256
request without changing the model or profile.

| Workload | Reference prefill / decode, tok/s | Probe prefill / decode, tok/s |
| --- | ---: | ---: |
| Apex 32K/512, pageable | 1744.21 / 46.374 | 1743.36 / 46.438 |
| Inkling 8192/256, first request | 276.94 / 18.272 | 279.63 / 17.459 |

The Apex measurement used the shared driver before the layout
guard fix. Against EXP-1950 the exact 512 IDs, text and `/props`
match, and logs confirm the route mailbox, direct dense MMVQ,
one-touch SLRU and predicted-route prefetch. Context 262144 and
the 6290 MiB expert limit were preserved. Peak board VRAM was
10701 vs 10707 MiB. Peak process RSS was 21475788 vs 21474144 KiB.
Both requests had zero system swap-out and zero process swap.
There were 252 vs 225 swap-out pages before the requests.
The published pinned Apex profile remains unverified on this probe.

The Inkling probe with the guard fix matches all 256 output IDs,
text and 8703 state fingerprints from the first EXP-1946 tile32
request. Post-prefill expert traffic also matches at 5389 fills
and 23243231232 bytes. Peak board VRAM was 11702 vs 11696 MiB
(the reference sample covers both its first and repeat requests).
Peak process RSS was 15624184 vs 14111736 KiB. Decode was 4.45%
slower. Client request wall time was 66.175 vs 43.952 seconds.
This comparison uses separately collected first requests. It
does not establish the cause of the wall-time or RSS gap. The
probe had zero request-interval swap-out and zero process swap,
with 508 swap-out pages before request submission.

Build with `bash source/build-probes.sh /absolute/new/scratch`
from the repository using the saved EXP-1880 and EXP-1946
artifacts. The script builds the post-fix probes with GCC 13
and reuses the original CUDA images. See `inputs.sha256` for
the local artifacts and `results.json` for full recorded launch
arguments. The raw directories include the HTTP responses,
logs, memory samples and client timings.

The full merge still needs one architecture dispatch for model
host and Goose, including the frozen cached decode graph ABI.
Next compare the Inkling decode critical path with its unchanged
EXP-1946 control using the same (sequential) A/B window, and
verify the final combined runtime against both accepted profiles.
