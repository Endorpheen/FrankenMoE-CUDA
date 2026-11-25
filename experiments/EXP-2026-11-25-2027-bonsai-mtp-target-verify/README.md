# EXP-2026-11-25-2027 — Bonsai MTP target verification and rollback

**Diagnostic rollback passed. Direct batch commit failed the bitwise
state gate.** The isolated adapter accepts up to three logits rows
from the existing Goose target graph. It allocates two additional
vocabulary rows (1,986,560 bytes) and leaves the CUDA image unchanged.
The production baseline stays at EXP-2019.

## Method and evidence

RTX 4070, Bonsai PQ2_0-mtp, context 1024, ubatch 128, Q4_0 K/V.
Use the first 32 and 129 IDs of EXP-1977. For each prefix, generate
the control continuation with the ordinary single-row Q8 decode.
Then evaluate three serially selected inputs in one target call
and compare every position's 248,320 logits and final state. In a
second three-row call, replace the third input with a forced error.
Check the first two logit rows for causal isolation, reject the last
proposal, and restore the target checkpoint. Replay the two retained
inputs in ordinary decode to compare the accepted state and
subsequent logits with the original control.

| Input length | Batch greedy match | Whole-logit relative L2 | State planes changed by direct batch commit | Forced reject → replacement |
| ---: | ---: | ---: | ---: | --- |
| 32 | 3/3 | 0.322–0.391% | 128/128 | 1001 → 1000 |
| 129 | 3/3 | 0.586–0.656% | 128/128 | 12905 → 12904 |

Replacing the last input left the preceding two logit rows
byte-identical in both runs. Checkpoint restore and ordinary replay
matched 157,519,872 and 159,307,776 bytes, respectively, including
the visible Q4 K/V prefix, GDN convolution histories and recurrent
states. The logits immediately after replay and after applying the
correct replacement match the serial control byte for byte
(496,640 floats per fixture). Restore writes through each layer's
current logical state pointer because GDN swaps its output and
scratch pointers between evaluations.

Three host snapshots and debug comparisons are used only for this
probe. The measured restore payload per transaction is 157,538,304
or 159,326,208 bytes, including the three tentative KV rows.
Prefill/decode throughput and peak RAM/VRAM were not measured.
These fixed proposals do not establish an integrated MTP acceptance
rate. The diagnostic does not change Apex or Inkling code or builds.

## Implication for the target verifier

Bonsai's single-row path invokes Q8 DP4A for eligible PQ2
projections. The three-row path instead takes the FP16-packed
Tensor Core MMQ route. Convolution, rotary application and GDN
also select different implementations at the multi-row boundary.
No single operation was isolated as the complete cause of the
state divergence.

The next exact candidate should carry the single-row Q8 quantization,
dot, accumulation and reduction order into a bounded multi-row
dispatch. Compare each layer's state against serial before fusing
the next operation. The rollback test is reusable for candidates
with append-only KV and separately stored recurrent state. For
timed runs, rollback must avoid the diagnostic host copies and
support committing accepted states without serially recomputing
the entire prefix.

## Reproduction

Run from the root with the EXP-2025 build already present.

```bash
exp=experiments/EXP-2026-11-25-2027-bonsai-mtp-target-verify
out=build/bonsai-mtp-target-repro
previous=build/bonsai-mtp-head-2025
req=experiments/EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json
model=/home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf
bash "$exp/source/build.sh" "$out"
read -r -a prompt < <(python3 -c 'import json,sys; print(*json.load(open(sys.argv[1]))["prompt"][:129])' "$req")
profile=(env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 BONSAI_UBATCH=128 BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1 BONSAI_FLASH_ORDERED_MERGE=1 BONSAI_FLASH_COMPACT=1 BONSAI_FLASH_ASYNC=1 BONSAI_MTP_TARGET_PROBE=1)
"${profile[@]}" taskset 0f0f "$out/target-probe" "$model" "$previous/stem.cubin" "$previous/frozen-goose.ptx" 1024 4 --ignore-eos "${prompt[@]:0:32}"
"${profile[@]}" taskset 0f0f "$out/target-probe" "$model" "$previous/stem.cubin" "$previous/frozen-goose.ptx" 1024 4 --ignore-eos "${prompt[@]}"
```

[Measured outputs and hashes](data/)
