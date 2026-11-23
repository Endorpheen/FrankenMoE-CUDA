# EXP-2026-11-23-1807 — Apex prefill is bound by the host, not the GPU

Status: **MEASUREMENT.** No candidate yet; the reference binary and every saved
response are untouched. Base: accepted checkpoint EXP-1805,
`checkpoints/exp1805-route-prefetch/frankenmoe-infer`,
SHA-256 `844c5a161965463ad21fd5f6fa926c6ee7c3a2db0d43dfb5cb315b4bcb657f4c`.

## Question

Prefill is 3081-3104 tok/s on the fixed profile. The next target is 3500, which
means `prompt_ms` falling from about 10557 to 9362 for 32768 prompt tokens. What
actually bounds it?

## Measurement

New tools, all in `tools/`: `prefill-profile.sh` (server under nsys, bounded by
the first MMQ kernel, no NVTX range), `prefill-budget.py` (window budget),
`prefill-scale.sh` (prompt-length scaling with per-request processor time) and
`prefill-sample.sh` with `tools/sampler.c` (LD_PRELOAD instruction-pointer
sampler, needed because `perf_event_paranoid=4` and yama ptrace both refuse).

### The GPU does the prompt in 6.43 s, not 12.5 s

`runs/prefill-prof-A0-20261123-195310` (one 32768-token prompt, one predicted
token, nsys):

| quantity | value |
| --- | --- |
| MMQ kernel span | 6.427 s |
| kernels (union) | 3.980 s (61.9 %) |
| memcpys (union) | 2.294 s (35.7 %) |
| union | 6.025 s (93.7 %) |
| GPU idle inside the window | 0.402 s (6.3 %) |
| expert staging HtoD | 51.98 GB in 2.283 s |
| `prompt_ms` reported for the same request | 12 746.6 (2571 tok/s) |

The GPU timeline is 94 % busy and needs 6.43 s for the prompt: **5098 tok/s is
what the GPU alone sustains.** The other ~6 s of `prompt_ms` is host time.

### The host burns processor time in proportion to the tokens

`runs/prefill-cpuscale-20261123-201015`, one server, processor time read from
`/proc/<pid>/stat` around each request:

| prompt tokens | `prompt_ms` | tok/s | processor time | ms per 1000 tokens |
| ---: | ---: | ---: | ---: | ---: |
| 4096 | 1510.7 | 2711.4 | 1.75 s | 427 |
| 8192 | 2910.6 | 2814.5 | 3.31 s | 404 |
| 16384 | 5823.7 | 2813.3 | 6.58 s | 402 |
| 32768 | 12374.3 | 2648.1 | 13.77 s | 420 |

The line is linear with no meaningful intercept, and processor time equals wall
time, so the work is serial. Per token the host spends 410 us and the GPU 196 us:
the host is the bound, by 2.1x.

### The expert staging path is not the cost

`runs/prefill-phase-20261123-200500`, instrumented diagnostic build
`/tmp/opencode/apex-pfstage` (`GOOSE_PREFILL_TRACE=1`, source patch kept with the
run):

```
Goose prefill phases: ec_prepare 2160 calls, 0.22 s total, 100.5 us each
Goose prefill staging: staged 198274 jobs/130.11 GB, pinned 198274/130.11 GB,
  ring memcpy 0/0.00 GB, parallel memcpy 0/0.00 GB, direct 0/0.00 GB
```

Two requests stage 130 GB, every job pinned, and no host memcpy happens at all:
the transfer is pure DMA. The SLRU plan costs 0.22 s. The 55.3 GB that the
acceptance log reports as prefill temporary uploads is this DMA, and it is
already overlapped inside the 6.43 s GPU window.

### Where the processor time goes

`runs/prefill-rip2-20261123-201213`, instruction-pointer sampler at 1 ms of
processor time over one 32768-token request, resolved against the binary's symbol
table (7.0 % of samples land in the executable's text, the rest in shared
libraries):

| share of in-executable samples | symbol |
| ---: | --- |
| 20.0 % | `gg_function_173` |
| 11.7 % | `gg_function_181` |
| 9.6 % | `gg_function_250` |
| 6.8 % | `gg_function_4903` |
| 5.8 % | `gg_div` |
| 5.2 % | `gg_owned_unwind_checked` |
| 4.7 % | `gg_mod` |
| 4.4 % | `gg_function_113` |
| 3.7 % | `gg_function_1004` |
| 2.7 % | `fg_cuda_expert_batch` |

These are Goose-generated functions: the module arithmetic (`gg_div`, `gg_mod`)
and the ownership check together take 15.7 % of the server's own time, and the
expert batch door 2.7 %. `fg_cuda_expert_batch` has one caller,
`goose/cpu/goose-cpu-backend-api/matrix.goose:332`, the CPU backend matrix path
that submits expert jobs one record at a time.

## What this means for 3500 tok/s

The target needs `prompt_ms` 9362 for 32768 tokens. The GPU alone needs 6.43 s
(5691 tok/s would be free if the host kept up), so the target lives entirely in
the host path, and the host path is the per-token expert-job submission with its
generated-code arithmetic. Removing 3.0 s of the 13.8 s host cost reaches the
target; removing all of it would land near 5000 tok/s.

## Next

1. Count the host calls per token: `fg_cuda_expert_batch` invocations, job
   records per call, and the arithmetic each record performs, on the 4096-token
   prompt where the whole request is 1.5 s.
2. Batch the records per ubatch instead of per token, or move the plan into the
   prefill workspace path that already exists (two 105 MiB planes, 627 prefetch
   hits per request).
3. A/B against `844c5a16` with the warm-up protocol (one warm-up and one measured
   request: a single cold request reads 2650 tok/s, the warm measured one 3103),
   the 512 IDs, the text `f8664331bd30…`, VRAM, RSS and swap.

## Artefacts

`runs/prefill-prof-A0-*`, `runs/prefill-scale-A0-*`, `runs/prefill-cpu-*`,
`runs/prefill-perf-*`, `runs/prefill-stacks-*`, `runs/prefill-stage-*`,
`runs/prefill-phase-*`, `runs/prefill-samples-*`, `runs/prefill-rip-*`,
`runs/prefill-rip2-*`, `runs/prefill-cpuscale-*`; tools as listed above; the
instrumented source is `/tmp/opencode/apex-pfstage/build-inputs/cuda-driver.c`
and its patch is being kept with the card.
