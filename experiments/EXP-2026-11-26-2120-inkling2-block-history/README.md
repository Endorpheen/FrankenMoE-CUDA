# EXP-2026-11-26-2120: Retain Inkling block boundary history on CUDA

Build on EXP-2119. Preserve the current activation when entering physical
layer 12 and store its normalized history in the existing CUDA arena.
Rewinds read the history plane for the current host loop-message consumer.
The final logits read only the last token's history row. Arena size is
unchanged. Set I2_DEVICE_BLOCK_HISTORY=0 to disable the new path.

| RTX 4070, frozen 32K/512 | Prefill tok/s | Decode tok/s |
| --- | ---: | ---: |
| EXP-2119 checkpoint | 346.457 | 36.033 |
| EXP-2120 candidate | 350.727 | 35.729 |

All output IDs, text and stop status match EXP-2119. The log shows 24
resident block boundaries and 304 MoE handoffs, up from 280. Peak RSS
was 14231548 KiB, process swap at end zero, system swap-out delta zero,
disk reads zero bytes and end board VRAM 11668 MiB. Peak VRAM not collected.
Single-run prefill improves by 1.23% and decode declines 0.84%. Paired and
cross-model acceptance remain open.

The local executable and full evidence are in build/inkling2-checkpoints/2120.
Rebuild with the exact EXP-2119 source tree:

```bash
bash source/build.sh /path/to/2119/source /path/to/hybrid-worktree /path/to/new-build-directory
```

The remaining loop-message and rewind paths still depend on host history.
The next step must preserve the prior pass output and bank through both
rewinds and remove the remaining full-plane history readbacks.
