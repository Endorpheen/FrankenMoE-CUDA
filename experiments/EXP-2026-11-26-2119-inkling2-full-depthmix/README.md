# EXP-2026-11-26-2119: Extend resident DepthMix to Inkling full attention

Build on EXP-2118. Compute all three full-attention inputs with the existing
device plane table and stage them in the retired raw attention workspace.
Extend eligible prefill MoE handoffs to subsequent full-attention visits.
The fixed arena size is unchanged. Set I2_FULL_DEVICE_DEPTHMIX=0 to disable
the new full-attention path and its additional handoffs. Block boundaries
and loop rewinds retain the existing host state path.

| RTX 4070, frozen 32K/512 | Prefill tok/s | Decode tok/s |
| --- | ---: | ---: |
| EXP-2118 pair, residency disabled | 319.238 | 35.919 |
| EXP-2118 pair, residency enabled | 344.094 | 36.379 |
| EXP-2119 candidate | 346.457 | 36.033 |

All 512 token IDs, text and stop status match EXP-2118. The log shows 64
full-attention device mixes and 280 resident MoE handoffs, up from 224.
Peak RSS was 14231280 KiB, process swap at end zero, system swap-out delta
zero and disk reads zero bytes. Board VRAM at request end was 11670 MiB;
peak board usage was not collected. The 0.68% prefill increase and 0.95%
decode decline are separate-run observations. Paired confirmation and
shared-runtime cross-model qualification remain open.

The local executable and full evidence are in build/inkling2-checkpoints/2119.
Rebuild with the exact EXP-2118 source tree:

```bash
bash source/build.sh /path/to/2118/source /path/to/hybrid-worktree /path/to/new-build-directory
```

Next keep boundary snapshots and loop history resident, then use a trace
to audit the remaining transfers by producer and consumer.
