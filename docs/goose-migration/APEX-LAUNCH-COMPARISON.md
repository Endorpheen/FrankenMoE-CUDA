# Apex 32K/512 launch and residency comparison

EXP-1597 uses the native EXP-1476 release at `e7c9155a` and
independent Goose at `4f73f42e`. The model, request, 262,144
context, Q4_0 K/V, one request, CPU limits 8/16 and batch/ubatch
4096/4096 are fixed. The RTX 4070 reports 12,282 MiB device
capacity. Request SHA-256 is
`484a12de00ec057e4b78d480ebb185153bb10af607eb23192a72995ba3026d0f`.
The release and its experimental copy both pass their manifest
checks after testing.

## Exact server commands

The arguments below match `argv.txt` and `/proc/<pid>/cmdline`
for the unprofiled measured controls and the selected Goose
profile. The profiling and monitoring harness is outside these
server commands. The same Goose binary is archived under
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/gdn-decode-exp1595/frankenmoe-infer`.

### Original EXP-1476

```bash
env LD_PRELOAD= LLAMA_ARG_LOG_VERBOSITY=4 \
  LD_LIBRARY_PATH=/home/end0/AI/LlamaServer/frankenmoe-release-20261120-exp1476 \
  /home/end0/AI/LlamaServer/frankenmoe-release-20261120-exp1476/llama-server \
  -m /home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf --alias Apex-35B-V1.0 \
  -c 262144 -ngl all -fit off --cpu-moe -fa on -ctk q4_0 -ctv q4_0 \
  -b 4096 -ub 4096 -t 8 -tb 16 --cpu-mask 0f0f --cpu-mask-batch ffff \
  --cpu-strict 1 --cpu-strict-batch 1 --poll 0 -np 1 --cache-ram 0 \
  --ctx-checkpoints 32 --load-mode mmap --spec-type none --jinja \
  --temp 1 --top-p 0.95 --top-k 20 --min-p 0 \
  --host 127.0.0.1 --port 9101 --moe-cache-mib 4992
```

The [Optimized Hybrid](OPTIMIZED-HYBRID.md) profile uses the
isolated copy under `/tmp/fg-exp1596-profile.tqx5t1/hybrid-cache-copy/`,
its own `LD_LIBRARY_PATH`, and `--moe-cache-mib 5632`. All library
and executable bytes match the original.

### Independent Goose, selected 5,920 MiB profile

```bash
env -u LD_LIBRARY_PATH LD_PRELOAD= \
  GGML_CUDA_REGISTER_HOST=1 GOOSE_EXPERT_PIN_MIB=4096 \
  GOOSE_MMQ=1 GOOSE_GDN_MMA=1 GOOSE_FLASH_MMA=1 GOOSE_DENSE_MMA=1 GOOSE_MMVQ=1 \
  GOOSE_MMQ_Q4_REFERENCE=1 GOOSE_MMQ_Q6_REFERENCE=1 GOOSE_DENSE_MMA_REFERENCE=1 \
  GOOSE_SSM_REFERENCE=1 GOOSE_SSM_DECODE_REFERENCE=0 GOOSE_RMS_REFERENCE=1 \
  GOOSE_KV_LAZY_VMM=1 GOOSE_FLASH_VEC=1 GOOSE_FLASH_TAIL_MMA=0 GOOSE_FLASH_STREAM_K=0 \
  GOOSE_CUDA_ACTIVATION_MIB=384 GOOSE_CUDA_ACTIVATION_REFERENCE=1 \
  GOOSE_CUDA_STAGE_MATRIX_INPUT=1 GOOSE_CUDA_ASYNC_PACKETS=1 GOOSE_CUDA_EXPERT_OVERLAP=1 \
  GOOSE_PREFILL_CHECKPOINT_TAIL=1 GOOSE_MMVQ_DENSE_TAIL=1 \
  /tmp/franken-goose-gdn-exp1595.cUYWlO/build/frankenmoe-infer \
  --ctx-size 262144 --batch-size 4096 --ubatch-size 4096 \
  --cache-type-k q4_0 --cache-type-v q4_0 --expert-cache-mib 5920 --threads 8 --threads-batch 16 \
  /home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf --serve 9101
