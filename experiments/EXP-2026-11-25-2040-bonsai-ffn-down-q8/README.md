# EXP-2026-11-25-2040 — Layer-bounded Bonsai Q8 FFN down

**Not promoted.** EXP-2035 attributes 7.315 s of 32K prefill
GPU work to FFN down. This trial uses the existing signed Q8
integer MMQ kernel only for the last 16, eight or four
down projections in prefill, with at least 128 input tokens.
It leaves the accepted Q8 gate/up dispatch unchanged. The
Q8 down scope uses the ordinary SwiGLU and signed FWHT so
the Q8 pack can read F32 activations. It bypasses the fused
FP16 tail only when Q8 down will run. Packed activation
and scale workspace are reused. The CUDA image and weights
stay unchanged.

## Independent quality gate

Use the EXP-1994 authored requests and exact tokenizer template,
greedy, ordinary EOG, context 36,864, ubatch 1024, MTP off.
For each case, compare complete free responses and sample the
full vocabulary under the identical reference history. Record
independent task scores using the existing harness. The all-float
label used by the harness means the accepted FP16 down path;
late Q8 gate/up is present in both arms.

| Scope | Code-trace output tokens, control/candidate | Matching prefix | Correct cells out of 63 | Sampled same-history top-1 |
| --- | ---: | ---: | ---: | ---: |
| Last 16 | 258/257 | 203 | 18/18 | 11/11 |
| Last eight | 258/258 | 258 | 18/18 | 11/11 |
| Last four | 258/258 | 258 | 18/18 | 11/11 |

The first divergence for last 16 occurs at zero-based step 203.
On the same 203 previous output IDs, control selects ID 15
(18.83325 versus 18.79870 for ID 18), and candidate selects
ID 18 (18.87611 versus 18.82196 for ID 15). Maximum absolute
whole-vocabulary difference at that step is 0.12917. Although
the cell score stays the same, this scope fails the sequence gate.

Last eight also matches all 23 ledger IDs, 63 scheduler IDs
and 47 incident-extraction IDs through EOG. All 37 sampled
same-history top-1 choices agree, with at least 9/10 top-10
overlap and maximum absolute difference 0.48751. Complete
responses and independent scores match. Ledger and scheduler
remain incorrect in both arms. Incident extraction is correct
in both arms. Last four is a code-trace diagnostic only.

## RTX 4070, 32K/512 A–B–B–A

Bonsai PQ2_0-mtp, Q4_0 K/V, context 36,864, ubatch 1024,
EXP-2038 MTP, same binary and device image. Each run uses its
own process. A uses the accepted FP16 down; B enables last-eight
Q8 down. Greedy with fixed output budget and EOG suppressed.
Decode counts 511 intervals after the first token.

| Arm | Prefill, tok/s | Decode, tok/s | Prefill, s | Combined, s |
| --- | ---: | ---: | ---: | ---: |
| A1 | 617.58 | 64.36 | 53.059 | 60.999 |
| B1 | 613.66 | 63.95 | 53.398 | 61.389 |
| B2 | 610.73 | 63.97 | 53.654 | 61.642 |
| A2 | 607.76 | 63.97 | 53.916 | 61.904 |

Prefill changes **612.67 → 612.20 tok/s (−0.08%)**, with
the two pairs moving in opposite directions. Decode changes
64.17 → 63.96 tok/s (−0.32%). Combined mean is
61.451 → 61.515 s. There is no reproducible improvement.
This trial does not separate integer MMQ time from the cost
of the materialized activation and the separate FWHT/pack.

All 512 IDs match the accepted oracle in both arms.
The candidate repeats bitwise and differs from control by
up to 0.09450 in the selected FP32 logits. All runs accept
326/370 MTP proposals. Target, draft and choice allocations
match. Peak RSS ranges are 7,420,420–7,420,544 KiB in A
and 7,420,032–7,420,124 KiB in B. Process swaps and major
faults are zero. Peak VRAM was not sampled.

Keep the accepted FP16 down route. The quality results
apply to the specific layer scope and tested prompts.
A new numerical route should show a faster full projection
including its input preparation before rerunning long A/B.
Apex, Inkling and production source paths are unchanged.

## Reproduction

Run from the root with the EXP-2025, EXP-2030, EXP-2032,
EXP-2034, EXP-2036 and EXP-2038 build trees present.

```bash
exp=experiments/EXP-2026-11-25-2040-bonsai-ffn-down-q8
out=build/bonsai-ffn-down-repro
model=/home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf
data=build/bonsai-integration
tokenizer=/home/end0/AI/LlamaServer/llama.cpp/build/bin/llama-tokenize
bash "$exp/source/build.sh" "$out"
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/quality.py" 16 "$out/quality-last16" "$out" "$model" "$data/inventory.json" "$data/vocabulary.gguf" "$tokenizer"
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/replay.py" 16 "$out" "$model" "$out/quality-last16/code-trace"
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/quality.py" 8 "$out/quality-last8" "$out" "$model" "$data/inventory.json" "$data/vocabulary.gguf" "$tokenizer" code-trace
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/quality.py" 4 "$out/quality-last4" "$out" "$model" "$data/inventory.json" "$data/vocabulary.gguf" "$tokenizer" code-trace
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/quality.py" 8 "$out/quality-last8-rest" "$out" "$model" "$data/inventory.json" "$data/vocabulary.gguf" "$tokenizer" ledger scheduler-ru incident-extraction
req=experiments/EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json
read -r -a prompt < <(python3 -c 'import json,sys; print(*json.load(open(sys.argv[1]))["prompt"])' "$req")
profile=(env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 BONSAI_UBATCH=1024 BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1 BONSAI_FLASH_ORDERED_MERGE=1 BONSAI_FLASH_COMPACT=1 BONSAI_FLASH_ASYNC=1 BONSAI_MTP_GREEDY=1 BONSAI_MTP_BATCH_PREFILL=1 BONSAI_MTP_VERIFY_Q8=1 BONSAI_MTP_VERIFY_EXACT=1 BONSAI_MTP_DEVICE_COMMIT=1 BONSAI_MTP_VERIFY_SHARED_ATTN=1 BONSAI_MTP_DEVICE_CHOICE=1 BONSAI_MTP_VERIFY_PARALLEL_MERGE=1 BONSAI_MTP_DRAFT_Q8_HEAD=1)
args=(/usr/bin/time -f 'max_rss_kib=%M major_faults=%F swaps=%W' taskset 0f0f "$out/bonsai-down-q8" "$model" "$out/pq2-int8.cubin" "$out/frozen-goose.ptx" 36864 512 --ignore-eos "${prompt[@]}")
"${profile[@]}" "${args[@]}" >"$out/a1.jsonl" 2>"$out/a1.log"
"${profile[@]}" BONSAI_FFN_DOWN_Q8_LAST=8 "${args[@]}" >"$out/b1.jsonl" 2>"$out/b1.log"
"${profile[@]}" BONSAI_FFN_DOWN_Q8_LAST=8 "${args[@]}" >"$out/b2.jsonl" 2>"$out/b2.log"
"${profile[@]}" "${args[@]}" >"$out/a2.jsonl" 2>"$out/a2.log"
cmp "$out/b1.jsonl" "$out/b2.jsonl"
diff <(jq -r .token "$out/a1.jsonl") <(jq -r .token "$out/b1.jsonl")
```

[Measured outputs, quality and hashes](data/)
