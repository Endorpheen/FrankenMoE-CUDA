# EXP-2026-11-25-2048 — Full-tile Bonsai PQ2 MMQ specialization

**Rejected for promotion.** The hypothesis was that static input widths and
branch-free complete tiles might remove enough overhead to matter on the
accepted EXP-2019 prefill path. The candidate selects two width-specific K64
kernels, for 5,120 and 17,408 input columns, only on complete 64×128 tiles.
The existing kernel handles tails and other widths. The half staging,
packed PQ2 layout and FP32 MMA accumulation order are identical.

The SM89 compiler uses 64 registers instead of 72, with 27,648 B of shared
memory and no spills in either arm. A CUDA component check compares all
33,554,432 FP32 outputs bitwise on three actual GGUF matrices at 1,024 tokens.

## RTX 4070 control

One sequential pair uses the same candidate executable and CUDA image,
the EXP-1977 raw request, context 36,864, batch/ubatch 4096/1024,
Q4_0 K/V and greedy generation with MTP off.

| Arm | Prefill, tok/s | Decode, tok/s | Client, s | VRAM after request, MiB | VmHWM, KiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| A, generic | 646.38 | 32.70 | 66.344 | 8932 | 7457420 |
| B, full tiles | 649.84 | 32.72 | 66.064 | 8932 | 7457936 |

Every returned ID and the completion text match the EXP-2019 rebuild
control. Prefill saves 270 ms, or 0.54% throughput in one pair. This is
smaller than the 382 ms difference between the generic arm and the prior
EXP-2019 rebuild control. The prior run is context for scale, not a paired
noise estimate. The observation does not justify a reverse pair for a
roughly 19.9 s gap to parity. No runtime default or other architecture
artifact was changed. Do not treat this as a verified speedup.

## Reproduce

Build the [EXP-2019 HTTP reference](../EXP-2026-11-25-2019-bonsai-mmq-pipeline/README.md)
and set `FRANKENMOE_CHECKPOINTS` to the saved build tree.

```bash
exp=experiments/EXP-2026-11-25-2048-bonsai-mmq-full-tile
bash "$exp/source/build.sh" build/bonsai-mmq-full-tile-2048 build/bonsai-http-reproduction
CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check.py" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration/inventory.json" \
  build/bonsai-mmq-full-tile-2048/pipeline.cubin
```

For HTTP, use the EXP-2019 profile and set `BONSAI_CUDA_IMAGE` to the candidate
image. Use `BONSAI_MMQ_FULL_TILE=0` for A and omit it for B. Run the exact
EXP-1977 32K/512 request separately in each server.

[Machine-readable results and hashes](data/results.json).
