# EXP-2026-11-25-1941: Stage Inkling prefill Q4_0 KV for Flash

**Verdict: INCONCLUSIVE for full throughput.** Both staging
variants preserve the tested arithmetic. The workspace reuse
variant enables staging on all full-attention visits without an
additional allocation. The measured request gain is small and
one observation per arm is available. Keep EXP-1940 as baseline.

## Mechanism

EXP-1940 unpacks quantized keys and values in the Flash CTA.
The candidate materializes the same FP16 values once per
eligible visit and passes the results to a separate batched
Flash entry. Its persistent KV cache remains Q4_0. The partial
kernel, masks and merge retain their reference FP32
calculation order. Scope is full-attention Q4_0 prefill from
4096 keys, with at least 16 queries in the input batch and
the EXP-1940 parallel relative-score path enabled. Sliding
attention and decode keep their original paths.

The first variant allocates a separate device workspace.
It falls back when the allocation would leave insufficient
headroom. The final variant writes into unused prefixes of
the input projection workspaces. It begins after K/V and Q
have already been prepared. It guards the raw prefix against
overlap with the live sink and gate rows. Both buffer capacities
are validated before dispatch. Use `I2_PREFILL_STAGE_KV=0` for
the original Flash input path.

## Device screen

Seeded Q4_0 KV, RTX 4070, partial output and metadata match
byte-for-byte on six shapes. Unpack timing is one pass over
each K/V pair. The same prepared pair can feed multiple
128-query kernel launches.

| Keys | Queries | Q4 Flash, ms | Staged Flash, ms | Unpack, ms |
| ---: | ---: | ---: | ---: | ---: |
| 1024 | 16 | 0.304 | 0.266 | 0.006 |
| 1024 | 128 | 0.672 | 0.722 | 0.006 |
| 4096 | 16 | 0.431 | 0.376 | 0.016 |
| 4096 | 128 | 1.503 | 1.458 | 0.016 |
| 32768 | 16 | 1.465 | 1.245 | 0.450 |
| 32768 | 128 | 7.036 | 6.132 | 0.450 |

## Full 32K/512 screen

EXP-1940 profile: RTX 4070, context 36864, batch and ubatch
4096, Q4_0 K/V, wide vectors, 736 MiB arena, expert cache
limit 6720 MiB. All profiles use the same requests and full
prompt recomputation. The control is the saved EXP-1940 run.

| Full request | EXP-1940 | Extra buffer | Reuse |
| --- | ---: | ---: | ---: |
| Prefill, tok/s | 219.015 | 218.716 | 220.058 |
| Decode, tok/s | 17.657 | 17.795 | 17.805 |
| Client, s | 179.034 | 179.017 | 178.090 |
| Peak board VRAM, MiB | 11731 | 11751 | 11750 |

The separate buffer only reached 16 MiB. The workspace reuse
path reached 128 MiB by reusing existing addresses and logged
all 64 full-attention visits. It saved 709 ms of prefill and
943 ms end to end in the single comparison. The prefill gain
was 0.48%. All 512 output IDs and text and 89.54 GB of prefill
expert uploads matched. Both candidates had zero process swap
or system swap-out. The reuse correctness request at 4096/128
matched 4351 fingerprints and all output IDs and text.

The independent kernel speedups did not translate to a large
request gain. The next trial should spend effort on the other
prefill costs or a larger scheduling change. Production
settings stayed intact. The frozen Apex manifest passed; no
Apex performance check ran.

## Reproduction

Apply the Flash and driver patches to EXP-1940 in the isolated
sibling layout. For the first candidate use `extra-buffer.patch`
in place of the final header patch. Use the supplied build
helper and `bench-staged.py`. The scripts link against EXP-1940
without rebuilding the common graph code. See `inputs.sha256`,
`compiler.ptxas`, `staged-bench.json` and `results.json`.
