# EXP-2026-11-26-2122: Keep Inkling rewind and tail activations on CUDA

Build on EXP-2121. Keep the final MoE output of each previous pass on
CUDA and normalize it directly into the next pass's loop storage.
At the final block, reuse a retired loop plane for the current
activation, preserve history at layer 18 and pass the tail MoE outputs
directly to the next visits. Set I2_DEVICE_REWIND_TAIL=0 to disable the
extended residency. The arena layout and allocation sizes are unchanged.

| RTX 4070, frozen 32K/512 | Prefill tok/s | Decode tok/s |
| --- | ---: | ---: |
| EXP-2121 checkpoint | 386.999 | 35.603 |
| EXP-2122 candidate | 393.665 | 35.726 |

All 512 token IDs, text and stop status match EXP-2121. The trace log
shows 16 resident rewinds, 8 resident boundaries at layer 18 and 368
MoE handoffs, up from 304. Disk reads, end process swap and system
swap-out delta are zero. Peak RSS is 13933968 KiB (53100 KiB below
EXP-2121); end board VRAM is 11662 MiB in both runs. Peak board VRAM was
not collected. Single-run prefill improves by 1.72% and decode by 0.35%.
Paired and cross-model acceptance remain open.

The local executable and full evidence are in build/inkling2-checkpoints/2122.
Rebuild with the exact EXP-2121 source tree:

```bash
bash source/build.sh /path/to/2121/source /path/to/hybrid-worktree /path/to/new-build-directory
```

The first entry to the loop still saves a host prelude, and final output
still reads back for the last logit and diagnostics. Audit those transfers
and profile compute before closing the residency work.
