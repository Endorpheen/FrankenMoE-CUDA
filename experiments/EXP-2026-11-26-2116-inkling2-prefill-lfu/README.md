# EXP-2026-11-26-2116: Reuse Inkling experts across prefill passes

Use the EXP-2113 frozen request, profile and executable as the direct control.
The candidate enables existing Inkling LFU for 4096-token prompt batches and
resets only the learned frequency and age counters when entering decode.
The expert bank retains its contents. Set I2_PREFILL_LFU=0 to disable the path.
The changed header is specific to Inkling.

| RTX 4070, 32K/512 | Prefill tok/s | Decode tok/s | Expert H2D during prefill |
| --- | ---: | ---: | ---: |
| EXP-2113 fresh control | 310.125 | 35.804 | 89.540 GB |
| EXP-2116 candidate | 317.548 | 36.325 | 49.363 GB |

All 512 output IDs, text and stop status match EXP-2113. The observed prefill
improvement is 2.39%; expert transfer falls 44.9%. Peak RSS was 14231056 KiB,
compared with 14231432 KiB in the fresh control. Both runs had zero process
swap at end and zero system swap-out delta. End board usage was 11678 MiB
versus 11668 MiB, and peak board usage was not collected. The candidate is
a cumulative development checkpoint. Paired confirmation and cross-model
release gates are open. EXP-2113 remains the acceptance reference.

The local executable and full evidence are in build/inkling2-checkpoints/2116.
Rebuild with the exact EXP-2113 source tree:

```bash
bash source/build.sh /path/to/2113/source /path/to/hybrid-worktree /path/to/new-build-directory
```

The next phase targets 600 prefill tok/s on 32K/512 while preserving decode,
correctness, memory and cross-model behavior. After prefill acceptance,
target 50 decode tok/s on the same workload.
