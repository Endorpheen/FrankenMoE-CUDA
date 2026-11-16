# EXP-2026-11-15-738 — thread count is not a lever; the configuration space is exhausted

Base: `33e4dccd`. At ub 2048 / cache 5376, `-t 8` was never re-checked against other thread counts.
Done now.

## 1. Result

| `-t` | prefill | decode |
|---:|---:|---:|
| 8 (baseline) | 2019.3 | 42.35 |
| 12 | 2020.2 | 42.35 |
| 16 | 2007.4 | 42.15 |

**Identical within noise.** That is consistent with the profile: the GPU is at 96% during decode and
the CPU is blocked in `cudaStreamSynchronize` 72% of the window, so extra host threads have nothing to
do. `-t 8` stays.

## 2. The configuration space is now exhausted

Every lever reachable without changing code has been measured and closed:

| lever | verdict | evidence |
|---|---|---|
| expert cache size | **5376 MiB is a threshold** | 5504 is 1.9 tok/s slower; 5632 intermittent OOM |
| batch size (`-ub`) | **2048 optimal** | 1536 and 1024 cost prefill with no decode gain |
| thread count | parity | this card, 12 and 16 against 8 |
| CPU masks | parity | 00ff +0.3%, ffff -1.3%, both inside spread |
| polling | parity | `--poll 50 --poll-batch 1` = 39.80 vs 39.86 |
| `--n-cpu-moe` | **impossible** | 14-15.2 GiB allocation on a 12 GiB device |
| `--no-kv-offload` | **unsupported** | aborts inside libggml-base |
| `--cache-ram` | not a lever | host RAM, not VRAM |
| CUDA dispatch ablations | parity | MMQ_DISPATCH/MM_IDS/Q8_1 all within noise |
| CUDA env knobs | parity | GRAPH_OPT, PDL, MMQ_PREC, REGISTER_HOST |
| speculative decoding | **closed** | MTP needs 2x weights; ngram -7.1%; draft-simple parity |
| KV cache type | candidate only | f16 +1.2% but **changes the answer** |

## 3. Where the remaining time is, measured

```
decode window (512 tokens)      17.0 s
  kernel work                    7.93 s   46.7%
  expert transfer at 21 GB/s     4.24 s   24.9%   (PCIe saturated)
  neither (GPU idle)             4.83 s   28.4%
```

and the CPU side:

```
cudaStreamSynchronize  6,475 calls  12.30 s   72% of the window
  of which 321 calls >10 ms hold 75% of that time
  one long stall every 1.59 tokens, after 1.51 tokens of queued work
```

Every one of those numbers points at the same thing: **the work is correct and the link is saturated;
the cost is 12.6 pipeline drains per token.** That is a graph-construction property, not a
configuration.

## 4. What was achieved this session

| milestone | value |
|---|---|
| first end-to-end Apex on the integrated Goose runtime | 855.71 prefill / 18.21 decode, output identical to reference |
| packaging fixes (three wrong libraries) | 855.71 -> 2429.9 prefill |
| cache sweep with a threshold found | 39.86 -> 42.35 decode at ub 2048 |
| **best decode measured** | **42.74** (ub 1024 / cache 5632) |
| against the historical reference | decode 42.35 **exceeds 41.94 by 1.0%**; prefill 2019 is 9.4% below |
| standalone now carries the Apex unit | 14 Apex exports, both gates passing |
| own C++ converted to C | graph-bindings and tensor-data, 0 C++ symbols imported |

## 5. What is left, and it is not configuration

One lever, named consistently since EXP-726 and never attempted: **issue expert copy nodes earlier in
the graph so the scheduler can overlap them with compute.** It is worth up to 4.24 s of a 17 s window
and needs no memory. It also carries a real risk - an expert read before its copy lands changes the
answer - so it needs its own experiment with a hash gate, not an afternoon of tuning.

## 6. Cost

Three configurations, six runs, no failed starts.
