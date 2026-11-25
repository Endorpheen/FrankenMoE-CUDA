# EXP-2026-11-25-2001 — Bonsai K32 PQ2 MMQ tiles

**INCONCLUSIVE at request level.** Keep EXP-2000 K64 as the accepted
checkpoint. Smaller K staging and alternative warp geometries preserve
the FP16 inputs and the F32 accumulation order. The component timings
justify a narrow test on full FFN gate/up batches. The full request
saves only 5.375 ms on average and the reverse pair saves 0.324 ms.

| 4097/16 run | Prefill, ms | Decode, ms |
| --- | ---: | ---: |
| A1 K64 | 5038.945 | 974.628 |
| B1 K32 | 5028.520 | 974.221 |
| B2 K32 | 5021.974 | 972.172 |
| A2 K64 | 5022.298 | 972.608 |

Mean prefill is 814.41 versus 815.28 tok/s (+0.107%). Mean decode
is 16.43 versus 16.44 tok/s. The candidate mean is 5025.247 ms,
4205.847 ms above the 819.4 ms budget at 5000 tok/s for this input.
This test does not establish a 32K speedup.

The candidate selects the 64x128 K32 tile with four warps for width
5120, 17408 output rows and exactly 1024 batch tokens. All other
projections keep the control dispatch. The new tile uses 94 registers,
15,360 bytes static shared memory and zero spills. K64 64x128 uses 56
registers and 27,648 bytes. At 1024 inputs, layer 0 gate/up improve
from 3.686/3.880 to 3.581/3.785 ms. Down projection results depend on
the batch shape and layer. The lookup-table variants add no consistent
gain. Component timings and all 12 bitwise checks are in
[data/component.json](data/component.json).

The A–B–B–A runs use one binary and an identical 4097-token prefix
of the [EXP-1977 request](../EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json).
RTX 4070, context 36864, ubatch 1024, Q4_0 K/V, no MTP, greedy, and
ignore EOS. All four outputs match byte for byte, including the
16 selected-token logits, and all 16 IDs match the saved PrismML
reference. Per-run logs report 7,481,217,024 bytes of device weight
payload and 1,720,400,640 bytes of workspace in both modes. Peak VRAM
and RSS were not sampled. The experiment changes only the standalone
Bonsai path. Apex, Inkling and their shared runtime are untouched.

## Reproduction

Run from the repository root. Build into a fresh directory, then run
the command once for each mode in the order 0, 1, 1, 0.

```bash
ex=experiments/EXP-2026-11-25-2001-bonsai-mmq-k32
out=build/bonsai-mmq-k32-repro
bash "$ex/source/build.sh" "$out"
read -r -a ids < <(python3 -c 'import json,sys; print(*json.load(open(sys.argv[1]))["prompt"][:4097])' \
    experiments/EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json)
mode=0
env -i PATH=/usr/bin:/bin HOME="$HOME" LANG=C CUDA_VISIBLE_DEVICES=0 \
    BONSAI_UBATCH=1024 BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1 \
    BONSAI_FLASH_ORDERED_MERGE=1 BONSAI_FLASH_COMPACT=1 BONSAI_FLASH_ASYNC=1 \
    BONSAI_PQ2_MMQ_K32_GATE="$mode" taskset 0f0f "$out/bonsai-mmq-k32" \
    /home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf \
    "$out/mmq-k32.cubin" "$out/frozen-goose.ptx" 36864 16 --ignore-eos "${ids[@]}"
```

For a future change to activation arithmetic, the INT8 quality gate
from EXP-1994 and EXP-1996 remains required in addition to fixed-workload
ID checks. Compare full-vocabulary logits and token choices on a
shared history, then compare independently generated long responses
and score diverse difficult tasks against independent answers.
