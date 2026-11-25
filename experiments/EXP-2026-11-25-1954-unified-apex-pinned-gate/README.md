# EXP-2026-11-25-1954: Check the combined executable on pinned Apex

**Verdict: pinned Apex correctness and in-request memory checks pass.
No material speed loss was observed in this block.** The historical
clean startup and absolute-throughput gate remains open.

Compare frozen EXP-1880 (A) with the exact EXP-1953 combined executable (B)
on RTX 4070 in fresh-server order A–B–B–A. Use the unmodified frozen argv
and environment, including context 262144, batch/ubatch 4096, Q4_0 K/V,
6290 MiB expert cache and host registration. The input has 32768 IDs
and each response has 512 tokens. Board usage immediately before each
launch was 1914–1915 MiB. A read-only `mincore` check before the block
confirmed all 5301139 model file pages in the OS cache.

| Arm | Prefill, tok/s | Decode, tok/s | Client time, s | Peak board, MiB | Peak process RSS, KiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| A1 | 2870.508 | 76.671 | 18.183 | 10730 | 21470364 |
| B1 | 2891.747 | 79.551 | 17.858 | 10730 | 21477448 |
| B2 | 2870.442 | 79.944 | 17.904 | 10730 | 21476024 |
| A2 | 2882.811 | 80.166 | 17.841 | 10730 | 21472564 |
| A mean | 2876.659 | 78.419 | 18.012 | 10730 | 21471464 |
| B mean | 2881.094 | 79.748 | 17.881 | 10730 | 21476736 |

The mean changes were +0.15% prefill, +1.69% decode and -0.73%
client wall time. Both metrics changed direction between the two
pairs, so this does not establish an acceleration. Peak RSS was
5272 KiB higher on average with the combined binary.

All 512 IDs, text and final `/props` were identical in all arms. Each
run registered 120 expert ranges covering 19504005120 bytes and
recorded 1045991 hits, 72508 fills and 138222477312 bytes of traffic.
Process swap was zero at readiness and after inference. System
swap-out during every request was zero. Startup still caused system
swap-out, 188354 / 39455 / 17764 / 6897 pages in run order. This is a
same-block comparison of the frozen and combined executables. Both
remain below the historical EXP-1880 clean mean of 3505.42 prefill
and 82.10 decode. These data do not establish the cause of the
historical gap or qualify a no-swap startup.

## Reproduction

Build the candidate with [EXP-1953](../EXP-2026-11-25-1953-apex-inkling-single-executable/README.md).
Verify both executable hashes and the fixture hash in `results.json`.
Use the recorded `argv_after_binary` and `explicit_env` for both
binaries. Wait for `/health`, record `/props`, and submit one request:

```bash
curl -fsS --max-time 120 -w '%{time_total}\n' \
    -H 'Content-Type: application/json' \
    -d @build/checkpoint-apex-exp1880-before-inkling/evidence/request.json \
    http://127.0.0.1:9101/completion -o response.json
```

Read `pswpout` before launch, at readiness and after the request.
Read VmSwap and VmHWM from `/proc/PID/status` at readiness and after
the response. Sample board VRAM every 250 ms. Stop the test process
before starting the next arm. Raw data are under
`build/inkling-apex-gate/pinned-1954`. No production checkpoint,
launcher, model or runtime source was changed.
