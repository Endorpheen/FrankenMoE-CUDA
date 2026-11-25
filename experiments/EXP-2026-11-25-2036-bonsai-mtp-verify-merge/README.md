# EXP-2026-11-25-2036 — Exact three-row Bonsai attention merge

**Verified improvement within the experimental MTP path.**
The EXP-2035 trace measured 1,334.86 ms in the frozen target
merge. Replace its three dispatches with one 72-CTA dispatch
specialized to three queries, 24 heads and width 256. Each
128-thread CTA handles one query/head. A warp prepares the
per-tile rescaling factors with prefix maxima. Four warps keep
the numerator for two output columns each and traverse the
tiles in chronological order. The mass recurrence and the
per-column multiplications, additions and final FP32 division
retain the frozen PTX arithmetic. Zero-mass tiles are skipped.

No tile producer, GPU choice, controlled draft state or
ordinary decode path changes. The dispatcher uses the existing
partial buffer and only selects the new merge for shared
verifier attention. The simpler direct variant is retained
for component comparison.

## Component and model checks

RTX 4070, SM89. The two CUDA variants match all 129,024
output values across seven three-query fixtures. Coverage
includes zero-mass tiles and heads, signed-zero maxima,
256-key tile boundaries and the 32,768-key geometry switch.
CUDA event medians cover either three frozen merge launches
or one candidate launch, with the same input partials.

| First key count | Frozen, µs | Direct, µs | Parallel, µs |
| ---: | ---: | ---: | ---: |
| 256 | 14.234 | 4.623 | 4.403 |
| 8192 | 110.660 | 7.091 | 5.043 |
| 32768 | 415.871 | 22.655 | 13.585 |
| 33279 | 421.218 | 23.014 | 13.926 |

The selected parallel merge uses 35 registers and 400
bytes of shared memory per CTA, with no spills. A model
probe with 255 input tokens checks each accepted prefix
length, including forced rejections, against serial KV/GDN
state and logits before and after the next token.

## Full 32K/512

RTX 4070, Bonsai PQ2_0-mtp, Q4_0 K/V, context 36,864,
ubatch 1024, exact target, batch draft prefill, GPU choice.
A–B–B–A, one invocation at a time, same host and CUDA binaries.
A uses the frozen merges, B the parallel merge.

| Arm | Prefill, tok/s | Decode, tok/s | Target, ms/block | Total, s |
| --- | ---: | ---: | ---: | ---: |
| A1 | 613.36 | 53.83 | 44.231 | 62.916 |
| B1 | 612.69 | 61.53 | 37.830 | 61.787 |
| B2 | 611.36 | 61.50 | 37.795 | 61.908 |
| A2 | 610.41 | 53.23 | 44.842 | 63.282 |

Mean decode rises **53.53 → 61.51 tok/s (+14.92%)**.
Mean prefill is **611.89 → 612.02 tok/s**, with opposite
directions in the two pairs. The exact target saves
**6.724 ms/block**, and combined time falls
**63.099 → 61.848 s (−1.251 s)**.

All runs match the EXP-2031 oracle for 512 IDs and their
selected FP32 logits. Draft acceptance is 326/370 in each
arm with 14/16/155 commits of one/two/three inputs.
Device allocations are unchanged. Peak RSS ranges are
7,420,212–7,420,384 KiB in A and 7,420,200–7,420,556 KiB
in B. Process swaps and major faults are zero. Peak VRAM
was not sampled. No Apex, Inkling or production changes.

Reuse the ordered coefficient preparation when the target
merge requires bitwise equivalence and each query's
normalizer is independent. Recheck occupancy and end-to-end
timing before applying this geometry elsewhere.

## Reproduction

Run from the root with EXP-2025, EXP-2030 and EXP-2032 through
EXP-2034 build trees present.

```bash
exp=experiments/EXP-2026-11-25-2036-bonsai-mtp-verify-merge
out=build/bonsai-mtp-merge-repro
frozen=build/bonsai-mtp-head-2025/frozen-goose.ptx
req=experiments/EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json
model=/home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf
bash "$exp/source/build-image.sh" "$out"
bash "$exp/source/build-host.sh" "$out"
env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 taskset 0f0f /usr/bin/python3 "$exp/source/check.py" build/bonsai-integration build/bonsai-integration/inventory.json "$out/merge.cubin" "$frozen"
read -r -a prompt < <(python3 -c 'import json,sys; print(*json.load(open(sys.argv[1]))["prompt"])' "$req")
profile=(env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1 BONSAI_FLASH_ORDERED_MERGE=1 BONSAI_FLASH_COMPACT=1 BONSAI_FLASH_ASYNC=1 BONSAI_MTP_VERIFY_Q8=1 BONSAI_MTP_VERIFY_EXACT=1 BONSAI_MTP_DEVICE_COMMIT=1 BONSAI_MTP_VERIFY_SHARED_ATTN=1)
"${profile[@]}" BONSAI_UBATCH=128 BONSAI_MTP_TARGET_PROBE=1 BONSAI_MTP_VERIFY_PARALLEL_MERGE=1 taskset 0f0f "$out/bonsai-mtp-merge" "$model" "$out/merge.cubin" "$frozen" 1024 5 --ignore-eos "${prompt[@]:0:255}"
mtp=(BONSAI_UBATCH=1024 BONSAI_MTP_GREEDY=1 BONSAI_MTP_BATCH_PREFILL=1 BONSAI_MTP_DEVICE_CHOICE=1)
args=(/usr/bin/time -f 'max_rss_kib=%M major_faults=%F swaps=%W' taskset 0f0f "$out/bonsai-mtp-merge" "$model" "$out/merge.cubin" "$frozen" 36864 512 --ignore-eos "${prompt[@]}")
"${profile[@]}" "${mtp[@]}" "${args[@]}" >"$out/a1.jsonl" 2>"$out/a1.log"
"${profile[@]}" "${mtp[@]}" BONSAI_MTP_VERIFY_PARALLEL_MERGE=1 "${args[@]}" >"$out/b1.jsonl" 2>"$out/b1.log"
"${profile[@]}" "${mtp[@]}" BONSAI_MTP_VERIFY_PARALLEL_MERGE=1 "${args[@]}" >"$out/b2.jsonl" 2>"$out/b2.log"
"${profile[@]}" "${mtp[@]}" "${args[@]}" >"$out/a2.jsonl" 2>"$out/a2.log"
oracle=experiments/EXP-2026-11-25-2031-bonsai-mtp-greedy/data/32768-greedy.jsonl
cmp "$oracle" "$out/a1.jsonl"
cmp "$oracle" "$out/b1.jsonl"
cmp "$oracle" "$out/b2.jsonl"
cmp "$oracle" "$out/a2.jsonl"
```

[Measured outputs and hashes](data/)
