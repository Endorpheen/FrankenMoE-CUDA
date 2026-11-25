# EXP-2026-11-25-2034 — Device choice for Bonsai speculative decode

**Verified improvement within the experimental MTP path.**
Two CUDA reductions select each row's greedy token directly
from the logits already on the GPU. The result is a 12-byte
record containing ID, original FP32 score and a nonfinite flag.
The selector preserves the lower-ID winner on ties, the
winning signed-zero bits and the handling of an optional EOG
mask. Nonfinite logits are detected before checking the mask.
The host consumes only choices that affect the verified prefix.
Target and drafter arithmetic and the commit algorithm stay
the same. Both arms share one speculative host loop. Its CPU
control retains the full-logit download and removes redundant
reselection of a winner it has already found.

## Correctness

The CUDA fixture checks the entire 248,320-entry vocabulary
including inter-tile ties, negative zero, masked and unmasked
choices, and invalid logits in masked positions. The 129/64
Bonsai fixture matches all 64 IDs and selected logits against
EXP-2031 in both arms. Natural acceptance is 34/54 in both.

On 32K/512, all four measured runs match all 512 IDs and
selected FP32 logits from EXP-2031 byte for byte. Acceptance
is 326/370 with 14/16/155 commits of one/two/three inputs.
The speed workload uses EOG masking and a fixed output budget.
The component covers EOG mask semantics; native early
termination was not part of this performance block.

## Measurements

RTX 4070, Bonsai PQ2_0-mtp, Q4_0 K/V, context 36,864,
ubatch 1024, EXP-2033 attention and EXP-2032 draft prefill.
A–B–B–A, one invocation at a time, same host and CUDA binaries.
A uses the shared CPU control, B selects on GPU.

| Arm | Prefill, tok/s | Decode, tok/s | Decode, s | Combined, s |
| --- | ---: | ---: | ---: | ---: |
| A1 | 616.04 | 51.33 | 9.956 | 63.147 |
| B1 | 611.47 | 53.55 | 9.543 | 63.131 |
| B2 | 610.07 | 53.54 | 9.544 | 63.256 |
| A2 | 607.49 | 50.61 | 10.097 | 64.037 |

Mean decode rises **50.97 → 53.54 tok/s (+5.05%)**,
prefill **611.77 → 610.77 tok/s (−0.16%, opposite
directions in the two pairs)**, and combined time falls
**63.592 → 63.194 s (−0.398 s)**. Mean draft time is
1.432 → 1.203 s and mean target time is 8.421 → 8.234 s.

In 185 measured full speculative blocks, the five logits
rows per block represent 918,784,000 D2H payload bytes in A.
B transfers 11,100 D2H choice bytes for those blocks.
These are counts from the transfer contracts and block
count, excluding the common initial-prefill logits.
The GPU choice workspace adds a 39,824-byte allocation.
Peak RSS is 7,419,888–7,420,060 KiB in A and
7,420,148–7,420,388 KiB in B. Process swaps and major faults
are zero. Peak VRAM was not sampled. The kernels use 18
and 20 registers with zero spills. No production serving,
Apex or Inkling changes.

Device reduction is reusable for greedy decode after
verifying the caller's tie, mask and invalid-value contract.
Other sampling policies require their own GPU implementation
and comparison. Target still dominates long-context decode.

## Reproduction

Run from the root with EXP-2025, EXP-2030, EXP-2032 and
EXP-2033 build trees. Each command is an individual run.

```bash
exp=experiments/EXP-2026-11-25-2034-bonsai-mtp-device-argmax
out=build/bonsai-mtp-choice-repro
frozen=build/bonsai-mtp-head-2025/frozen-goose.ptx
req=experiments/EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json
model=/home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf
bash "$exp/source/build.sh" "$out"
env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 taskset 0f0f /usr/bin/python3 "$exp/source/check.py" build/bonsai-integration build/bonsai-integration/inventory.json "$out/choice.cubin"
read -r -a prompt < <(python3 -c 'import json,sys; print(*json.load(open(sys.argv[1]))["prompt"])' "$req")
profile=(env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 BONSAI_UBATCH=1024 BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1 BONSAI_FLASH_ORDERED_MERGE=1 BONSAI_FLASH_COMPACT=1 BONSAI_FLASH_ASYNC=1 BONSAI_MTP_GREEDY=1 BONSAI_MTP_BATCH_PREFILL=1 BONSAI_MTP_VERIFY_Q8=1 BONSAI_MTP_VERIFY_EXACT=1 BONSAI_MTP_DEVICE_COMMIT=1 BONSAI_MTP_VERIFY_SHARED_ATTN=1)
args=(/usr/bin/time -f 'max_rss_kib=%M major_faults=%F swaps=%W' taskset 0f0f "$out/bonsai-mtp-choice" "$model" "$out/choice.cubin" "$frozen" 36864 512 --ignore-eos "${prompt[@]}")
"${profile[@]}" "${args[@]}" >"$out/a1.jsonl" 2>"$out/a1.log"
"${profile[@]}" BONSAI_MTP_DEVICE_CHOICE=1 "${args[@]}" >"$out/b1.jsonl" 2>"$out/b1.log"
"${profile[@]}" BONSAI_MTP_DEVICE_CHOICE=1 "${args[@]}" >"$out/b2.jsonl" 2>"$out/b2.log"
"${profile[@]}" "${args[@]}" >"$out/a2.jsonl" 2>"$out/a2.log"
oracle=experiments/EXP-2026-11-25-2031-bonsai-mtp-greedy/data/32768-greedy.jsonl
cmp "$oracle" "$out/a1.jsonl"
cmp "$oracle" "$out/b1.jsonl"
cmp "$oracle" "$out/b2.jsonl"
cmp "$oracle" "$out/a2.jsonl"
```

[Measured outputs and hashes](data/)