```

The Goose control changes only `--expert-cache-mib 4992`.
The unspecified RoPE and scalar GDN selectors use the accepted
defaults. The experimental scratch VMM and Attention CUDA Graph
selectors are unset in these runs. Request sampling is specified
by the exact archived JSON in both cases.

## Observed configuration

| Parameter or allocation | EXP-1476, original | Independent Goose |
|---|---|---|
| GPU placement | Verbose log counts 42/42 offloaded, including 40 repeating layers and output. Original routed expert weights remain host-side with `--cpu-moe`. | `/props` reports 40 trunk layers, with 1,361,551,360 bytes of resident dense CUDA weights. The executor places operators and weights individually. GPU layer counts are not a CLI abstraction here. |
| Dense CUDA allocation | 1,302.90 MiB | 1,298.48 MiB |
| Actual expert bank at 4,992 MiB budget | 4,991.48 MiB | 4,991.65 MiB |
| Actual expert bank at selected 5,920 MiB budget | Original stays at 4,991.48 MiB | 5,918.70 MiB |
| Model weights and host pinning | File-backed model with 18,600 MiB of host experts. Registered host transfers are present in the CUDA trace. | All 5,301,139 mapped pages resident. The 4,096 MiB host pin budget registers 28 spans totaling 4,227,973,120 bytes. |
| KV and recurrence | Q4_0 K/V, 1,440 MiB logical lazy CUDA KV, 200 MiB physical in this trace. CUDA recurrent state 62.8125 MiB. | Same formats, logical and physical KV, and recurrent state. `/props` reports logical KV and state together as 1,575,813,120 bytes. |
| Context and execution batch | `n_ctx=262144`, `n_batch=4096`, `n_ubatch=4096` in live log. | Same values in `/props`. Matched-tail selectors preserve the 4,092 + 4 last prefill split. |
| CPU configuration | Runtime log 8 / 16. Live main-process CPU set `0-31`. The earlier affinity trace records zero successful affinity changes despite the supplied masks. | `/props` 8 / 16 and main-process CPU set `0-31`. Model GPU submissions and the batch gather worker pool are visible. |
| CUDA paths | Device trace shows full graph replay, native MMQ/Attention/GDN and lazy VMM. | Driver-visible, independently compiled Goose kernels and lazy VMM. The comparison profile uses direct submissions. Captured process maps have no libggml or libstdc++. |

The [profile and allocation reconstruction](APEX-PERFORMANCE-AUDIT.md#device-allocation-attribution)
explain the 930 MiB baseline gap. Compute, activation and scratch
allocations explain 872.409 MiB. Physical KV is 200 MiB in both.

## Expert-cache selection

The timing series uses ordinary requests without either
profiler. A 250 ms collector samples process and whole-card VRAM.
RSS high-water comes from `/proc/<pid>/status`. All rows show
zero process swap and zero system swap-out growth during the
request. The pairs at 4,992 and 5,920 MiB on Goose and 4,992 and
5,632 MiB on the hybrid were repeated in both orders. The extra
Goose 5,632 and 6,144 MiB settings have two observations each.
The hybrid and Goose generated
different continuations, so cross-runtime throughput and traffic
describe the served workload and do not isolate a single kernel.

| Runtime and expert budget | Runs | Prefill, tok/s | Decode, steps/s | Peak process VRAM, MiB | Peak card VRAM, MiB | Max RSS high-water, KiB |
|---|---:|---:|---:|---:|---:|---:|
| Original EXP-1476, 4,992 | 2 | 2,671.366 | 62.016 | 8,202 | 10,496 | 21,861,340 |
| Separate hybrid, 5,632 | 2 | 2,668.076 | 65.377 | 8,842 | 11,135 | 21,861,008 |
| Goose, 4,992 | 2 | 1,119.823 | 28.550 | 7,272 | 9,529 | 22,953,968 |
| Goose, 5,632 | 2 | 1,122.585 | 30.364 | 7,910 | 10,106 | 22,956,780 |
| **Goose, 5,920** | **2** | **1,121.743** | **31.053** | **8,200** | **10,396** | **22,955,972** |
| Goose, 6,144 | 2 | 1,120.403 | 31.180 | 8,424 | 10,694 | 22,955,588 |

With 5,920 MiB, Goose's bank grows by 927.047 MiB and
peak process VRAM remains 2 MiB below the original hybrid.
After readiness, the process grows from 7,438 to 8,200 MiB,
leaving at least 1,886 MiB on the card at the sampled peak.
This covers this 32K/512 run. The larger logical context would
require more physical KV pages when populated. At 6,144 MiB
the sampled process peak exceeds the original hybrid by 222 MiB.
Its two decode observations, 31.551 and 30.809, straddle the
selected 5,920 MiB profile's observations. Keep 6,144 MiB as an
experimental higher-memory setting.

| Goose SLRU request totals | 4,992 MiB | 5,632 MiB | 5,920 MiB | 6,144 MiB |
|---|---:|---:|---:|---:|
| Expert fills | 75,902 | 70,688 | 68,793 | 67,543 |
| Hit rate | 88.160% | 88.973% | 89.269% | 89.464% |
| Expert H2D uploads, bytes | 145,893,605,376 | 135,674,634,240 | 131,964,911,616 | 129,528,422,400 |

Goose 5,920 MiB saves 13.929 GB of expert uploads and
1.443 s median decode. It keeps the answer, 512 IDs,
probabilities, stop reason and full model residency. It also
keeps the known record-count error. Prefill differs by 50 ms
in the median; the two pairs disagree on the direction of the
prefill change. Record 5,920 MiB as the tested equal-VRAM
RTX 4070 profile. The common 4,992 MiB comparison stays in
the acceptance plan and can be selected explicitly for rollback.

The separate hybrid saves 0.424 s median decode. Its live
`ubatch <= 8` hit rate goes from 82.96% to 85.81%, and recorded
uploads in that counter go from 52,123.78 to 43,492.85 MiB.
It keeps the original response, IDs and probabilities while
using 640 MiB more device memory. This is evidence for a budget
change, with no measured reverse port of a Goose compute kernel.

Raw commands, status, samples, profiles, traces and checksums
are under `/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/launch-vram-exp1597`.
