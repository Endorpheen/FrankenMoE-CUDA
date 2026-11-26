# EXP-2026-11-26-2126: Expand Inkling Q8 projection weights to FP32

Build on EXP-2123. For selected Q8 projections at 4096 input rows, expand
the weights once per projection and reuse the expanded values across
input tiles. The FP32 FMA and reduction order is preserved. Device tests
match all output bytes, including the selected major projection shapes.
Timings include weight expansion and input packing. Both temporary arrays
reuse the existing MMQ workspace; selected combined capacity is at most
75 MiB. Set I2_Q8_FP32_EXPAND=0 to disable the new path.

| RTX 4070, frozen 32K/512 | Prefill tok/s | Decode tok/s |
| --- | ---: | ---: |
| EXP-2123 checkpoint | 395.904 | 36.270 |
| EXP-2126 candidate | 409.620 | 36.239 |

All 512 token IDs, text and stop status match EXP-2123. Disk reads,
end process swap and system swap-out delta were zero. Peak RSS was
13824540 KiB and end board VRAM 11656 MiB. Peak VRAM not collected.
The single run observes a 3.46% prefill increase and a 0.09% decode
decline. Paired and cross-model acceptance remain open.

The local executable and full evidence are in build/inkling2-checkpoints/2126.
Rebuild with the exact EXP-2123 source tree:

```bash
bash source/build.sh /path/to/2123/source /path/to/hybrid-worktree /path/to/new-build-directory
```
