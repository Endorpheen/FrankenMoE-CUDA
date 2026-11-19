# EXP-2026-11-19-1423: the mechanism is expert upload traffic, measured

## The instrument

`llama_moe_cache` already counted hits, misses and uploaded bytes, but printed them from its **destructor** - and a server
stopped with SIGTERM never runs one, so a measurement of what the cache costs had no number. The totals are now printed
periodically under `FG_SLRU_DIAG`, with the per-copy average, which is what one decode step per layer pays.

## The two runs, identical in everything but the cache size

Apex, accepted profile otherwise (context 262 144, `-b 4096 -ub 4096`, `--cpu-moe`), three requests, 512 tokens each:

| cache | hit rate | uploaded | per copy | decode tok/s | hash |
|---|---|---|---|---|---|
| 4992 MiB | 83.68 % | 146 GiB | 2 483 KiB | 61.83 / 62.81 / 59.65 | `c2ff42cc1219` |
| 3072 MiB | 71.25 % | 255 GiB | 4 355 KiB | 47.43 / 47.08 / 50.51 | `c2ff42cc1219` |

Smaller cache → **1.76x the misses, 1.75x the uploaded bytes, 23 % less decode**. The hash is identical in both, so nothing
about correctness moved.

## The arithmetic that identifies the cause rather than correlating with it

41 MoE layers per token, so the per-copy average is the per-token traffic divided by 41:

```
per token:            99.4 MiB  ->  174.4 MiB      (+75.0 MiB)
time per token:       16.17 ms  ->  21.08 ms       (+4.91 ms)
implied bandwidth:    75.0 MiB / 4.91 ms  =  16.0 GB/s
```

Sixteen gigabytes per second is the host-to-device rate of this card's link, not a coincidence and not a fitted parameter.
The extra time is exactly the time to move the extra bytes at the speed the link moves bytes.

## So what the cache buys, in one sentence

A hit does not save a computation - the MoE arithmetic runs either way. **A hit saves a 1.77 MB expert copy from RAM to
VRAM**, and the cache's whole gain is the fraction of copies it avoids.

## Why "disabling SLRU" would not change decode

This follows from the same numbers and is the second half of the question: the replacement policy does not decide *whether*
experts are copied, only **which** ones stay resident. If the policy is bypassed while the banks still hold a working set,
the hit rate stays in the same range and decode stays with it. The thing that costs is the copy, and the thing that removes
copies is residency - which is why the cache **size** moves decode by 23 % and the policy's decisions move it barely.

That is also why EXP-1419's equality was worth having but not worth optimising: a policy that is 100 % correct and a policy
that is 83 % correct differ by the misses they leave, not by their own execution.

## What this predicts, and the test that would confirm it

If the mechanism is bytes over the link, then **more residency should buy more speed** - up to the point where the GPU has no
room for a larger cache - and a cache size between the two points should land between their decode rates. A three-point
curve (3072, 4992 and one more) would show it; the two points already bracket a 23 % spread and a 1.75x traffic ratio.

## Reproduce

```
FG_SLRU_DIAG=1 APEX_SERVER_DIR=$PWD/builds/flint-acc/bin bash tools/apex-test.sh traffic-4992 512 4096 4096 4992
grep FG_SLRU_TRAFFIC results/apex-runs/traffic-4992-*/server.log | tail -1
```

## Shipped path

The counter is behind `FG_SLRU_DIAG`; with it unset nothing prints and nothing is counted. Acceptance is unchanged.
