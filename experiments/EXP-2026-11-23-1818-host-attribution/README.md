# EXP-2026-11-23-1818 — Request-only host attribution: the driver's own threads

Status: **MEASUREMENT, lead closed by attribution.** Baseline `37ac0f1d`
unchanged (prefill 3122.4, decode 78.225).

## Method

Instruction-pointer sampler with two dumps: one immediately after `/health`
(startup discarded, counter reset), one after a 32768-token request, so only
request-time processor time is counted. `runs/hostattr-020426`.

## Result

| mapping | samples | share |
| --- | ---: | ---: |
| `libcuda.so.580.178.04` | 9 044 | **88.3 %** |
| `frankenmoe-infer` | 681 | 6.7 % |
| `libc.so.6` | 514 | 5.0 % |

Inside the executable the same generated functions dominate as before:
`gg_function_173` 25.8 %, `gg_function_181` 14.8 %, `gg_function_250` 12.8 %,
`gg_div` 11.3 %, `gg_mod` 6.0 %, `fg143_mmq_tiles` 2.8 %,
`fg_cuda_expert_batch` 1.5 %.

## The contradiction that closes the lead

The complete CUDA API histogram of the same request sums to **5.64 s**:

```
cuMemcpyHtoD_v2        1860 calls  4897 ms
cuMemcpyDtoH_v2         165 calls   464 ms
cuMemcpyHtoDAsync_v2  80548 calls   184 ms
cuLaunchKernel        11582 calls    41 ms
everything else                      55 ms
```

So the driver holds 88 % of the processor samples while its API entry points
account for less than half of the request. The remainder is spent in the
driver's **own internal threads**, driven by the number of transfer operations:
80 548 asynchronous expert copies and 1 860 synchronous plan copies per request.

That is why five separate attempts to change the transport from the outside
(packet ring, `decode_upload` ring, plan stream, two device slots, windowed
upload stream) all moved the work by less than 3 %: the cost is not in our
submission path, it is in the driver's per-operation machinery, and the operation
count is set by the miss pattern.

## What would actually reduce it

Reducing the number of transfer *operations* rather than their scheduling. The
misses are scattered runs, so coalescing them means copying resident experts too:
a full 256-expert plane per layer-ubatch is 158 MB and 960 of those would move
152 GB against 55 GB today, three times the bytes, which loses. There is no
middle ground without a different eviction layout, and the Belady bound already
shows the policy has only 2.3 % of headroom.

Prefill therefore stands at 3122.4 tok/s in the current block conditions
(~3248 in the quiet-machine equivalent) with every reachable axis measured.
'

## Artefacts

`runs/hostattr-020426` (two dumps), `runs/waittrace-020249` (the wait timers that
showed no event or stream waits), `runs/fullprof-000121` (the API histogram).

