# EXP-2026-11-26-2113: Preserve LFU during Inkling verification

The 16-token verifier uses the decode expert-cache phase and the existing LFU
eviction order. Size the victim list for the model's 128 experts. The initial
probe with the original eight-victim cap aborted in the first verifier MoE
operation. Larger batches can require more simultaneous misses. The 4096-token
prefill and its expert-cache policy are unchanged. Set I2_VERIFY_LFU=0 to
disable the verifier policy.

| RTX 4070, 32K/512 | Prefill tok/s | Decode tok/s |
| --- | ---: | ---: |
| EXP-2111 direct control | 310.736 | 32.100 |
| EXP-2113 cumulative candidate | 307.698 | 36.121 |
| Objective | 600 | 35 |

All 512 output IDs, text and stop status match EXP-2111. The single candidate
run improves decode by 12.53% and observes a 0.98% prefill decline. Neither
difference has paired confirmation. The verifier records 32 groups, 7090
expert fills and 30579793920 bytes of expert CPU-to-GPU transfer. Equivalent
traffic was not recorded for the EXP-2111 arm. Peak RSS was 14230812 KiB, process
swap at end zero, system swap-out delta zero and end board usage 11670 MiB.
Peak board usage was not collected. The separate release gates remain open.

The local executable and full evidence are in build/inkling2-checkpoints/2113.
Rebuild with the exact EXP-2111 source tree:

```bash
bash source/build.sh /path/to/2111/source /path/to/hybrid-worktree /path/to/new-build-directory
```

The next Inkling candidate uses this binary and profile as its direct control.
The 35 tok/s decode threshold was observed on the frozen fixture. The remaining
goal is to confirm this decode rate while closing the prefill gap.
