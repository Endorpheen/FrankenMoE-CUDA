# EXP-2026-11-25-2043 — Pair query rows in the four-row Bonsai verifier

**Verified within experimental Bonsai MTP.** EXP-2039's
four-row target reduced the number of verifier calls, but
was slower than three rows. Its Q4 tile kept eight query/head
accumulators per warp and used 201 registers. This candidate
schedules two independent pairs of queries and groups three
heads per warp. Each pair uses the EXP-2042 common-prefix
loop and exact causal tail. The host launches both pairs
in the same grid. The four-row PQ2, prefix commit, and
ordered merge reuse the existing experimental mechanisms.
The three-row control retains EXP-2042.

## Component and state checks

RTX 4070, SM89, synthetic finite Q4_0 K/V and FP32 queries,
256-wide heads and 6:1 GQA. The four variants match all
10,520,208 active tile values and all 28 output rows on seven
key sets. The oracle uses four single-query launches and
frozen merges. All measured shared variants use the same
96-CTA exact merge.

| First key count | Old four-row, ms | Four-row tail, ms | Pairs with two heads, ms | Pairs with three heads, ms |
| ---: | ---: | ---: | ---: | ---: |
| 256 | 0.1941 | 0.1371 | 0.1055 | 0.1222 |
| 8192 | 0.4495 | 0.3214 | 0.2944 | 0.2309 |
| 32768 | 1.3704 | 1.0264 | 0.9526 | 0.8626 |
| 33279 | 1.4369 | 1.1313 | 0.9639 | 0.9839 |

The selected variant uses 153 registers and no spills.
Pairing gives up K/V reuse between the two panels, while
retaining sharing within each panel. The component evidence
shows a favorable tradeoff on long contexts. It does not
identify a single hardware stall cause. The alternative
two-head kernel uses 120 registers and wins at some lengths.
No dynamic crossover is enabled.

A 255-input forced-commit probe (context 1024, ubatch 128)
matches visible KV, GDN state and logits for all four commit
lengths, with three forced rejections. The next serial step
also matches byte for byte.

## RTX 4070, 32K/512 A–B–B–A

Bonsai PQ2_0-mtp, Q4_0 K/V, context 36,864, ubatch 1024,
EXP-2041 packed-query prefill, EXP-2038 Q8 draft vocabulary.
Same binary and CUDA image. A uses two proposals with
EXP-2042 attention. B uses three proposals and paired Q4
tiles. Each run has its own process. Decode counts 511
intervals after the prefill-provided first token.

| Arm | Prefill, tok/s | Decode, tok/s | Target, ms/block | Combined, s |
| --- | ---: | ---: | ---: | ---: |
| A1 | 618.57 | 67.72 | 35.357 | 60.519 |
| B1 | 616.18 | 70.29 | 41.815 | 60.449 |
| B2 | 614.37 | 70.39 | 41.699 | 60.595 |
| A2 | 611.52 | 67.32 | 35.587 | 61.176 |

Mean decode rises **67.52 → 70.34 tok/s (+4.18%)**.
Mean prefill is 615.04 → 615.28 tok/s. Combined time
falls **60.847 → 60.522 s (−0.326 s)** with improvements
in both pairs. Target blocks fall 185 → 145. Acceptance
changes 326/370 (88.11%) → 365/435 (83.91%), with more
committed inputs per block: 2.762 → 3.517. The candidate's
41.757 ms/block compares with 49.126 ms/block from the
older EXP-2039 four-row experiment. That is a cross-run
comparison, separate from the current A/B.

All four runs match the EXP-2031 oracle, including all
512 output IDs and selected logits. The broader target
adds 157,889,392 bytes (150.57 MiB) of explicit GPU
workspace, primarily the additional GDN prefix slots.
Before draft and choice allocations, the device reports
574–584 MiB free in the two candidate runs. Peak VRAM was
not sampled. Peak RSS is 7,420,668–7,421,360 KiB in A
and 7,420,584–7,420,636 KiB in B. Process swaps and
major faults stay zero.

The result supports the four-row candidate for the
measured greedy workload when it fits. Other contexts,
sampling distributions and memory budgets require
independent measurements. There are no production
changes or changes to Apex and Inkling.

## Reproduction

Run from the root with EXP-2025, EXP-2032, EXP-2038,
EXP-2039 and EXP-2042 build trees.

```bash
exp=experiments/EXP-2026-11-25-2043-bonsai-mtp-pair-tiles
out=build/bonsai-mtp-pairs-repro
frozen=build/bonsai-mtp-head-2025/frozen-goose.ptx
req=experiments/EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json
model=/home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf
bash "$exp/source/build-image.sh" "$out"
bash "$exp/source/build-host.sh" "$out"
env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 taskset 0f0f /usr/bin/python3 "$exp/source/check.py" build/bonsai-integration build/bonsai-integration/inventory.json "$out/attention.cubin" "$frozen"
read -r -a prompt < <(python3 -c 'import json,sys; print(*json.load(open(sys.argv[1]))["prompt"])' "$req")
profile=(env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1 BONSAI_FLASH_ORDERED_MERGE=1 BONSAI_FLASH_COMPACT=1 BONSAI_FLASH_ASYNC=1 BONSAI_FLASH_LINEAR_Q=1 BONSAI_MTP_BATCH_PREFILL=1 BONSAI_MTP_VERIFY_Q8=1 BONSAI_MTP_VERIFY_EXACT=1 BONSAI_MTP_DEVICE_COMMIT=1 BONSAI_MTP_VERIFY_SHARED_ATTN=1 BONSAI_MTP_DEVICE_CHOICE=1 BONSAI_MTP_VERIFY_PARALLEL_MERGE=1 BONSAI_MTP_DRAFT_Q8_HEAD=1 BONSAI_MTP_VERIFY_ATTN_TAIL=1)
probe=(taskset 0f0f "$out/bonsai-mtp-pairs" "$model" "$out/attention.cubin" "$frozen")
"${profile[@]}" BONSAI_UBATCH=128 BONSAI_MTP_TARGET_PROBE=1 BONSAI_MTP_VERIFY_ROWS=4 "${probe[@]}" 1024 5 --ignore-eos "${prompt[@]:0:255}"
args=(/usr/bin/time -f 'max_rss_kib=%M major_faults=%F swaps=%W' "${probe[@]}" 36864 512 --ignore-eos "${prompt[@]}")
"${profile[@]}" BONSAI_UBATCH=1024 BONSAI_MTP_GREEDY=1 "${args[@]}" >"$out/a1.jsonl" 2>"$out/a1.log"
"${profile[@]}" BONSAI_UBATCH=1024 BONSAI_MTP_GREEDY=1 BONSAI_MTP_VERIFY_ROWS=4 "${args[@]}" >"$out/b1.jsonl" 2>"$out/b1.log"
"${profile[@]}" BONSAI_UBATCH=1024 BONSAI_MTP_GREEDY=1 BONSAI_MTP_VERIFY_ROWS=4 "${args[@]}" >"$out/b2.jsonl" 2>"$out/b2.log"
"${profile[@]}" BONSAI_UBATCH=1024 BONSAI_MTP_GREEDY=1 "${args[@]}" >"$out/a2.jsonl" 2>"$out/a2.log"
oracle=experiments/EXP-2026-11-25-2031-bonsai-mtp-greedy/data/32768-greedy.jsonl
cmp "$oracle" "$out/a1.jsonl"
cmp "$oracle" "$out/b1.jsonl"
cmp "$oracle" "$out/b2.jsonl"
cmp "$oracle" "$out/a2.jsonl"
```

[Measured outputs and hashes](data/)
