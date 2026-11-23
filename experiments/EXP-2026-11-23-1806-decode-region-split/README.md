# EXP-2026-11-23-1806 — Submit the post-FFN decode region early

Status: **CANDIDATE, not accepted yet** (all pairs favour B, but two of three pairs had
system swap-out; a clean confirmation is pending). Base: EXP-1805 checkpoint.

## Mechanism (`source/cuda-driver.patch`)

The decode host records the region that follows a routed FFN (next layer's norm, projections,
mixer and router) and only submits it at the route readback barrier. CPU timing showed about
74 us of host recording plus 27 us of graph launch after each FFN submission, while the FFN
itself runs about 50 us on the GPU, so the GPU idles (Nsight: 24 us average gap between
`goose_moe_add_shared_residual` and the next `goose_rms_decode_2048`).
`GOOSE_DECODE_SPLIT=N` submits the buffered region once after N kernels so the GPU starts the
next layer while the host records the rest; `GOOSE_DECODE_SPLIT_COUNT` limits the number of early
submissions per region. Kernels and their order are unchanged; the region cache grows from 128 to
256 graph execs per step. The early submission is disarmed at the routed FFN entry (an early
submission inside the FFN sequence failed with CUDA 401 at N=8 before that fix).

## Results (Apex 32K/512, ctx 262144, cache 6290 MiB, all answers exact)

Sweep, one arm each: N=2/count 2 80.60, N=3/2 81.01, N=4/2 81.02, N=6/2 77.12, N=8/2 78.03,
N=3/1 81.06, **N=4/1 81.22**, N=3/3 79.99 tok/s.

| Pair | Decode A -> B, tok/s | system pswpout A / B |
| --- | ---: | --- |
| A1 / B1 | 79.67 -> 80.70 | 0 / 1002 |
| B2 / A2 | 81.20 vs 80.53 | 0 / 0 |
| A3 / B3 | 80.04 -> 81.29 | 66770 / 3411 |

Prefill unchanged (3081-3104 tok/s), full request shorter in every pair, peak VRAM 8774 vs
8770 MiB, process VmSwap 0. Raw data: `results/ab.tsv`.

Next: one clean pair with zero system swap-out, then make N=4, count 1 the default
(`GOOSE_DECODE_SPLIT=0` to disable) and fold it into the cumulative build.
