# EXP-2026-11-25-1964: Reproduce warm Apex prefill on the unified runtime

**Verdict: first-request and warm measurements were previously mixed.**
The unified runtime retains the warm Apex path. The remaining absolute
gap to the historical peak exists on the frozen binary as well.

EXP-1880 scored the second 32K/512 request on each fresh server.
Its B1 first request was 2934.35 tok/s prefill and its scored
request was 3515.98 tok/s. EXP-1954 and EXP-1963 recorded one
request on each fresh server. Prompt length, context, batch,
ubatch, expert cache and token settings matched, but the
prefill execution state differed.

The driver's `prefill_wait_decode` gate holds the temporary
prefill expert workspace until a decode step sets
`cache->decode_seen`. At startup, logs show zero temporary
prefill projections; after the second request, both binaries
show 960 projections, 59856 resident planes, 55334240256
uploaded bytes, and 627 prefetched projections. The second
request recomputes all 32768 input tokens (`cache_n=0`).

| Binary | First prefill / decode, tok/s | Second prefill / decode, tok/s |
| --- | ---: | ---: |
| EXP-1962, both Inkling flags present | 2857.43 / 79.54 | 3406.32 / 80.39 |
| Frozen EXP-1880 | 2856.19 / 78.00 | 3392.36 / 80.29 |

Both scored requests match all 512 IDs, text and post-request
properties. Request-interval system swap-out and process swap
were zero for both scored requests. Peak process RSS differs by
272 KiB. This is one launch per binary with one warm-up and one
scored request. The reference's historical 3505.42 tok/s paired
mean was not reproduced. This test does not assign the remaining
absolute difference to any individual hardware or host factor.

No runtime, model, operational profile or Inkling build changed.
For comparisons to the historical Apex score, use one complete
32K/512 warm-up on each fresh server, then score the next identical
request. Keep first-request and warm numbers separately. The exact
argv, env and raw run paths are in `results.json`. Use
`build/checkpoint-apex-exp1880-before-inkling/evidence/request.json`.
