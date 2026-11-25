# EXP-2026-11-25-1970: Qualify the packed Inkling Q8 build on warm Apex

**Verdict: Apex regression gate passes for the EXP-1969 build.**
The prior Inkling result is now the active research baseline.
No deployed binary or production profile changed.

Compare EXP-1966 (A) and the EXP-1969 packed Q8 build (B) on
RTX 4070. Use the exact pinned Apex launch profile with context
262144, batch and ubatch 4096, Q4_0 K/V and expert cache 6290 MiB.
Both launches include the Inkling experiment flags. The packed
dispatch does not activate on Apex.

Order A–B–B–A, one full 32K/512 warm-up then one identical scored
request per process. All reported throughput below is from the
second request with `cache_n=0`. Both executables use the warm
temporary workspace with 960 projections, 59856 resident planes,
55334240256 uploaded bytes and 627 prefetches.

| Apex second request | Prefill tok/s | Decode tok/s | Client s | Board peak MiB |
| --- | ---: | ---: | ---: | ---: |
| A1 | 3417.550 | 79.997 | 16.069 | 10813 |
| B1 | 3419.555 | 80.527 | 16.018 | 10813 |
| B2 | 3417.964 | 80.273 | 16.041 | 10726 |
| A2 | 3419.297 | 80.366 | 16.029 | 10725 |

Means are 3418.423 → 3418.759 prefill and 80.181 → 80.400 decode
tok/s. Mean response time is 16.049 → 16.029 s. The sign changes
between the two pairs, and this result does not assert an Apex
speedup. All 512 IDs, text and final properties match. Peak board
VRAM is identical in the first pair and differs by 1 MiB in the
second; background board use is 1998 MiB for the first pair and
1910 MiB for the second. Maximum process RSS does not grow.

System swap-out is zero within each scored request and all four
processes have zero VmSwap at its end. Startup generated some
system swap-out pages in three arms, recorded separately in
`results.json`. The affected EXP-1969 attempts remain excluded.

The new [Inkling baseline](../../docs/INKLING_PERFORMANCE.md) is
296.24 prefill / 22.03 decode on the 32K/512 first-request
EXP-1969 fixture. The target remains 500/25 and subsequent
candidates should use this accepted reference.

For reproduction use the exact argv and env in `results.json`
and post the frozen EXP-1880 request twice to each fresh server.
Verify zero process swap and no increment in system swap-out
during the second request. Compare both responses and properties.
Raw receipts are under `build/inkling-apex-gate/apex-q8-gate-1970`.
