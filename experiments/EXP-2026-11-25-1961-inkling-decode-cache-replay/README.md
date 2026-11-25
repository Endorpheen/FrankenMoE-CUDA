# EXP-2026-11-25-1961: Replay the Inkling decode expert cache

**Verdict: prioritize a decode-only frequency policy for a live test.**
This experiment measures routing and simulates evictions. It does not
claim a model throughput improvement.

Add a gated trace hook to the accepted CUDA expert preparation path.
Capture the initial physical slot contents and one 128-bit active set
per gate/up projection during the 8192/256 decode. The first attempt
with the joined bridge failed to allocate at its first decode step.
The retry used the separate attention and MoE graphs and the same
cache, prompt, sampler and Q4_0 KV. All 256 IDs, text and 8703 state
fingerprints match EXP-1955. There was zero process swap or system
swap-out and no request storage read. The trace reached all 12240 route
groups across 255 decode steps.

There were 1633 resident slots at the start of decode and 2337 distinct
experts requested across the continuation. Each fill copies two planes
totaling 4313088 bytes. Actual SLRU fills and bytes come from subtracting
the prefill snapshot from the completed request counters.

## Replay results

All replays start from the captured resident set, hold all eight
required experts for each visit, and admit on demand without speculative
copies. Frequency starts at zero, updates from the current route after
victim selection and breaks ties by last use. The offline oracle evicts
the expert whose next use is furthest away. It is a bound for this
demand-cache model. It does not represent an implementable predictor.

| Policy | Fills | Expert H2D, GB |
| --- | ---: | ---: |
| Actual SLRU | 6479 | 27.94 |
| Replay LRU | 6753 | 29.13 |
| Replay using previous token's route | 6753 | 29.13 |
| Frequency, no decay | 4213 | 18.17 |
| Frequency, halve every 2 tokens | 6753 | 29.13 |
| Frequency, halve every 8 tokens | 6749 | 29.11 |
| Frequency, halve every 32 tokens | 5208 | 22.46 |
| Offline oracle | 2580 | 11.13 |

The fixed-capacity offline headroom is 60.2% of expert payload. The
best causal policy saves 2266 fills and 9.77 GB (35.0%) on this trace.
At the earlier measured 18.49 GB/s that volume is about 2.07 ms of copy
service per decode step. An actual time improvement will depend on
overlap, submission and eviction cost. Do not add this estimate to
other overlapping timing categories.

Neighboring tokens share only 32.5% of routes at the same logical
visit on average. Favor tracking longer-term frequency over replaying
one prior token. Test the same count and tie rules in a bounded host
policy, preserve prefill placement and graph settings, and compare
multiple prompts for locality sensitivity. The current cache object
and Apex code path were left unchanged apart from instrumentation.

## Reproduction

Build with `bash source/build.sh /absolute/new/scratch` using the local
EXP-1959 files. Use `ASSERT_SOURCE_DIR` pointing to the absolute
original `build/inkling-apex-gate/expert-trace-1961` directory for
byte-identical assertion paths. Use the argv and env in `results.json`,
including `I2_EXPERT_ROUTE_TRACE=1`. After one 8192/256 request, run:

```bash
python3 replay.py /absolute/path/server.log --measured-fills 6479
```

The raw trace is under `build/inkling-apex-gate/expert-trace-1961`.
The committed results contain the aggregate counters and per-token
replay counts. No production build or launch profile changed.
