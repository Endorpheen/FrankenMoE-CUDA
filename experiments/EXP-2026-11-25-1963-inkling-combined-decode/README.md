# EXP-2026-11-25-1963: Combine the Inkling decode cache and CUDA bridge

**Verdict: combined candidate confirmed on 8192/256.**
Use it for further research. Full-context Inkling and release
qualification remain open.

No source change. Use the exact EXP-1962 binary. The accepted
experimental frequency policy is on in both arms. Toggle only
`I2_JOIN_ATTENTION_MOE` to measure the additional CUDA bridge.
Retained bridge history from EXP-1960 is outside this build.

## Inkling 8192/256

RTX 4070. Fresh process and one request per arm in A–B–B–A order,
with context 36864, batch/ubatch 4096, Q4_0 K/V and a 6720 MiB
expert limit.

| Arm | Prefill, tok/s | Decode, tok/s | Decode, ms | Peak board, MiB |
| --- | ---: | ---: | ---: | ---: |
| LFU A1 | 278.921 | 21.256 | 11996.402 | 11804 |
| Both B1 | 279.140 | 22.047 | 11566.420 | 11806 |
| Both B2 | 278.647 | 22.027 | 11576.546 | 11770 |
| LFU A2 | 278.848 | 21.553 | 11831.084 | 11720 |

Mean prefill is 278.885 → 278.894 tok/s, mean decode
21.405 → 22.037 tok/s (+2.95%) and client time 41.585 → 41.251 s.
The bridge saves 430 and 255 ms in the two pairs. Compared with
the fixed upstream reference from EXP-1955 the remaining decode
gap is about 4.21 ms per inter-token step, across separate blocks.

All 256 output IDs, text, 8703 state fingerprints and final
`/props` match across both modes. The combined runs preserve
decode traffic at 4213 expert fills and 18171039744 bytes.
The maximum board peak is 2 MiB higher, while board usage
before launch varies from 1925 to 2008 MiB. The maximum process
RSS is lower by 240 KiB. No process swap or system swap-out.

## Separate prompts and reset

The comparison starts a fresh server for each mode. Send the
English code prompt, the Russian systems prompt, and the same
English prompt again in that order to each server. The input
lengths are 36, 45 and 36 tokens with 64 output tokens per request.
Use unmodified SLRU/separate graphs as control and LFU/joined
graphs as the candidate. All 192 output IDs, all text and 498
state fingerprints match between modes. The repeated English
request also matches its first response. These are correctness
checks, rather than throughput claims for short prompts.

## Apex 32K/512

Test the frozen pinned settings with context 262144 and both
Inkling flags present on the EXP-1962 binary. Compare with
EXP-1953. The initial control is excluded because there was
system swap-out during its request. The subsequent candidate and
repeated control have zero in-request system swap-out and zero
process swap. The candidate incurred system swap-out at startup.

Prefill is 2893.758 → 2874.836 tok/s (-0.65%), decode
79.424 → 80.166 tok/s (+0.93%) and client time
17.853 → 17.873 s. The board peaks are 10718 and 10740 MiB.
Process peak RSS is 21475976 and 21461820 KiB. The entire
512-token response and post-request `/props` match. One usable
pair qualifies correctness on this case and does not establish
a release non-regression result.

Build using EXP-1962 and verify the binary hash in
`results.json`. Run the EXP-1955 Inkling request or the frozen
Apex EXP-1880 request with the recorded argv/env. The short
fixtures are in `prompts`. Raw responses are in
`build/inkling-apex-gate/combined-1963`. No working binary or
production profile changed.
