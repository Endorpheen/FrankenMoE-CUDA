# EXP-2026-11-26-2123: Keep Inkling prelude and final prefill outputs resident

Build on EXP-2122. Store the raw prelude and reciprocal RMS scales in the
existing CUDA arena and reconstruct its normalized history when consumed.
The final MoE output remains on CUDA; only the last row is copied for
logits. Full activation diagnostics keep the existing host path.
Set I2_DEVICE_PREFILL_EDGES=0 to disable the new path.

| RTX 4070, frozen 32K/512 | Prefill tok/s | Decode tok/s |
| --- | ---: | ---: |
| EXP-2122 checkpoint | 393.665 | 35.726 |
| EXP-2123 candidate | 395.904 | 36.270 |

All 512 token IDs, text and stop status match EXP-2122. The log shows
eight resident preludes, eight resident final activations and 384
resident MoE outputs. Disk reads, end process swap and system swap-out
delta are zero. Peak RSS is 13826244 KiB (107724 KiB below EXP-2122).
End board usage rose from 11662 to 11670 MiB, and peak VRAM was not
collected. Single-run prefill improves by 0.57%, decode by 1.52%.
Paired and cross-model acceptance remain open.

The local executable and full evidence are in build/inkling2-checkpoints/2123.
Rebuild with the exact EXP-2122 source tree:

```bash
bash source/build.sh /path/to/2122/source /path/to/hybrid-worktree /path/to/new-build-directory
```

Profile remaining D2H/H2D and the critical compute path on this checkpoint.
