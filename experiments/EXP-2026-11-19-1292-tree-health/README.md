# EXP-2026-11-19-1292: tree health after the sampling work

Run at the point where the numeric investigation was stopped, so the state is recorded rather than remembered.

```
verification suite        18 passed, 0 failed, 1 stale
check-unresolved-symbols  ok - every undefined symbol is provided
check-abi-pairs           agrees (fg386s_pick, fg386s_enabled)
check-unwired-modules     PASS
acceptance (512 tokens)   sha c2ff42cc1219, decode 45.09-46.16, prefill 1968-2286
```

The acceptance hash is the same one the session started with, re-run after every change that could reach the
sampling path beyond the profile's configuration - which none did, because the fixed profile enables neither DRY
nor typical, XTC or adaptive-p.

## What the session changed in the shipped runtime

| change | why it was kept |
|---|---|
| five index expressions in the DRY functions | the context was read in the opposite order to the one the caller builds, so a context ending in a single-token breaker received a penalty the native never applies |
| a pre-ranking sort before the XTC door | the native sorts before it ranks probabilities, and the door branch ran before that sort |
| `mt_fmt_g6` provider in the shim | the metrics module declared it and nothing defined it, which left the library unloadable |
| the XTC door's `sorted` flag | measured over 100 rounds: the survivors stay descending, so the flag is true as on the native path |

## What it did not resolve

The 15-21 round difference in `typical+xtc+adaptive` chains, attributed to adaptive's door by mixing the switches,
with `p` differing by one ULP and the residual in the draw. Off the acceptance path, recorded with the next probe
named rather than papered over.
