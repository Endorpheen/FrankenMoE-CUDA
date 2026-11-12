# Reproducible point: expert device cache 3584 MiB (candidate base)

Status: measured candidate, not yet the approved base (that is 3072). The exact-content caveat is identical to every
other cache point and has not been waived.

## Command

Base argv is unchanged from `runtime/exp244/results-apex-or244-or-goose/argv.json`; add exactly one flag:

    --moe-cache-mib 3584

Runner: `python3 runtime/reconstructed/run_metrics.py`. Same model, same 32K prompt, seed and sampling.

## Measurements on the current build (full 3072-config port, pinning, EXP087, EXP101)

| generated tokens | decode | prefill | VRAM MiB |
|---|---|---|---|
| 64 | 37.89 (38.12 / 37.65) | 2325.2 | 6778 |
| 128 | 39.61 (39.55 / 39.65 / 39.62) | 2325.5 | 6778 |
| 256 | 41.01 (40.93 / 41.09) | 2326.5 | 6778 |
| 512 | 41.94 (41.95 / 41.93) | 2326.8 | 6778 |

VRAM is identical at every length, so there is no eviction pressure up to 512 generated tokens; the rising curve is
warm-up. Compare with the approved base 3072: 37.23 / 37.79 / 39.10 at 64/128/256, i.e. 3584 is ahead everywhere for
+512 MiB, and with 4096: 40.78 at 128 for +1026 MiB.

Efficiency: the step 3072 -> 3584 buys 3.6 tok/s per GiB, the step 3584 -> 4096 only 2.3.

## Rollback

No build change - it is one CLI flag. Drop `--moe-cache-mib` for the exact path (2311-2345 / ~24 / 3190, e68df05b), or set
2048/3072/4096 for the other ladder points. Library backups and checksums:
`runtime/checkpoints/CONTROL-OPT-20261112/artifact/SHA256SUMS`.
