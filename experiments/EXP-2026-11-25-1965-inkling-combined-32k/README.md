# EXP-2026-11-25-1965: Measure combined Inkling at 32K/512

**Verdict: faster full response than the historical upstream reference.
Decode is within 0.7% of that reference in separate blocks.**

Run the first 32768-input, 512-output request on a fresh instance
of EXP-1962 with both the LFU cache and the joined CUDA bridge enabled.
Retain the EXP-1963 context 36864, Q4_0 K/V, logical batch and ubatch 4096,
6720 MiB expert limit and CPU settings. The local Inkling weights were
read into the OS page cache before starting the server. This is a first
request in a fresh process with cached model pages. It is not a repeat
request after model inference. Zero prompt tokens were reused.

| First request, Inkling 32K/512 | Prefill tok/s | Decode tok/s | Client s | Peak board MiB |
| --- | ---: | ---: | ---: | ---: |
| EXP-1943 Goose checkpoint | 247.12 | 17.63 | See EXP-1943 | See EXP-1943 |
| EXP-1962 LFU + CUDA bridge | 267.38 | 21.22 | 146.938 | 11794 |
| Historical llama.cpp EXP-1927 | 138.44 | 21.37 | 260.634 | 11282 |

The upstream row uses the identical byte-for-byte fixture,
with its stronger measured CUDA placement and ubatch 128.
All 512 output IDs and text match. The combined Goose process
used 14228848 KiB peak RSS, with no process swap, no system
swap-out and no storage reads during the request. Its expert
transfers were 89539706880 bytes in prefill and 28280918016
bytes in decode. Each runtime has one observed request from
a different measurement block. The approximate 0.33 ms per
decode step gap to upstream does not establish parity.

Use the binary and exact argv/env in `results.json`, then
submit the recorded request to `/completion` after `/health`.
Raw receipt is under
`build/inkling-apex-gate/inkling-full-1965/combined-first`.
No runtime code, production profile, Apex binary or model changed.
