# Apex test plan — staged Goose runtime against the working Apex baseline

Status: **draft, not executed**. No server, model or GPU run happens until Igor
explicitly approves this plan (goal, duration, number of runs).

## Baseline to preserve

| item | value |
|---|---|
| model | `Apex-35B-V1.0-Q4_K_M.gguf` |
| flags | `--cpu-moe -ngl all -fa on`, KV `q4_0`, `-c 262144`, `-b 4096 -ub 4096`, `-t 8 -tb 16`, `--moe-cache-mib 3584` |
| prefill | 2228.76 tok/s |
| decode | 41.62 tok/s (29–33 tok/s without the GPU expert cache) |
| output | 128 predicted tokens, content sha `9d90a010…`, exit 0 |

The baseline binary and profile stay untouched; every run uses a staged copy.

## What is compared

1. **Correctness**: same request, same flags, identical output hash, or a
   documented intentional difference; unit/differential gates green.
2. **Performance**: prefill and decode, medians and ranges.
3. **Memory**: VRAM and RSS, plus swap and major faults.
4. **Stability**: exit code, restart behaviour, no leaked child processes.

## Run protocol

- Arms: baseline (A) and staged (B), alternating A/B/B/A.
- Per arm: 1 warmup + 5 measured runs, same model, same GPU (RTX 4070 12 GB),
  same flags, no other GPU/CPU load.
- Report medians and ranges per arm, `B/A` ratios and the paired spread.
- Acceptance: no correctness difference and no regression above 2% beyond the
  observed spread; any regression larger than the spread is investigated before
  the stage is kept.
- Rollback: switch the stage off, restore the previous artifact; the working
  Apex binary is never overwritten.

## Order of stages

1. S1.1 schema converter (EXP626).
2. S1.2 GGUF (EXP634), S1.3 quantizers (EXP635) — quantizers last within S1
   because they sit on the decode path.
3. S1.4 metrics (EXP628), S1.5 tokenizer (EXP639), S1.6 streaming (EXP637).
4. S2.1 config (EXP630), S2.2 speculative (EXP640).
5. S3.1 model management (EXP642), S3.2 session (EXP643).

Each stage is accepted separately; a failed stage is reverted before the next.

## What Igor must approve

- The exact goal and success criterion for the first A/B.
- The number of runs (default: 1 warmup + 5 measured per arm, two arms).
- The expected duration (one run is minutes, the full pair set is roughly an
  hour on the current host).
- Whether the staged binary may be built with all ten flags ON for the first
  run, or one stage at a time.

Until that approval, only builds, unit gates and symbol checks are run.
