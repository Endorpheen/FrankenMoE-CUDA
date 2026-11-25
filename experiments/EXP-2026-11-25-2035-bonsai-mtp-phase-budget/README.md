# EXP-2026-11-25-2035 — Phase budget for Bonsai 32K/512 with MTP

**Diagnostic, current EXP-2034 GPU-choice path.** This adds NVTX
ranges to a separate CLI build and uses the existing Goose object
and CUDA image. RTX 4070, Bonsai PQ2_0-mtp, 32,768 input and 512
output tokens, context 36,864, ubatch 1024, Q4_0 K/V, two draft
proposals, exact target and GPU choice. The captured output matches
all 512 EXP-2031 token IDs and selected logits. Acceptance stays at
326/370 with 185 calls to the target verifier.

GPU activity is clipped to the enclosing request phase and assigned
to the submitting NVTX child by CUDA correlation ID. Submission wall
intervals include blocked CUDA API calls and can assign upstream GPU
waiting to downstream host ranges. They are reported separately
from GPU work. GPU idle is the complement of the kernel/copy union.
Event sums below do not double-count work in this trace: the summed
request activity equals the union to the stated precision.

Nsight Systems 2024.5.1 reports a nonfatal 65,535-byte string limit
during import and falls back to its CUDA 12.6 collection library with
driver 13.0. The output transport also interleaved stderr inside one
stdout line. The extraction removes the interleaved records and
checks the recovered JSONL against the complete stored oracle. The
SQLite trace contains all 185 draft, verify and repair ranges and
the expected 33 target and 33 draft prefill ranges. All 549,921 GPU
kernels match unique CUDA API correlation IDs. Cross-checks include
185 × 16 = 2,960 shared target attention tiles and 185 × 16 × 3 =
8,880 corresponding merge launches. No activity records in the
measured phases lack a matching CUDA API. The timing is diagnostic.

## Prefill

Wall **53,431.79 ms**. GPU active **53,276.33 ms**, uncovered
**155.46 ms (0.29%)**. Copies occupy 2.66 ms of device time.
Target and draft GPU activities account for 50,543.95 and
2,732.38 ms, respectively. CPU API wall values include
waiting behind queued kernels and do not add to these costs.

| GPU work | Time, ms |
| --- | ---: |
| Target FFN down projections | 7,315.33 |
| Target FFN gate projections | 6,660.07 |
| Target FFN up projections | 6,610.89 |
| Other target projections, including output | 9,421.26 |
| Target Flash Attention including tile merges | 14,704.47 |
| Target GDN recurrence, decay and convolution | 2,731.03 |
| Other target kernels | 3,100.77 |
| Draft KV-only projections | 2,696.53 |
| Other draft kernels | 33.31 |

Projection groups include their marked packing operations.
The largest individual kernels are
`bonsai_pq2_cached_64x128_k64` (27,332.26 ms),
`bonsai_flash_kv_lean2` (11,236.33 ms) and
`bonsai_flash_compact_ordered_merge` (2,729.20 ms).
The three FFN projections alone account for 20,586.29 ms.
The draft Q4 packing/projection kernel uses 2,694.67 ms.
Prefill performs 671.74 MB of D2D copies, but its entire
GPU-copy union is 2.66 ms.

## Decode

Wall **9,570.43 ms**. GPU active **9,237.24 ms**, uncovered
**333.19 ms (3.48%)**. At the point of optimization, the
exact target accounts for most of the remaining cost.

| GPU work | Time, ms | Per verifier block, ms |
| --- | ---: | ---: |
| Target Q4 attention | 3,660.59 | 19.787 |
| Target PQ2 projections and their packing | 3,542.86 | 19.151 |
| Target GDN recurrence, decay and convolution | 348.01 | 1.881 |
| Other target activity | 428.40 | 2.316 |
| Two draft proposals | 1,180.28 | 6.380 |
| Accepted draft history repair | 77.10 | 0.417 |

Within target attention, `bonsai_verify_q4_gqa2` takes
2,325.73 ms and `goose_q4_attention_merge` 1,334.86 ms.
The target PQ2 path is dominated by
`bonsai_pq2_mvq_q8_three_2` at 3,133.90 ms.
Within draft work, the shared full-vocabulary FP32 head
`bonsai_pq2_matvec_rows2` takes 607.32 ms over 370 calls.
The measured post-prefill D2H payload is 11,100 bytes,
confirming EXP-2034's transfer count. GPU choice kernels
take 4.36 ms across target and draft.

