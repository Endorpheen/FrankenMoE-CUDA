# EXP-2026-11-26-2118: Keep Inkling MoE output resident across sliding visits

The candidate builds on EXP-2116 and combines the sliding attention CUDA
DepthMix path with a direct MoE handoff. The next sliding visit can consume
the current activation from a free history plane. Rewinds, residual history
boundaries, the final layers and places with CPU state consumers keep their
existing host path. Set I2_RETAIN_PREFILL_MOE=0 to disable the handoff or
I2_SLIDING_DEVICE_DEPTHMIX=0 to disable both changes.

| RTX 4070, frozen 32K/512 | Prefill tok/s | Decode tok/s |
| --- | ---: | ---: |
| EXP-2116 checkpoint | 317.548 | 36.325 |
| EXP-2118 candidate | 343.948 | 35.814 |

All 512 token IDs, text and stop status match EXP-2116. The candidate logs
224 resident MoE handoffs, reads no bytes from disk, and adds no process
or system swap. Peak RSS is 14231092 KiB (36 KiB above the checkpoint);
board VRAM at request end is 11662 MiB (16 MiB below the checkpoint).
Peak board VRAM was not collected. The single candidate run observes
8.31% higher prefill and 1.41% lower decode. The measured decode stays
above 35 tok/s. Paired and shared-runtime cross-model acceptance remain open.

The local executable and full evidence are in build/inkling2-checkpoints/2118.
Rebuild with the exact EXP-2116 source tree:

```bash
bash source/build.sh /path/to/2116/source /path/to/hybrid-worktree /path/to/new-build-directory
```
