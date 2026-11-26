# EXP-2026-11-26-2128: Skip unused Inkling host staging

Build on EXP-2126. Skip CPU preparation when the CUDA-resident attention
or MoE path has no remaining host consumer. Preserve boundary bookkeeping,
rewinds and host fallbacks. Stop writing full host shadow buffers with NaN
during performance runs. Set I2_PRUNE_PREFILL_HOST_STAGING=0 or enable
I2_CHECK_STATE to retain the diagnostic behavior.

| RTX 4070, frozen 32K/512 | Prefill tok/s | Decode tok/s |
| --- | ---: | ---: |
| EXP-2126 checkpoint | 409.620 | 36.239 |
| EXP-2128 candidate | 436.581 | 35.995 |

All 512 token IDs, text and stop status match EXP-2126. Logs show 352
attention preparations and 320 MoE preparations skipped. Disk reads,
end process swap and system swap-out delta were zero. Peak RSS is
13566552 KiB, 257988 KiB below EXP-2126. End board usage is 11668 MiB;
peak VRAM was not collected. The single run observes a 6.58% prefill
increase and a 0.67% decode decrease. Paired and cross-model acceptance
remain open.

The local executable and full evidence are in build/inkling2-checkpoints/2128.
Rebuild with the exact EXP-2126 source tree:

```bash
bash source/build.sh /path/to/2126/source /path/to/hybrid-worktree /path/to/new-build-directory
```
