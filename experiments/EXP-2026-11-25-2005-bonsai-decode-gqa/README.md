# EXP-2026-11-25-2005 — Share Q4 K/V loads across Bonsai decode heads

**Accepted in the Bonsai experimental HTTP line.** A specialized single-token
tile processes two query heads at short and medium lengths, or three from
32768 keys, on the same warp. It loads and dequantizes each Q4_0 key and
value once per group. The 6:1 head geometry makes both group sizes exact
divisors. Each head retains its FP32 dot chain, butterfly reduction, row
order, softmax recurrence and scratch layout. The original merge consumes
those scratch rows unchanged. No extra workspace is allocated. The
original tile remains available with `BONSAI_DECODE_GQA=0`. Multi-token
prefill continues to use the prior Flash implementation. The final
single-token step that closes prefill can use the same grouped path.

## Component correctness and performance

All scratch and output bytes match the frozen Q4_0 CUDA image for
synthetic FP32 queries and CUDA-quantized K/V at six lengths. Times are
medians of ten CUDA event samples per variant and include the original
merge. They measure isolated GPU work.

| Visible KV rows | Frozen, ms | One head, ms | Two heads, ms | Three heads, ms |
| ---: | ---: | ---: | ---: | ---: |
| 65 | 0.1311 | 0.0480 | 0.0389 | 0.0584 |
| 257 | 0.4813 | 0.1597 | 0.1260 | 0.2028 |
| 1024 | 0.4832 | 0.1625 | 0.1280 | 0.2048 |
| 8192 | 0.5537 | 0.2222 | 0.1812 | 0.2400 |
| 32768 | 2.3209 | 1.0081 | 0.5868 | 0.5304 |
| 33279 | 2.3629 | 1.0254 | 0.6001 | 0.5335 |

Even the single-head specialization saves time, so the full gain
includes geometry specialization and unrolling in addition to shared
K/V work. The two- and three-head kernels use 64 and 80 registers
with no spills. The selected crossover is conservative. The cases below
32768 do not locate the exact crossover.

## End-to-end results on RTX 4070

A is the frozen tile in the same HTTP binary. B is grouped attention.
The 4097/64 runs use the EXP-1977 prefix and run A–B–B–A with explicit
flags. All 64 output IDs and the completion text match across the four
runs. The 32768/512 block uses the exact EXP-1977 request. Both pairs use
the same binary within the pair. The reverse pair is rebuilt with B
enabled by default, using byte-identical CUDA images.

| Workload | Prefill A → B, tok/s | Decode A → B, tok/s | Decode gain |
| --- | ---: | ---: | ---: |
| 4097/64, two pairs | 813.69 → 814.83 | 15.417 → 17.447 | +13.17% |
| 32768/512, first pair | 613.06 → 612.46 | 11.256 → 16.191 | +43.85% |
| 32768/512, reverse pair | 611.84 → 613.79 | 11.249 → 16.195 | +43.97% |
| 32768/512, aggregate of pair times | 612.45 → 613.12 | 11.252 → 16.193 | +43.91% |

All four long responses reproduce every PrismML ID and the text SHA-256.
The mean long decode drops from 45.413 s to 31.557 s. Each long run
peaks at 8932 MiB process VRAM. VmHWM stays within 7,456,772–7,457,008
KiB. Process swap and system swap-out stay at zero. Prefill varies by
less than 0.4% in each pair. Apex and Inkling sources, CUDA images
and their published artifacts are outside this experiment. The full
shared-candidate regression gates and Bonsai chat integration remain
open. The long decode target of 81.55 tok/s remains open.

## Reproduction

Build beside the saved EXP-2000 and EXP-2003 artifacts. The standalone
component cubin and integrated cubin have separate build recipes.

```bash
ex=experiments/EXP-2026-11-25-2005-bonsai-decode-gqa
base=build/bonsai-mmq-k64-2000-default
bash "$ex/source/build.sh" build/bonsai-gqa-component-repro
OPENBLAS_NUM_THREADS=1 python3 "$ex/source/check.py" build/bonsai-integration \
    build/bonsai-integration/inventory.json "$base/frozen-goose.ptx" \
    build/bonsai-gqa-component-repro/grouped.cubin
out=build/bonsai-gqa-http-repro
bash "$ex/source/build-runtime.sh" "$out"
env -i PATH=/usr/bin:/bin HOME="$HOME" LANG=C CUDA_VISIBLE_DEVICES=0 \
    BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1 BONSAI_FLASH_ORDERED_MERGE=1 \
    BONSAI_FLASH_COMPACT=1 BONSAI_FLASH_ASYNC=1 \
    BONSAI_CUDA_IMAGE="$out/combined.cubin" BONSAI_FROZEN_PTX="$base/frozen-goose.ptx" \
    taskset 0f0f "$out/frankenmoe-infer" --ctx-size 36864 --batch-size 4096 \
    --ubatch-size 1024 --cache-type-k q4_0 --cache-type-v q4_0 \
    /home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf --serve 19105
```

After startup, post the EXP-1977 request file to `/completion` and
record the process memory and client time as in EXP-2004. Restart the
server for each arm. Set `BONSAI_DECODE_GQA=0` in the explicit environment
for the control. For the short workload, take the first 4097 prompt IDs
and set `n_predict=64`, retaining other request parameters. See
[data/results.json](data/results.json) and [component samples](data/component.json).
