# EXP-2026-11-25-2041 — Pack reusable Bonsai Flash Q panels once

**Verified within the experimental Bonsai prefill path.**
EXP-2035 records 53,184 launches of the long-context Flash
panel kernel. Each CTA converts its original F32 queries to
FP16 on entry. The candidate packs all of each layer's
queries once, in the same 16-row panel order, then copies
those panels to shared memory in aligned 16-byte segments.
It preserves the existing K/V stage, FP16 values, MMA
order, mask, partial layout and ordered merge.

The dispatcher applies the candidate only where EXP-2018
already uses the shared panel path. This includes at least
8192 keys, even paired-panel geometry and supported
dynamic shared memory. The prefill pack borrows the
existing device activation packing arena and invalidates
its prior cached identity. The next matrix input is
packed normally. No extra workspace allocation.

## Component evidence

RTX 4070, SM89. The CUDA fixture includes Q4 K/V staging,
Q preparation, all waves and ordered merge. Three
variants compare with the current lean panel implementation.
All active output and final-wave partial bytes match on
six shapes, including padded query and key tails.

| Keys / queries | Control, ms | Packed token-major Q, ms | Direct fragments, ms | Packed panels, ms |
| --- | ---: | ---: | ---: | ---: |
| 257 / 31 | 0.0430 | 0.0460 | 0.0614 | 0.0460 |
| 8192 / 1024 | 12.098 | 11.916 | 16.782 | 11.166 |
| 16384 / 1024 | 25.174 | 24.778 | 34.026 | 24.528 |
| 32768 / 1024 | 58.354 | 56.688 | 75.225 | 56.675 |
| 32767 / 1023 | 58.601 | 56.383 | 75.871 | 56.091 |
| 32767 / 127 | 7.485 | 7.664 | 9.768 | 7.346 |

Only packed panels are wired into the runtime. Direct register
loads reduce the arena from 50,176 to 33,280 B but lose on all
tested shapes. The selected tile uses 80 registers and no spills.
The separate pack uses 15 registers. Test medians include the
one-time preparation cost.

## RTX 4070, 32K/512 A–B–B–A

Bonsai PQ2_0-mtp, Q4_0 K/V, context 36,864, ubatch 1024,
EXP-2038 MTP, same binary and CUDA image. A uses the original
panel route. B uses the one-time Q panel packing. One process
per observation. Decode counts 511 intervals after prefill.

| Arm | Prefill, tok/s | Decode, tok/s | Prefill, s | Combined, s |
| --- | ---: | ---: | ---: | ---: |
| A1 | 614.32 | 64.38 | 53.340 | 61.277 |
| B1 | 615.91 | 64.08 | 53.203 | 61.177 |
| B2 | 614.33 | 64.10 | 53.339 | 61.311 |
| A2 | 608.28 | 64.01 | 53.870 | 61.853 |

Mean prefill rises **611.30 → 615.12 tok/s (+0.62%)**.
The paired prefill reductions are 137 and 531 ms.
Mean decode is **64.20 → 64.09 tok/s**, with opposite
directions in the two pairs. Combined time falls
**61.565 → 61.244 s (−0.320 s)**, with improvements in
both pairs. This is a small model-level gain.

All runs match all 512 EXP-2031 IDs and selected logits,
with identical 326/370 acceptance and 14/16/155
commit counts. All explicit device allocations agree.
Peak RSS ranges overlap (7,419,816–7,420,216 KiB in A,
7,420,064–7,420,584 KiB in B). No process swaps or major
faults. Peak VRAM was not sampled. No Apex, Inkling or
production path changes.

Reusable pattern: prepack token/head data in the exact
consumer layout when repeated tiles share the same inputs.
The preceding direct-load result shows that reducing shared
memory alone does not establish a useful speedup.

## Reproduction

Run from the root with EXP-2025, EXP-2030, EXP-2032,
EXP-2034, EXP-2036 and EXP-2038 build trees present.

```bash
exp=experiments/EXP-2026-11-25-2041-bonsai-flash-query-pack
out=build/bonsai-flash-qpack-repro
frozen=build/bonsai-mtp-head-2025/frozen-goose.ptx
req=experiments/EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json
model=/home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf
bash "$exp/source/build-image.sh" "$out"
bash "$exp/source/build-host.sh" "$out"
env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 taskset 0f0f /usr/bin/python3 "$exp/source/check.py" build/bonsai-integration build/bonsai-integration/inventory.json "$frozen" "$out/flash.cubin"
read -r -a prompt < <(python3 -c 'import json,sys; print(*json.load(open(sys.argv[1]))["prompt"])' "$req")
profile=(env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 BONSAI_UBATCH=1024 BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1 BONSAI_FLASH_ORDERED_MERGE=1 BONSAI_FLASH_COMPACT=1 BONSAI_FLASH_ASYNC=1 BONSAI_MTP_GREEDY=1 BONSAI_MTP_BATCH_PREFILL=1 BONSAI_MTP_VERIFY_Q8=1 BONSAI_MTP_VERIFY_EXACT=1 BONSAI_MTP_DEVICE_COMMIT=1 BONSAI_MTP_VERIFY_SHARED_ATTN=1 BONSAI_MTP_DEVICE_CHOICE=1 BONSAI_MTP_VERIFY_PARALLEL_MERGE=1 BONSAI_MTP_DRAFT_Q8_HEAD=1)
args=(/usr/bin/time -f 'max_rss_kib=%M major_faults=%F swaps=%W' taskset 0f0f "$out/bonsai-qpack" "$model" "$out/flash.cubin" "$frozen" 36864 512 --ignore-eos "${prompt[@]}")
"${profile[@]}" "${args[@]}" >"$out/a1.jsonl" 2>"$out/a1.log"
"${profile[@]}" BONSAI_FLASH_LINEAR_Q=1 "${args[@]}" >"$out/b1.jsonl" 2>"$out/b1.log"
"${profile[@]}" BONSAI_FLASH_LINEAR_Q=1 "${args[@]}" >"$out/b2.jsonl" 2>"$out/b2.log"
"${profile[@]}" "${args[@]}" >"$out/a2.jsonl" 2>"$out/a2.log"
oracle=experiments/EXP-2026-11-25-2031-bonsai-mtp-greedy/data/32768-greedy.jsonl
cmp "$oracle" "$out/a1.jsonl"
cmp "$oracle" "$out/b1.jsonl"
cmp "$oracle" "$out/b2.jsonl"
cmp "$oracle" "$out/a2.jsonl"
```

[Measured outputs and hashes](data/)
