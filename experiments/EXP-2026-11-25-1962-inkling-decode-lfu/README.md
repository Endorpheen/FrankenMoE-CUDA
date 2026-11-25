# EXP-2026-11-25-1962: Use frequency eviction for the Inkling decode cache

**Verdict: confirmed improvement on Inkling 8192/256.**
Keep the decode-only policy as an opt-in candidate. Other prompts,
the joined bridge, 32K/512 and Apex qualification remain open.

Follow the EXP-1961 frequency simulation. Preserve the common cache
metadata, slot ownership, selected experts, upload order, and the
existing Goose expert staging. On a decode miss, choose among resident
slots by increasing frequency, last visit and expert ID. Keep all
active experts pinned for that visit. Scan physical slots once and
retain at most eight victims, without sorting the entire cache.
Update frequencies once per successful gate/up visit and clear them
when a new prefill starts. Prefill and the Apex branch still take
their original paths.

The trace request reproduces all 12240 route masks and the starting
cache snapshot from EXP-1961. The real implementation matches the
simulated 4213 fills and 18171039744 decode bytes exactly. SLRU uses
6479 fills and 27944497152 bytes. Prefill stays at 5389 fills and
23243231232 bytes in every arm.

## Inkling 8192/256

Same final binary, fresh server and one request per arm, A–B–B–A.
RTX 4070, context 36864, batch and ubatch 4096, Q4_0 K/V and a
6720 MiB expert limit. Joined bridge and route tracing are off in
the timing block. Only `I2_DECODE_LFU` changes.

| Arm | Prefill tok/s | Decode tok/s | Decode ms | Client s |
| --- | ---: | ---: | ---: | ---: |
| A1 | 279.328 | 19.848 | 12847.956 | 42.478 |
| B1 | 278.855 | 21.127 | 12069.844 | 41.746 |
| B2 | 278.925 | 21.232 | 12010.106 | 41.670 |
| A2 | 278.259 | 19.782 | 12890.269 | 42.623 |

Mean decode improved from 19.815 to 21.180 tok/s (+6.89%).
The paired savings are 778 and 880 ms, about 3.25 ms per
inter-token step on average. Prefill is nearly flat (278.793
to 278.890 tok/s). Mean client time went from 42.550 to
41.708 s. Decode upload volume fell 35.0%.

All 256 output IDs, text and 8703 fingerprints match. The
board peak is 11726 MiB across all arms. Peak process RSS
increased by a maximum-to-maximum 792 KiB. There was no
process swap or system swap-out. No Apex benchmark ran in
this block. The LFU policy is selected only on supported
Inkling single-token decode; the working checkpoints remain
preserved.

## Reproduction

Build with `bash source/build.sh /absolute/new/scratch`. Use
`ASSERT_SOURCE_DIR` pointing to the absolute original
`build/inkling-apex-gate/decode-lfu-1962` directory for
byte-identical assertion paths. The normalized rebuild
reproduced the binary exactly with KeelWing v0.3.0 and GCC 13.

Run the EXP-1955 fixture with argv and env from `results.json`.
Set `I2_DECODE_LFU=0` for control and `1` for the candidate.
Use `I2_EXPERT_ROUTE_TRACE=1` only for the routing receipt.
Raw evidence lives under `build/inkling-apex-gate/decode-lfu-1962`.
