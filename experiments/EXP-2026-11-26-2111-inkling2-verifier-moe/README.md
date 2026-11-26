# EXP-2026-11-26-2111: Single-job IQ4 dispatch for the Inkling verifier

Use the EXP-2110 executable, frozen request and profile as the direct control.
The candidate assigns one expert job per warp during short speculative
verification and enables the existing IQ4_XS block path when its width is
eligible. The large prefill and ordinary decode dispatch are unaffected.

| RTX 4070, 32K/512 | Prefill tok/s | Decode tok/s |
| --- | ---: | ---: |
| EXP-2110 direct control | 309.460 | 30.321 |
| EXP-2111 candidate | 310.736 | 32.100 |

All 512 output IDs, text and stop status match EXP-2110. The log confirms the
new verifier dispatch. The decode observation is 5.87% higher in this single
run. Peak RSS was 14231536 KiB, process swap at end was zero and system
swap-out delta was zero pages. End board usage was 11661 MiB; peak board usage
was not collected. This is a cumulative development checkpoint and has not
passed paired performance, partial rejection or Apex release qualification.

The local executable and full evidence are in build/inkling2-checkpoints/2111.
From this directory, rebuild using the saved EXP-2110 source directory:

```bash
bash source/build.sh /path/to/2110/source /path/to/hybrid-worktree /path/to/new-build-directory
```

Further Inkling candidates build on this checkpoint and compare with its
binary, profile and request. The remaining decode gap is about 1.32 s for
512 generated tokens.
