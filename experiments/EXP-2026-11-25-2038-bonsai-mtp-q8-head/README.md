# EXP-2026-11-25-2038 — Use the existing Q8 vocabulary projection in the Bonsai drafter

**Verified decode improvement within the experimental MTP path.**
The structured-sparsity census already exists in EXP-1995.
It found 60.30% of weight groups require residual work in
one exact 2:4 pass. This experiment addresses the separate
607 ms drafter vocabulary cost in EXP-2035.

Only the draft PQ2 vocabulary projection takes the existing
EXP-2012 single-row Q8 DP4A path. The Q4 draft projections,
draft KV, target arithmetic, verifier and commit retain
their EXP-2036 implementations. The host switch reuses the
existing CUDA image and Q8 scratch without new allocation.

## Correctness and proposal quality

Compare full draft logits on four identical EXP-2025
conditioning inputs. Both captures replay bitwise and their
20,480-value attention-residual and post-norm arrays each
match. All four greedy IDs and top-10 sets agree. Across
993,280 logits the maximum absolute difference is 0.05074
and relative L2 ranges from 0.239% to 0.363%. The drafter's
logits change. The target still verifies with its unchanged
arithmetic.

Two free generations use the stored EXP-1994 code-trace
and Russian scheduler prompt IDs, context 1024, ubatch 128,
greedy, ordinary EOG. FP32 and Q8 draft paths produce
identical final IDs and selected target logits byte for byte,
so each task's generated content and any deterministic
score are unchanged. This check compares candidate and
same-binary control. It does not claim higher task quality.

| Task | IDs through EOG | FP32 accepted/proposed | Q8 accepted/proposed |
| --- | ---: | ---: | ---: |
| Code trace | 256 | 158/196 | 158/196 |
| Russian scheduler | 63 | 42/42 | 42/42 |

All four 32K/512 runs match the EXP-2031 oracle, including
all 512 IDs and selected target logits. Each accepts
326/370 proposals with 14/16/155 commits of one/two/three
inputs.

## RTX 4070, 32K/512 A–B–B–A

Bonsai PQ2_0-mtp, Q4_0 K/V, context 36,864, ubatch 1024,
fixed greedy budget with EOG masked. One invocation at a time,
same binary and CUDA image. A uses FP32 draft vocabulary,
B uses Q8. Rates are per-run rates; decode counts the 511
intervals after the prefill-provided first token.

| Arm | Prefill, tok/s | Decode, tok/s | Drafter, s | Combined, s |
| --- | ---: | ---: | ---: | ---: |
| A1 | 616.62 | 61.76 | 1.213 | 61.416 |
| B1 | 611.18 | 64.10 | 0.894 | 61.585 |
| B2 | 610.84 | 64.11 | 0.898 | 61.615 |
| A2 | 609.09 | 61.48 | 1.211 | 62.111 |

Mean decode rises **61.62 → 64.11 tok/s (+4.04%)**.
Drafter time falls **1.212 → 0.896 s (−26.08%)**.
Average prefill is 612.85 → 611.01 tok/s. The prefill
and combined intervals move in opposite directions between
the two pairs. The combined mean is 61.763 → 61.600 s.
The repeated evidence establishes a decode gain.

All explicit GPU allocations match. Peak RSS ranges are
7,419,860–7,420,388 KiB in A and 7,420,064–7,420,148 KiB
in B. Process swaps and major faults are zero. Peak VRAM
was not sampled. The scope is Bonsai's isolated CLI.
There are no Apex, Inkling or production changes.

The numeric path can be reused for a separately verified
drafter. Preserve the target's acceptance contract and
compare long outputs, fixed-history logits, acceptance
and full wall time before enabling it in another model.

## Reproduction

Run from the root with EXP-2025, EXP-2030, EXP-2032,
EXP-2034 and EXP-2036 build trees present.

