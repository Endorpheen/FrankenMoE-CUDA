# EXP-2026-11-25-2028 — Exact three-row Bonsai Q8 projections

**Experimental component.** Extend the single-token Q8 DP4A
mechanism from EXP-2012 to three token rows for the EXP-2027
verification graph. Both paths use the same Q8 packet kernel.
Each warp loads and unpacks each code pair once for three tokens.
Each independent accumulator follows the same integer dot,
floating-point scale and summation order as serial Q8 decode.
The candidate uses one pack launch and one projection launch
instead of three pairs. No serving changes.

## Component result

RTX 4070, SM89, Bonsai PQ2_0-mtp. Exact comparison of 843,261
FP32 outputs and 110,592 bytes of Q8 packets across four real
GGUF tensors. Inputs include a zero block, ties and a lower-scale
row. The QKV tensor is trimmed by one output row to verify the
partial CTA. CUDA event medians cover packing plus projection of
all three rows. One process, six samples per arm, 12 repetitions
per sample, with each control/candidate block in A-B-B-A order.
The matrices are already resident and repeatedly accessed.

| Matrix | Serial, ms | Shared, ms | Reduction |
| --- | ---: | ---: | ---: |
| QKV, 10,239 rows | 0.04739 | 0.02853 | 39.8% |
| FFN gate | 0.07108 | 0.04517 | 36.5% |
| FFN down | 0.07839 | 0.04698 | 40.1% |
| Vocabulary | 2.26539 | 0.99162 | 56.2% |

The one-row-group variant uses 40 registers (serial 33).
The two-row-group variant uses 64 (serial 40). Neither spills.
These figures describe the projection component on this workload.
They do not estimate or establish full-target speed.

## Target verification

With context 1024, ubatch 128, Q4_0 K/V and the EXP-2027
fixed prefixes, run the same binary with candidate disabled
on 32 tokens, then enable it on 32 and 129 tokens. The control
reproduces the 32-token EXP-2027 trace byte for byte.

| Input length | Baseline differing state planes | Candidate differing state planes | Candidate logit relative L2 vs serial |
| ---: | ---: | ---: | ---: |
| 32 | 128/128 | 122/128 | 0.357–0.473% |
| 129 | 128/128 | 122/128 | 0.703–0.722% |

The six tested greedy selections and the forced rejection decisions
match the serial reference. The rollback and serial replay remain
bitwise exact. Committing the current three-row graph still fails the
bitwise state gate. In particular, greater agreement in state planes
does not imply smaller logit error. The 1024-output projections still
follow the old batch route. Single-row decode uses a different path for
them. Rotary, attention and GDN also still dispatch batch implementations.

There are no additional device allocations compared to EXP-2027.
Prefill/decode throughput, peak RAM/VRAM and integrated speculative
acceptance were not measured. The fixed three-row geometry only serves
the probe. Other PQ2 consumers can reuse the activation pack and
weight-code sharing after testing their own geometry and register budget.
Where exact target behavior is mandatory, each remaining operation must
meet the serial state and logit gate before the verifier commits a batch.

## Reproduction

Run from the root with EXP-2025 and EXP-2027 build trees present.

```bash
exp=experiments/EXP-2026-11-25-2028-bonsai-mtp-q8-batch
out=build/bonsai-mtp-q8-repro
previous=build/bonsai-mtp-head-2025
req=experiments/EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json
model=/home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf
bash "$exp/source/build.sh" "$out"
env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 taskset 0f0f /usr/bin/python3 "$exp/source/check.py" build/bonsai-integration build/bonsai-integration/inventory.json "$out/verify.cubin"
read -r -a prompt < <(python3 -c 'import json,sys; print(*json.load(open(sys.argv[1]))["prompt"][:129])' "$req")
profile=(env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 BONSAI_UBATCH=128 BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1 BONSAI_FLASH_ORDERED_MERGE=1 BONSAI_FLASH_COMPACT=1 BONSAI_FLASH_ASYNC=1 BONSAI_MTP_TARGET_PROBE=1)
"${profile[@]}" taskset 0f0f "$out/target-q8" "$model" "$out/verify.cubin" "$previous/frozen-goose.ptx" 1024 4 --ignore-eos "${prompt[@]:0:32}"
"${profile[@]}" BONSAI_MTP_VERIFY_Q8=1 taskset 0f0f "$out/target-q8" "$model" "$out/verify.cubin" "$previous/frozen-goose.ptx" 1024 4 --ignore-eos "${prompt[@]:0:32}"
"${profile[@]}" BONSAI_MTP_VERIFY_Q8=1 taskset 0f0f "$out/target-q8" "$model" "$out/verify.cubin" "$previous/frozen-goose.ptx" 1024 4 --ignore-eos "${prompt[@]}"
```

[Measured outputs and hashes](data/)