The CUDA allocation log records 9,559,940,200 live bytes
(9,117.07 MiB) of explicit model, workspace, draft, and
choice allocations. This excludes the CUDA driver, display
and implicit runtime overhead.

## Priorities derived from the trace

1. **Prefill, P0 — PQ2 MMQ.** Measure exact throughput and
   occupancy by FFN down, up/gate and remaining projection
   groups on the real 1024-token tile. Test a new reuse
   strategy on FFN down first, including its 17,408-wide
   input. Preserve EXP-2019 dequantization and K64 reduction
   order in the exact arm. EXP-2022 already tested the
   half2/LUT substitution without a consistent large-batch
   win. Use the same model and exact logits/IDs for the next
   full paired measurement. Arithmetic-changing paths require
   the existing fixed-history and quality gates.
2. **Prefill, P0 — Flash tile and merge work.** At 1093.21
   tok/s in the reference, the +70% goal allows 17.63 s.
   Eliminating the entire measured 30.01 s of projection
   activity would still leave 23.42 s of the captured wall
   time under a fixed-other-cost calculation. Attention and
   projection must both improve. Investigate panel reuse
   and fewer partial writes while preserving ordered merge
   arithmetic. A larger scratch wave already lost in EXP-2007.
3. **Decode, P0 — Exact target attention and PQ2 verifier.**
   First test a single dispatch for the three per-row tile
   merges, then test a wider parallel merge retaining the
   original per-tile FP32 recurrence. Component gate: all
   partials and logits across the 256 and 32,768 key
   boundaries. The full merge is a 1.335 s measured upper
   bound, so launch fusion alone cannot recover that amount.
   Independently measure larger PQ2 row or token tiles
   before enlarging the MTP proposal count.
4. **Decode, P1 — Draft head.** Replace full-logit materialization
   or use a scoped Q8 projection only after measuring draft
   acceptance, unchanged verified tokens and full wall time.
   The measured draft output head is 607 ms, while removing
   all uncovered GPU gaps would save at most 333 ms.
   For comparison, +70% of the 47.97 tok/s reference allows
   6.27 s for 511 decode intervals, about 3.30 s below this
   trace. This requires target-path gains beyond host scheduling.

All bounds assume unchanged other costs. No optimization or
release change is claimed from profiling.

## Reproduction

Run from the root with EXP-2025 and EXP-2030 through EXP-2034
build trees present.

```bash
exp=experiments/EXP-2026-11-25-2035-bonsai-mtp-phase-budget
out=build/bonsai-mtp-budget-repro
model=/home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf
req=experiments/EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json
oracle=experiments/EXP-2026-11-25-2031-bonsai-mtp-greedy/data/32768-greedy.jsonl
cubin=build/bonsai-mtp-choice-2034/choice.cubin
frozen=build/bonsai-mtp-head-2025/frozen-goose.ptx
bash "$exp/source/build.sh" "$out"
read -r -a prompt < <(python3 -c 'import json,sys; print(*json.load(open(sys.argv[1]))["prompt"])' "$req")
profile=(env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 BONSAI_UBATCH=1024 BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1 BONSAI_FLASH_ORDERED_MERGE=1 BONSAI_FLASH_COMPACT=1 BONSAI_FLASH_ASYNC=1 BONSAI_MTP_GREEDY=1 BONSAI_MTP_BATCH_PREFILL=1 BONSAI_MTP_VERIFY_Q8=1 BONSAI_MTP_VERIFY_EXACT=1 BONSAI_MTP_DEVICE_COMMIT=1 BONSAI_MTP_VERIFY_SHARED_ATTN=1 BONSAI_MTP_DEVICE_CHOICE=1)
"${profile[@]}" taskset 0f0f /usr/local/bin/nsys profile --sample=none --cpuctxsw=none --trace=cuda,nvtx --cuda-memory-usage=true --stats=false --output="$out/trace" "$out/bonsai-mtp-profile" "$model" "$cubin" "$frozen" 36864 512 --ignore-eos "${prompt[@]}" >"$out/trace.log" 2>&1
python3 "$exp/source/normalize.py" "$out/trace.log" "$oracle" "$out/tokens.jsonl"
/usr/local/bin/nsys export --type=sqlite --output="$out/trace.sqlite" "$out/trace.nsys-rep"
python3 "$exp/source/analyze.py" "$out/trace.sqlite" >"$out/budget.json"
```

[Measured phase data and hashes](data/)