```bash
exp=experiments/EXP-2026-11-25-2038-bonsai-mtp-q8-head
out=build/bonsai-mtp-head-q8-repro
frozen=build/bonsai-mtp-head-2025/frozen-goose.ptx
image=build/bonsai-mtp-merge-2036/merge.cubin
req=experiments/EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json
model=/home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf
bash "$exp/source/build.sh" "$out"
profile=(env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1 BONSAI_FLASH_ORDERED_MERGE=1 BONSAI_FLASH_COMPACT=1 BONSAI_FLASH_ASYNC=1)
args=(taskset 0f0f "$out/bonsai-mtp-head-q8" "$model" "$image" "$frozen")
"${profile[@]}" BONSAI_UBATCH=128 BONSAI_MTP_HEAD_CAPTURE="$out/trace-fp32.bin" "${args[@]}" 1024 1 1812 5020 220 15
"${profile[@]}" BONSAI_UBATCH=128 BONSAI_MTP_HEAD_CAPTURE="$out/trace-q8.bin" BONSAI_MTP_DRAFT_Q8_HEAD=1 "${args[@]}" 1024 1 1812 5020 220 15
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/compare.py" "$out/trace-fp32.bin" "$out/trace-q8.bin"
mtp=(BONSAI_MTP_GREEDY=1 BONSAI_MTP_BATCH_PREFILL=1 BONSAI_MTP_VERIFY_Q8=1 BONSAI_MTP_VERIFY_EXACT=1 BONSAI_MTP_DEVICE_COMMIT=1 BONSAI_MTP_VERIFY_SHARED_ATTN=1 BONSAI_MTP_DEVICE_CHOICE=1 BONSAI_MTP_VERIFY_PARALLEL_MERGE=1)
read -r -a code < <(python3 -c 'import json,sys; print(*json.load(open(sys.argv[1])))' "$exp/data/code-ids.json")
"${profile[@]}" "${mtp[@]}" BONSAI_UBATCH=128 "${args[@]}" 1024 384 "${code[@]}" >"$out/code-fp32.jsonl"
"${profile[@]}" "${mtp[@]}" BONSAI_UBATCH=128 BONSAI_MTP_DRAFT_Q8_HEAD=1 "${args[@]}" 1024 384 "${code[@]}" >"$out/code-q8.jsonl"
cmp "$out/code-fp32.jsonl" "$out/code-q8.jsonl"
read -r -a prompt < <(python3 -c 'import json,sys; print(*json.load(open(sys.argv[1]))["prompt"])' "$req")
time_cmd=(/usr/bin/time -f 'max_rss_kib=%M major_faults=%F swaps=%W')
"${profile[@]}" "${mtp[@]}" BONSAI_UBATCH=1024 "${time_cmd[@]}" "${args[@]}" 36864 512 --ignore-eos "${prompt[@]}" >"$out/a1.jsonl" 2>"$out/a1.log"
"${profile[@]}" "${mtp[@]}" BONSAI_UBATCH=1024 BONSAI_MTP_DRAFT_Q8_HEAD=1 "${time_cmd[@]}" "${args[@]}" 36864 512 --ignore-eos "${prompt[@]}" >"$out/b1.jsonl" 2>"$out/b1.log"
"${profile[@]}" "${mtp[@]}" BONSAI_UBATCH=1024 BONSAI_MTP_DRAFT_Q8_HEAD=1 "${time_cmd[@]}" "${args[@]}" 36864 512 --ignore-eos "${prompt[@]}" >"$out/b2.jsonl" 2>"$out/b2.log"
"${profile[@]}" "${mtp[@]}" BONSAI_UBATCH=1024 "${time_cmd[@]}" "${args[@]}" 36864 512 --ignore-eos "${prompt[@]}" >"$out/a2.jsonl" 2>"$out/a2.log"
oracle=experiments/EXP-2026-11-25-2031-bonsai-mtp-greedy/data/32768-greedy.jsonl
cmp "$oracle" "$out/a1.jsonl"
cmp "$oracle" "$out/b1.jsonl"
cmp "$oracle" "$out/b2.jsonl"
cmp "$oracle" "$out/a2.jsonl"
```

For the scheduler check use `scheduler-ids.json` and the
192-token cap in the free-generation commands.
[Evidence and hashes](data/)
